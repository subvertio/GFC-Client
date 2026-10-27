# Packages the standalone Windows app (Win64, Shipping) into Packaged\Windows, then builds the installer.
# Usage:  .\Scripts\PackageWindows.ps1                   (Shipping build + installer)
#         .\Scripts\PackageWindows.ps1 -Config Development  (logs and console, for testing a packaged build)
#         .\Scripts\PackageWindows.ps1 -NoInstaller      (just the folder)
#         .\Scripts\PackageWindows.ps1 -SkipBuild        (reuse the last staged build, only redo the installer)
# Output: Packaged\Windows\                           the app folder; runs as is (GFCClient.exe)
#         Packaged\GFC-Setup-<version>-<commit>.exe   the installer
# The installed app's Start menu entry and gfc: handler start GFCClient\Binaries\Win64\GFCClient-Win64-Shipping.exe.
# The installer needs Inno Setup 6 (winget install JRSoftware.InnoSetup); without it you get the folder only.
param(
    [ValidateSet('Development', 'Shipping')]
    [string]$Config = 'Shipping',
    [switch]$NoInstaller,
    [switch]$SkipBuild,
    [string]$EngineDir = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.7' }),
    # Parallel compile jobs; lower it if the compiler runs out of memory.
    [int]$Jobs = 8,
    [string]$InnoSetup = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root 'GFCClient.uproject'
$archive = Join-Path $root 'Packaged'
$appDir = Join-Path $archive 'Windows'

if (-not $SkipBuild) {
    if ((Get-PSDrive C).Free / 1GB -lt 15) { throw 'Less than 15 GB free on C:. Free some space first.' }
    if (-not (Test-Path (Join-Path $root 'Build\Windows\Application.ico'))) { & (Join-Path $PSScriptRoot 'MakeIcon.ps1') }

    # Cooking needs the editor; capped parallelism avoids running out of memory for PCHs (C3859 / C1076).
    & (Join-Path $PSScriptRoot 'Build.ps1') -Targets Editor -Jobs $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Editor build failed.' }

    # A stale folder from an older build would leave removed files in the installer.
    if (Test-Path $appDir) { Remove-Item $appDir -Recurse -Force }
    $uat = Join-Path $EngineDir 'Engine\Build\BatchFiles\RunUAT.bat'
    $uatArgs = @("-project=$project", '-platform=Win64', "-clientconfig=$Config",
        '-build', '-cook', '-stage', '-pak', '-package', '-archive', "-archivedirectory=$archive",
        # The VC++ runtime installer, for PCs that don't have it (the installer runs it when needed).
        '-prereqs',
        '-nocompileeditor', "-ubtargs=-MaxParallelActions=$Jobs", '-nop4', '-utf8output', '-unattended')
    if ($Config -eq 'Shipping') { $uatArgs += '-distribution'; $uatArgs += '-nodebuginfo' }
    & $uat BuildCookRun @uatArgs
    if ($LASTEXITCODE -ne 0) { throw "Packaging failed (exit $LASTEXITCODE). See the log above." }
}

$suffix = if ($Config -eq 'Shipping') { '-Win64-Shipping' } else { '' }
$gameExe = Join-Path $appDir "GFCClient\Binaries\Win64\GFCClient$suffix.exe"
if (-not (Test-Path $gameExe)) { throw "No packaged app at $gameExe. Run without -SkipBuild (and with the same -Config)." }
Write-Host "App folder: $appDir" -ForegroundColor Green
if ($NoInstaller) { return }

if (-not $InnoSetup) {
    $InnoSetup = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $InnoSetup) {
    Write-Host 'Inno Setup 6 not found, so no installer. Install it with:  winget install JRSoftware.InnoSetup' -ForegroundColor Yellow
    return
}

$version = (Select-String (Join-Path $root 'Config\DefaultGame.ini') -Pattern '^ProjectVersion=(.+)$').Matches[0].Groups[1].Value.Trim()
$commit = (git -C $root rev-parse --short HEAD 2>$null)
if (-not $commit) { $commit = 'local' }
& $InnoSetup "/DAppVersion=$version" "/DAppCommit=$commit" "/DAppDir=$appDir" "/DGameExe=$(Split-Path $gameExe -Leaf)" `
    "/DOutDir=$archive" "/DIconFile=$(Join-Path $root 'Build\Windows\Application.ico')" (Join-Path $root 'Installer\GFC.iss')
if ($LASTEXITCODE -ne 0) { throw "Installer build failed (exit $LASTEXITCODE)." }
Get-ChildItem $archive -Filter "GFC-Setup-$version-$commit.exe" | Select-Object FullName, @{n = 'MB'; e = { [math]::Round($_.Length / 1MB, 1) } }
