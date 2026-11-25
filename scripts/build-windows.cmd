@echo off
rem ==================================================================================================
rem CoralUI Windows build entry point
rem ==================================================================================================
rem Runs scripts\build-windows.ps1 with the execution policy bypassed, so a fresh checkout works even
rem when the machine policy forbids local scripts. All arguments are forwarded:
rem
rem   scripts\build-windows.cmd
rem   scripts\build-windows.cmd -Tests
rem   scripts\build-windows.cmd -Configuration Debug -ThirdParty D:\deps\third_party
rem ==================================================================================================

setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-windows.ps1" %*
exit /b %ERRORLEVEL%
