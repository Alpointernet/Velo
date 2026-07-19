@echo off
setlocal
cd /d "%~dp0\.."

:: Locate Visual Studio 2022
set "VSPATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
if not exist "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" (
    echo Visual Studio 2022 Community not found!
    pause
    exit /b 1
)

:: Initialize MSVC Environment for x64
call "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" x64

echo.
echo ========================================
echo Building Scintilla...
echo ========================================
cd scintilla\scintilla\win32
nmake -f scintilla.mak
if %errorlevel% neq 0 (
    echo Failed to build Scintilla.
    pause
    exit /b %errorlevel%
)
cd ..\..\..

echo.
echo ========================================
echo Copying SciLexer.dll to output directory...
echo ========================================
copy /Y scintilla\scintilla\bin\Scintilla.dll .\

echo.
echo ========================================
echo Compiling main.cpp...
echo ========================================
if not exist "obj" mkdir "obj"
rc.exe /fo resource.res resource.rc
cl.exe /EHsc /MD /O2 /nologo /I. /Foobj/ src/*.cpp src/components/*.cpp resource.res user32.lib gdi32.lib shell32.lib dwmapi.lib /link /OUT:Velo.exe
if %errorlevel% neq 0 (
    echo Failed to compile Velo.
    pause
    exit /b %errorlevel%
)

echo.
echo ========================================
echo Build Successful! Run Velo.exe
echo ========================================

if not defined NOPAUSE pause