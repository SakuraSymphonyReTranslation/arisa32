import sys
from pathlib import Path

from cleanup_qa_libraries import Profile, main


PROFILE = Profile(
    csv_path=Path(
        r"C:\Users\adamb\AppData\Local\Temp\adv32roid-comu-cleanup-20261109-094131-1burct8b"
    ) / "dc4-analysis-cleanup-reviewed.csv",
    csv_sha256="7ae12851e4d9c8f53648840b4028e0c79c99a3868280d9af293a2925468dcb72",
    fingerprints_name="dc4-analysis-cleanup-fingerprints.json",
    fingerprints_sha256="b5ff8fbd26b4589b8db2afb1e31f9829b141933cc509563c5d210476dd75d0c4",
    expected_files=5962,
    expected_bytes=2723171594,
    allowed_roots=tuple(Path("build") / group for group in (
        "apaini-analysis",
        "comu-custom-font",
        "comu-pretendard-20261108",
        "comu-sakuramusubi",
        "comu-video",
        "dc4-face-validation",
        "dc4-navigation-validation",
        "dc4-progression",
        "dc4-replay-validation",
        "dc4-sakura-font",
        "dc4-textbox",
        "dc4-unlock-audit",
        "dc4-visual-effects",
        "dc4-voice-layer",
        "dc4ph-startup-video",
        "face-video-analysis",
        "fullscreen-comu-gear",
        "fxaa-legacy",
        "gallery-preview",
        "menu-regression",
        "music-locked-marker",
        "nino-validation",
        "portrait-audit",
        "sakuramusubi-check",
        "sprite-motion-audit",
    )),
    allowed_names=frozenset(),
    allowed_suffixes=frozenset({
        "", ".1", ".bmp", ".class", ".dex", ".exe", ".jar", ".jpg", ".obj",
        ".png", ".ppm", ".py", ".pyc", ".pyd", ".so", ".txt", ".typed",
    }),
    log_prefix="dc4-analysis-cleanup",
)


if __name__ == "__main__":
    sys.exit(main(profile=PROFILE))
