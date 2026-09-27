param(
    [Parameter(Mandatory=$true)][System.Net.IPAddress]$CameraIP,
    [Parameter(Mandatory=$true)][string]$Interface,
    [ValidateRange(10,600)][int]$Seconds = 120
)
$ErrorActionPreference = 'Stop'
$dumpcap = Join-Path $env:ProgramFiles 'Wireshark\dumpcap.exe'
if (!(Test-Path -LiteralPath $dumpcap)) { throw 'Wireshark dumpcap not found.' }
$captureDir = Join-Path $PSScriptRoot '..\captures'
New-Item -ItemType Directory -Path $captureDir -Force | Out-Null
$file = Join-Path $captureDir ("remote-{0}.pcapng" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
# Capture the camera and SSDP/mDNS discovery; do not assume a control port.
$filter = "host $CameraIP or (udp and (port 1900 or port 5353))"
Write-Host "Capture: $file"
Write-Host 'Now reconnect Remote, wait 10 seconds, perform one action at a time, then disconnect.'
& $dumpcap -i $Interface -f $filter -s 0 -a "duration:$Seconds" -w $file
if ($LASTEXITCODE -ne 0) { throw "dumpcap failed: $LASTEXITCODE" }
Write-Host "Saved: $file"
