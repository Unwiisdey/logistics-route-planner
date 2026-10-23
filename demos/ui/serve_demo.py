"""Compatibility entry: the approved UI now belongs to the formal project."""
import runpy
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
runpy.run_module("ui.server", run_name="__main__")
