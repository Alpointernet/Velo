@echo off
rem An explicit ISCC path takes precedence over installed compiler discovery.
if defined ISCC (
    if exist "%ISCC%" exit /b 0
    echo [ERROR] ISCC points to a missing compiler: "%ISCC%"
    exit /b 1
)

for /f "delims=" %%I in ('where ISCC.exe 2^>nul') do if not defined ISCC set "ISCC=%%I"
if defined ISCC exit /b 0

for %%D in (
    "%ProgramFiles(x86)%\Inno Setup 6"
    "%ProgramFiles%\Inno Setup 6"
    "%LOCALAPPDATA%\Programs\Inno Setup 6"
    "%ProgramFiles(x86)%\Inno Setup 7"
    "%ProgramFiles%\Inno Setup 7"
    "%LOCALAPPDATA%\Programs\Inno Setup 7"
) do if not defined ISCC if exist "%%~D\ISCC.exe" set "ISCC=%%~D\ISCC.exe"
if defined ISCC exit /b 0

rem Fall back to compilers bundled with locally installed development tools.
if exist "%LOCALAPPDATA%\Programs\" (
    for /f "delims=" %%I in ('where /r "%LOCALAPPDATA%\Programs" ISCC.exe 2^>nul') do if not defined ISCC set "ISCC=%%I"
)
if defined ISCC exit /b 0

echo [ERROR] Inno Setup was not found. Install Inno Setup 6 or set ISCC to the full path of ISCC.exe.
exit /b 1
