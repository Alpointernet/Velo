@echo off
rem Called by the build scripts; exports the x64 compiler environment to the caller.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] Visual Studio Installer was not found. Install Visual Studio with Desktop development with C++.
    exit /b 1
)

set "VSPATH="
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%I"
if not defined VSPATH (
    echo [ERROR] No Visual Studio installation with the C++ build tools was found.
    exit /b 1
)

if not exist "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" (
    echo [ERROR] The Visual Studio C++ environment script is missing.
    exit /b 1
)
call "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" x64
exit /b %errorlevel%
