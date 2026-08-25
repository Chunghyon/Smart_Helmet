@echo off
color f
setlocal enabledelayedexpansion

::trb or usbdbg
SET "PROGRAM_TOOL=usbdbg"
SET "PORT_NUMBER=100"

set "PYTHON_PATH=C:\qtil\ADK_Toolkit_1.2.16.21_x64\tools\python27"
set "TOOLKIT_PATH=C:\qtil\ADK_Toolkit_1.2.16.21_x64"
set "UBUILD_PY=C:\qtil\ADK_Toolkit_1.2.16.21_x64\tools\ubuild\ubuild.py"
set "SOURCE_PATH=D:\Projects\Projects_Qualcomm\HECA_11BCBe\headset\src"
set "PROJECT_PATH=D:\Projects\Projects_Qualcomm\HECA_11BCBe\headset\workspace\QCC3044-AA_DEV-BRD-R2-AA"
set "HYDRA_OS_PROJECT=D:\Projects\Projects_Qualcomm\HECA_11BCBe\os\qcc514x_qcc304x\hydra_os\src\fw\src\os.x2p"
set "LIB_PROJECT=D:\Projects\Projects_Qualcomm\HECA_11BCBe\adk\src\libs\libs_qcc514x_qcc304x.x2p"
set "BLUESUITE_PATH=C:\Program Files (x86)\QTIL\BlueSuite 4.0.9"

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
    
    echo !clean_line! | findstr "Z1_debug" >nul
    if !errorlevel! equ 0 (
        set "APP_NAME=Z1"
        set "APP_TYPE=debug"
        echo Found configuration: APP_NAME=!APP_NAME!, APP_TYPE=!APP_TYPE!
        goto result
    )
    
    echo !clean_line! | findstr "Z1_release" >nul
    if !errorlevel! equ 0 (
        set "APP_NAME=Z1"
        set "APP_TYPE=release"
        echo Found configuration: APP_NAME=!APP_NAME!, APP_TYPE=!APP_TYPE!
        goto result
    )
    
    echo !clean_line! | findstr "11Be_debug" >nul
    if !errorlevel! equ 0 (
        set "APP_NAME=11Be"
        set "APP_TYPE=debug"
        echo Found configuration: APP_NAME=!APP_NAME!, APP_TYPE=!APP_TYPE!
        goto result
    )
    
    echo !clean_line! | findstr "11Be_release" >nul
    if !errorlevel! equ 0 (
        set "APP_NAME=11Be"
        set "APP_TYPE=release"
        echo Found configuration: APP_NAME=!APP_NAME!, APP_TYPE=!APP_TYPE!
        goto result
    )
    
    echo !clean_line! | findstr "11BC_debug" >nul
    if !errorlevel! equ 0 (
        set "APP_NAME=11BC"
        set "APP_TYPE=debug"
        echo Found configuration: APP_NAME=!APP_NAME!, APP_TYPE=!APP_TYPE!
        goto result
    )
    
    echo !clean_line! | findstr "11BC_release" >nul
    if !errorlevel! equ 0 (
        set "APP_NAME=11BC"
        set "APP_TYPE=release"
        echo Found configuration: APP_NAME=!APP_NAME!, APP_TYPE=!APP_TYPE!
        goto result
    )
)

goto ERROR


:result
if exist "%TEMP_FILE%" del "%TEMP_FILE%" >nul 2>&1

if "%APP_NAME%"=="" (
    echo Warning: No valid configuration found in the file.
) else (
    echo.
    echo APP_NAME: %APP_NAME%
    echo APP_TYPE: %APP_TYPE%
)

set "BUILD_PATH=%PROJECT_PATH%\depend_%APP_NAME%_%APP_TYPE%_qcc514x_qcc304x"

for /F "tokens=5" %%a in ('findstr build_id_number %PROJECT_PATH%\build_id_str.c') do set "PRE_EXTRACT=%%a"
for /F "tokens=1" %%a in ('echo %PRE_EXTRACT:~-13,10%') do set "APP_BUILD_ID=%%a"

set "APP_NEW=%APP_NAME%_%APP_TYPE%_test"

set "OUT_FOLDER=D:\Projects\Projects_Qualcomm\HECA_11BCBe\0.build\test"

set "XUV_TMP_PATH=%PROJECT_PATH%\image\tmp"

if exist "%OUT_FOLDER%"\ (
rd /s/q "%OUT_FOLDER%" >nul
)

md "%OUT_FOLDER%"
if NOT "%ERRORLEVEL%" == "0" goto ERROR

%PYTHON_PATH%\python.exe -m menus.buildFlashImage -k "%TOOLKIT_PATH%" -w "%PROJECT_PATH%\headset.x2w" -n tmp -e false
if NOT "%ERRORLEVEL%" == "0" goto ERROR

copy "%XUV_TMP_PATH%"\output\flash_image.xuv "%OUT_FOLDER%"\%APP_NEW%.xuv >nul
if NOT "%ERRORLEVEL%" == "0" goto ERROR

copy "%XUV_TMP_PATH%"\output\*.xuv "%OUT_FOLDER%" >nul
if NOT "%ERRORLEVEL%" == "0" goto ERROR

"%BLUESUITE_PATH%"\nvscmd.exe burn "%OUT_FOLDER%"\flash_image.xuv_apps_p1.xuv -deviceid 4 0 -%PROGRAM_TOOL% %PORT_NUMBER% -nvstype sqif
if NOT "%ERRORLEVEL%" == "0" goto ERROR

goto GOOD

:ERROR
color c
echo.
echo ================================================
echo %APP_NEW% generation Failed
echo ================================================
echo.
pause
goto END

:GOOD
echo.
echo ================================================
echo %APP_NEW% generation and program OK
echo ================================================
echo.

:END
if exist "%TEMP_FILE%" del "%TEMP_FILE%" >nul 2>&1

if exist "%XUV_TMP_PATH%"\ (
rd /s/q "%XUV_TMP_PATH%"
)
color f
