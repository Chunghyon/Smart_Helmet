@echo off
setlocal enabledelayedexpansion
set "PROJECT_PATH=D:\Projects\Projects_Qualcomm\Smart_Helmet\headset\workspace\QCC3044-AA_DEV-BRD-R2-AA"
set "FILE_PATH=%PROJECT_PATH%\headset.x2p"
set "APP_NAME="
set "APP_TYPE="

if not exist "%FILE_PATH%" (
    echo Error: File not found: %FILE_PATH%
    pause
    exit /b 1
)

set "TEMP_FILE=%TEMP%\config_temp.txt"
findstr /i "default=" "%FILE_PATH%" > "%TEMP_FILE%"

for /f "usebackq tokens=*" %%a in ("%TEMP_FILE%") do (
    set "line=%%a"
    set "clean_line=!line!"
    set "clean_line=!clean_line:>= !"
    set "clean_line=!clean_line:<= !"
    
    echo !clean_line! | findstr "debug" >nul
    if !errorlevel! equ 0 (
        set "APP_NAME=SM"
        set "APP_TYPE=debug"
        echo Found configuration: APP_NAME=!APP_NAME!, APP_TYPE=!APP_TYPE!
        goto SUCCESS
    )
    
    echo !clean_line! | findstr "release" >nul
    if !errorlevel! equ 0 (
        set "APP_NAME=SM"
        set "APP_TYPE=release"
        echo Found configuration: APP_NAME=!APP_NAME!, APP_TYPE=!APP_TYPE!
        goto SUCCESS
    )
)

:ERROR
::if exist "%TEMP_FILE%" del "%TEMP_FILE%" >nul 2>&1
echo Failed to set APP_NAME and/or APP_TYPE
exit /b -1

:SUCCESS
if exist "%TEMP_FILE%" del "%TEMP_FILE%" >nul 2>&1
REM Use endlocal & set to return variables to calling script
endlocal & set "APP_NAME=%APP_NAME%" & set "APP_TYPE=%APP_TYPE%"
exit /b 0