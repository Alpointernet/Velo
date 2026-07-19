@echo off
cd /d "%~dp0\.."

echo ========================================
echo   Velo Application Builder           
echo ========================================
echo.

echo [1/3] Cleaning previous builds...
if exist "Velo.exe" del /f /q "Velo.exe"
if exist "obj" rmdir /s /q "obj"
if exist "output" rmdir /s /q "output"

echo [2/3] Publishing Velo...
set "NOPAUSE=1"
call scripts\build.bat
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Publishing failed! Please check your C++ code for errors.
    pause
    exit /b %errorlevel%
)

echo.
echo [3/3] Compiling Installer using Inno Setup...

rem Find ISCC.exe dynamically
set "ISCC="
for /f "delims=" %%i in ('where /r "%LOCALAPPDATA%" ISCC.exe 2^>nul') do if not defined ISCC set "ISCC=%%i"
if not defined ISCC for /f "delims=" %%i in ('where /r "%ProgramFiles(x86)%" ISCC.exe 2^>nul') do if not defined ISCC set "ISCC=%%i"
if not defined ISCC for /f "delims=" %%i in ('where /r "%ProgramFiles%" ISCC.exe 2^>nul') do if not defined ISCC set "ISCC=%%i"

if not defined ISCC (
    echo.
    echo [ERROR] Inno Setup not found! Please install Inno Setup 6.
    pause
    exit /b 1
)

"%ISCC%" scripts\installer.iss
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Installer compilation failed!
    pause
    exit /b %errorlevel%
)

echo.
echo ========================================
echo   SUCCESS!                              
echo ========================================
echo Your installer has been generated at:
echo output\VeloInstaller.exe
echo.
start "" "output"
pause
