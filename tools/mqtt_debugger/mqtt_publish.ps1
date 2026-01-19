param(
    [string]$BrokerHost = "jbar.dev",
    [int]$BrokerPort = 8883,
    [string]$SpecPath,
    [string]$CAFile,
    [string]$CertFile,
    [string]$KeyFile
)

# Helper: pause if launched standalone (e.g., double-click)
function Pause-IfStandalone {
    if (-not $env:WT_SESSION -and -not $env:TERM_PROGRAM) {
        Write-Host ""; Read-Host "Press Enter to close"
    }
}

# Helper: stop with message and optional pause
function Stop-WithMessage {
    param([string]$Message)
    Write-Error $Message
    Pause-IfStandalone
    return
}

# Resolve defaults
$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not $SpecPath) { $SpecPath = Join-Path $workspaceRoot "MQTT_MESSAGES.json" }
if (-not $CAFile)   { $CAFile   = Join-Path $PSScriptRoot "certs\ca.crt" }
if (-not $CertFile) { $CertFile = Join-Path $PSScriptRoot "certs\dev99.crt" }
if (-not $KeyFile)  { $KeyFile  = Join-Path $PSScriptRoot "certs\dev99.key" }

# Check dependencies
if (-not (Get-Command mosquitto_pub -ErrorAction SilentlyContinue)) {
    Stop-WithMessage "mosquitto_pub not found on PATH. Install Mosquitto or add mosquitto_pub to PATH."
}
foreach ($p in @($SpecPath,$CAFile,$CertFile,$KeyFile)) {
    if (-not (Test-Path $p)) { Stop-WithMessage "Missing required file: $p" }
}

# Load spec
$specRaw = Get-Content $SpecPath -Raw
$spec = $specRaw | ConvertFrom-Json

Write-Host "`n****************************************" -ForegroundColor Green
Write-Host "*     MQTT PUBLISHER                  *" -ForegroundColor Green
Write-Host "****************************************`n" -ForegroundColor Green

# Main publishing loop
do {
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "MQTT Publisher" -ForegroundColor Cyan
    Write-Host "========================================`n" -ForegroundColor Cyan

    Write-Host "Environment (prod/debug) [debug]: " -NoNewline
    $envType = Read-Host
    if ([string]::IsNullOrWhiteSpace($envType)) { $envType = "debug" }
    $debugMode = ($envType -eq "debug")

# Build topic list with required parameters
$topics = @(
    @{ name = "weather/<zipcode>"; msgtypes = @("current_weather","forecast_weather"); requires = @("zipcode") }
    @{ name = "<device_name>"; msgtypes = @("version"); requires = @("device_name") }
    @{ name = "dev_bootup"; msgtypes = @("device_config"); requires = @("device_name","zipcode") }
    @{ name = "dev_heartbeat"; msgtypes = @("heartbeat"); requires = @("device_name") }
    @{ name = "shared_view"; msgtypes = @("shared_view_req","shared_view_frame"); requires = @() }
)

# Display topic list
Write-Host "`nAvailable Topics:"
for ($i = 0; $i -lt $topics.Count; $i++) {
    Write-Host "  [$($i+1)] $($topics[$i].name)"
}
Write-Host "`nSelect topic [1]: " -NoNewline
$sel = Read-Host
if ([string]::IsNullOrWhiteSpace($sel)) { $sel = "1" }
[int]$selIdx = $sel - 1

if ($selIdx -lt 0 -or $selIdx -ge $topics.Count) { Stop-WithMessage "Invalid selection" }
$selectedTopic = $topics[$selIdx]

# Collect required variables
$vars = @{}
foreach ($req in $selectedTopic.requires) {
    switch ($req) {
        "zipcode" { if (-not $vars.zipcode) { $vars.zipcode = Read-Host "Zipcode (e.g. 60607)" } }
        "device_name" { if (-not $vars.device_name) { $vars.device_name = Read-Host "Device name (e.g. dev0)" } }
    }
}

# Construct actual topic
$topic = $selectedTopic.name
if ($topic -match "<zipcode>") { $topic = $topic -replace "<zipcode>", $vars.zipcode }
if ($topic -match "<device_name>") { $topic = $topic -replace "<device_name>", $vars.device_name }
if ($debugMode) { $topic = "debug_" + $topic }

$allowedMsgs = $selectedTopic.msgtypes

# Select message type
if ($allowedMsgs.Count -eq 1) {
    $msgKey = $allowedMsgs[0]
} else {
    Write-Host "`nMessage Types:"
    for ($i = 0; $i -lt $allowedMsgs.Count; $i++) {
        Write-Host "  [$($i+1)] $($allowedMsgs[$i])"
    }
    Write-Host "Select message type [1]: " -NoNewline
    $sel = Read-Host
    if ([string]::IsNullOrWhiteSpace($sel)) { $sel = "1" }
    # Allow both number and string selection
    if ($sel -match '^\d+$') {
        [int]$idx = $sel - 1
        if ($idx -lt 0 -or $idx -ge $allowedMsgs.Count) { Stop-WithMessage "Invalid selection" }
        $msgKey = $allowedMsgs[$idx]
    } else {
        if ($allowedMsgs -contains $sel) {
            $msgKey = $sel
        } else {
            Stop-WithMessage "Unknown message type: $sel"
        }
    }
}

if (-not $spec."message types".PSObject.Properties.Name.Contains($msgKey)) {
    Stop-WithMessage "Unknown message key: $msgKey"
}
$msgSpec = $spec."message types".$msgKey
# Parse type from spec (supports "0xNN" hex strings or integers)
$typeRaw = $msgSpec.type
if ($typeRaw -is [string] -and $typeRaw.StartsWith("0x")) {
    $type = [Convert]::ToInt32($typeRaw.Substring(2), 16)
} else {
    $type = [int]$typeRaw
}

# Build payload
$payload = New-Object System.Collections.Generic.List[byte]

switch ($msgKey) {
    "current_weather" {
        $temp = [int](Read-Host "Actual temperature (°F) e.g. 20")
        $encoded = $temp + 50
        if ($encoded -lt 0 -or $encoded -gt 255) { Stop-WithMessage "Temperature out of range (-50..205°F)" }
        $payload.Add([byte]$encoded) | Out-Null
    }
    "forecast_weather" {
        $num = [int](Read-Host "Number of days (1-7)")
        if ($num -lt 1 -or $num -gt 7) { Stop-WithMessage "Days must be 1-7" }
        $payload.Add([byte]$num) | Out-Null
        for ($i=1; $i -le $num; $i++) {
            $high   = [int](Read-Host "Day $i high (°F 0-255)")
            $precip = [int](Read-Host "Day $i precip (%) 0-100")
            $moonPct= [int](Read-Host "Day $i moon (%) 0-100")
            $moonPhase = if ($moonPct -lt 93) { 0 } elseif ($moonPct -lt 100) { 1 } else { 2 }
            foreach ($v in @($high,$precip,$moonPhase)) {
                if ($v -lt 0 -or $v -gt 255) { Stop-WithMessage "Value out of range" }
                $payload.Add([byte]$v) | Out-Null
            }
        }
    }
    "version" {
        $ver = [int](Read-Host "Version (0-65535)")
        if ($ver -lt 0 -or $ver -gt 65535) { Stop-WithMessage "Version must be 0-65535" }
        # big-endian uint16
        $payload.Add([byte](($ver -band 0xFF00) -shr 8)) | Out-Null
        $payload.Add([byte]($ver -band 0x00FF)) | Out-Null
    }
    "device_config" {
        $strings = @($vars.device_name, $vars.zipcode)
        $payload.Add([byte]$strings.Length) | Out-Null
        foreach ($s in $strings) {
            $bytes = [System.Text.Encoding]::UTF8.GetBytes($s)
            if ($bytes.Length -gt 255) { Stop-WithMessage "String too long" }
            $payload.Add([byte]$bytes.Length) | Out-Null
            $payload.AddRange([byte[]]$bytes)
        }
    }
    "heartbeat" {
        $nameBytes = [System.Text.Encoding]::UTF8.GetBytes($vars.device_name)
        if ($nameBytes.Length -gt 255) { Stop-WithMessage "Device name too long" }
        $payload.Add([byte]$nameBytes.Length) | Out-Null
        $payload.AddRange([byte[]]$nameBytes)
    }
    "shared_view_req" {
        # No payload
    }
    "shared_view_frame" {
        # Full frame: 98 bytes payload (2 seq + 96 pixel data)
        $seq = [int](Read-Host "Sequence (0-65535)")
        if ($seq -lt 0 -or $seq -gt 65535) { Stop-WithMessage "Seq out of range" }
        # big-endian sequence
        $payload.Add([byte](($seq -band 0xFF00) -shr 8)) | Out-Null
        $payload.Add([byte]($seq -band 0x00FF)) | Out-Null
        
        Write-Host "Enter 96 bytes of pixel data (32 uint16 values for red, green, blue)"
        Write-Host "For testing, you can enter all zeros or use a pattern."
        $useZeros = Read-Host "Fill with zeros? (y/n) [y]"
        if ([string]::IsNullOrWhiteSpace($useZeros) -or $useZeros -eq "y") {
            for ($i = 0; $i -lt 96; $i++) {
                $payload.Add([byte]0) | Out-Null
            }
        } else {
            Write-Host "Enter 96 hex bytes (e.g. 00 FF 00 ...): " -NoNewline
            $hexInput = Read-Host
            $hexBytes = $hexInput -split ' ' | ForEach-Object { [Convert]::ToByte($_, 16) }
            if ($hexBytes.Count -ne 96) { Stop-WithMessage "Expected 96 bytes, got $($hexBytes.Count)" }
            $payload.AddRange([byte[]]$hexBytes)
        }
    }
    default { Stop-WithMessage "Unsupported message builder for $msgKey" }
}

# Construct final bytes: [type][length][payload]
$length = $payload.Count
if ($length -gt 255) { Stop-WithMessage "Payload length $length exceeds 255" }
$bytes = New-Object System.Collections.Generic.List[byte]
$bytes.Add([byte]$type) | Out-Null
$bytes.Add([byte]$length) | Out-Null
$bytes.AddRange([byte[]]$payload)

# Print human-readable message summary
$summary = "topic=[$topic]"
if ($vars.device_name) { $summary += " name=[$($vars.device_name)]" }
if ($vars.zipcode) { $summary += " zip=[$($vars.zipcode)]" }
$summary += " payload=[$($bytes -join ' ')]"
Write-Host "`n$summary`n"

# Write to persistent file for inspection
$payloadDir = Join-Path $PSScriptRoot "payloads"
if (-not (Test-Path $payloadDir)) { New-Item -ItemType Directory -Path $payloadDir | Out-Null }
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$payloadFile = Join-Path $payloadDir "payload_${timestamp}.bin"
[System.IO.File]::WriteAllBytes($payloadFile, $bytes.ToArray())
Write-Host "Wrote payload to $payloadFile (length=$($bytes.Count))"

# Publish
$cmd = @(
    "mosquitto_pub",
    "-h", $BrokerHost,
    "-p", $BrokerPort,
    "--cafile", $CAFile,
    "--cert", $CertFile,
    "--key", $KeyFile,
    "-t", $topic,
    "-f", $payloadFile
)
Write-Host "Publishing to topic '$topic'..."
& $cmd[0] $cmd[1..($cmd.Length-1)]
if ($LASTEXITCODE -ne 0) { Write-Error "Publish failed with code $LASTEXITCODE" } else { Write-Host "Publish succeeded." }

    # Ask if user wants to send another message
    Write-Host "`nSend another message? (y/n) [y]: " -NoNewline
    $continue = Read-Host
    if ([string]::IsNullOrWhiteSpace($continue)) { $continue = "y" }
    if ($continue -ne "y") { break }
} while ($true)

Write-Host "`nExiting...`n"

# Pause if launched standalone so window stays open
Pause-IfStandalone
