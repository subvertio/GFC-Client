# Builds GFC Client targets with UnrealBuildTool.
# Usage:  .\Scripts\Build.ps1                 (Development Editor + Development Game, Win64)
#         .\Scripts\Build.ps1 -Targets Editor
param(
    [ValidateSet('Editor', 'Game')]
    [string[]]$Targets = @('Editor', 'Game'),
    [string]$Configuration = 'Development',
    [string]$Platform = 'Win64',
    # Each compiler process reserves a lot of memory for the PCH; cap them if you hit C3859/C1076.
    [int]$Jobs = 8,
    [string]$EngineDir = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.7' })
)
$ErrorActionPreference = 'Stop'
$project = Join-Path (Split-Path $PSScriptRoot -Parent) 'GFCClient.uproject'
$build = Join-Path $EngineDir 'Engine\Build\BatchFiles\Build.bat'
if (-not (Test-Path $build)) { throw "UnrealBuildTool not found at $build (set UE_ROOT)" }

foreach ($t in $Targets) {
    $name = if ($t -eq 'Editor') { 'GFCClientEditor' } else { 'GFCClient' }
    Write-Host "== Building $name $Platform $Configuration"
    & $build $name $Platform $Configuration "-Project=$project" -WaitMutex -NoHotReload "-MaxParallelActions=$Jobs"
    if ($LASTEXITCODE -ne 0) { throw "$name build failed (exit $LASTEXITCODE)" }
}
Write-Host '== Build succeeded'
