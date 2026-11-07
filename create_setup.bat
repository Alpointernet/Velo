@echo off
call "%~dp0scripts\create_setup.bat" %*
exit /b %errorlevel%
