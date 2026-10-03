"""PlatformIO post-build: make a flash-at-zero image and record its hashes."""
Import("env")
from pathlib import Path
import hashlib
import json
import shutil
import subprocess

def package(source, target, env):
    project = Path(env.subst("$PROJECT_DIR"))
    build = Path(env.subst("$BUILD_DIR"))
    out = project / "firmware"
    out.mkdir(exist_ok=True)
    framework = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
    esptool = Path(env.PioPlatform().get_package_dir("tool-esptoolpy")) / "esptool.py"
    parts = [(0x0, build / "bootloader.bin"), (0x8000, build / "partitions.bin"),
             (0xe000, framework / "tools/partitions/boot_app0.bin"),
             (0x10000, build / "firmware.bin")]
    merged = out / "RFExplorer-v2.8-cardputer-adv-merged.bin"
    command = [env.subst("$PYTHONEXE"), str(esptool), "--chip", "esp32s3", "merge_bin",
               "-o", str(merged), "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "8MB"]
    for address, path in parts:
        command += [hex(address), str(path)]
    subprocess.run(command, check=True)
    shutil.copy2(build / "firmware.bin", out / "RFExplorer-v2.8-app-only.bin")
    # Verify all embedded parts byte-for-byte; merge_bin may update boot header bytes 2/3.
    merged_data = merged.read_bytes()
    for address, path in parts:
        data = path.read_bytes()
        start = 4 if address == 0 else 0
        assert merged_data[address+start:address+len(data)] == data[start:], path
    assert len(merged_data) <= 0x340000, "Merged image exceeds first application partition"
    records = {}
    for path in sorted(out.glob("RFExplorer-v2.8-*.bin")):
        data=path.read_bytes()
        records[path.name] = {"bytes":len(data), "sha256":hashlib.sha256(data).hexdigest(),
                             "flash_offset":"0x0" if "merged" in path.name else "0x10000"}
    (out / "manifest.json").write_text(json.dumps({"target":"M5Stack Cardputer ADV + Cap CC1101 U219",
                             "version":"2.8.0", "hardware_tested":False, "images":records},indent=2)+"\n",encoding="utf-8")
    source_records = {}
    for directory in ("src", "include", "tests", "scripts", "licenses"):
        for path in sorted((project / directory).rglob("*")):
            if path.is_file() and not any(part in (".build", "__pycache__") for part in path.parts) and path.suffix != ".bak":
                source_records[path.relative_to(project).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    for name in (".gitignore", "CHANGELOG.md", "LICENSE", "partitions.csv", "platformio.ini", "README.md", "THIRD_PARTY.md", "VALIDATION.md", "BUILD_2_8.ps1"):
        path = project / name
        if path.is_file():
            source_records[name] = hashlib.sha256(path.read_bytes()).hexdigest()
    (out / "source-sha256.json").write_text(json.dumps(source_records, indent=2) + "\n", encoding="utf-8")
    print("RF Explorer: merged-image offsets and hashes verified")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", package)



