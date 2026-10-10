@echo off
rem usage: patch.bat [Hammer++ bin\x64 folder] [-Restore]
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0patch.ps1" %*
pause
