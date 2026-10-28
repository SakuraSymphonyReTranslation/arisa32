@echo off
setlocal
rem Host table generators need the Visual C++ runtime headers and linker.
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "FFMPEG_VS=%%i"
if not defined FFMPEG_VS (
  echo Install Visual Studio C++ build tools before building FFmpeg. 1>&2
  exit /b 1
)
call "%FFMPEG_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
"%ProgramFiles%\Git\bin\bash.exe" "%~dp0build_ffmpeg.sh" %*
exit /b %errorlevel%
