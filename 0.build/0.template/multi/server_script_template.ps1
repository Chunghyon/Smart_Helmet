$COMPORTNUMBER = "COM9"
$pipeName = "HecaPipe"
# COM 포트 객체 생성 및 열기
$port = New-Object System.IO.Ports.SerialPort($COMPORTNUMBER, 115200, "None", 8, 1)
try {
    $port.Open()
    Write-Output "$COMPORTNUMBER Open Success"
} catch {
    Write-Output "$COMPORTNUMBER Open failed: $_"
    exit
}

$drainTime = 1000         # 대기 시간: 1000밀리초 (1초)
$stopWatch = [System.Diagnostics.Stopwatch]::StartNew()
$buffer = ""              # 누적 버퍼 초기화

while ($stopWatch.ElapsedMilliseconds -lt $drainTime) {
    if ($port.BytesToRead -gt 0) {
        $comData = $port.ReadExisting()
        $buffer += $comData

        # 버퍼에 새 줄('\n') 문자가 포함되어 있는지 확인
        if ($buffer -match "`n") {
            $lines = $buffer -split "`n"
            # 마지막 항목은 완전한 줄이 아닐 수 있으므로 그 전까지 출력
            for ($i = 0; $i -lt $lines.Count - 1; $i++) {
                # Windows에서는 '\r'이 함께 있을 수 있으므로 제거
                $line = $lines[$i].TrimEnd("`r")
                Write-Output $line
            }
            # 마지막 남은 미완성 줄을 버퍼에 보관
            $buffer = $lines[$lines.Count - 1]
        }
    } else {
        Start-Sleep -Milliseconds 50
    }
}

$stopWatch.Stop()

# 만약 버퍼에 남은 데이터가 있다면 (마지막 줄에 \n가 없다면) 출력할 수 있음
if ($buffer -ne "") {
    Write-Output $buffer
}

# 파이프 서버 생성을 위한 함수 정의
function Create-PipeServer {
    param([string]$pipeName)
    return New-Object System.IO.Pipes.NamedPipeServerStream(
        $pipeName,
        [System.IO.Pipes.PipeDirection]::InOut,
        1,
        [System.IO.Pipes.PipeTransmissionMode]::Message,
        [System.IO.Pipes.PipeOptions]::None
    )
}

$pipeServer = Create-PipeServer -pipeName $pipeName



$cancellationTokenSource = [System.Threading.CancellationTokenSource]::new()

$handler = [ConsoleCancelEventHandler]{
    param($sender, $eventArgs)
    $eventArgs.Cancel = $true
    Write-Output "Ctrl+C pressed. Cancelling waiting for connection..."
    $script:cancellationTokenSource.Cancel()
    $script:pipeServer.Dispose()
    exit
}

[Console]::add_CancelKeyPress($handler)

Write-Output "`nWaiting for client connection..."
$pipeServer.WaitForConnection()
Write-Output "Client connected!"
Write-Output "*************************************"

if (!(Test-Path ".\tmp")) {
    New-Item -ItemType Directory -Path ".\tmp" | Out-Null
}

if (Test-Path ".\tmp\tx.log") {
    Remove-Item ".\tmp\tx.log"
}

# 파이프의 StreamReader 생성 및 비동기 읽기 시작
$reader = New-Object System.IO.StreamReader($pipeServer)
$pipeTask = $reader.ReadLineAsync()

# 메인 루프: COM 포트 데이터와 파이프 데이터를 동시에 처리
while ($true) {

    # COM 포트에 읽을 데이터가 있다면
    if ($port.BytesToRead -gt 0) {
        $comData = $port.ReadExisting()
        $buffer += $comData

        if ($buffer -match "`n") {
            $lines = $buffer -split "`n"
            for ($i = 0; $i -lt $lines.Count - 1; $i++) {
                $line = $lines[$i].TrimEnd("`r")
                Write-Output $line
                Add-Content -Path ".\tmp\tx.log" -Value $line
            }
            $buffer = $lines[$lines.Count - 1]
        }
    }

    # 비동기 파이프 읽기 태스크가 완료되었는지 확인
    if ($pipeTask.IsCompleted) {
        $data = $pipeTask.Result

        # 클라이언트가 정상적으로 'exit' 메시지를 보내지 않고, 스트림을 닫은 경우를 감지
        if ([string]::IsNullOrEmpty($data)) {
            Write-Output "`nClient disconnected."

            # 기존 리더와 파이프 서버 정리
            $reader.Close()
            $pipeServer.Dispose()

            # 새 파이프 서버 인스턴스를 생성하여 클라이언트 연결 대기
            $pipeServer = Create-PipeServer -pipeName $pipeName
            Write-Output "`nWaiting for client connection..."
            $pipeServer.WaitForConnection()
            Write-Output "Client connected!"
            Write-Output "*************************************"

            # 새로운 StreamReader와 비동기 읽기 태스크 생성
            $reader = New-Object System.IO.StreamReader($pipeServer)
            $pipeTask = $reader.ReadLineAsync()
            continue
        }

        if ($data -eq "exit") {
            Write-Output "Exit command received"
            break
        }

        if ($data -match "^::") {
            if (Test-Path ".\tmp\tx.log") {
                Remove-Item ".\tmp\tx.log"
            }
            $msg = $data -replace "^::", ""
            Write-Output "`n$msg"
            $port.Write("$msg`r")
            $port.BaseStream.Flush()
        }
        # 새로운 비동기 파이프 읽기 태스크 시작
        $pipeTask = $reader.ReadLineAsync()
    }
    Start-Sleep -Milliseconds 10
}

# 정리
$port.Close()
$reader.Close()
$pipeServer.Close()
Write-Output "Server closed."
