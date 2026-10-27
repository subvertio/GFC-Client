# Runs the client in a game window from the editor build (no packaging needed).
# Usage:  .\Scripts\Run.ps1                                   (node from Config\DefaultGame.ini)
#         .\Scripts\Run.ps1 -Node http://127.0.0.1:8000       (local test node)
#         .\Scripts\Run.ps1 -Screen wallet                     (open on a screen: wallet, feed, seed:<id>)
param(
    [string]$Node = '',
    [string]$Screen = '',
    [int]$Width = 1600,
    [int]$Height = 900,
    [string]$EngineDir = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.7' })
)
$project = Join-Path (Split-Path $PSScriptRoot -Parent) 'GFCClient.uproject'
$editor = Join-Path $EngineDir 'Engine\Binaries\Win64\UnrealEditor.exe'
$runArgs = @("`"$project`"", '-game', '-windowed', "-ResX=$Width", "-ResY=$Height", '-log')
if ($Node) { $runArgs += "-GFCNode=$Node" }
if ($Screen) { $runArgs += "-GFCScreen=$Screen" }
Start-Process -FilePath $editor -ArgumentList $runArgs -PassThru
