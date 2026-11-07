@echo off
setlocal EnableExtensions DisableDelayedExpansion
if /i "%~1"=="--no-pause" set "NOPAUSE=1"
set "RESULT=1"
pushd "%~dp0.." || exit /b 1

echo Building Velo...
call "%~dp0msvc_env.bat"
if errorlevel 1 goto :done

for %%F in (resource.rc Velo.exe.manifest Scintilla.dll Lexilla.dll) do (
    if not exist "%%F" (
        echo [ERROR] Required file not found: %%F
        goto :done
    )
)

if not exist "obj\" mkdir "obj"
if not exist "obj\" (
    echo [ERROR] Could not create the obj directory.
    goto :done
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0generate_icon.ps1"
if errorlevel 1 goto :done

rc.exe /nologo /fo "obj\resource.res" "resource.rc"
if errorlevel 1 goto :done

cl.exe /nologo /std:c++17 /EHsc /MD /O2 /GL /utf-8 /I. /Foobj\ src\*.cpp src\components\*.cpp obj\resource.res user32.lib gdi32.lib shell32.lib dwmapi.lib winmm.lib ole32.lib /link /LTCG /OUT:obj\Velo.exe
if errorlevel 1 goto :done

copy /y "obj\Velo.exe" "Velo.exe" >nul
if errorlevel 1 (
    echo [ERROR] Could not update Velo.exe. Close any running copy and try again.
    goto :done
)

set "RESULT=0"
echo Build complete: %CD%\Velo.exe

:done
if not "%RESULT%"=="0" echo [ERROR] Build failed. See the error above.
popd
if not defined NOPAUSE pause
exit /b %RESULT%
