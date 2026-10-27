# Runs the client in a game window from the editor build (no packaging needed).
# Usage:  .\Scripts\Run.ps1                                   (node from Config\DefaultGame.ini)
#         .\Scripts\Run.ps1 -Node http://127.0.0.1:8000       (local test node)
#         .\Scripts\Run.ps1 -Screen wallet                     (open on a screen: wallet, feed, seed:<id>)
#         .\Scripts\Run.ps1 -Link "gfc://seed/<id>"            (test a deep link)
#         .\Scripts\Run.ps1 -Desktop                           (desktop renderer instead of mobile ES3.1)
#         .\Scripts\Run.ps1 -Desktop -Width 1280 -Height 800   (the desktop layout: landscape windows 1000 dp or wider)
# The default window is the target phone (1080x2388) at 45% size, 486x1075, so it fits a 1152 px tall monitor: same aspect, and the same 443 dp
# layout because UI scale follows the screen width. -Full opens it at 1080x2388 if your monitor is tall enough.
# It renders with the mobile feature level.
param(
    [string]$Node = '',
    [string]$Screen = '',
    [string]$Link = '',
    [int]$Width = 486,
    [int]$Height = 1075,
    [switch]$Full,
    [switch]$Desktop,
    [string]$EngineDir = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.7' })
)
if ($Full) { $Width = 1080; $Height = 2388 }
$project = Join-Path (Split-Path $PSScriptRoot -Parent) 'GFCClient.uproject'
$editor = Join-Path $EngineDir 'Engine\Binaries\Win64\UnrealEditor.exe'
$runArgs = @("`"$project`"", '-game', '-windowed', "-ResX=$Width", "-ResY=$Height", '-log')
if ($Node) { $runArgs += "-GFCNode=$Node" }
if ($Screen) { $runArgs += "-GFCScreen=$Screen" }
if ($Link) { $runArgs += "-GFCLink=$Link" }
if (-not $Desktop) { $runArgs += '-featureleveles31' }
Start-Process -FilePath $editor -ArgumentList $runArgs -PassThru
