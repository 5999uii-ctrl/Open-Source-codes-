# =========================================================================
# A.A OS - QEMU Launcher Script
# Boots the compiled os.img in the QEMU x86 Hardware Emulator
# NOTE: Image is on D: because C: drive is full
# =========================================================================

$QEMU_EXE = "D:\msys64\ucrt64\bin\qemu-system-i386.exe"
if (-not (Test-Path $QEMU_EXE)) {
    $QEMU_EXE = "C:\msys64\ucrt64\bin\qemu-system-i386.exe"
}
if (-not (Test-Path $QEMU_EXE)) {
    $cmd = Get-Command qemu-system-i386.exe -ErrorAction SilentlyContinue
    if ($cmd) { $QEMU_EXE = $cmd.Source }
}

if (-not (Test-Path $QEMU_EXE)) {
    Write-Host "[ERROR] QEMU not found at $QEMU_EXE" -ForegroundColor Red
    exit 1
}

$localImg = Join-Path $PSScriptRoot "os.img"
$dImg = "D:\agy_os.img"

if (Test-Path $localImg) {
    $IMG = $localImg
} elseif (Test-Path $dImg) {
    $IMG = $dImg
} else {
    Write-Host "[INFO] os.img not found. Building A.A OS first..." -ForegroundColor Yellow
    & "$PSScriptRoot\build.ps1"
    $IMG = $localImg
}

Write-Host "`n=======================================================" -ForegroundColor Cyan
Write-Host " Starting A.A OS in QEMU x86 Emulator" -ForegroundColor Green
Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host "Tips:" -ForegroundColor Yellow
Write-Host "  * Click inside QEMU window to type commands." -ForegroundColor White
Write-Host "  * Press Ctrl+Alt+G to release mouse/focus back to Windows." -ForegroundColor White
Write-Host "  * Type 'help', 'cpu', 'time', 'pci', 'memory', 'about' in the OS.`n" -ForegroundColor White

# Boot as hard drive (hd0) with standard VGA display and serial
& $QEMU_EXE -drive "file=$IMG,format=raw,index=0,if=ide" -m 128M -serial stdio -vga std
