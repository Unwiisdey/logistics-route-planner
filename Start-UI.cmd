@echo off
cd /d "%~dp0"
where py >nul 2>nul
if errorlevel 1 goto python
py -3 scripts\start_ui.py
goto done
:python
python scripts\start_ui.py
:done
if errorlevel 1 pause
