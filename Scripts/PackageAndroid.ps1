# Packages the Android build (arm64, ASTC) into Packaged\, keeping caches off C:.
# Usage:  .\Scripts\PackageAndroid.ps1                 (Development, the default for phones)
#         .\Scripts\PackageAndroid.ps1 -Config Shipping
#         .\Scripts\PackageAndroid.ps1 -Release        (Shipping, signed with your release key: for testers and Play)
#         .\Scripts\PackageAndroid.ps1 -Release -Bundle  (an .aab for Google Play)
# -Release asks for the release key's password (or reads GFC_KEYSTORE_PASSWORD). The key and password are
# handed to Unreal only for this build, through files that are git-ignored and deleted afterwards.
# Create the key once with .\Scripts\CreateReleaseKey.ps1.
# Needs: Android support for UE 5.7 (Epic Games Launcher > UE 5.7 > Options > Android) and the
# Android SDK/NDK (README > Android). Stops with a checklist if anything is missing.
param(
    [ValidateSet('Development', 'Shipping')]
    [string]$Config = 'Development',
    [switch]$Release,
    [string]$KeyStore = 'D:\GFCKeys\gfc-release.keystore',
    [string]$KeyAlias = 'gfc',
    [switch]$Bundle,
    [string]$EngineDir = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.7' }),
    [string]$CacheRoot = 'D:\UnrealCache',
    # Parallel compile jobs; lower it if the compiler runs out of memory.
    [int]$Jobs = 8,
    # A JDK 17/21 for Gradle; defaults to the newest D:\Java\jdk-21* folder.
    [string]$JavaHome = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root 'GFCClient.uproject'

# The SDK variables are user-level; shells opened before they were set don't have them.
# UE 5.7's Gradle (8.7) runs on JDK 17 or 21, not the JDK 25 bundled with newer Android Studio. A JDK 21
# under D:\Java is used for packaging only, without changing the user's JAVA_HOME.
if (-not $JavaHome) {
    $JavaHome = Get-ChildItem 'D:\Java' -Directory -Filter 'jdk-21*' -ErrorAction SilentlyContinue | Sort-Object Name -Descending |
        Where-Object { Test-Path (Join-Path $_.FullName 'bin\java.exe') } | Select-Object -First 1 -ExpandProperty FullName
}
if ($JavaHome) { $env:JAVA_HOME = $JavaHome }
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
# java -version prints to stderr; read it through cmd so PowerShell 5.1 doesn't treat that as an error.
$javaVersion = cmd /c "`"$(Join-Path $env:JAVA_HOME 'bin\java.exe')`" -version 2>&1" | Out-String
if ($javaVersion -notmatch 'version "(17|21)\.') {
    $missing += "JDK 17 or 21 for Gradle 8.7 (found: $(($javaVersion -split "`n")[0].Trim())). Unzip one to D:\Java\jdk-21..., or pass -JavaHome"
}
if ($Release -and -not (Test-Path $KeyStore)) { $missing += "Release key at ${KeyStore}: create it once with .\Scripts\CreateReleaseKey.ps1 (or pass -KeyStore)" }
if ($missing.Count) {
    Write-Host 'Android packaging needs:' -ForegroundColor Yellow
    $missing | ForEach-Object { Write-Host "  - $_" }
    exit 2
}
if ((Get-PSDrive C).Free / 1GB -lt 30) { throw 'Less than 30 GB free on C:. Free some space first.' }

# Release signing: Unreal reads the key from Build\Android and its settings from the Android platform ini.
$signingIni = Join-Path $root 'Config\Android\AndroidEngine.ini'
$keyCopy = Join-Path $root 'Build\Android\gfc-release.keystore'
$archive = Join-Path $root 'Packaged'
if ($Release) {
    $Config = 'Shipping'
    if (Test-Path $signingIni) { throw "$signingIni already exists. This script writes it for one build and deletes it; remove it (it may hold a key password)." }
    $archive = Join-Path $root 'Packaged\Release'
    $password = $env:GFC_KEYSTORE_PASSWORD
    if (-not $password) {
        $secure = Read-Host -AsSecureString "Password for $KeyStore"
        $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
        try { $password = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr) } finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr) }
    }
    if (-not $password -or $password -match '["\\\r\n]') { throw 'The key password is empty or has " or \ (Unreal settings files cannot hold those).' }
}

# Gradle and Android build caches go to D:, not the user profile on C:.
$env:GRADLE_USER_HOME = Join-Path $CacheRoot 'Gradle'
$env:ANDROID_USER_HOME = Join-Path $CacheRoot 'AndroidUser'
New-Item -ItemType Directory -Force $env:GRADLE_USER_HOME, $env:ANDROID_USER_HOME | Out-Null

# Build the Win64 editor (needed for cooking) with capped parallelism first: at full parallelism the
# compiler runs out of memory for precompiled headers on this machine (C3859 / C1076).
& (Join-Path $PSScriptRoot 'Build.ps1') -Targets Editor -Jobs $Jobs
if ($LASTEXITCODE -ne 0) { throw 'Editor build failed.' }

$uat = Join-Path $EngineDir 'Engine\Build\BatchFiles\RunUAT.bat'
$uatArgs = @("-project=$project", '-platform=Android', '-cookflavor=ASTC', "-clientconfig=$Config",
    '-build', '-cook', '-stage', '-pak', '-package', '-archive', "-archivedirectory=$archive",
    '-nocompileeditor', "-ubtargs=-MaxParallelActions=$Jobs -WaitMutex", '-nop4', '-utf8output', '-unattended',
    '-WaitForUATMutex') # queue behind another Unreal build on this PC instead of failing
try {
    if ($Release) {
        # -distribution makes Unreal sign with the release key instead of the debug one.
        $uatArgs += '-distribution'
        New-Item -ItemType Directory -Force (Split-Path $signingIni -Parent), (Split-Path $keyCopy -Parent) | Out-Null
        Copy-Item $KeyStore $keyCopy -Force
        $ini = @('[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]', 'KeyStore=gfc-release.keystore',
            "KeyAlias=$KeyAlias", "KeyStorePassword=$password", 'KeyPassword=')
        if ($Bundle) { $ini += 'bEnableBundle=True'; $ini += 'bEnableUniversalAPK=False' }
        Set-Content -Path $signingIni -Value $ini -Encoding ascii
        $password = $null
    }
    & $uat BuildCookRun @uatArgs
    if ($LASTEXITCODE -ne 0) { throw "Packaging failed (exit $LASTEXITCODE). See the log above." }
} finally {
    if ($Release) { Remove-Item $signingIni, $keyCopy -Force -ErrorAction SilentlyContinue }
}

$outputs = Get-ChildItem $archive -Recurse -Include *.apk, *.aab | Sort-Object LastWriteTime -Descending
if ($Release -and $outputs) {
    # A versioned copy to hand out: GFC-<version>-<commit>.apk
    $version = (Select-String (Join-Path $root 'Config\DefaultEngine.ini') -Pattern '^VersionDisplayName=(.+)$').Matches[0].Groups[1].Value.Trim()
    $commit = (git -C $root rev-parse --short HEAD 2>$null)
    $latest = $outputs[0]
    $named = Join-Path $archive ("GFC-$version-$commit" + $latest.Extension)
    Copy-Item $latest.FullName $named -Force
    Write-Host "Signed release: $named" -ForegroundColor Green
}
$outputs | Select-Object FullName, @{n = 'MB'; e = { [math]::Round($_.Length / 1MB, 1) } }
