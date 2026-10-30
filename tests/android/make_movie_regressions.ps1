param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '../../work/movie-regression'),
    [string]$Ffmpeg = 'ffmpeg',
    [string]$Ffprobe = 'ffprobe',
    [switch]$Regenerate
)
$ErrorActionPreference = 'Stop'
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null

# Every input is a generated test pattern or tone. No game assets are read.
$fixtures = @(
    @{
        Name = 'delayed-audio.mkv'
        Arguments = @('-f', 'lavfi', '-i', 'testsrc2=size=320x240:rate=30:duration=7',
            '-itsoffset', '5', '-f', 'lavfi', '-i', 'sine=frequency=880:sample_rate=44100:duration=2',
            '-map', '0:v', '-map', '1:a', '-c:v', 'mpeg1video', '-q:v', '5', '-bf', '0',
            '-c:a', 'pcm_s16le', '-ac', '2', '-t', '7', '-f', 'matroska')
    },
    @{
        Name = 'silent-tail.mpg'
        Arguments = @('-f', 'lavfi', '-i', 'testsrc2=size=640x360:rate=30:duration=8,noise=alls=20:allf=t:all_seed=1',
            '-f', 'lavfi', '-i', 'sine=frequency=660:sample_rate=44100:duration=1',
            '-map', '0:v', '-map', '1:a', '-c:v', 'mpeg1video', '-q:v', '8', '-bf', '0',
            '-c:a', 'mp2', '-b:a', '192k', '-ac', '2', '-t', '8', '-f', 'mpeg')
    },
    @{
        Name = 'slow-poll-h264.mp4'
        Arguments = @('-f', 'lavfi', '-i', 'testsrc2=size=1920x1080:rate=60:duration=6',
            '-f', 'lavfi', '-i', 'sine=frequency=550:sample_rate=44100:duration=6',
            '-map', '0:v', '-map', '1:a', '-c:v', 'libx264', '-preset', 'ultrafast',
            '-crf', '35', '-bf', '2', '-pix_fmt', 'yuv420p', '-c:a', 'aac', '-ac', '2',
            '-t', '6', '-movflags', '+faststart', '-f', 'mp4')
    },
    @{
        Name = 'synchronized-control.mpg'
        Arguments = @('-f', 'lavfi', '-i', 'testsrc2=size=320x240:rate=30:duration=3',
            '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=44100:duration=3',
            '-map', '0:v', '-map', '1:a', '-c:v', 'mpeg1video', '-q:v', '5', '-bf', '0',
            '-c:a', 'mp2', '-b:a', '192k', '-ac', '2', '-t', '3', '-f', 'mpeg')
    }
)

$results = @()
foreach ($fixture in $fixtures) {
    $path = Join-Path $outputRoot $fixture.Name
    $command = @('-hide_banner', '-loglevel', 'error', '-nostdin') + $(if ($Regenerate) { '-y' } else { '-n' }) + $fixture.Arguments + @($path)
    if ($Regenerate -or -not (Test-Path -LiteralPath $path)) {
        & $Ffmpeg @command
        if ($LASTEXITCODE -ne 0) { throw "Fixture encoding failed: $path" }
    }
    $metadataText = & $Ffprobe -v error -show_entries 'stream=index,codec_type,codec_name,start_time,duration,width,height,sample_rate:format=duration,start_time,size' -of json $path
    if ($LASTEXITCODE -ne 0) { throw "Fixture metadata failed: $path" }
    $metadata = $metadataText | ConvertFrom-Json
    $packetText = & $Ffprobe -v error -show_packets -show_entries 'packet=stream_index,pts_time,duration_time,size' -of json $path
    if ($LASTEXITCODE -ne 0) { throw "Fixture packet inspection failed: $path" }
    $packets = ($packetText | ConvertFrom-Json).packets
    $audioIndex = ($metadata.streams | Where-Object codec_type -EQ audio).index
    $videoIndex = ($metadata.streams | Where-Object codec_type -EQ video).index
    $audioPackets = @($packets | Where-Object { $_.stream_index -eq $audioIndex -and $null -ne $_.pts_time })
    $videoPackets = @($packets | Where-Object { $_.stream_index -eq $videoIndex -and $null -ne $_.pts_time })
    if (-not $audioPackets.Count -or -not $videoPackets.Count) { throw "Fixture must have timestamped audio and video: $path" }
    $audioStart = ($audioPackets | ForEach-Object { [double]$_.pts_time } | Measure-Object -Minimum).Minimum
    $audioEnd = ($audioPackets | ForEach-Object { [double]$_.pts_time + [double]$_.duration_time } | Measure-Object -Maximum).Maximum
    $videoStart = ($videoPackets | ForEach-Object { [double]$_.pts_time } | Measure-Object -Minimum).Minimum
    $videoEnd = ($videoPackets | ForEach-Object { [double]$_.pts_time + [double]$_.duration_time } | Measure-Object -Maximum).Maximum
    $tailBytes = ($videoPackets | Where-Object { [double]$_.pts_time -ge $audioEnd } | ForEach-Object { [long]$_.size } | Measure-Object -Sum).Sum
    $summary = @{
        AudioStart = $audioStart; AudioEnd = $audioEnd
        VideoStart = $videoStart; VideoEnd = $videoEnd
        AudioDelay = $audioStart - $videoStart
        AudioDuration = $audioEnd - $audioStart
        VideoDuration = $videoEnd - $videoStart
        VideoBytesAfterAudio = [long]$tailBytes
    }
    if ($fixture.Name -eq 'delayed-audio.mkv' -and ([Math]::Abs($summary.AudioDelay - 5) -gt .05 -or $summary.VideoDuration -lt 6.95)) {
        throw 'Delayed fixture must start audio five seconds after video and contain seven seconds of video.'
    }
    if ($fixture.Name -eq 'silent-tail.mpg' -and ($summary.AudioDuration -lt .95 -or $summary.AudioDuration -gt 1.1 -or $summary.VideoDuration -lt 7.95 -or $summary.VideoBytesAfterAudio -le 2MB)) {
        throw 'Tail fixture must contain one second of audio, eight seconds of video, and over 2 MiB of compressed video after audio ends.'
    }
    if ($fixture.Name -eq 'synchronized-control.mpg' -and ([Math]::Abs($summary.AudioDelay) -gt .05 -or $summary.VideoDuration -lt 2.95)) {
        throw 'Control fixture must have synchronized audio/video and three seconds of video.'
    }
    if ($fixture.Name -eq 'slow-poll-h264.mp4' -and ($videoPackets.Count -ne 360 -or $summary.VideoDuration -lt 5.95)) {
        throw 'Slow-poll fixture must contain 360 H.264 frames over six seconds.'
    }
    $results += @{ Path = $path; Command = @($Ffmpeg) + $command; Metadata = $metadata; PacketSummary = $summary }
    Write-Output ("{0}: video={1:N3}s audio={2:N3}s audio_delay={3:N3}s tail_bytes={4}" -f $fixture.Name, $summary.VideoDuration, $summary.AudioDuration, $summary.AudioDelay, $summary.VideoBytesAfterAudio)
}
$version = (& $Ffmpeg -version | Select-Object -First 1)
@{ GeneratedUtc = [DateTime]::UtcNow.ToString('o'); FfmpegVersion = $version; Fixtures = $results } |
    ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $outputRoot 'manifest.json') -Encoding utf8
