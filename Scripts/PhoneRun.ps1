# Installs the newest packaged APK on the USB-connected phone, launches it, and saves logcat to D:.
# Usage:  .\Scripts\PhoneRun.ps1                         (install, launch, capture logs until Ctrl+C)
#         .\Scripts\PhoneRun.ps1 -NoInstall -Seconds 60  (just launch and capture 60 s)
#         .\Scripts\PhoneRun.ps1 -Node http://127.0.0.1:8100     (local test node on this PC, tunnelled over USB)
param(
    [switch]$NoInstall,
    [int]$Seconds = 0,
    [string]$Node = '',
    [string]$LogDir = 'D:\GFCLogs'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$sdk = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { [Environment]::GetEnvironmentVariable('ANDROID_HOME', 'User') }
$adb = Join-Path $sdk 'platform-tools\adb.exe'
$package = 'tech.galacticfederation.gfc'

$devices = & $adb devices | Select-String "`tdevice$"
if (-not $devices) { throw 'No authorised phone. Connect it over USB, accept the "Allow USB debugging?" prompt, and run: adb devices' }

if (-not $NoInstall) {
    $apk = Get-ChildItem (Join-Path $root 'Packaged') -Recurse -Filter *.apk | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $apk) { throw 'No APK in Packaged\. Run .\Scripts\PackageAndroid.ps1 first.' }
    Write-Host "Installing $($apk.Name) ($([math]::Round($apk.Length / 1MB, 1)) MB)"
    & $adb install -r $apk.FullName
    if ($LASTEXITCODE -ne 0) { throw 'adb install failed.' }
}

# Optional: a command line for this launch (UE reads UECommandLine.txt from the app's files folder).
if ($Node) {
    # A local test node (127.0.0.1 on this PC) is reached from the phone through the USB cable, so the node
    # never has to listen on the network.
    if ($Node -match '^http://(127\.0\.0\.1|localhost):(\d+)') {
        & $adb reverse "tcp:$($Matches[2])" "tcp:$($Matches[2])" | Out-Null
    }
    $cmd = "../../../GFCClient/GFCClient.uproject -GFCNode=$Node -GFCDebug"
    $tmp = Join-Path $env:TEMP 'UECommandLine.txt'
    Set-Content -Path $tmp -Value $cmd -NoNewline -Encoding ascii
    & $adb push $tmp "/sdcard/Android/data/$package/files/UnrealGame/GFCClient/UECommandLine.txt" | Out-Null
}

New-Item -ItemType Directory -Force $LogDir | Out-Null
$log = Join-Path $LogDir ("gfc-{0}.log" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
& $adb logcat -c
& $adb shell monkey -p $package -c android.intent.category.LAUNCHER 1 | Out-Null
Write-Host "Logging to $log (Ctrl+C to stop)"
if ($Seconds -gt 0) {
    $job = Start-Job { param($a, $l) & $a logcat -v time -s UE GFC AndroidRuntime DEBUG libc *:F | Out-File -FilePath $l -Encoding utf8 } -ArgumentList $adb, $log
    Start-Sleep $Seconds
    Stop-Job $job; Remove-Job $job
    Write-Host "Saved $log"
} else {
    & $adb logcat -v time -s UE GFC AndroidRuntime DEBUG libc *:F | Tee-Object -FilePath $log
}
