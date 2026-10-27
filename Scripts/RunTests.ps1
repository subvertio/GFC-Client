# Runs the GFC automation tests headless (no window, no GPU).
# Usage:  .\Scripts\RunTests.ps1                     (all GFC tests; the E2E test skips itself)
#         .\Scripts\RunTests.ps1 -E2ENode http://127.0.0.1:8000 [-ExpectSeed <id>] [-SaveSession]
# Only point -E2ENode at a local test node (Scripts\RunLocalNode.ps1): it registers throwaway accounts.
param(
    [string]$Filter = 'GFC',
    [string]$E2ENode = '',
    [string]$ExpectSeed = '',
    [switch]$SaveSession,
    [string]$EngineDir = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.7' })
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root 'GFCClient.uproject'
$editor = Join-Path $EngineDir 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$report = Join-Path $root 'Saved\Automation\Report'
$log = Join-Path $root 'Saved\Logs\GFCTests.log'
if (Test-Path $report) { Remove-Item -Recurse -Force $report }

$testArgs = @(
    $project,
    "-ExecCmds=Automation RunTests $Filter; Quit",
    '-unattended', '-nopause', '-nosplash', '-nullrhi', '-NoSound',
    "-ReportExportPath=$report", "-abslog=$log"
)
if ($E2ENode) { $testArgs += "-GFCE2ENode=$E2ENode" }
if ($ExpectSeed) { $testArgs += "-GFCE2EExpectSeed=$ExpectSeed" }
if ($SaveSession) { $testArgs += '-GFCE2ESaveSession' }

& $editor @testArgs | Out-Null
$code = $LASTEXITCODE

$json = Join-Path $report 'index.json'
if (-not (Test-Path $json)) { throw "No test report at $json (editor exit $code). See $log" }
$r = Get-Content $json -Raw -Encoding UTF8 | ConvertFrom-Json
foreach ($t in $r.tests) {
    '{0,-8} {1}' -f $t.state, $t.fullTestPath
    foreach ($e in @($t.entries)) {
        if ($e.event.type -eq 'Error' -or $e.event.type -eq 'Warning') { "         [$($e.event.type)] $($e.event.message)" }
        elseif ($e.event.type -eq 'Info' -and $t.fullTestPath -like 'GFC.E2E*') { "         $($e.event.message)" }
    }
}
"Passed: $($r.succeeded)  Passed with warnings: $($r.succeededWithWarnings)  Failed: $($r.failed)  Not run: $($r.notRun)"
if ($r.failed -gt 0 -or ($r.succeeded + $r.succeededWithWarnings) -eq 0) { exit 1 }
