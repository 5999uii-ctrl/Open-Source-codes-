# =========================================================================
# A.A OS - Master Test Suite Runner
# Executes test_build.ps1, test_symbols.ps1, and test_video_e2e.ps1
# =========================================================================

$ErrorActionPreference = "Continue"
$projectRoot = (Resolve-Path "$PSScriptRoot\..").Path
Set-Location $projectRoot

Write-Host "===============================================================================" -ForegroundColor Magenta
Write-Host "  A.A OS MASTER TEST RUNNER - COMPLETE E2E VERIFICATION PIPELINE" -ForegroundColor White
Write-Host "===============================================================================" -ForegroundColor Magenta

$suites = @(
    @{ Name = "Build & Disk Image Verification"; Script = "$projectRoot\tests\test_build.ps1" },
    @{ Name = "Symbols, Assembly & Hardware Registers"; Script = "$projectRoot\tests\test_symbols.ps1" },
    @{ Name = "Video Playback Engine 5-Tier E2E Suite"; Script = "$projectRoot\tests\test_video_e2e.ps1" },
    @{ Name = "Preemptive Multitasking & Scheduler Suite"; Script = "$projectRoot\tests\test_multitasking.ps1" }
)

$suiteResults = @()
$allPassed = $true

foreach ($s in $suites) {
    Write-Host "`n>>> Running Suite: $($s.Name)..." -ForegroundColor Cyan
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & powershell -ExecutionPolicy Bypass -File $s.Script
    $exitCode = $LASTEXITCODE
    $sw.Stop()
    
    $passed = ($exitCode -eq 0)
    if (-not $passed) { $allPassed = $false }
    
    $suiteResults += [PSCustomObject]@{
        Suite = $s.Name
        Status = if ($passed) { "PASS" } else { "FAIL" }
        Duration = "$([Math]::Round($sw.Elapsed.TotalSeconds, 2))s"
        ExitCode = $exitCode
    }
}

Write-Host "`n===============================================================================" -ForegroundColor Magenta
Write-Host " MASTER TEST SUITE CONSOLIDATED REPORT" -ForegroundColor Magenta
Write-Host "===============================================================================" -ForegroundColor Magenta

foreach ($r in $suiteResults) {
    $color = if ($r.Status -eq "PASS") { "Green" } else { "Red" }
    Write-Host "  [$($r.Status)] $($r.Suite.PadRight(45)) (Duration: $($r.Duration), Exit: $($r.ExitCode))" -ForegroundColor $color
}

Write-Host "===============================================================================" -ForegroundColor Magenta
if ($allPassed) {
    Write-Host "  ALL TEST SUITES PASSED CLEANLY (EXIT CODE 0)" -ForegroundColor Green
    Write-Host "===============================================================================" -ForegroundColor Magenta
    exit 0
} else {
    Write-Host "  ONE OR MORE TEST SUITES FAILED" -ForegroundColor Red
    Write-Host "===============================================================================" -ForegroundColor Magenta
    exit 1
}
