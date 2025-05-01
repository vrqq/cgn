@echo off
setlocal ENABLEDELAYEDEXPANSION

rem === Parse arguments ===
set "VC_ARG=%1"
set "OUTFILE=%2"
set "DEPFILE=%OUTFILE%.d"


if "%VC_ARG%"=="" (
    echo "Error: Missing argument 1 (vcvarsall arg, e.g. x64)."
    exit /b 1
)

if "%OUTFILE%"=="" (
    echo "Error: Missing argument 2 (output batch file name)."
    exit /b 1
)

rem === Use VCVARSALL_PATH env var if provided ===
set "VCVARSALL=%VCVARSALL_PATH%"
if defined VCVARSALL (
    if exist "!VCVARSALL!" (
        echo Using VCVARSALL_PATH override: "!VCVARSALL!"
        goto :found
    ) else (
        echo Error: VCVARSALL_PATH is set but file does not exist: "!VCVARSALL!"
        set "VCVARSALL="
        exit /b 1
    )
)

rem === Search predefined locations ===
set FOUND=0

set "CANDIDATE[0]=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
set "CANDIDATE[1]=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
set "CANDIDATE[2]=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
set "CANDIDATE[3]=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
set "CANDIDATE[4]=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
set "CANDIDATE[5]=C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
set "CANDIDATE[6]=C:\Program Files (x86)\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
set "CANDIDATE[7]=C:\Program Files (x86)\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"

set "VCVARSALL="

for /L %%i in (0,1,7) do (
    call set "TEST=%%CANDIDATE[%%i]%%"
    if exist "!TEST!" (
        set "VCVARSALL=!TEST!"
        set "FOUND=1"
        goto :found
    )
)

:found
if "!FOUND!"=="0" (
    echo Error: Could not find vcvarsall.bat in known locations.
    exit /b 1
)

echo Found vcvarsall.bat: "!VCVARSALL!"

rem === Capture environment before ===
set "ENV_BEFORE=%TEMP%\env_before_%RANDOM%.tmp"
set "ENV_AFTER=%TEMP%\env_after_%RANDOM%.tmp"
set "ENV_DIFF=%TEMP%\env_diff_%RANDOM%.tmp"

set > "!ENV_BEFORE!"

rem === Call vcvarsall ===
call "!VCVARSALL!" %VC_ARG%
if errorlevel 1 (
    echo Error: Failed to run vcvarsall.bat
    exit /b 1
)

set > "!ENV_AFTER!"

rem === Compute environment diff ===
type NUL > "!ENV_DIFF!"
for /f "usebackq delims=" %%L in ("!ENV_AFTER!") do (
    set "LINE=%%L"
    set "FOUND_LINE="
    for /f "usebackq delims=" %%B in ("!ENV_BEFORE!") do (
        if /i "%%B"=="!LINE!" (
            set "FOUND_LINE=1"
        )
    )
    if not defined FOUND_LINE (
        echo set "%%L" >> "!ENV_DIFF!"
    )
)

rem === Write set-env batch ===
(
    echo @echo off
    for /f "usebackq delims=" %%D in ("!ENV_DIFF!") do (
        echo %%D
    )
) > "%OUTFILE%"

rem === Escape backslashes and spaces for Ninja depfile ===
set "ESCAPED_PATH=!VCVARSALL:\=\\!"
set "ESCAPED_PATH=!ESCAPED_PATH: =\ !"
@REM set "ESCAPED_PATH=!ESCAPED_PATH:(=\(!"
@REM set "ESCAPED_PATH=!ESCAPED_PATH:)=\)!"

rem === Write depfile using Ninja-style escaping ===
> "%DEPFILE%" echo %OUTFILE%: !ESCAPED_PATH!

rem === Cleanup ===
del /q "!ENV_BEFORE!" "!ENV_AFTER!" "!ENV_DIFF!" >nul 2>&1

echo Environment setup script generated: %OUTFILE%
echo Depfile generated: %DEPFILE%
exit /b 0
