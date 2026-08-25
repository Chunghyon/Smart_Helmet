@echo off
@SET SUCCESS_COUNT=0
@SET FILE_NAME=CHANGE_FILE_NAME
@SET BIN_PATH="C:\Program Files (x86)\QTIL\BlueSuite 4.0.9"

::trb or usbdbg
@SET PROGRAM_TOOL=trb
@SET PORT_NUMBER=176244

@SET BT_NAME_BASE=CHANGE_BT_NAME_BASE

:HOME
%BIN_PATH%\nvscmd.exe burn .\%FILE_NAME%.xuv -deviceid 4 0 -%PROGRAM_TOOL% %PORT_NUMBER% -nvstype sqif
@if NOT "%ERRORLEVEL%" == "0" goto ERROR

@set "APP_MONTH_DATE="
@set "APP_HOUR="
@set "APP_MIN="
@set "APP_SEC="

@for /F "tokens=1,2,3 delims=- " %%i in ('date /t') do @set APP_MONTH_DATE=%%j%%k
@for /F "tokens=1 delims= " %%i in ('echo %APP_MONTH_DATE%') do (
	if %%i LSS 1000 (
		@for /F "tokens=1 delims= " %%i in ('echo %APP_MONTH_DATE:~1,3%') do @set APP_MONTH_DATE=%%i
	)
)
@for /F "tokens=1,2,3 delims=.: " %%i in ('echo %time%') do (
  @set APP_HOUR=%%i
  if %%i LSS 10 (
    @set APP_HOUR=0%%i
  )
  @set APP_MIN=%%j
  @set APP_SEC=%%k
)

@set APP_MONTH_DATE_HOUR_MIN_SEC=%APP_MONTH_DATE%%APP_HOUR%%APP_MIN%%APP_SEC%
echo APP_MONTH_DATE_HOUR_MIN_SEC=%APP_MONTH_DATE_HOUR_MIN_SEC%

@copy /Y .\ref\dev_cfg_v2.htf .\ref\tmp_cfg.htf
@if NOT "%ERRORLEVEL%" == "0" goto ERROR

setlocal EnableDelayedExpansion
set DecValue=%APP_MONTH_DATE_HOUR_MIN_SEC%
call :ConvertDecToHex %DecValue% HexValue
echo HexValue = %HexValue%

@for /F "tokens=1,2,3 delims= " %%i in ('echo %HexValue:~-2,2%') do @set BYTE0=%%i
@for /F "tokens=1,2,3 delims= " %%i in ('echo %HexValue:~-4,2%') do @set BYTE1=%%i
@for /F "tokens=1,2,3 delims= " %%i in ('echo %HexValue:~-6,2%') do @set BYTE2=%%i
@for /F "tokens=1,2,3 delims= " %%i in ('echo %HexValue:~-8,2%') do @set BYTE3=%%i

if /I "%BT_NAME_BASE%" == "HJC_11BC" (
  set BT_ADDRESS="%BYTE0% %BYTE1% %BYTE2% %BYTE3% 11 BC"
) else (
  if /I "%BT_NAME_BASE%" == "HJC_11Be" (
    set BT_ADDRESS="%BYTE0% %BYTE1% %BYTE2% %BYTE3% 11 BE"
  ) else (
    set BT_ADDRESS="%BYTE0% %BYTE1% %BYTE2% %BYTE3% 11 BF"
  )
)
@set PREFIX_BT_NAME=%BYTE1%%BYTE0%

@fart -q .\ref\tmp_cfg.htf CHANGE_BT_ADDRESS %BT_ADDRESS%
@fart -q .\ref\tmp_cfg.htf CHANGE_BT_NAME %BT_NAME_BASE%_%PREFIX_BT_NAME%

@timeout /t 2

@%BIN_PATH%\configcmd txt2dev .\ref\tmp_cfg.htf REPLACE -%PROGRAM_TOOL% %PORT_NUMBER% -database .\ref\hydracore_config.sdb -system QCC514X_CONFIG -reset
@if NOT "%ERRORLEVEL%" == "0" goto ERROR

@del .\ref\tmp_cfg.htf

::%BIN_PATH%\btcli %PROGRAM_TOOL% %PORT_NUMBER% -xreset.script >nul
@if NOT "%ERRORLEVEL%" == "0" goto ERROR

@goto GOOD

:: A function to convert Decimal to Hexadecimal
:: you need to pass the Decimal as first parameter
:: and return it in the second
:: This function needs setlocal EnableDelayedExpansion to be set at the start if this batch file
:: Refer to https://gist.github.com/ijprest/1207832
:ConvertDecToHex
set LOOKUP=0123456789ABCDEF
set HEXSTR=
set PREFIX=

if "%1" EQU "" (
set "%2=0"
Goto:eof
)
set /a A=%1 || exit /b 1
if !A! LSS 0 set /a A=0xfffffff + !A! + 1 & set PREFIX=f
:loop
set /a B=!A! %% 16 & set /a A=!A! / 16
set HEXSTR=!LOOKUP:~%B%,1!%HEXSTR%
if %A% GTR 0 Goto :loop
set "%2=%PREFIX%%HEXSTR%"
Goto:eof

:ERROR
@color c
@echo.
@echo =================
@echo *** Program Fail ***
@echo =================
@goto END

:GOOD
@set /a SUCCESS_COUNT=%SUCCESS_COUNT%+1
@echo.
@echo ================================================
@echo *** SET BT Address OK (BT_ADDR : %BT_ADDRESS%)
@echo *** SET BT Name OK (BT_Name : %BT_NAME_BASE%_%PREFIX_BT_NAME%)
@echo ================================================

:End
@echo ** Success Count : %SUCCESS_COUNT%
@pause
@color f
@goto HOME
