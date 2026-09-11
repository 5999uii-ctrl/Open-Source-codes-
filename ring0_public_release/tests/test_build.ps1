# =========================================================================
# A.A OS - Automated Build & Disk Image Verification Test Suite
# Tests clean compilation, binary generation, sector alignment, and MBR format
# =========================================================================

$ErrorActionPreference = "Continue"
$projectRoot = (Resolve-Path "$PSScriptRoot\..").Path
Set-Location $projectRoot

Write-Host "===============================================================================" -ForegroundColor Cyan
Write-Host "  A.A OS BUILD VERIFICATION & DISK IMAGE INTEGRITY TEST SUITE" -ForegroundColor Green
Write-Host "===============================================================================" -ForegroundColor Cyan

$testResults = @()
$totalTests = 0
$passedTests = 0
$failedTests = 0

function Record-TestResult {
    param(
        [string]$TestId,
        [string]$Description,
        [bool]$Passed,
        [string]$Details = ""
    )
    $script:totalTests++
    if ($Passed) {
        $script:passedTests++
        Write-Host "  [PASS] $TestId : $Description" -ForegroundColor Green
        if ($Details) { Write-Host "         $Details" -ForegroundColor DarkGray }
    } else {
        $script:failedTests++
        Write-Host "  [FAIL] $TestId : $Description" -ForegroundColor Red
        if ($Details) { Write-Host "         Error: $Details" -ForegroundColor Yellow }
    }
    $script:testResults += [PSCustomObject]@{
        TestId = $TestId
        Description = $Description
        Status = if ($Passed) { "PASS" } else { "FAIL" }
        Details = $Details
    }
}

# -------------------------------------------------------------------------
# Test 1: Toolchain Discovery & Availability
# -------------------------------------------------------------------------
$nasmExe = "C:\Users\ALLAH\AppData\Local\bin\NASM\nasm.exe"
if (-not (Test-Path $nasmExe)) {
    $cmd = Get-Command nasm.exe -ErrorAction SilentlyContinue
    if ($cmd) { $nasmExe = $cmd.Source }
}
$nasmAvailable = (Test-Path $nasmExe)
Record-TestResult "BUILD-T01" "NASM Assembler Toolchain Existence" $nasmAvailable "NASM Path: $nasmExe"

$gccExe = "D:\msys64\ucrt64\bin\gcc.exe"
if (-not (Test-Path $gccExe)) {
    $gccExe = "C:\msys64\ucrt64\bin\gcc.exe"
}
$gccAvailable = (Test-Path $gccExe)
Record-TestResult "BUILD-T02" "GCC 32-bit Freestanding C Compiler Existence" $gccAvailable "GCC Path: $gccExe"

$ldExe = "D:\msys64\ucrt64\bin\ld.exe"
if (-not (Test-Path $ldExe)) {
    $ldExe = "C:\msys64\ucrt64\bin\ld.exe"
}
$ldAvailable = (Test-Path $ldExe)
Record-TestResult "BUILD-T03" "GNU Linker (ld.exe) Existence" $ldAvailable "LD Path: $ldExe"

$objcopyExe = "D:\msys64\ucrt64\bin\objcopy.exe"
if (-not (Test-Path $objcopyExe)) {
    $objcopyExe = "C:\msys64\ucrt64\bin\objcopy.exe"
}
$objcopyAvailable = (Test-Path $objcopyExe)
Record-TestResult "BUILD-T04" "GNU Objcopy Utility Existence" $objcopyAvailable "Objcopy Path: $objcopyExe"

# -------------------------------------------------------------------------
# Test 2: Execute Clean Build via build.ps1
# -------------------------------------------------------------------------
Write-Host "`nExecuting build pipeline (build.ps1)..." -ForegroundColor Yellow
$buildStartTime = Get-Date
$buildOutput = & powershell -ExecutionPolicy Bypass -File "$projectRoot\build.ps1" 2>&1
$buildExitCode = $LASTEXITCODE
$buildDuration = ((Get-Date) - $buildStartTime).TotalSeconds

Record-TestResult "BUILD-T05" "build.ps1 Clean Execution Exit Code (0)" ($buildExitCode -eq 0) "Exit Code: $buildExitCode (Duration: $($buildDuration.ToString('F2'))s)"

# -------------------------------------------------------------------------
# Test 3: Bootloader Binary Verification (boot.bin)
# -------------------------------------------------------------------------
$bootBinPath = Join-Path $projectRoot "boot.bin"
$bootBinExists = Test-Path $bootBinPath
Record-TestResult "BUILD-T06" "boot.bin Artifact Generation" $bootBinExists "Path: $bootBinPath"

if ($bootBinExists) {
    $bootBytes = [System.IO.File]::ReadAllBytes($bootBinPath)
    $bootSize = $bootBytes.Length
    Record-TestResult "BUILD-T07" "boot.bin Sector Size (Exactly 512 Bytes)" ($bootSize -eq 512) "Size: $bootSize bytes"

    # Verify MBR Magic Signature 0x55, 0xAA at offsets 510 and 511
    $sig0 = $bootBytes[510]
    $sig1 = $bootBytes[511]
    $mbrSigValid = ($sig0 -eq 0x55 -and $sig1 -eq 0xAA)
    $sigHex = "0x" + $sig0.ToString("X2") + " 0x" + $sig1.ToString("X2")
    Record-TestResult "BUILD-T08" "MBR Boot Signature (0x55 0xAA at offset 510-511)" $mbrSigValid "Signature: $sigHex"
} else {
    Record-TestResult "BUILD-T07" "boot.bin Sector Size" $false "File not found"
    Record-TestResult "BUILD-T08" "MBR Boot Signature" $false "File not found"
}

# -------------------------------------------------------------------------
# Test 4: Kernel Binary Verification (kernel.bin)
# -------------------------------------------------------------------------
$kernelBinPath = Join-Path $projectRoot "kernel.bin"
$kernelBinExists = Test-Path $kernelBinPath
Record-TestResult "BUILD-T09" "kernel.bin Artifact Generation" $kernelBinExists "Path: $kernelBinPath"

if ($kernelBinExists) {
    $kernelBytes = [System.IO.File]::ReadAllBytes($kernelBinPath)
    $kernelSize = $kernelBytes.Length
    Record-TestResult "BUILD-T10" "kernel.bin Non-Zero Binary Size" ($kernelSize -gt 0) "Size: $kernelSize bytes ($([Math]::Ceiling($kernelSize / 512)) sectors)"

    # Check against bootloader SECTOR_COUNT
    $bootAsm = Get-Content "$projectRoot\boot.asm" -Raw
    $match = [regex]::Match($bootAsm, 'SECTOR_COUNT\s+equ\s+(\d+)')
    $configuredSectors = if ($match.Success) { [int]$match.Groups[1].Value } else { 768 }
    $sectorsNeeded = [Math]::Ceiling($kernelSize / 512)
    $sectorFit = ($sectorsNeeded -le $configuredSectors)
    Record-TestResult "BUILD-T11" "Kernel Sector Fit (Needed $sectorsNeeded <= Configured $configuredSectors)" $sectorFit "Sectors Needed: $sectorsNeeded, Boot Capacity: $configuredSectors"
} else {
    Record-TestResult "BUILD-T10" "kernel.bin Non-Zero Binary Size" $false "File not found"
    Record-TestResult "BUILD-T11" "Kernel Sector Fit" $false "File not found"
}

# -------------------------------------------------------------------------
# Test 5: Bootable Disk Image Verification (os.img)
# -------------------------------------------------------------------------
$osImgPath = Join-Path $projectRoot "os.img"
$osImgExists = Test-Path $osImgPath
Record-TestResult "BUILD-T12" "os.img Bootable Disk Image Generation" $osImgExists "Path: $osImgPath"

if ($osImgExists) {
    $osImgBytes = [System.IO.File]::ReadAllBytes($osImgPath)
    $osImgSize = $osImgBytes.Length
    # Standard 1.44 MB floppy image is exactly 1,474,560 bytes
    $isStandardSize = ($osImgSize -eq 1474560)
    Record-TestResult "BUILD-T13" "os.img Standard 1.44MB Floppy Geometry (1,474,560 Bytes)" $isStandardSize "Size: $osImgSize bytes ($([math]::Round($osImgSize / 1MB, 2)) MB)"

    # Verify sector 0 in os.img matches boot.bin
    $sector0Match = $true
    if ($bootBinExists) {
        for ($i = 0; $i -lt 512; $i++) {
            if ($osImgBytes[$i] -ne $bootBytes[$i]) {
                $sector0Match = $false
                break
            }
        }
    } else {
        $sector0Match = $false
    }
    Record-TestResult "BUILD-T14" "os.img Sector 0 Exact Match with boot.bin" $sector0Match "Byte-for-byte verification of MBR sector"

    # Verify kernel starts at offset 512
    $kernelMatch = $true
    if ($kernelBinExists) {
        $checkLen = [Math]::Min(1024, $kernelBytes.Length)
        for ($i = 0; $i -lt $checkLen; $i++) {
            if ($osImgBytes[512 + $i] -ne $kernelBytes[$i]) {
                $kernelMatch = $false
                break
            }
        }
    } else {
        $kernelMatch = $false
    }
    Record-TestResult "BUILD-T15" "os.img Sector 1+ Exact Match with kernel.bin" $kernelMatch "Byte-for-byte verification of kernel payload at offset 512"
} else {
    Record-TestResult "BUILD-T13" "os.img Standard Size" $false "File not found"
    Record-TestResult "BUILD-T14" "os.img Sector 0 Match" $false "File not found"
    Record-TestResult "BUILD-T15" "os.img Kernel Payload Match" $false "File not found"
}

# -------------------------------------------------------------------------
# Test 6: Freestanding & Architecture Compilation Invariants
# -------------------------------------------------------------------------
$buildScriptContent = Get-Content "$projectRoot\build.ps1" -Raw
$hasFreestanding = $buildScriptContent.Contains("-ffreestanding")
$hasNoPie = $buildScriptContent.Contains("-fno-pie")
$hasNostdlib = $buildScriptContent.Contains("-nostdlib")
$hasMno80387 = $buildScriptContent.Contains("-mno-80387")
$hasM32 = $buildScriptContent.Contains("-m32")

Record-TestResult "BUILD-T16" "Freestanding C Flags Enforced (-ffreestanding -fno-pie -nostdlib)" ($hasFreestanding -and $hasNoPie -and $hasNostdlib) "Ensures Ring 0 bare-metal execution without OS runtime"
Record-TestResult "BUILD-T17" "x86 32-bit Protected Mode Target (-m32 -mno-80387)" ($hasM32 -and $hasMno80387) "Ensures pure 32-bit Q16.16 integer pipeline compatibility"

# -------------------------------------------------------------------------
# Summary Report
# -------------------------------------------------------------------------
Write-Host "`n===============================================================================" -ForegroundColor Cyan
Write-Host " BUILD TEST SUITE SUMMARY" -ForegroundColor Cyan
Write-Host "===============================================================================" -ForegroundColor Cyan
Write-Host " Total Tests  : $totalTests" -ForegroundColor White
Write-Host " Passed Tests : $passedTests" -ForegroundColor Green
Write-Host " Failed Tests : $failedTests" -ForegroundColor $(if ($failedTests -eq 0) { "Green" } else { "Red" })
$successRate = if ($totalTests -gt 0) { [Math]::Round(($passedTests / $totalTests) * 100, 1) } else { 0 }
Write-Host " Pass Rate    : $successRate%" -ForegroundColor $(if ($failedTests -eq 0) { "Green" } else { "Yellow" })
Write-Host "===============================================================================" -ForegroundColor Cyan

if ($failedTests -gt 0) {
    exit 1
} else {
    exit 0
}
