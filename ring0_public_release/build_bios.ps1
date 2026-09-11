# =========================================================================
# A.A BIOS - Build Script for 64 KB ROM Firmware Image
# =========================================================================
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$NASM = "C:\Users\ALLAH\AppData\Local\bin\NASM\nasm.exe"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " Building A.A BIOS ROM (64 KB Bare-Metal)" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Cyan

Write-Host "[1/2] Assembling bios.asm with NASM..." -ForegroundColor Yellow
& $NASM -f bin bios.asm -o bios.bin
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] Assembly failed!" -ForegroundColor Red; exit 1 }

Write-Host "[2/2] Finalizing 64 KB ROM & Checksum..." -ForegroundColor Yellow
$bytes = [System.IO.File]::ReadAllBytes("$PSScriptRoot\bios.bin")
if ($bytes.Length -lt 65536) {
    $padded = New-Object byte[] (65536)
    [System.Array]::Copy($bytes, 0, $padded, 0, $bytes.Length)
    for ($i = $bytes.Length; $i -lt 65535; $i++) { $padded[$i] = 0x90 }
    $bytes = $padded
}

$sum = 0
for ($i = 0; $i -lt 65535; $i++) { $sum = ($sum + $bytes[$i]) % 256 }
$checksum = (256 - $sum) % 256
$bytes[65535] = [byte]$checksum
[System.IO.File]::WriteAllBytes("$PSScriptRoot\bios.bin", $bytes)

Write-Host "[SUCCESS] bios.bin created: 65536 bytes (Checksum: 0x$($checksum.ToString('X2')))" -ForegroundColor Green
