@echo off
@set PYTHON_PATH=C:\qtil\ADK_Toolkit_1.2.16.21_x64\tools\python27
@set TOOLKIT_PATH=C:\qtil\ADK_Toolkit_1.2.16.21_x64
@set APP_PATH=D:\Projects\Projects_Qualcomm\HECA_11BCBe\headset\workspace\QCC3044-AA_DEV-BRD-R2-AA

@for /F "tokens=1,2,3 delims=-:" %%i in ('findstr QTIL %APP_PATH%\build_id_str.c') do @set FIRST_EXTRAC=%%i%%j%%k
@for /F "tokens=1,2 delims= " %%i in ('echo %FIRST_EXTRAC:~-11,11%') do @set APP_VERSION=%%i%%j

@set APP_NAME="HECA(debug)"
@set APP_NEW=%APP_NAME%_v%APP_VERSION%

@set OUT_FOLDER=D:\Projects\Projects_Qualcomm\HECA_11BCBe\0.build\%APP_NEW%

@set XUV_TMP_PATH=%APP_PATH%\image\tmp
@set DFU_TMP_PATH=%APP_PATH%\dfu\tmp

@if exist %OUT_FOLDER%\ (
@rd /s/q %OUT_FOLDER%
)
@md %OUT_FOLDER%
@if NOT "%ERRORLEVEL%" == "0" goto ERROR

%PYTHON_PATH%\python.exe -m menus.buildFlashImage -k %TOOLKIT_PATH% -w %APP_PATH%\headset.x2w -n tmp -e false

copy %XUV_TMP_PATH%\output\flash_image.xuv %OUT_FOLDER%\%APP_NEW%.xuv
@if NOT "%ERRORLEVEL%" == "0" goto ERROR

@if exist %XUV_TMP_PATH%\ (
@rd /s/q %XUV_TMP_PATH%
)

%PYTHON_PATH%\\python.exe -m menus.buildDFU -k %TOOLKIT_PATH% -w %APP_PATH%\headset.x2w -n tmp -p "['curator_config', 'firmware_config', 'apps0', 'customer_ro', 'app/p1']" -e encrypted -a %APP_PATH%\dfu\dfu_encryption.key -s all -f %APP_PATH%\dfu
copy %DFU_TMP_PATH%\output\QCC514X_dfu_file.bin %OUT_FOLDER%\dfu_%APP_NEW%.bin
@if NOT "%ERRORLEVEL%" == "0" goto ERROR

@if exist %DFU_TMP_PATH%\ (
@rd /s/q %DFU_TMP_PATH%
)

@goto GOOD

:ERROR
@if exist %XUV_TMP_PATH%\ (
@rd /s/q %XUV_TMP_PATH%
)
@if exist %DFU_TMP_PATH%\ (
@rd /s/q %DFU_TMP_PATH%
)

@color c
@echo.
@echo ================================================
@echo !! %APP_NEW% generation Fail !!
@echo ================================================
@echo.
@pause
@goto END

:GOOD
@echo.
@echo ================================================
@echo !! %APP_NEW% generation OK !!
@echo ================================================
@echo.

:END
@timeout /t -1
@color f
