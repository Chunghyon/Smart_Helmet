@echo off
setlocal enabledelayedexpansion
set "Exec=C:\Program Files (x86)\QTIL\BlueSuite 4.0.9\x64\CdaProdTestCmd.exe"
"%Exec%" -setup .\ptsetup_led.txt -sernum 0
endlocal