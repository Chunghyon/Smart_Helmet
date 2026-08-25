@echo off

::trb or usbdbg
SET PROGRAM_TOOL=usbdbg
SET PORT_NUMBER=110
SET BT_NAME_BASE=HJC_11Be

SET "BIN_PATH=C:\Program Files (x86)\QTIL\BlueSuite 4.0.9"
SET "TEMPLATE_PATH=D:\Projects\Projects_Qualcomm\HECA_11BCBe\0.build\0.template"

copy /Y "%TEMPLATE_PATH%"\dev_cfg_v2.htf .\tmp_cfg.htf
if NOT %ERRORLEVEL% == 0 goto ERROR

set BT_ADDRESS="FF FF FF FF 0B 0A"
set "PREFIX_BT_NAME=FFFF"

fart -q .\tmp_cfg.htf CHANGE_BT_ADDRESS %BT_ADDRESS%
if NOT %ERRORLEVEL% == 1 goto ERROR
fart -q .\tmp_cfg.htf CHANGE_HEX_VERSION 313142655F312E312E3032
if NOT %ERRORLEVEL% == 1 goto ERROR
fart -q .\tmp_cfg.htf CHANGE_BT_NAME %BT_NAME_BASE%_%PREFIX_BT_NAME%
if NOT %ERRORLEVEL% == 1 goto ERROR

"%BIN_PATH%"\configcmd txt2dev .\tmp_cfg.htf REPLACE -%PROGRAM_TOOL% %PORT_NUMBER% -database "%TEMPLATE_PATH%"\hydracore_config.sdb -system QCC514X_CONFIG -reset

goto GOOD

:ERROR
color c
echo.
echo =================
echo *** Program Fail ***
echo =================
pause
goto END

:GOOD
echo.
echo ================================================
echo *** SET BT Address OK (BT_ADDR : %BT_ADDRESS%)
echo *** SET BT Name OK (BT_Name : %BT_NAME_BASE%_%PREFIX_BT_NAME%)
echo ================================================

:End
if exist .\tmp_cfg.htf (
del .\tmp_cfg.htf
)
color f
