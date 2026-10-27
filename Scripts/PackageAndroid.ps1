# Packages the Android Development build (arm64, ASTC) into Packaged\, keeping caches off C:.
# Usage:  .\Scripts\PackageAndroid.ps1                 (Development, the default for phones)
#         .\Scripts\PackageAndroid.ps1 -Config Shipping
# Needs: Android support for UE 5.7 (Epic Games Launcher > UE 5.7 > Options > Android) and the
# Android SDK/NDK (README > Android). Stops with a checklist if anything is missing.
param(
    [ValidateSet('Development', 'Shipping')]
    [string]$Config = 'Development',
    [string]$EngineDir = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.7' }),
    [string]$CacheRoot = 'D:\UnrealCache'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root 'GFCClient.uproject'

# The SDK variables are user-level; shells opened before they were set don't have them.
foreach ($name in 'ANDROID_HOME', 'NDKROOT', 'JAVA_HOME') {
    if (-not [Environment]::GetEnvironmentVariable($name, 'Process')) {
        [Environment]::SetEnvironmentVariable($name, [Environment]::GetEnvironmentVariable($name, 'User'), 'Process')
    }
}
$missing = @()
if (-not (Test-Path (Join-Path $EngineDir 'Engine\Build\Android'))) { $missing += 'UE 5.7 Android support: Epic Games Launcher > Library > 5.7 > Options > tick Android > Apply' }
if (-not $env:ANDROID_HOME -or -not (Test-Path $env:ANDROID_HOME)) { $missing += 'Android SDK (ANDROID_HOME): run Engine\Extras\Android\SetupAndroid.bat' }
if (-not $env:NDKROOT -or -not (Test-Path $env:NDKROOT)) { $missing += 'Android NDK r27c (NDKROOT): run SetupAndroid.bat' }
if (-not $env:JAVA_HOME -or -not (Test-Path $env:JAVA_HOME)) { $missing += "JDK (JAVA_HOME): Android Studio's jbr folder" }
if ($missing.Count) {
    Write-Host 'Android packaging needs:' -ForegroundColor Yellow
    $missing | ForEach-Object { Write-Host "  - $_" }
    exit 2
}
if ((Get-PSDrive C).Free / 1GB -lt 30) { throw 'Less than 30 GB free on C:. Free some space first.' }

# Gradle and Android build caches go to D:, not the user profile on C:.
$env:GRADLE_USER_HOME = Join-Path $CacheRoot 'Gradle'
$env:ANDROID_USER_HOME = Join-Path $CacheRoot 'AndroidUser'
New-Item -ItemType Directory -Force $env:GRADLE_USER_HOME, $env:ANDROID_USER_HOME | Out-Null

$uat = Join-Path $EngineDir 'Engine\Build\BatchFiles\RunUAT.bat'
& $uat BuildCookRun "-project=$project" -platform=Android -cookflavor=ASTC "-clientconfig=$Config" `
    -build -cook -stage -pak -package -archive "-archivedirectory=$(Join-Path $root 'Packaged')" `
    -nop4 -utf8output -unattended
if ($LASTEXITCODE -ne 0) { throw "Packaging failed (exit $LASTEXITCODE). See the log above." }
Get-ChildItem (Join-Path $root 'Packaged') -Recurse -Filter *.apk | Select-Object FullName, @{n = 'MB'; e = { [math]::Round($_.Length / 1MB, 1) } }
