"""Release gate for complete ZIPs, checksums and production-only app contents."""

import hashlib
import json
import sys
import zipfile
from pathlib import Path

version = json.loads(Path("package.json").read_text(encoding="utf-8"))["version"]
base = f"PetDesk-{version}"
variants = ["Starter"] + (["Windows-x64"] if "--windows" in sys.argv else [])
for variant in variants:
    archive = Path(f"out/{base}-{variant}.zip")
    recorded_hash, name = Path(str(archive) + ".sha256").read_text().split()
    with archive.open("rb") as stream:
        hasher = hashlib.sha256()
        for block in iter(lambda: stream.read(1 << 20), b""):
            hasher.update(block)
    assert (hasher.hexdigest(), archive.name) == (recorded_hash, name)
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None, "damaged ZIP member"
        files = {i.filename.replace("\\", "/"): i for i in z.infolist()}
        root = base if variant == "Starter" else f"{base}-{variant}"
        app = root + ("/app" if variant == "Starter" else "/resources/app")
        assert json.loads(z.read(files[app + "/package.json"]))["version"] == version
        for rel in [
            "desktop/main.mjs",
            "desktop/preload.cjs",
            "dist/index.html",
            "dist/art/terrace.webp",
            "dist/models/Mint.glb",
        ]:
            assert app + "/" + rel in files, rel
        keys = json.loads(z.read(files[app + "/core/qwen-atlas.json"]))
        assert len(keys) == 11
        for key in keys:
            assert files[f"{app}/dist/art/motion/{key}.webp"].file_size > 1024
        assert not any("native-probe" in k or "win-hit-probe" in k for k in files)
        assert root + "/使用说明.txt" in files
        for launcher in (
            ["Start-Windows.cmd", "Start-Linux.sh", "Start-macOS.command"]
            if variant == "Starter"
            else ["PetDesk.exe"]
        ):
            assert root + "/" + launcher in files, launcher
    print(f"{name}: OK, {len(files)} entries, SHA-256 {recorded_hash}")
