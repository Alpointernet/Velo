@echo off
setlocal EnableExtensions DisableDelayedExpansion
if /i "%~1"=="--no-pause" set "NOPAUSE=1"
set "RESULT=1"
pushd "%~dp0.." || exit /b 1

echo [1/3] Finding Inno Setup...
call "%~dp0find_iscc.bat"
if errorlevel 1 goto :done

echo [2/3] Building Velo...
call "%~dp0build.bat" --no-pause
if errorlevel 1 goto :done

echo [3/3] Compiling the installer...
"%ISCC%" /Qp "%~dp0installer.iss"
if errorlevel 1 goto :done
if not exist "output\VeloInstaller.exe" (
    echo [ERROR] Inno Setup did not produce output\VeloInstaller.exe.
    goto :done
)

set "RESULT=0"
echo Installer complete: %CD%\output\VeloInstaller.exe
if not defined NOPAUSE start "" "%CD%\output"

:done
if not "%RESULT%"=="0" echo [ERROR] Installer build failed. See the error above.
popd
if not defined NOPAUSE pause
exit /b %RESULT%
