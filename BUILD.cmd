@echo off
cd /d "%~dp0"
python "%~dp0tools\build_release.py" %*
if errorlevel 1 pause
