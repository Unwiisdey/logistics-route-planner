@echo off
cd /d "%~dp0"
python build_live.py
if errorlevel 1 goto done
python serve_demo.py
:done
pause
