@echo off
rem usage: patch.bat [game bin folder] [-Restore]
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0patch.ps1" %*
pause
