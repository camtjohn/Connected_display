param(
    [string]$BrokerHost = "jbar.dev",
    [int]$BrokerPort = 8883,
    [string]$Topic,
    [string]$Environment,  # prod|debug
    [string]$Zipcode,
    [string]$DeviceName,
    [string]$CAFile,
    [string]$CertFile,
    [string]$KeyFile
)

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not $CAFile)   { $CAFile   = Join-Path $PSScriptRoot "certs\ca.crt" }
if (-not $CertFile) { $CertFile = Join-Path $PSScriptRoot "certs\device002.crt" }
if (-not $KeyFile)  { $KeyFile  = Join-Path $PSScriptRoot "certs\device002.key" }

if (-not (Get-Command mosquitto_sub -ErrorAction SilentlyContinue)) {
    Write-Error "mosquitto_sub not found on PATH. Install Mosquitto or add mosquitto_sub to PATH."
    exit 1
}
foreach ($p in @($CAFile,$CertFile,$KeyFile)) {
    if (-not (Test-Path $p)) { Write-Error "Missing required file: $p"; exit 1 }
}

Write-Host "`n****************************************" -ForegroundColor Green
Write-Host "*     MQTT SUBSCRIBER                 *" -ForegroundColor Green
Write-Host "****************************************`n" -ForegroundColor Green

if (-not $Topic) {
    if (-not $Environment) {
        Write-Host "Environment (prod/debug) [debug]: " -NoNewline
        $Environment = Read-Host
        if ([string]::IsNullOrWhiteSpace($Environment)) { $Environment = "debug" }
    }
    $debugMode = ($Environment -eq "debug")

    # Build topic list
    $topicGroups = @(
        @{ name = "weather"; label = "Weather Updates"; param = "Zipcode"; example = "60607" }
        @{ name = "device_specific"; label = "Device Version"; param = "Device Name"; example = "dev0" }
        @{ name = "shared_view"; label = "Shared View"; param = $null; example = $null }
        @{ name = "bootup"; label = "Device Bootup"; param = $null; example = $null }
        @{ name = "heartbeat"; label = "Device Heartbeat"; param = $null; example = $null }
    )

    Write-Host "`nAvailable Topic Groups:"
    for ($i = 0; $i -lt $topicGroups.Count; $i++) {
        Write-Host "  [$($i+1)] $($topicGroups[$i].label)"
    }
    Write-Host "`nSelect topic group [1]: " -NoNewline
    $sel = Read-Host
    if ([string]::IsNullOrWhiteSpace($sel)) { $sel = "1" }
    
    # Allow both number and string selection
    $group = $null
    if ($sel -match '^\d+$') {
        [int]$idx = $sel - 1
        if ($idx -lt 0 -or $idx -ge $topicGroups.Count) { Write-Error "Invalid selection"; exit 1 }
        $group = $topicGroups[$idx].name
    } else {
        $found = $topicGroups | Where-Object { $_.name -eq $sel }
        if ($found) {
            $group = $found.name
        } else {
            Write-Error "Unknown group: $sel"; exit 1
        }
    }

    switch ($group) {
        "weather" {
            if (-not $Zipcode) { $Zipcode = Read-Host "Zipcode (e.g. 60607)" }
            $Topic = if ($debugMode) { "debug_weather/$Zipcode" } else { "weather/$Zipcode" }
        }
        "device_specific" {
            if (-not $DeviceName) { $DeviceName = Read-Host "Device name (e.g. dev0)" }
            $Topic = if ($debugMode) { "debug_$DeviceName" } else { $DeviceName }
        }
        "shared_view" {
            $Topic = if ($debugMode) { "debug_shared_view" } else { "shared_view" }
        }
        "bootup" {
            $Topic = if ($debugMode) { "debug_dev_bootup" } else { "dev_bootup" }
        }
        "heartbeat" {
            $Topic = if ($debugMode) { "debug_dev_heartbeat" } else { "dev_heartbeat" }
        }
    }
}

# Subscribe in current terminal
Write-Host "Subscribing to topic: $Topic"
Write-Host "Press Ctrl+C to stop...`n"
& mosquitto_sub -h $BrokerHost -p $BrokerPort --cafile $CAFile --cert $CertFile --key $KeyFile -t $Topic -F "%t %x" | ForEach-Object {
    $line = $_
    if ($line -match '^(.+?) (.+)$') {
        $topic = $Matches[1]
        $hexString = $Matches[2] -replace '\s', ''  # Remove spaces
        $bytes = @()
        for ($i = 0; $i -lt $hexString.Length; $i += 2) {
            $bytes += [Convert]::ToInt32($hexString.Substring($i, 2), 16)
        }
        
        # Parse message type and decode
        $msgType = $bytes[0]
        $msgLength = $bytes[1]
        $payload = $bytes[2..($bytes.Length-1)]
        
        $decoded = ""
        switch ($msgType) {
            1 {  # Current Weather
                if ($payload.Count -ge 1) {
                    $temp = [int]$payload[0] - 50
                    $decoded = " temp=$temp"
                }
            }
            2 {  # Forecast Weather
                if ($payload.Count -ge 1) {
                    $numDays = $payload[0]
                    $decoded = " days=$numDays"
                }
            }
            16 { # Version (uint16 big-endian)
                if ($payload.Count -ge 2) {
                    $version = ([int]$payload[0] -shl 8) -bor [int]$payload[1]
                    $decoded = " version=$version"
                }
            }
            17 { # Heartbeat
                if ($payload.Count -ge 1) {
                    $nameLen = $payload[0]
                    if ($payload.Count -ge (1 + $nameLen)) {
                        $nameBytes = $payload[1..($nameLen)]
                        $deviceName = [System.Text.Encoding]::UTF8.GetString($nameBytes)
                        $decoded = " device=$deviceName"
                    }
                }
            }
        }
        
        Write-Host "topic=[$topic]$decoded payload=[$($bytes -join ' ')]"
    } else {
        Write-Host $_
    }
}
