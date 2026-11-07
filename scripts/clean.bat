@echo off
setlocal EnableExtensions DisableDelayedExpansion
if /i "%~1"=="--no-pause" set "NOPAUSE=1"
set "RESULT=0"
pushd "%~dp0.." || exit /b 1

rem Only generated build outputs; application settings live outside this project.
for %%F in (Velo.exe resource.res icon\app.ico) do (
    if exist "%%F" del /q "%%F"
    if exist "%%F" set "RESULT=1"
)
for %%D in (obj output scripts\obj) do (
    if exist "%%D\" rmdir /s /q "%%D"
    if exist "%%D\" set "RESULT=1"
)

if "%RESULT%"=="0" (
    echo Generated build files cleaned.
) else (
    echo [ERROR] Some generated files are still in use. Close Velo and try again.
)
popd
if not defined NOPAUSE pause
exit /b %RESULT%
