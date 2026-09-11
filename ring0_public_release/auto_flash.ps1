# =========================================================================
# A.A OS - 100% Fully Automated USB Auto-Detector & Sector 0 Flasher
# =========================================================================

Clear-Host
Write-Host "===============================================================================" -ForegroundColor Cyan
Write-Host "   A.A OS - DIRECT SECTOR 0 USB FLASHING ENGINE                                " -ForegroundColor Green
Write-Host "===============================================================================" -ForegroundColor Cyan
Write-Host ""

# Check Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "[!] Not running as Administrator. Requesting elevation..." -ForegroundColor Yellow
    Start-Process powershell -Verb RunAs -ArgumentList "-NoExit", "-ExecutionPolicy", "Bypass", "-File", "`"$PSCommandPath`""
    exit
}

# 1. Locate os.img
$imgPath = "c:\Users\ALLAH\OneDrive\Desktop\my_os\os.img"
if (-not (Test-Path $imgPath)) {
    Write-Host "[ERROR] os.img not found at $($imgPath)! Please build it first." -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit
}

$imgSize = (Get-Item $imgPath).Length
Write-Host "[1/4] Loaded os.img ($($imgSize) bytes)..." -ForegroundColor Cyan

# 2. Auto-Detect USB Drive
Write-Host "[2/4] Scanning for connected USB flash drives..." -ForegroundColor Cyan
$usbList = @(Get-Disk | Where-Object { $_.BusType -eq 'USB' })

if ($usbList.Count -eq 0) {
    Write-Host "[ERROR] No USB flash drive detected! Please plug in your USB drive." -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit
}

$target = $usbList[0]
$diskNum = $target.Number
$diskName = $target.FriendlyName
$diskSizeGB = [math]::Round($target.Size / 1GB, 2)

Write-Host "  -> Found USB Drive: Disk $($diskNum) ($($diskName), $($diskSizeGB) GB)" -ForegroundColor Green

# 3. Unlock with Diskpart
Write-Host "`n[3/4] Unlocking and wiping physical sector 0 on Disk $($diskNum)..." -ForegroundColor Cyan
$tmpFile = "$($env:TEMP)\aa_clean_usb.txt"
"select disk $($diskNum)`nclean`nrescan" | Set-Content -Path $tmpFile -Encoding ASCII

$dpOut = diskpart /s $tmpFile
Remove-Item $tmpFile -Force -ErrorAction SilentlyContinue

Start-Sleep -Milliseconds 1500

# 4. Write os.img using dd.exe or FileStream
Write-Host "[4/4] Writing os.img (MBR + 32-bit Kernel) directly to \\.\PhysicalDrive$($diskNum)..." -ForegroundColor Cyan
$ddExe = "D:\msys64\usr\bin\dd.exe"

if (Test-Path $ddExe) {
    & $ddExe "if=$($imgPath)" "of=\\.\PhysicalDrive$($diskNum)" bs=512 status=progress
} else {
    $bytes = [System.IO.File]::ReadAllBytes($imgPath)
    $targetDev = "\\.\PhysicalDrive$($diskNum)"
    $fs = New-Object System.IO.FileStream($targetDev, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::ReadWrite)
    $fs.Seek(0, [System.IO.SeekOrigin]::Begin) | Out-Null
    $fs.Write($bytes, 0, $bytes.Length)
    $fs.Flush()
    $fs.Close()
}

Update-HostStorageCache -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "===============================================================================" -ForegroundColor Green
Write-Host " [SUCCESS] A.A OS IS 100% FLASHED ON YOUR USB (Disk $($diskNum) - $($diskName))! " -ForegroundColor Green
Write-Host "===============================================================================" -ForegroundColor Green
Write-Host ""
Write-Host "How to boot on PC / Laptop:" -ForegroundColor Yellow
Write-Host "  1. Leave USB plugged in (or insert into your target PC)."
Write-Host "  2. Restart PC and press Boot Key (F12, F9, F11, or F8)."
Write-Host "  3. In BIOS Setup: Disable 'Secure Boot' and Enable 'Legacy/CSM Boot'."
Write-Host "  4. Select your USB drive and press Enter to boot A.A OS directly!"
Write-Host ""
Read-Host "Flashing complete! Press Enter to close this window"
