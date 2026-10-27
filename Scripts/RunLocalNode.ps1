# Starts an isolated local GFC node for testing the client (P2P off, scratch data).
# Usage:  .\Scripts\RunLocalNode.ps1                       (backend at D:\projects\GFC, port 8000)
#         .\Scripts\RunLocalNode.ps1 -BackendDir C:\src\GFCCoin -Port 8100 -Fresh
# Then launch the client with -GFCNode=http://127.0.0.1:8000 (see Scripts\Run.ps1).
param(
    [string]$BackendDir = 'D:\projects\GFC',
    [int]$Port = 8000,
    [string]$DataDir = (Join-Path $env:TEMP 'gfc-client-test-node'),
    [switch]$Fresh
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path (Join-Path $BackendDir 'main.py'))) { throw "No GFC backend (main.py) in $BackendDir" }
if ($Fresh -and (Test-Path $DataDir)) { Remove-Item -Recurse -Force $DataDir }
New-Item -ItemType Directory -Force $DataDir | Out-Null

$env:GFC_BACKEND_DIR = (Resolve-Path $BackendDir).Path
$env:GFC_DATA_DIR = $DataDir
$env:GFC_BOOTSTRAP_PEERS = ''
Write-Host "Local test node: http://127.0.0.1:$Port  (data: $DataDir)"
Push-Location $PSScriptRoot
# uvicorn logs to stderr; with 'Stop', Windows PowerShell 5.1 would treat that as a fatal error.
$ErrorActionPreference = 'Continue'
try {
    py -3 -m uvicorn local_node:app --host 127.0.0.1 --port $Port
} finally {
    Pop-Location
}
