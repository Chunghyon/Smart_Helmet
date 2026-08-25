@echo off
color f

::trb or usbdbg
SET "PROGRAM_TOOL=trb"
SET "PORT_NUMBER=176244"

call chkNameType.bat
if %errorlevel% neq 0 (
  color c
  echo.
  echo ====================================================
  echo !! chk APP_NAME and/or APP_TYPE Failed !!
  echo ====================================================
  echo.
  pause
  goto END
)

set "PYTHON_PATH=C:\qtil\ADK_Toolkit_1.2.16.21_x64\tools\python27"
set "TOOLKIT_PATH=C:\qtil\ADK_Toolkit_1.2.16.21_x64"
set "UBUILD_PY=C:\qtil\ADK_Toolkit_1.2.16.21_x64\tools\ubuild\ubuild.py"
set "SOURCE_PATH=D:\Projects\Projects_Qualcomm\HECA_11BCBe\headset\src"
set "PROJECT_PATH=D:\Projects\Projects_Qualcomm\HECA_11BCBe\headset\workspace\QCC3044-AA_DEV-BRD-R2-AA"
set "HYDRA_OS_PROJECT=D:\Projects\Projects_Qualcomm\HECA_11BCBe\os\qcc514x_qcc304x\hydra_os\src\fw\src\os.x2p"
set "LIB_PROJECT=D:\Projects\Projects_Qualcomm\HECA_11BCBe\adk\src\libs\libs_qcc514x_qcc304x.x2p"
set "BLUESUITE_PATH=C:\Program Files (x86)\QTIL\BlueSuite 4.0.9"
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
if NOT %ERRORLEVEL% == 0 goto ERROR

%PYTHON_PATH%\python.exe -m menus.buildFlashImage -k "%TOOLKIT_PATH%" -w "%PROJECT_PATH%\headset.x2w" -n tmp -e false
if NOT %ERRORLEVEL% == 0 goto ERROR

copy "%XUV_TMP_PATH%"\output\flash_image.xuv "%OUT_FOLDER%"\%APP_NEW%.xuv >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

copy "%XUV_TMP_PATH%"\output\*.xuv "%OUT_FOLDER%" >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

"%BLUESUITE_PATH%"\nvscmd.exe burn "%OUT_FOLDER%"\flash_image.xuv_apps_p1.xuv -deviceid 4 0 -%PROGRAM_TOOL% %PORT_NUMBER% -nvstype sqif
if NOT %ERRORLEVEL% == 0 goto ERROR

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
timeout /t 1

:END
color f
if exist "%TEMP_FILE%" del "%TEMP_FILE%" >nul 2>&1
if exist "%XUV_TMP_PATH%"\ (
rd /s/q "%XUV_TMP_PATH%"
)
