$CONST_USBPORT = @(111, 222, 333, 444, 555)
$FUSING_FILE = "CHANGE_FUSING_FILE_NAME"
$SERIAL_FILE = ".\CHANGE_SERIAL_FILE_NAME.txt"
$HTF_FILE = ".\CHANGE_HTF_FILE_NAME.htf"

$AverageThreshold = -80
$pipeName = "HecaPipe"

if (!(Test-Path ".\tmp")) {
    New-Item -ItemType Directory -Path ".\tmp" | Out-Null
}

$pipeClient = New-Object System.IO.Pipes.NamedPipeClientStream(".", $pipeName, [System.IO.Pipes.PipeDirection]::InOut)
try {
    Write-Output "Connecting to pipe server..."
    $pipeClient.Connect(5000)  # 5초 타임아웃
} catch {
    Write-Output "Could not connect to server: $_"
    Write-Output ""
    exit
}

$writer = New-Object System.IO.StreamWriter($pipeClient)
$writer.AutoFlush = $true
Write-Output "Connected to Server!"

$array_index = 0
do {
	# TEST0 : Check all slots
    $COMMAND0 = '"C:\qtil\ADK_Toolkit_1.2.16.21_x64\tools\bin\TransportUnlock.exe" list'
    $RESULT = cmd /c $COMMAND0 | Out-String
    $missingSlots = @()

    $SKIP_SLOTS = @(0, 0, 0, 0, 0)  # 매 실행마다 초기화
    $ADDRESS_SLOTS = @(0, 0, 0, 0, 0)

    $array_index = 0
    foreach ($USBPORT in $CONST_USBPORT) {
        $slot_number = $array_index + 1
        if ($RESULT -notmatch "USBDBG.*\($USBPORT\).*SPIPORT=(\d+)") {
            $SKIP_SLOTS[$array_index] = 1  # 해당 Slot 누락 → 1 저장
            Write-Host "Slot$slot_number, $USBPORT, Not found" -ForegroundColor Red
            $missingSlots += "Slot$slot_number"
        }
        $array_index += 1
    }

	if ($missingSlots.Count -eq 5) {
		Write-Host ""
		Write-Host "All slots are missed. Press any key when ready..." -ForegroundColor Red
		[System.Console]::ReadKey() | Out-Null
		continue
	} elseif ($missingSlots.Count -gt 0) {
        Write-Host "Missing slots: $($missingSlots -join ', ')" -ForegroundColor Yellow
        $userChoice = Read-Host "Some slots are missing. Continue? (Y/N)"
        if ($userChoice -imatch "^[Nn]$") {
            Write-Host "Process terminated due to missing slots." -ForegroundColor Red
            continue
        } else {
            Write-Host "Continuing process..." -ForegroundColor Green
        }
    }

    # TEST1 : Program all slots except for skipped slot
	Write-Output ""
	Write-Output "*******************************************************"
	Write-Output "** Bluetooth Programming($FUSING_FILE)..."
	Write-Output "*******************************************************"
    $array_index = 0
    $program_fail_count = 0

    foreach ($USBPORT in $CONST_USBPORT) {
        $slot_number = $array_index + 1
        if ($SKIP_SLOTS[$array_index] -eq 1) {
            Write-Host "Slot$slot_number skipped." -ForegroundColor Yellow
        } else {
            Write-Host "Slot$slot_number Programming(usbdbg# $($CONST_USBPORT[$array_index]))..." -NoNewLine
            $COMMAND1 = '"C:\Program Files (x86)\QTIL\BlueSuite 4.0.9\nvscmd.exe" burn .\' + $FUSING_FILE + '.xuv -deviceid 4 0 -usbdbg ' + $USBPORT + ' -nvstype sqif >nul'
            cmd /c $COMMAND1
            if ($LASTEXITCODE -ne 0) {
                Write-Host " Failed." -ForegroundColor Red
                $SKIP_SLOTS[$array_index] = 1  # 실패 발생 시 해당 Slot을 1로 변경
                $program_fail_count += 1
            } else {
                Write-Host " Success." -ForegroundColor Green
            }
        }
        $array_index += 1
    }

    if ($program_fail_count -gt 0) {
        $userChoice = Read-Host "Some slots Program Failed. Continue? (Y/N)"
        if ($userChoice -imatch "^[Nn]$") {
            Write-Host "Process terminated due to program failure. Press any key to re-start..." -ForegroundColor Red
            [System.Console]::ReadKey() | Out-Null
            continue
        } else {
            Write-Host "Continuing process..." -ForegroundColor Green
        }
    }

	Start-Sleep -Seconds 2  # 2초 동안 대기

	Write-Output ""
	Write-Output "*******************************************************"
	Write-Output "** Bluetooth Setting..."
	Write-Output "*******************************************************"
    $array_index = 0
    $setting_fail_count = 0
    foreach ($USBPORT in $CONST_USBPORT) {
        $slot_number = $array_index + 1
        if ($SKIP_SLOTS[$array_index] -eq 1) {
            Write-Host "Slot$slot_number skipped." -ForegroundColor Yellow
        } else {
			# TEST3 : Update Serial/BTAddress/BTName
			$MY_HTF_FILE = ".\tmp\my.htf"
			if (Test-Path $MY_HTF_FILE) {
				Remove-Item $MY_HTF_FILE -Force
			}
			
			# 3-1. SERIAL_FILE에서 현재 HEX 값 읽기
			$my_serial = Get-Content $SERIAL_FILE | Select-Object -First 1
			# Endian 변환
			$byte_array = $my_serial -split '(..)' | Where-Object { $_ -ne "" }
			$big_endian_serial = $byte_array[-1..-$byte_array.Count] -join ''
			$ADDRESS_SLOTS[$array_index] = $big_endian_serial

			# 3-2. HTF 파일명에서 첫 4글자(XXXX) 추출 & 시리얼 끝 4자리(YYYY) 추출
			$XXXX = [System.IO.Path]::GetFileNameWithoutExtension($HTF_FILE).Substring(0,4)
			$YYYY = $my_serial.Substring(8,4)
			$my_bt_name = "HJC_" + $XXXX + "_" + $YYYY

			# 3-3. HTF_FILE 수정 및 저장
			(Get-Content $HTF_FILE) -replace "CHANGE_BT_ADDRESS", $ADDRESS_SLOTS[$array_index] `
									-replace "CHANGE_BT_NAME", $my_bt_name `
									| Set-Content $MY_HTF_FILE

			# 3-4. HEX 값을 숫자로 변환 후 1 증가
			$tmp_number = [convert]::ToInt64($my_serial, 16) + 1
			$next_serial = "{0:X12}" -f $tmp_number  # 12자리 HEX 값으로 변환

			# 3-5. 변경된 값을 다시 SERIAL_FILE에 저장
			$next_serial | Set-Content $SERIAL_FILE
    
			# TEST4 :
			$PTSETUP_TEMPLATE = ".\ptsetup_template.txt"
			$MY_PTSETUP_FILE = ".\tmp\my_ptsetup.txt"
			if (Test-Path $MY_PTSETUP_FILE) {
				Remove-Item $MY_PTSETUP_FILE -Force
			}

            $current_spiport = 0
            $COMMAND0 = '"C:\qtil\ADK_Toolkit_1.2.16.21_x64\tools\bin\TransportUnlock.exe" list'
            $RESULT = cmd /c $COMMAND0 | Out-String
            if ($RESULT -match "USBDBG.*\($USBPORT\).*SPIPORT=(\d+)") {
                $current_spiport = $matches[1]
                Write-Host "Slot$slot_number Setting($my_serial, spiport# $current_spiport)... " -NoNewLine
                (Get-Content $PTSETUP_TEMPLATE) -replace "CHANGE_SPI_PORT", $current_spiport | Set-Content $MY_PTSETUP_FILE
            }
            else {
				$SKIP_SLOTS[$array_index] = 1
				$setting_fail_count += 1
				Write-Host "Slot$slot_number, $USBPORT is not found." -ForegroundColor Red
				$array_index += 1
				continue;
            }

			$COMMAND2 = '"C:\Program Files (x86)\QTIL\BlueSuite 4.0.9\x64\CdaProdTestCmd.exe" -setup ' + $MY_PTSETUP_FILE + ' -sernum ' + $my_serial + ' >nul'
			cmd /c $COMMAND2
            if ($LASTEXITCODE -ne 0) {
                $SKIP_SLOTS[$array_index] = 1  # 실패 발생 시 해당 Slot을 1로 변경
                $setting_fail_count += 1
                Write-Host "Failed." -ForegroundColor Red
				$array_index += 1
                continue;
            } else {
                Write-Host "Success." -ForegroundColor Green
            }
		}

		$array_index += 1
	}

	if ($setting_fail_count -gt 0) {
        $userChoice = Read-Host "Some slots Setting Failed. Continue? (Y/N)"
        if ($userChoice -imatch "^[Nn]$") {
            Write-Host "Process terminated due to Setting failure. Press any key to re-start..." -ForegroundColor Red
            [System.Console]::ReadKey() | Out-Null
            continue
        } else {
            Write-Host "Continuing process..." -ForegroundColor Yellow
        }
	}

	Write-Output ""
	Write-Output "*******************************************************"
	Write-Output "** Bluetooth TX/RX Testing..."
	Write-Output "*******************************************************"

	$targetLogFile = ".\log\pt_results.txt"
	$array_index = 0
    foreach ($USBPORT in $CONST_USBPORT) {
        $slot_number = $array_index + 1
        if ($SKIP_SLOTS[$array_index] -eq 1) {
            Write-Host "Slot$slot_number skipped due to previous failure or missing USBPORT." -ForegroundColor Yellow
        } else {
			if (Test-Path ".\tmp\tx.log") {
			  Remove-Item ".\tmp\tx.log"
			}
			if (Test-Path ".\tmp\rx.log") {
			  Remove-Item ".\tmp\rx.log"
			}

			# Endian 변환
			$byte_array = $ADDRESS_SLOTS[$array_index] -split '(..)' | Where-Object { $_ -ne "" }
			$big_endian_serial = $byte_array[-1..-$byte_array.Count] -join ''
			$serial = $big_endian_serial
			
			Write-Host "Slot#$slot_number Testing($serial)... " -NoNewLine
			$writer.WriteLine('::$start,' + $serial)
			$cmdLine = '"C:\Program Files (x86)\QTIL\BlueSuite 4.0.9\btcli" usbdbg ' + $CONST_USBPORT[$array_index] + ' -xbtcli.script > .\tmp\rx.log'
			cmd.exe /c $cmdLine

			# rx.log 파일의 라인 중 "0xcacbcccdcecf"가 포함된 것만 필터링
			$rxFilteredLines = Get-Content -Path ".\tmp\rx.log" | Where-Object { $_ -match "0xcacbcccdcecf" }

			# 각 필터된 라인에서 "rssi:" 뒤에 나오는 정수를 정규표현식으로 추출
			$rxRssiValues = foreach ($rxLine in $rxFilteredLines) {
				if ($rxLine -match "rssi\s*:\s*(-?\d+)") {
					[int]$Matches[1]
				}
			}

			if ($rxRssiValues.Count -gt 0) {
				$rxAverage = ($rxRssiValues | Measure-Object -Average).Average
				$intRxAverage = [int]$rxAverage
			} else {
				Write-Host "RX FAIL." -ForegroundColor Red
				$appendText = ", BT RX Fail"
				(Get-Content $targetLogFile) | ForEach-Object {
					if ($_ -like "*$serial*") {
						$_ + $appendText   # 해당 줄 끝에 추가
					}
					else {
						$_
					}
				} | Set-Content $targetLogFile              
				$array_index += 1
				continue
			}

			# tx.log 파일의 라인 중 "rssi:"가 포함된 것만 필터링하여 TX RSSI 값 추출
			$txFilteredLines = Get-Content -Path ".\tmp\tx.log" | Where-Object { $_ -match "rssi\s*:\s*(-?\d+)" }
			$txRssiValues = foreach ($txLine in $txFilteredLines) {
				if ($txLine -match "rssi\s*:\s*(-?\d+)") {
					[int]$Matches[1]
				}
			}
			if ($txRssiValues.Count -gt 0) {
				$txAverage = ($txRssiValues | Measure-Object -Average).Average
				$intTxAverage = [int]$txAverage
			} else {
				Write-Host "TX FAIL." -ForegroundColor Red
				$appendText = ", BT TX Fail"
				(Get-Content $targetLogFile) | ForEach-Object {
					if ($_ -like "*$serial*") {
						$_ + $appendText   # 해당 줄 끝에 추가
					}
					else {
						$_
					}
				} | Set-Content $targetLogFile              
				$array_index += 1
				continue
			}

			Write-Host "average_rssi rx: $intRxAverage, tx: $intTxAverage => " -NoNewLine
			if (($rxAverage -gt $AverageThreshold) -and ($txAverage -gt $AverageThreshold)) {
				Write-Host "PASS" -ForegroundColor Green
				$appendText = ", average_rssi rx $intRxAverage, tx $intTxAverage => PASS"
				(Get-Content $targetLogFile) | ForEach-Object {
					if ($_ -like "*$serial*") {
						$_ + $appendText   # 해당 줄 끝에 추가
					}
					else {
						$_
					}
				} | Set-Content $targetLogFile              
			} else {
				Write-Host "FAIL" -ForegroundColor Red
				$appendText = ", average_rssi rx $intRxAverage, tx $intTxAverage => FAIL"
				(Get-Content $targetLogFile) | ForEach-Object {
					if ($_ -like "*$serial*") {
						$_ + $appendText   # 해당 줄 끝에 추가
					}
					else {
						$_
					}
				} | Set-Content $targetLogFile              
			}
        }
        
        $array_index += 1
	}

	# 루프 종료: 키보드 입력을 받아 ESC가 아닌 경우 루프 재시작
	Write-Output "`n!! Press ESC key to exit or any other key to restart the Program and Test."
	$key = [Console]::ReadKey($true)

	# ESC가 아니라면, 재시작 전에 파이프 연결 상태 확인 및 재연결 시도
	if ($key.Key -ne 'Escape') {
		if (-not $pipeClient.IsConnected) {
			Write-Host "Pipe disconnected. Attempting to reconnect..." -ForegroundColor Yellow
			try {
				$pipeClient.Dispose()
			} catch {}
			$pipeClient = New-Object System.IO.Pipes.NamedPipeClientStream(".", $pipeName, [System.IO.Pipes.PipeDirection]::InOut)
			try {
				$pipeClient.Connect(5000)
				$writer = New-Object System.IO.StreamWriter($pipeClient)
				$writer.AutoFlush = $true
				Write-Host "Reconnected to Server!" -ForegroundColor Green
			} catch {
				Write-Host "Reconnection failed: $_" -ForegroundColor Red
				# 필요한 경우, 재연결 실패 시 추가 처리를 할 수 있음.
			}
		}
	}
} while ($key.Key -ne 'Escape')

$writer.Close()
$pipeClient.Close()
Write-Output "Client closed."
