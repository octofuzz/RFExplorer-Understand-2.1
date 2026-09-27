"""M5HAL 0.1.2 includes Arduino Wire/SPI without declaring them as dependencies.
Expose the pinned framework headers to all library build environments.
No downloaded source files are changed.
"""
Import("env")
from pathlib import Path
framework=Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
env.Append(CPPPATH=[str(framework / "libraries" / name / "src") for name in ("Wire","SPI")])
