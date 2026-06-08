Import("env")

import shutil
from pathlib import Path


def _copy_firmware(source, target, env):
    build_dir = Path(env.subst("$BUILD_DIR"))
    project_dir = Path(env.subst("$PROJECT_DIR"))
    dest = project_dir.parent / "flasher_tool" / "data" / "firmware" / "motherboard"
    dest.mkdir(parents=True, exist_ok=True)

    copied = []
    for fname in ("bootloader.bin", "partitions.bin", "firmware.bin"):
        src = build_dir / fname
        if src.exists():
            shutil.copy2(src, dest / fname)
            copied.append(fname)

    if copied:
        print(f"\n[copy_firmware] Copiado a {dest}:")
        for f in copied:
            print(f"  ok {f}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", _copy_firmware)
