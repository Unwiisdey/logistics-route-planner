"""Compatibility entry for the old Demo startup command."""
import runpy
from pathlib import Path
import sys

SOURCE = Path(__file__).resolve().parents[2] / "ui"
sys.path.insert(0, str(SOURCE))
runpy.run_path(str(SOURCE / "build_live.py"), run_name="__main__")
