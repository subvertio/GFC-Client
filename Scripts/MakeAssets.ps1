# Regenerates the few binary assets the project needs, from scripts (no hand editing):
#   /Game/GFC/M_GFCModel   material used by the runtime 3D viewer
# Usage:  .\Scripts\MakeAssets.ps1
param(
    [string]$EngineDir = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.7' })
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root 'GFCClient.uproject'
$editor = Join-Path $EngineDir 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$script = Join-Path $PSScriptRoot 'make_viewer_material.py'
$log = Join-Path $root 'Saved\Logs\GFCMakeAssets.log'

& $editor $project "-ExecutePythonScript=$script" -unattended -nopause -nosplash -NoSound "-abslog=$log" | Out-Null
if (-not (Test-Path (Join-Path $root 'Content\GFC\M_GFCModel.uasset'))) { throw "Material wasn't created. See $log" }
Write-Host 'Created Content\GFC\M_GFCModel.uasset'
