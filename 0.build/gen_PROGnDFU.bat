@echo off
color f

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
set "SOURCE_PATH=D:\Projects\Projects_Qualcomm\Smart_Helmet\headset\src"
set "APP_PATH=D:\Projects\Projects_Qualcomm\Smart_Helmet\headset\workspace\QCC3044-AA_DEV-BRD-R2-AA"
set "BUILD_PATH=%APP_PATH%\depend_%APP_NAME%_%APP_TYPE%_qcc514x_qcc304x"

for /F "tokens=3" %%a in ('findstr README_VERSION_%APP_NAME% %SOURCE_PATH%\main.c') do set "APP_VERSION=%%a"
for /F "tokens=5" %%a in ('findstr build_id_number %APP_PATH%\build_id_str.c') do set "PRE_EXTRACT=%%a"
for /F "tokens=1" %%a in ('echo %PRE_EXTRACT:~-13,10%') do set "APP_BUILD_ID=%%a"
for /f "tokens=3" %%A in ('findstr README_HEX_VERSION_%APP_NAME% %SOURCE_PATH%\main.c') do set "HEX_VERSION=%%A"

if /I "%APP_TYPE%"=="debug" (
  set "APP_NEW=%APP_NAME%_%APP_TYPE%_%APP_VERSION%(%APP_BUILD_ID%)"
) else (
  set "APP_NEW=%APP_NAME%_%APP_VERSION%(%APP_BUILD_ID%)"
)

set "OUT_FOLDER=D:\Projects\Projects_Qualcomm\Smart_Helmet\0.build\%APP_NEW%"
set "DFU_TMP_PATH=%APP_PATH%\dfu\tmp"
set "XUV_TMP_PATH=%APP_PATH%\image\tmp"
set "REF_FOLDER=%OUT_FOLDER%\ref"
set "XUV_FOLDER=%OUT_FOLDER%\ref\xuv"
set "MULTI_FOLDER=%OUT_FOLDER%\multi_%APP_NEW%"

for /f "tokens=1-6 delims=-: " %%A in ('powershell -command "&{[System.DateTime]::Parse('1970-01-01 00:00:00').AddSeconds(%APP_BUILD_ID%).ToString('yyyy MM dd HH mm ss')}"') do (
    set "YEAR=%%A"
    set "MONTH=%%B"
    set "DAY=%%C"
    set "DAY=%%C"
    set "HOUR=%%D"
    set "MINUTE=%%E"
    set "SECOND=%%F"
)

set /a "pYEAR=YEAR-2020"

for /f %%A in ('powershell -command "&{'{0:X}' -f %pYEAR%}"') do set "pYEAR=%%A"
for /f %%A in ('powershell -command "&{'{0:X}' -f %MONTH%}"') do set "pMONTH=%%A"

echo Build Time: %YEAR%-%MONTH%-%DAY%-%HOUR%-%MINUTE%-%SECOND%
echo pYEAR (HEX) = %pYEAR%
echo pMONTH (HEX) = %pMONTH%

if /I "%APP_NAME%"=="Z1" (
  set "BUILD_SERIAL=F%pYEAR%%pMONTH%"
) else (
  if /I "%APP_NAME%"=="11Be" (
    set "BUILD_SERIAL=E%pYEAR%%pMONTH%"
  ) else (
    set "BUILD_SERIAL=C%pYEAR%%pMONTH%"
  )
)

echo BUILD_SERIAL : %BUILD_SERIAL%

if exist %OUT_FOLDER%\ (
rd /s/q "%OUT_FOLDER%"
)

md %OUT_FOLDER%
if NOT %ERRORLEVEL% == 0 goto ERROR

md %REF_FOLDER%
if NOT %ERRORLEVEL% == 0 goto ERROR

md %XUV_FOLDER%
if NOT %ERRORLEVEL% == 0 goto ERROR

md %MULTI_FOLDER%
if NOT %ERRORLEVEL% == 0 goto ERROR

copy %BUILD_PATH%\headset.elf %REF_FOLDER% >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

%PYTHON_PATH%\python.exe -m menus.buildFlashImage -k %TOOLKIT_PATH% -w %APP_PATH%\headset.x2w -n tmp -e false
if NOT %ERRORLEVEL% == 0 goto ERROR

copy %XUV_TMP_PATH%\output\flash_image.xuv %OUT_FOLDER%\%APP_NEW%.xuv >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

copy %XUV_TMP_PATH%\output\*.xuv %XUV_FOLDER% >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

%PYTHON_PATH%\python.exe -m menus.buildDFU -k %TOOLKIT_PATH% -w %APP_PATH%\headset.x2w -n tmp -p "['curator_config', 'firmware_config', 'apps0', 'customer_ro', 'app/p1']" -e none -s all -f %APP_PATH%\dfu
if NOT %ERRORLEVEL% == 0 goto ERROR

copy %DFU_TMP_PATH%\output\QCC514X_dfu_file.bin %OUT_FOLDER%\%APP_NEW%_full.bin >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

%PYTHON_PATH%\python.exe -m menus.buildDFU -k %TOOLKIT_PATH% -w %APP_PATH%\headset.x2w -n tmp -p "['app/p1']" -e none -s all -f %APP_PATH%\dfu
if NOT %ERRORLEVEL% == 0 goto ERROR

copy %DFU_TMP_PATH%\output\QCC514X_dfu_file.bin %OUT_FOLDER%\%APP_NEW%_app.bin >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

copy /Y .\0.template\program_template_diff_name_trb.bat %OUT_FOLDER%\program_and_rename_%APP_VERSION%_trb.bat >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

copy /Y .\0.template\program_template_diff_name_usb.bat %OUT_FOLDER%\program_and_rename_%APP_VERSION%_usb.bat >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

copy /Y .\0.template\set_bt_addr_bt_name_trb.bat %OUT_FOLDER% >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

copy /Y .\0.template\set_bt_addr_bt_name_usb.bat %OUT_FOLDER% >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

copy /Y .\0.template\reset.script %OUT_FOLDER% >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

copy /Y .\0.template\dev_cfg_v2.htf %REF_FOLDER% >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

copy /Y .\0.template\hydracore_config.sdb %REF_FOLDER% >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

xcopy /Y /E /I .\0.template\multi\*.* %MULTI_FOLDER%\ >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

copy /Y .\0.template\dev_cfg_v2.htf %MULTI_FOLDER%\%APP_NAME%_%APP_TYPE%_%APP_VERSION%.htf >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

copy %XUV_TMP_PATH%\output\flash_image.xuv %MULTI_FOLDER%\%APP_NEW%.xuv >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

copy /Y .\0.template\hydracore_config.sdb %MULTI_FOLDER% >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

move /Y %MULTI_FOLDER%\Serial.txt %MULTI_FOLDER%\Serial_%BUILD_SERIAL%.txt >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

move %MULTI_FOLDER%\server_script_template.ps1 %MULTI_FOLDER%\server_script.ps1 >nul
if NOT %ERRORLEVEL% == 0 goto ERROR
move %MULTI_FOLDER%\client_script_template.ps1 %MULTI_FOLDER%\client_script.ps1 >nul
if NOT %ERRORLEVEL% == 0 goto ERROR

fart -q %OUT_FOLDER%\program_and_rename_%APP_VERSION%_trb.bat CHANGE_FILE_NAME %APP_NEW%
if %ERRORLEVEL% == 0 goto ERROR
fart -q %OUT_FOLDER%\program_and_rename_%APP_VERSION%_usb.bat CHANGE_FILE_NAME %APP_NEW%
if %ERRORLEVEL% == 0 goto ERROR

if /I "%APP_NAME%" == "Z1" (
  fart -q "%OUT_FOLDER%\program_and_rename_%APP_VERSION%_trb.bat" CHANGE_BT_NAME_BASE %APP_NAME%
  if %ERRORLEVEL% == 0 goto ERROR
  fart -q "%OUT_FOLDER%\set_bt_addr_bt_name_trb.bat" CHANGE_BT_NAME_BASE %APP_NAME%
  if %ERRORLEVEL% == 0 goto ERROR
  fart -q "%OUT_FOLDER%\program_and_rename_%APP_VERSION%_usb.bat" CHANGE_BT_NAME_BASE %APP_NAME%
  if %ERRORLEVEL% == 0 goto ERROR
  fart -q "%OUT_FOLDER%\set_bt_addr_bt_name_usb.bat" CHANGE_BT_NAME_BASE %APP_NAME%
  if %ERRORLEVEL% == 0 goto ERROR
) else (
  fart -q "%OUT_FOLDER%\program_and_rename_%APP_VERSION%_trb.bat" CHANGE_BT_NAME_BASE HJC_%APP_NAME%
  if %ERRORLEVEL% == 0 goto ERROR
  fart -q "%OUT_FOLDER%\set_bt_addr_bt_name_trb.bat" CHANGE_BT_NAME_BASE HJC_%APP_NAME%
  if %ERRORLEVEL% == 0 goto ERROR
  fart -q "%OUT_FOLDER%\program_and_rename_%APP_VERSION%_usb.bat" CHANGE_BT_NAME_BASE HJC_%APP_NAME%
  if %ERRORLEVEL% == 0 goto ERROR
  fart -q "%OUT_FOLDER%\set_bt_addr_bt_name_usb.bat" CHANGE_BT_NAME_BASE HJC_%APP_NAME%
  if %ERRORLEVEL% == 0 goto ERROR
)

fart -q %REF_FOLDER%\dev_cfg_v2.htf CHANGE_HEX_VERSION %HEX_VERSION%
if %ERRORLEVEL% == 0 goto ERROR

fart -q %MULTI_FOLDER%\%APP_NAME%_%APP_TYPE%_%APP_VERSION%.htf CHANGE_HEX_VERSION %HEX_VERSION%
if %ERRORLEVEL% == 0 goto ERROR
fart -q %MULTI_FOLDER%\client_script.ps1 CHANGE_FUSING_FILE_NAME %APP_NEW%
if %ERRORLEVEL% == 0 goto ERROR
fart -q %MULTI_FOLDER%\client_script.ps1 CHANGE_SERIAL_FILE_NAME Serial_%BUILD_SERIAL%
if %ERRORLEVEL% == 0 goto ERROR
fart -q %MULTI_FOLDER%\client_script.ps1 CHANGE_HTF_FILE_NAME %APP_NAME%_%APP_TYPE%_%APP_VERSION%
if %ERRORLEVEL% == 0 goto ERROR

if /I "%APP_NAME%" == "11BC" (
fart -q "%MULTI_FOLTER%"\Serial_%BUILD_SERIAL%.txt 0A0B00000000 BC11%BUILD_SERIAL%00000
if %ERRORLEVEL% == 0 goto ERROR
) else (
if /I "%APP_NAME%" == "11Be" (
fart -q "%MULTI_FOLTER%"\Serial_%BUILD_SERIAL%.txt 0A0B00000000 BE11%BUILD_SERIAL%00000
if %ERRORLEVEL% == 0 goto ERROR
) else (
fart -q "%MULTI_FOLTER%"\Serial_%BUILD_SERIAL%.txt 0A0B00000000 BF11%BUILD_SERIAL%00000
if %ERRORLEVEL% == 0 goto ERROR
)
)

goto GOOD

:ERROR
if exist %OUT_FOLDER%\ (
rd /s/q %OUT_FOLDER% >nul
)

color c
echo.
echo ====================================================
echo !! %APP_NEW% generation Fail !!
echo ====================================================
echo.
pause
goto END

:GOOD
echo.
echo ====================================================
echo !! %APP_NEW% generation OK !!
echo ====================================================
echo.

:END
if exist %XUV_TMP_PATH%\ (
rd /s/q %XUV_TMP_PATH% >nul
)
if exist %DFU_TMP_PATH%\ (
rd /s/q %DFU_TMP_PATH% >nul
)
pause
color f
