# Checks the settings that make every build look the same, sized to its screen. Build.ps1 and
# PackageAndroid.ps1 run it first and stop if anything is off, so a build from an old branch or with
# changed UI settings fails here instead of shipping a different-looking app.
# Usage:  .\Scripts\CheckLayout.ps1                  (all checks)
#         .\Scripts\CheckLayout.ps1 -Platform Android (adds the Android-only ones)
param(
    [ValidateSet('Win64', 'Android')]
    [string]$Platform = 'Win64'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent

# The layout's scale: UGFCDPIScalingRule applies this curve to the screen's shortest side. Change it here
# and in Config\DefaultEngine.ini together, on purpose.
$expected = [ordered]@{
    UIScaleRule            = 'Custom'
    CustomScalingRuleClass = '/Script/GFCClient.GFCDPIScalingRule'
    UIScaleCurve           = '(EditorCurveData=(Keys=((Time=443,Value=1.0),(Time=486,Value=1.09707),(Time=1080,Value=2.4375))),ExternalCurve=None)'
}
if ($Platform -eq 'Android') {
    # Types into the field itself; without it every field opens Android's full-screen white text box.
    $expected['bEnableNewKeyboard'] = 'True'
}

# Unreal reads these in this order, later ones winning. Saved\Config is per machine (the editor writes it).
$inis = @(
    'Config\DefaultEngine.ini'
    "Config\$Platform\${Platform}Engine.ini"
    "Saved\Config\$(if ($Platform -eq 'Win64') { 'Windows' } else { 'Android' })\Engine.ini"
) | ForEach-Object { Join-Path $root $_ } | Where-Object { Test-Path $_ }

$problems = @()
foreach ($file in 'Source\GFCClient\UI\GFCDPIScalingRule.h', 'Source\GFCClient\UI\GFCDPIScalingRule.cpp') {
    if (-not (Test-Path (Join-Path $root $file))) {
        $problems += "$file is missing: this checkout predates the screen-size layout. Build from the branch in the README."
    }
}
foreach ($key in $expected.Keys) {
    $value = $null
    $from = $null
    foreach ($ini in $inis) {
        Select-String -Path $ini -Pattern "^\s*$key\s*=\s*(.+?)\s*$" | ForEach-Object { $value = $_.Matches[0].Groups[1].Value; $from = $ini }
    }
    if ($null -eq $value) {
        $problems += "$key isn't set (expected $($expected[$key]) in Config\DefaultEngine.ini)"
    } elseif ($value -ne $expected[$key]) {
        $problems += "$key is '$value' in $($from.Substring($root.Length + 1)), expected '$($expected[$key])'"
    }
}

if ($problems.Count) {
    Write-Host 'The interface would not match other builds:' -ForegroundColor Yellow
    $problems | ForEach-Object { Write-Host "  - $_" }
    throw 'Layout settings check failed (Scripts\CheckLayout.ps1).'
}
Write-Host "== Layout settings OK ($Platform)"
