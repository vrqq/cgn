@echo off
REM ==========================================
REM Script to copy a file or folder to a destination directory
REM Auto-creates destination directory and updates based on mtime
REM Usage: cp_parent_update.bat <source> <destination>
REM ==========================================

REM Ensure correct number of arguments
if "%~1"=="" (
    echo ERROR: Source file or folder is missing.
    echo Usage: %~0 <source> <destination>
    exit /b 1
)

if "%~2"=="" (
    echo ERROR: Destination directory is missing.
    echo Usage: %~0 <source> <destination>
    exit /b 1
)

set "SOURCE=%~1"
set "DEST=%~2"

REM Check if the source exists
if not exist "%SOURCE%" (
    echo ERROR: The source path "%SOURCE%" does not exist.
    exit /b 1
)

REM Get the full path of the source and its drive root
for %%I in ("%SOURCE%") do set "FULL_SOURCE=%%~fI"
for %%I in ("%SOURCE%") do set "ROOT=%%~dI\"

REM Calculate the relative path by removing the root from the source
set "RELATIVE_PATH=%FULL_SOURCE:%ROOT%=%"

REM Combine the destination with the relative path
set "FINAL_DEST=%DEST%\%RELATIVE_PATH%"

REM Ensure the destination directory exists
echo Ensuring destination directory: "%FINAL_DEST%"
mkdir "%FINAL_DEST%" 2>nul

REM Copy source into the final destination, updating only if mtime is newer
if exist "%SOURCE%\*" (
    REM Source is a directory
    echo Updating directory "%SOURCE%" to "%FINAL_DEST%\"...
    xcopy /e /i /h /d "%SOURCE%" "%FINAL_DEST%\" >nul
    if errorlevel 1 (
        echo ERROR: Failed to update directory.
        exit /b 1
    )
) else (
    REM Source is a file
    echo Updating file "%SOURCE%" to "%FINAL_DEST%"...
    xcopy /h /d "%SOURCE%" "%FINAL_DEST%\" >nul
    if errorlevel 1 (
        echo ERROR: Failed to update file.
        exit /b 1
    )
)

echo Update operation completed successfully.
exit /b 0
