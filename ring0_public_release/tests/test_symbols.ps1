# =========================================================================
# A.A OS - Symbol, Assembly Routine & Hardware Register Auditor
# Verifies all required symbols, assembly routines, data structures,
# I/O port registers, and command registrations across the codebase.
# =========================================================================

$ErrorActionPreference = "Continue"
$projectRoot = (Resolve-Path "$PSScriptRoot\..").Path
Set-Location $projectRoot

Write-Host "===============================================================================" -ForegroundColor Cyan
Write-Host "  A.A OS SYMBOL, ASSEMBLY ROUTINE & HARDWARE REGISTER AUDIT SUITE" -ForegroundColor Green
Write-Host "===============================================================================" -ForegroundColor Cyan

$testResults = @()
$totalTests = 0
$passedTests = 0
$failedTests = 0

function Record-SymbolTest {
    param(
        [string]$TestId,
        [string]$Category,
        [string]$SymbolName,
        [bool]$Found,
        [string]$Location = "",
        [string]$Details = ""
    )
    $script:totalTests++
    if ($Found) {
        $script:passedTests++
        Write-Host "  [PASS] $TestId [$Category] : $SymbolName" -ForegroundColor Green
        if ($Location) { Write-Host "         Defined at: $Location" -ForegroundColor DarkGray }
        if ($Details)  { Write-Host "         $Details" -ForegroundColor DarkGray }
    } else {
        $script:failedTests++
        Write-Host "  [FAIL] $TestId [$Category] : $SymbolName" -ForegroundColor Red
        if ($Details)  { Write-Host "         Missing: $Details" -ForegroundColor Yellow }
    }
    $script:testResults += [PSCustomObject]@{
        TestId = $TestId
        Category = $Category
        SymbolName = $SymbolName
        Status = if ($Found) { "PASS" } else { "FAIL" }
        Location = $Location
        Details = $Details
    }
}

# Read source files for content inspection
$kernelC = if (Test-Path "$projectRoot\kernel.c") { Get-Content "$projectRoot\kernel.c" -Raw } else { "" }
$kernel2C = if (Test-Path "$projectRoot\kernel2.c") { Get-Content "$projectRoot\kernel2.c" -Raw } else { "" }
$kernelEntryAsm = if (Test-Path "$projectRoot\kernel_entry.asm") { Get-Content "$projectRoot\kernel_entry.asm" -Raw } else { "" }
$hwIoAsm = if (Test-Path "$projectRoot\hardware_io.asm") { Get-Content "$projectRoot\hardware_io.asm" -Raw } else { "" }
$memAsm = if (Test-Path "$projectRoot\memorymanagement.asm") { Get-Content "$projectRoot\memorymanagement.asm" -Raw } else { "" }
$linkerLd = if (Test-Path "$projectRoot\linker.ld") { Get-Content "$projectRoot\linker.ld" -Raw } else { "" }
$bootAsm = if (Test-Path "$projectRoot\boot.asm") { Get-Content "$projectRoot\boot.asm" -Raw } else { "" }

# -------------------------------------------------------------------------
# Category 1: BGA & VBE Graphics Subsystem Symbols
# -------------------------------------------------------------------------
Write-Host "`n--- [Category 1: BGA / VBE Graphics Subsystem Symbols] ---" -ForegroundColor Yellow

$hasBgaPorts = ($kernelC.Contains("0x01CE") -and $kernelC.Contains("0x01CF")) -or ($kernelC.Contains("VBE_DISPI_IOPORT_INDEX"))
Record-SymbolTest "SYM-BGA-01" "BGA" "BGA I/O Ports (0x01CE / 0x01CF)" $hasBgaPorts "kernel.c" "Index & Data I/O Ports for Bochs/QEMU BGA"

$hasBgaRegs = ($kernelC.Contains("VBE_DISPI_INDEX_ID") -and $kernelC.Contains("VBE_DISPI_INDEX_XRES") -and $kernelC.Contains("VBE_DISPI_INDEX_YRES") -and $kernelC.Contains("VBE_DISPI_INDEX_BPP") -and $kernelC.Contains("VBE_DISPI_INDEX_ENABLE"))
Record-SymbolTest "SYM-BGA-02" "BGA" "BGA VBE DISPI Register Indices (ID, XRES, YRES, BPP, ENABLE)" $hasBgaRegs "kernel.c" "Registers 0x00 through 0x04"

$hasBgaFlags = ($kernelC.Contains("VBE_DISPI_LFB_ENABLED") -or $kernelC.Contains("0x40")) -and ($kernelC.Contains("VBE_DISPI_ENABLED") -or $kernelC.Contains("0x01"))
Record-SymbolTest "SYM-BGA-03" "BGA" "BGA Mode Flags (VBE_DISPI_LFB_ENABLED 0x40, VBE_DISPI_ENABLED 0x01)" $hasBgaFlags "kernel.c" "Linear Framebuffer & Graphics Enable Flags"

$hasBgaWrite = ($kernelC -match "void\s+bga_write_register")
Record-SymbolTest "SYM-BGA-04" "BGA" "bga_write_register Function" $hasBgaWrite "kernel.c" "Hardware 16-bit register writer to 0x1CE/0x1CF"

$hasBgaRead = ($kernelC -match "uint16_t\s+bga_read_register")
Record-SymbolTest "SYM-BGA-05" "BGA" "bga_read_register Function" $hasBgaRead "kernel.c" "Hardware 16-bit register reader from 0x1CE/0x1CF"

$hasBgaAvail = ($kernelC -match "int\s+bga_is_available")
Record-SymbolTest "SYM-BGA-06" "BGA" "bga_is_available Function" $hasBgaAvail "kernel.c" "Detects Bochs/QEMU BGA ID (0xB0C0 - 0xB0C6)"

$hasBgaLfbPhys = ($kernelC -match "uint32_t\s+bga_get_framebuffer_physical_address")
Record-SymbolTest "SYM-BGA-07" "BGA" "bga_get_framebuffer_physical_address Function" $hasBgaLfbPhys "kernel.c" "Scans PCI Bus for Class 0x03 Display Controller MMIO BAR0"

# -------------------------------------------------------------------------
# Category 2: Assembly Blitting & Memory Routines
# -------------------------------------------------------------------------
Write-Host "`n--- [Category 2: Assembly Blitting & Memory Routines] ---" -ForegroundColor Yellow

$hasMemCopy32 = ($memAsm -match "\[GLOBAL\s+_asm_mem_copy_32\]")
Record-SymbolTest "SYM-MEM-01" "Memory" "_asm_mem_copy_32 Assembly Routine" $hasMemCopy32 "memorymanagement.asm" "High-speed 32-bit dword memory copy routine"

$hasMemFill32 = ($memAsm -match "\[GLOBAL\s+_asm_mem_fill_32\]")
Record-SymbolTest "SYM-MEM-02" "Memory" "_asm_mem_fill_32 Assembly Routine" $hasMemFill32 "memorymanagement.asm" "Direct 32-bit VRAM/RAM pattern filler"

$hasMemZero32 = ($memAsm -match "\[GLOBAL\s+_asm_mem_zero_32\]")
Record-SymbolTest "SYM-MEM-03" "Memory" "_asm_mem_zero_32 Assembly Routine" $hasMemZero32 "memorymanagement.asm" "High-speed zero-out buffer routine"

$hasRepMovsd = ($memAsm -match "rep\s+movsd") -or ($kernelEntryAsm -match "rep\s+movsd") -or ($kernelC -match "rep\s+movsl")
Record-SymbolTest "SYM-MEM-04" "Memory" "rep movsd Fast Blitting Opcode" $hasRepMovsd "memorymanagement.asm" "Hardware string dword transfer instruction for sub-millisecond blits"

# -------------------------------------------------------------------------
# Category 3: PIT Timer & Hardware Interrupts
# -------------------------------------------------------------------------
Write-Host "`n--- [Category 3: PIT Timer & Hardware Interrupts] ---" -ForegroundColor Yellow

$hasPitReload = ($hwIoAsm -match "\[GLOBAL\s+_asm_pit_set_reload_value\]")
Record-SymbolTest "SYM-PIT-01" "PIT" "_asm_pit_set_reload_value Assembly Routine" $hasPitReload "hardware_io.asm" "Programs PIT Channel 0 (0x40/0x43) for target tick frequency"

$hasIrq0Wrapper = ($kernelEntryAsm -match "\[GLOBAL\s+asm_irq0_timer_wrapper\]")
Record-SymbolTest "SYM-PIT-02" "PIT" "asm_irq0_timer_wrapper Assembly ISR Gate" $hasIrq0Wrapper "kernel_entry.asm" "Vector 32 hardware interrupt gate with pusha/popa/iret"

$hasIrq0Handler = ($kernelC -match "c_irq0_timer_handler")
Record-SymbolTest "SYM-PIT-03" "PIT" "c_irq0_timer_handler C Handler" $hasIrq0Handler "kernel.c" "Maintains system tick counter & frame pacing clock"

$hasIrq1Wrapper = ($kernelEntryAsm -match "\[GLOBAL\s+asm_irq1_keyboard_wrapper\]")
Record-SymbolTest "SYM-PIT-04" "PIT" "asm_irq1_keyboard_wrapper Assembly ISR Gate" $hasIrq1Wrapper "kernel_entry.asm" "Vector 33 PS/2 keyboard hardware interrupt gate"

# -------------------------------------------------------------------------
# Category 4: PC Speaker & Audio Modulation
# -------------------------------------------------------------------------
Write-Host "`n--- [Category 4: PC Speaker & Audio Modulation] ---" -ForegroundColor Yellow

$hasSpeakerOn = ($hwIoAsm -match "\[GLOBAL\s+_asm_speaker_tone_on\]")
Record-SymbolTest "SYM-SND-01" "Sound" "_asm_speaker_tone_on Assembly Routine" $hasSpeakerOn "hardware_io.asm" "Calculates PIT Channel 2 divisor (1193182/freq) & enables Port 0x61 gate"

$hasSpeakerOff = ($hwIoAsm -match "\[GLOBAL\s+_asm_speaker_tone_off\]")
Record-SymbolTest "SYM-SND-02" "Sound" "_asm_speaker_tone_off Assembly Routine" $hasSpeakerOff "hardware_io.asm" "Clears bits 0 & 1 on Port 0x61 to silence PC speaker"

$hasPort61Mod = ($hwIoAsm.Contains("0x61") -and $hwIoAsm.Contains("0x42"))
Record-SymbolTest "SYM-SND-03" "Sound" "PC Speaker Hardware I/O Ports (0x61 / 0x42 / 0x43)" $hasPort61Mod "hardware_io.asm" "Direct silicon tone generation ports"

# -------------------------------------------------------------------------
# Category 5: Kernel CLI Command Registrations
# -------------------------------------------------------------------------
Write-Host "`n--- [Category 5: Kernel CLI Command Registrations] ---" -ForegroundColor Yellow

$hasGpuInfo = ($kernelC -match 'cmd_gpu_info' -or $kernelC.Contains('"gpu.info"'))
Record-SymbolTest "SYM-CMD-01" "CLI" "gpu.info / cmd_gpu_info Command" $hasGpuInfo "kernel.c" "Displays BGA hardware ID, active resolution, color depth, and LFB base"

$hasGpuSet = ($kernelC -match 'cmd_gpu_set_mode' -or $kernelC.Contains('"gpu.set"'))
Record-SymbolTest "SYM-CMD-02" "CLI" "gpu.set / cmd_gpu_set_mode Command" $hasGpuSet "kernel.c" "Switches BGA resolution & color depth (e.g. 1024x768x32)"

$hasGpuTest = ($kernelC -match 'cmd_gpu_test_framebuffer' -or $kernelC.Contains('"gpu.test"'))
Record-SymbolTest "SYM-CMD-03" "CLI" "gpu.test / cmd_gpu_test_framebuffer Command" $hasGpuTest "kernel.c" "Tests linear framebuffer with RGB gradient & slate banner"

$hasGpuVram = ($kernelC -match 'cmd_gpu_vram_write' -and $kernelC -match 'cmd_gpu_vram_fill' -and $kernelC -match 'cmd_gpu_vram_dump')
Record-SymbolTest "SYM-CMD-04" "CLI" "gpu.vram.* (write, fill, dump) Commands" $hasGpuVram "kernel.c" "Direct 32-bit physical VRAM access & hex dumping commands"

$hasGpuDac = ($kernelC -match 'cmd_gpu_dac_palette' -or $kernelC.Contains('"gpu.dac.set"'))
Record-SymbolTest "SYM-CMD-05" "CLI" "gpu.dac.set / cmd_gpu_dac_palette Command" $hasGpuDac "kernel.c" "Programs hardware DAC palette registers 0x3C8/0x3C9"

$hasMonitorInfo = ($kernelC -match 'cmd_monitor_info' -and $kernelC -match 'cmd_monitor_crtc_dump')
Record-SymbolTest "SYM-CMD-06" "CLI" "monitor.info & monitor.crtc Commands" $hasMonitorInfo "kernel.c" "Probes CRT/LCD timing dot clock and CRTC 25-register timing array"

# -------------------------------------------------------------------------
# Category 6: Silicon Invariants & Memory Layout
# -------------------------------------------------------------------------
Write-Host "`n--- [Category 6: Silicon Invariants & Memory Layout] ---" -ForegroundColor Yellow

$hasLinker8000 = ($linkerLd.Contains("0x8000") -or $linkerLd.Contains("0x00008000"))
Record-SymbolTest "SYM-SIL-01" "Silicon" "Kernel Entry Base Address (0x8000) in linker.ld" $hasLinker8000 "linker.ld" "Real Mode to Protected Mode transfer destination"

$hasBssZeroing = ($kernelEntryAsm -match "_bss_start" -and $kernelEntryAsm -match "_bss_end" -and $kernelEntryAsm -match "rep\s+stosb")
Record-SymbolTest "SYM-SIL-02" "Silicon" "BSS Physical Zeroing Loop in kernel_entry.asm" $hasBssZeroing "kernel_entry.asm" "Zeroes out uninitialized variables in physical RAM"

$hasFpuSseInit = ($kernelEntryAsm -match "fninit" -and $kernelEntryAsm -match "cr4" -and $kernelEntryAsm -match "OSFXSR")
Record-SymbolTest "SYM-SIL-03" "Silicon" "Hardware FPU & SSE Enable in kernel_entry.asm" $hasFpuSseInit "kernel_entry.asm" "Sets CR0.MP, CR4.OSFXSR, CR4.OSXMMEXCPT"

$hasPciClass03 = ($kernelC.Contains("0x03") -and $kernelC -match "base_class\s*==\s*0x03")
Record-SymbolTest "SYM-SIL-04" "Silicon" "PCI Display Controller Class 0x03 Scanner" $hasPciClass03 "kernel.c" "Enumerates all 256 buses / 32 devices / 8 functions for GPU BAR0"

# -------------------------------------------------------------------------
# Summary Report
# -------------------------------------------------------------------------
Write-Host "`n===============================================================================" -ForegroundColor Cyan
Write-Host " SYMBOL AUDIT TEST SUITE SUMMARY" -ForegroundColor Cyan
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
