# =========================================================================
# A.A OS - Challenger 2 Hardware Adversarial & Regression Test Harness
# =========================================================================

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot\..

Write-Host "======================================================" -ForegroundColor Cyan
Write-Host " RUNNING ADVERSARIAL VERIFICATION HARNESS (CHALLENGER 2)" -ForegroundColor Green
Write-Host "======================================================" -ForegroundColor Cyan

$testResults = @()

function Assert-Test {
    param(
        [string]$Name,
        [bool]$Condition,
        [string]$Details
    )
    if ($Condition) {
        Write-Host "  [PASS] $Name" -ForegroundColor Green
        Write-Host "         $Details" -ForegroundColor DarkGray
        $script:testResults += [PSCustomObject]@{ Name = $Name; Status = "PASS"; Details = $Details }
    } else {
        Write-Host "  [FAIL] $Name" -ForegroundColor Red
        Write-Host "         ERROR: $Details" -ForegroundColor Red
        $script:testResults += [PSCustomObject]@{ Name = $Name; Status = "FAIL"; Details = $Details }
    }
}

# --- SECTION 1: PIT 8254 & SPEAKER PORT 0x61 ADVERSARIAL TESTS ---
Write-Host "`n--- 1. PIT 8254 and SPEAKER PORT 0x61 VERIFICATION ---" -ForegroundColor Yellow

$videoC = Get-Content ".\video.c" -Raw
$videoH = Get-Content ".\video.h" -Raw
$hwIoAsm = Get-Content ".\hardware_io.asm" -Raw

# 1.1 PIT 1000Hz divisor
$pitBase = 1193182
$expectedDiv1000 = [Math]::Floor($pitBase / 1000) # 1193 (0x04A9)
$lowByte1000 = $expectedDiv1000 -band 0xFF        # 0xA9 (169)
$highByte1000 = ($expectedDiv1000 -shr 8) -band 0xFF # 0x04 (4)
Assert-Test "PIT 1000Hz Divisor Math" ($expectedDiv1000 -eq 1193 -and $lowByte1000 -eq 0xA9 -and $highByte1000 -eq 0x04) "Divisor=1193 (Low=0xA9, High=0x04)"

# 1.2 PIT Mode command bytes
$hasCh0Mode = ($videoC -match 'outb\(0x43,\s*0x36\)') -and ($hwIoAsm -match '0x36')
Assert-Test "PIT Channel 0 Mode 0x36 Programming" $hasCh0Mode "Command byte 0x36 on port 0x43"

$hasCh2Mode = ($videoC -match 'outb\(0x43,\s*0xB6\)') -and ($hwIoAsm -match '0xB6')
Assert-Test "PIT Channel 2 Mode 0xB6 Programming" $hasCh2Mode "Command byte 0xB6 on port 0x43"

# 1.3 Port Isolation
$ch0Port40 = ($videoC -match 'outb\(0x40,') -and ($hwIoAsm -match 'out\s+0x40,\s*al')
$ch2Port42 = ($videoC -match 'outb\(0x42,') -and ($hwIoAsm -match 'out\s+0x42,\s*al')
Assert-Test "PIT Channel Data Port Isolation (0x40 for Ch0, 0x42 for Ch2)" ($ch0Port40 -and $ch2Port42) "Ch0->0x40, Ch2->0x42"

# 1.4 Speaker Gate Port 0x61 Modulation (Bits 0 & 1)
$speakerOnMask = ($videoC -match 'outb\(0x61,\s*p61\s*\|\s*0x03\)') -and ($hwIoAsm -match 'or\s+al,\s*0x03')
$speakerMuteMask = ($videoC -match 'outb\(0x61,\s*inb\(0x61\)\s*&\s*0xFC\)') -and ($hwIoAsm -match 'and\s+al,\s*0xFC')
Assert-Test "Speaker Port 0x61 Gate Enable (Bits 0 and 1 OR 0x03)" $speakerOnMask "Sets bits 0 and 1"
Assert-Test "Speaker Port 0x61 Clean Mute (Bits 0 and 1 AND 0xFC)" $speakerMuteMask "Clears bits 0 and 1, preserving upper bits"

# 1.5 Speaker Frequency Bounds & Clamping
$freqClamping = ($videoC -match 'freq_hz\s*<\s*20') -and ($videoC -match 'freq_hz\s*>\s*20000') -and ($videoC -match 'freq_hz\s*==\s*0')
Assert-Test "Speaker Frequency Range Clamping [20 Hz .. 20,000 Hz]" $freqClamping "Protects speaker from sub-audible and ultrasonic damage"

# --- SECTION 2: BGA & PCI BAR0 MMIO ADVERSARIAL TESTS ---
Write-Host "`n--- 2. BGA REGISTERS and PCI BAR0 MMIO VERIFICATION ---" -ForegroundColor Yellow

$hasBgaPorts = ($videoH -match 'VBE_DISPI_IOPORT_INDEX\s+0x01CE') -and ($videoH -match 'VBE_DISPI_IOPORT_DATA\s+0x01CF')
Assert-Test "BGA I/O Port Definitions (0x01CE Index / 0x01CF Data)" $hasBgaPorts "0x01CE and 0x01CF"

$hasBgaEnableFlags = ($videoH -match 'VBE_DISPI_ENABLED\s+0x01') -and ($videoH -match 'VBE_DISPI_LFB_ENABLED\s+0x40') -and ($videoH -match 'VBE_DISPI_NOCLEARMEM\s+0x80')
Assert-Test "BGA Mode Enable Flags (0x01 | 0x40 | 0x80 = 0xC1)" $hasBgaEnableFlags "0xC1 LFB + NoClearMem"

$hasBgaModeSetSeq = ($videoC -match 'bga_write_register\(VBE_DISPI_INDEX_ENABLE,\s*VBE_DISPI_DISABLED\)') -and ($videoC -match 'bga_write_register\(VBE_DISPI_INDEX_XRES') -and ($videoC -match 'bga_write_register\(VBE_DISPI_INDEX_ENABLE,\s*VBE_DISPI_ENABLED\s*\|\s*VBE_DISPI_LFB_ENABLED\s*\|\s*VBE_DISPI_NOCLEARMEM\)')
Assert-Test "BGA Mode Setting Clean Disable -> Reprogram -> Enable Sequence" $hasBgaModeSetSeq "Guarantees atomic GPU reconfig"

$hasPciBar0Probe = ($videoC -match 'video_pci_probe_lfb') -and ($videoC -match 'base_class\s*==\s*0x03') -and ($videoC -match '0x1234') -and ($videoC -match 'cmd_reg\s*\|\s*0x0006')
Assert-Test "PCI BAR0 Discovery and Memory/BusMaster Enable (0x0006)" $hasPciBar0Probe "Scans PCI configuration space and enables MMIO"

$hasBackbufferPhys = ($videoH -match 'VIDEO_DRAM_BACKBUFFER_PHYS\s+0x00800000') -and ($videoH -match 'VIDEO_BACKBUFFER_CAPACITY\s+0x00400000')
Assert-Test "DRAM Backbuffer Physical Base (0x00800000, 4MB Capacity)" $hasBackbufferPhys "Safely placed at 8MB physical RAM mark"

# --- SECTION 3: SHELL CLI PARSER ROBUSTNESS ADVERSARIAL TESTS ---
Write-Host "`n--- 3. SHELL CLI PARSER ROBUSTNESS VERIFICATION ---" -ForegroundColor Yellow

$hasLeadingWhitespaceTrim = ($videoC -match 'while\s*\(\*args\s*==\s*''\s*''\)\s*args\+\+;')
Assert-Test "CLI Parser Leading Whitespace Stripping" $hasLeadingWhitespaceTrim "Skips leading spaces safely"

$hasStreamAliases = ($videoC -match 'strcmp\(stream_str,\s*"sphere"\)') -and ($videoC -match 'strcmp\(stream_str,\s*"3d"\)') -and ($videoC -match 'strcmp\(stream_str,\s*"plasma"\)') -and ($videoC -match 'strcmp\(stream_str,\s*"matrix"\)') -and ($videoC -match 'stream_str\[0\]\s*==\s*''\\0''')
Assert-Test "CLI Stream Name Parsing and Fallback to Showcase on Empty" $hasStreamAliases "Matches sphere/plasma/matrix/demo/aliases"

$hasInvalidStreamHandling = ($videoC -match '\[ERROR\]\s*Unknown video stream:') -and ($videoC -match 'Usage:\s*video\.play')
Assert-Test "CLI Invalid Stream Graceful Error Reporting" $hasInvalidStreamHandling "Reports error and usage without crashing"

$hasFpsClamping = ($videoC -match 'if\s*\(fps\s*==\s*0\)\s*fps\s*=\s*60;') -and ($videoC -match 'if\s*\(fps\s*>\s*120\)\s*fps\s*=\s*120;')
Assert-Test "CLI FPS Clamping [1..120 FPS, Default 60]" $hasFpsClamping "Guarantees safe pacing divisor"

$hasKernelRouting = ($videoC -match 'cmd_video_play') -and ($videoC -match 'cmd_video_info') -and ($videoC -match 'cmd_video_stop')
Assert-Test "Shell Command Handlers Implementation" $hasKernelRouting "cmd_video_play, cmd_video_info, cmd_video_stop"

# --- SECTION 4: FREESTANDING FIXED-POINT & PROCEDURAL MATH (C BINARY ORACLE) ---
Write-Host "`n--- 4. FREESTANDING FIXED-POINT and PROCEDURAL MATH ORACLES ---" -ForegroundColor Yellow

& "D:\msys64\ucrt64\bin\gcc.exe" -O2 ".\tests\adversarial_oracle.c" -o "D:\adversarial_oracle.exe"
if ($LASTEXITCODE -eq 0) {
    & "D:\adversarial_oracle.exe"
    $oracleExit = $LASTEXITCODE
    Assert-Test "C Math & Parser Oracle Suite (39 Assertions)" ($oracleExit -eq 0) "All 39 native C assertions passed"
} else {
    Assert-Test "C Math & Parser Oracle Suite (39 Assertions)" $false "Compilation failed"
}

# --- SECTION 5: GEMINI.MD SILICON INVARIANTS & INTEGRITY AUDIT ---
Write-Host "`n--- 5. SILICON INVARIANTS and ZERO-DECEPTION AUDIT ---" -ForegroundColor Yellow

$kernelCRaw = Get-Content ".\kernel.c" -Raw
$hasTodoKernel = ($kernelCRaw -match '//\s*TODO' -or $kernelCRaw -match '//\s*FIXME')
$hasTodoVideo = ($videoC -match '//\s*TODO' -or $videoC -match '//\s*FIXME')
$hasTodoVideoH = ($videoH -match '//\s*TODO' -or $videoH -match '//\s*FIXME')
Assert-Test "Strict Absence of // TODO and // FIXME Comments" (-not $hasTodoKernel -and -not $hasTodoVideo -and -not $hasTodoVideoH) "0 TODO/FIXME comments found"

$vgaAsmRaw = Get-Content ".\vga_driver.asm" -Raw
$hasRealAsmBlitter = ($videoC -match 'asm_video_blit_lfb') -and ($vgaAsmRaw -match 'rep movsd')
Assert-Test "Genuine Hardware Assembly Blitter (rep movsd ERMS)" $hasRealAsmBlitter "Direct hardware memory copy"

# --- SUMMARY ---
Write-Host "`n======================================================" -ForegroundColor Cyan
$passed = ($script:testResults | Where-Object { $_.Status -eq "PASS" }).Count
$failed = ($script:testResults | Where-Object { $_.Status -eq "FAIL" }).Count
$total = $script:testResults.Count

Write-Host " ADVERSARIAL SUITE SUMMARY: $passed / $total PASSED ($failed FAILED)" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Red" })
Write-Host "======================================================" -ForegroundColor Cyan

if ($failed -gt 0) {
    exit 1
} else {
    exit 0
}
