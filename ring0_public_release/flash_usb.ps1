# =========================================================================
# A.A OS - High-Speed Raw Sector USB Flashing Utility (Windows PowerShell)
# Writes os.img directly to Physical Sector 0 (MBR) of the selected USB drive.
# Includes strict hardware safety checks to prevent overwriting internal SSDs/HDDs.
# =========================================================================

param(
    [Parameter(Mandatory=$false)]
    [int]$DiskNumber = -1
)

$imagePath = "$PSScriptRoot\os.img"

if (-not (Test-Path $imagePath)) {
    Write-Host "[ERROR] os.img not found at $imagePath! Please run build.ps1 first." -ForegroundColor Red
    exit 1
}

Write-Host "===============================================================================" -ForegroundColor Cyan
Write-Host "   A.A OS - DIRECT SECTOR USB FLASHING UTILITY                                 " -ForegroundColor Green
Write-Host "===============================================================================" -ForegroundColor Cyan

# 1. Detect connected USB storage devices
$usbDisks = Get-Disk | Where-Object { $_.BusType -eq 'USB' }

if ($null -eq $usbDisks -or @($usbDisks).Count -eq 0) {
    Write-Host "[WAITING] No USB flash drive detected! Please plug in your USB drive." -ForegroundColor Yellow
    Write-Host "Run this command again after inserting your USB drive: .\flash_usb.ps1" -ForegroundColor Gray
    exit 1
}

Write-Host "`nDetected USB Flash Drives:" -ForegroundColor Yellow
$usbDisks | Select-Object Number, FriendlyName, @{Name="Size(GB)";Expression={[math]::Round($_.Size/1GB,2)}}, OperationalStatus | Format-Table -AutoSize

if ($DiskNumber -eq -1) {
    if (@($usbDisks).Count -eq 1) {
        $selectedDisk = $usbDisks[0]
        $DiskNumber = $selectedDisk.Number
        Write-Host "Auto-selected single USB drive: Disk $DiskNumber ($($selectedDisk.FriendlyName))" -ForegroundColor Cyan
    } else {
        $inputNumber = Read-Host "Enter the USB Disk Number to flash (e.g. 4)"
        $DiskNumber = [int]$inputNumber
    }
}

# 2. Strict Safety Verification (Must be a USB bus type)
$targetDisk = Get-Disk -Number $DiskNumber -ErrorAction SilentlyContinue
if ($null -eq $targetDisk) {
    Write-Host "[ERROR] Disk $DiskNumber does not exist!" -ForegroundColor Red
    exit 1
}

if ($targetDisk.BusType -ne 'USB') {
    Write-Host "[SAFETY LOCK] Refusing to write to Disk $DiskNumber ($($targetDisk.FriendlyName)) because it is NOT a USB drive (BusType: $($targetDisk.BusType))!" -ForegroundColor Red
    Write-Host "This safety check protects your internal hard drives and SSDs from data loss." -ForegroundColor Yellow
    exit 1
}

Write-Host "`n[WARNING] All data on USB Disk $DiskNumber ($($targetDisk.FriendlyName) - $([math]::Round($targetDisk.Size/1GB,2)) GB) will be replaced with A.A OS!" -ForegroundColor Yellow
$confirm = Read-Host "Type 'YES' to start flashing physical sector 0"
if ($confirm -ne 'YES') {
    Write-Host "[CANCELLED] Flashing aborted by user." -ForegroundColor Gray
    exit 0
}

# 3. Direct Raw Sector Write to \\.\PhysicalDriveN
Write-Host "`n[1/3] Preparing raw disk stream for \\.\PhysicalDrive$DiskNumber..." -ForegroundColor Cyan
$imgBytes = [System.IO.File]::ReadAllBytes($imagePath)

$devicePath = "\\.\PhysicalDrive$DiskNumber"

try {
    # Open PhysicalDrive with FileAccess.ReadWrite and FileShare.ReadWrite
    $fileHandle = [System.IO.File]::Open($devicePath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::ReadWrite)
    
    Write-Host "[2/3] Writing $($imgBytes.Length) bytes (MBR + Protected Mode Kernel) to sector 0..." -ForegroundColor Cyan
    $fileHandle.Seek(0, [System.IO.SeekOrigin]::Begin) | Out-Null
    $fileHandle.Write($imgBytes, 0, $imgBytes.Length)
    $fileHandle.Flush()
    $fileHandle.Close()
    
    Write-Host "[3/3] Syncing hardware caches and verifying partition table..." -ForegroundColor Cyan
    Update-HostStorageCache
    
    Write-Host "`n===============================================================================" -ForegroundColor Green
    Write-Host " [SUCCESS] A.A OS HAS BEEN SUCCESSFULLY FLASHED ONTO USB DISK $DiskNumber!     " -ForegroundColor Green
    Write-Host "===============================================================================" -ForegroundColor Green
    Write-Host "You can now plug this USB into your physical PC/laptop and boot directly!" -ForegroundColor Cyan
    Write-Host "Reminder: Disable 'Secure Boot' and enable 'Legacy/CSM Boot' in PC BIOS." -ForegroundColor Yellow
}
catch {
    Write-Host "[ERROR] Could not open direct raw write stream to $($devicePath) - $_" -ForegroundColor Red
    Write-Host "Tip: Make sure PowerShell is running as Administrator." -ForegroundColor Yellow
}
