"""Build, generate and run the project's local web interface. Stdlib only."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

def find_cmake():
    configured = os.environ.get("CMAKE_COMMAND") or shutil.which("cmake")
    if configured:
        return configured
    if sys.platform == "win32":
        # Visual Studio bundles CMake even when it has not been added to PATH.
        for variable in ("ProgramFiles(x86)", "ProgramFiles"):
            directory = os.environ.get(variable)
            if directory:
                matches = sorted(Path(directory).glob("Microsoft Visual Studio/*/*/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"), reverse=True)
                if matches:
                    return str(matches[0])
    raise RuntimeError("CMake was not found. Install CMake 3.16+ or set CMAKE_COMMAND.")

def main():
    parser = argparse.ArgumentParser(description="Start the formal logistics web UI")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    parser.add_argument("--no-build", action="store_true", help="Use the existing bridge binary")
    parser.add_argument("--no-browser", action="store_true")
    options = parser.parse_args()
    if not 1 <= options.port <= 65535:
        parser.error("port must be between 1 and 65535")
    build_dir = options.build_dir.resolve()
    if not options.no_build:
        cmake = find_cmake()
        subprocess.run([cmake,"-S",str(ROOT),"-B",str(build_dir),"-DCMAKE_BUILD_TYPE=Release","-DLOGISTICS_BUILD_UI=ON"], check=True)
        subprocess.run([cmake,"--build",str(build_dir),"--config","Release","--target","logistics_ui_bridge"], check=True)
    if "LOGISTICS_UI_BRIDGE" not in os.environ:
        for path in (build_dir / "Release/logistics_ui_bridge.exe", build_dir / "logistics_ui_bridge", build_dir / "logistics_ui_bridge.exe"):
            if path.is_file():
                os.environ["LOGISTICS_UI_BRIDGE"] = str(path)
                break
    subprocess.run([sys.executable,str(ROOT / "ui/build_live.py")], check=True)
    from ui.server import run
    run(options.port, open_browser=not options.no_browser)

if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"UI startup failed: {error}", file=sys.stderr)
        raise SystemExit(1)
