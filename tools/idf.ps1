param(
    [ValidateSet('build', 'flash', 'monitor', 'menuconfig', 'size', 'reconfigure')]
    [string]$Action = 'build',
    [string]$Port = 'COM8',
    [string]$ProjectDirectory = (Join-Path $PSScriptRoot '..'),
    [string]$IdfPath = 'C:\Espressif\frameworks\esp-idf-v5.5.1',
    [string]$IdfPython = 'C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe'
)
$ErrorActionPreference = 'Stop'
if (!(Test-Path -LiteralPath (Join-Path $IdfPath 'export.ps1'))) {
    throw "ESP-IDF not found at $IdfPath; supply -IdfPath."
}
$env:IDF_PATH = $IdfPath
$env:PYTHONNOUSERSITE = '1'
$env:PATH = "$(Split-Path $IdfPython);$env:PATH"
. (Join-Path $IdfPath 'export.ps1')
Push-Location -LiteralPath $ProjectDirectory
try {
    & $IdfPython (Join-Path $IdfPath 'tools\idf.py') -p $Port $Action
    if ($LASTEXITCODE -ne 0) { throw "idf.py failed ($LASTEXITCODE)" }
} finally {
    Pop-Location
}
