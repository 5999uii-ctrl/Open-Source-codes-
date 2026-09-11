# =========================================================================
# A.A OS - Build and Package Script
# NOTE: Temp build files go to D:\ because C:\ is full
# =========================================================================

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$NASM    = "C:\Users\ALLAH\AppData\Local\bin\NASM\nasm.exe"
$GCC     = "D:\msys64\ucrt64\bin\gcc.exe"
$LD      = "D:\msys64\ucrt64\bin\ld.exe"
$OBJCOPY = "D:\msys64\ucrt64\bin\objcopy.exe"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " Building A.A OS (x86 Bare-Metal)" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Cyan

# 1. Assemble Bootloader
Write-Host "[1/5] Assembling bootloader (boot.asm)..." -ForegroundColor Yellow
& $NASM -f bin boot.asm -o boot.bin
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] boot.asm assembly failed!" -ForegroundColor Red; exit 1 }

# 2. Assemble Kernel Assembly Modules
Write-Host "[2/5] Assembling kernel assembly modules..." -ForegroundColor Yellow
& $NASM -f win32 kernel_entry.asm -o "D:\agy_entry.o"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] kernel_entry.asm failed!" -ForegroundColor Red; exit 1 }

& $NASM -f win32 memorymanagement.asm -o "D:\agy_mem.o"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] memorymanagement.asm failed!" -ForegroundColor Red; exit 1 }

& $NASM -f win32 hardware_io.asm -o "D:\agy_io.o"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] hardware_io.asm failed!" -ForegroundColor Red; exit 1 }

& $NASM -f win32 hardware_debug.asm -o "D:\agy_dbg.o"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] hardware_debug.asm failed!" -ForegroundColor Red; exit 1 }

& $NASM -f win32 ps2_driver.asm -o "D:\agy_ps2.o"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] ps2_driver.asm failed!" -ForegroundColor Red; exit 1 }

& $NASM -f win32 serial_driver.asm -o "D:\agy_serial.o"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] serial_driver.asm failed!" -ForegroundColor Red; exit 1 }

& $NASM -f win32 vga_driver.asm -o "D:\agy_vga.o"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] vga_driver.asm failed!" -ForegroundColor Red; exit 1 }

& $NASM -f win32 hardware_override.asm -o "D:\agy_override.o"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] hardware_override.asm failed!" -ForegroundColor Red; exit 1 }

& $NASM -f win32 storage_editor.asm -o "D:\agy_editor.o"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] storage_editor.asm failed!" -ForegroundColor Red; exit 1 }

& $NASM -f win32 silicon_engine.asm -o "D:\agy_silicon.o"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] silicon_engine.asm failed!" -ForegroundColor Red; exit 1 }

& $NASM -f win32 task_switch.asm -o "D:\agy_task_switch.o"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] task_switch.asm failed!" -ForegroundColor Red; exit 1 }

# 3. Compile C Kernels (kernel.c, kernel2.c, video.c, task.c) (output to D: to avoid C: full)
Write-Host "[3/5] Compiling C kernels (kernel.c, kernel2.c, video.c, task.c)..." -ForegroundColor Yellow
& $GCC -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -mno-sse -mno-sse2 -mno-mmx -mno-80387 -c kernel.c -o "D:\agy_kernel.o"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] kernel.c compile failed!" -ForegroundColor Red; exit 1 }

& $GCC -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -mno-sse -mno-sse2 -mno-mmx -mno-80387 -c kernel2.c -o "D:\agy_kernel2.o"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] kernel2.c compile failed!" -ForegroundColor Red; exit 1 }

& $GCC -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -mno-sse -mno-sse2 -mno-mmx -mno-80387 -c video.c -o "D:\agy_video.o"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] video.c compile failed!" -ForegroundColor Red; exit 1 }

& $GCC -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -mno-sse -mno-sse2 -mno-mmx -mno-80387 -c task.c -o "D:\agy_task.o"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] task.c compile failed!" -ForegroundColor Red; exit 1 }

# 4. Link on D: with linker.ld, extract binary, copy back
Write-Host "[4/5] Linking kernel objects at 0x8000 via linker.ld..." -ForegroundColor Yellow
& $LD -m i386pe --image-base 0x0 -T "$PSScriptRoot\linker.ld" -o "D:\agy_kernel.tmp" "D:\agy_entry.o" "D:\agy_task_switch.o" "D:\agy_mem.o" "D:\agy_io.o" "D:\agy_dbg.o" "D:\agy_ps2.o" "D:\agy_serial.o" "D:\agy_vga.o" "D:\agy_override.o" "D:\agy_editor.o" "D:\agy_silicon.o" "D:\agy_kernel.o" "D:\agy_kernel2.o" "D:\agy_video.o" "D:\agy_task.o"
& $OBJCOPY -O binary -j .text -j .rdata -j .data -j .bss "D:\agy_kernel.tmp" "D:\agy_kernel.bin"
if (-not (Test-Path "D:\agy_kernel.bin")) {
    Write-Host "[FAIL] Link step failed!" -ForegroundColor Red; exit 1
}
Copy-Item "D:\agy_kernel.bin" ".\kernel.bin" -Force

$bootAsmText = Get-Content "$PSScriptRoot\boot.asm" -Raw
$sectorCountMatch = [regex]::Match($bootAsmText, 'SECTOR_COUNT\s+equ\s+(\d+)')
$bootSectorCount = if ($sectorCountMatch.Success) { [int]$sectorCountMatch.Groups[1].Value } else { 384 }

$kernelSize    = (Get-Item ".\kernel.bin").Length
$sectorsNeeded = [Math]::Ceiling($kernelSize / 512)
Write-Host "  kernel.bin = $kernelSize bytes = $sectorsNeeded sectors (boot SECTOR_COUNT=$bootSectorCount)" -ForegroundColor DarkCyan
if ($sectorsNeeded -gt $bootSectorCount) {
    Write-Host "[WARNING] kernel too large! Increase SECTOR_COUNT in boot.asm to $($sectorsNeeded+16)" -ForegroundColor Red
}

# 5. Package Disk Image
Write-Host "[5/5] Creating combined bootable disk image (os.img)..." -ForegroundColor Yellow
$bootBytes   = [System.IO.File]::ReadAllBytes("$PSScriptRoot\boot.bin")
$kernelBytes = [System.IO.File]::ReadAllBytes("$PSScriptRoot\kernel.bin")
$diskImage   = New-Object byte[] (1474560)
[System.Array]::Copy($bootBytes,   0, $diskImage, 0,   $bootBytes.Length)
[System.Array]::Copy($kernelBytes, 0, $diskImage, 512, $kernelBytes.Length)
[System.IO.File]::WriteAllBytes("$PSScriptRoot\os.img", $diskImage)
try {
    Copy-Item "$PSScriptRoot\os.img" "D:\agy_os.img" -Force -ErrorAction SilentlyContinue
} catch {}

Write-Host ""
Write-Host "[SUCCESS] Bootable disk image created: os.img" -ForegroundColor Green
Write-Host "Image size: $([math]::Round((Get-Item "$PSScriptRoot\os.img").Length / 1MB, 2)) MB" -ForegroundColor Cyan

# 6. Compile USB Flasher Executable (flash_usb.exe)
Write-Host "[6/6] Compiling USB Flasher executable (flash_usb.exe)..." -ForegroundColor Yellow
& $GCC -O2 -Wall "$PSScriptRoot\flasher.c" -o "$PSScriptRoot\flash_usb.exe" -luser32 -lshell32
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] flasher.c compile failed!" -ForegroundColor Red; exit 1
}
Write-Host "[SUCCESS] Flasher executable created: flash_usb.exe" -ForegroundColor Green
Write-Host ""

