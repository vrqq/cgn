@echo off
setlocal

:: Check if both inputs are provided
if "%~1"=="" (
    echo Please provide the source file or folder.
    goto :end
)
if "%~2"=="" (
    echo Please provide the destination folder.
    goto :end
)

set "SRC=%~1"
set "DEST=%~2"

:: Check if the source exists
if not exist "%SRC%" (
    echo Source "%SRC%" does not exist.
    goto :end
)

:: Ensure the destination folder exists
if not exist "%DEST%" (
    mkdir "%DEST%"
)

:: Copy the source to the destination
if exist "%SRC%\*" (
    xcopy "%SRC%" "%DEST%" /E /H /Y /F /I
) else (
    copy "%SRC%" "%DEST%" /Y
)

echo Copy operation completed.

:end
endlocal
