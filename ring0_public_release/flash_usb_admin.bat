@echo off
:: ========================================================================
:: A.A OS - 1-Click Administrator USB Flashing Tool
:: ========================================================================
title A.A OS - USB Flash Tool

:: Check for Administrator permissions
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [REQUESTING ADMIN PRIVILEGES]
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

cd /d "%~dp0"
cls
echo ===============================================================================
echo    A.A OS - 100%% BARE-METAL SECTOR 0 USB FLASHING ENGINE
echo ===============================================================================
echo.

echo Available USB Flash Drives:
powershell -Command "Get-Disk | Where-Object { $_.BusType -eq 'USB' } | Select-Object Number, FriendlyName, @{Name='Size(GB)';Expression={[math]::Round($_.Size/1GB,2)}}, OperationalStatus | Format-Table -AutoSize"

echo.
set /p DISK_NUM="Enter the USB Disk Number (e.g. 4): "

if "%DISK_NUM%"=="" (
    echo [ERROR] No disk number entered!
    pause
    exit /b 1
)

echo.
echo [WARNING] All existing files on Disk %DISK_NUM% will be wiped!
set /p CONFIRM="Type YES to write A.A OS to physical sector 0: "
if /i not "%CONFIRM%"=="YES" (
    echo [CANCELLED] Aborted by user.
    pause
    exit /b 0
)

echo.
echo [1/3] Preparing and unlocking USB Disk %DISK_NUM%...
(
echo select disk %DISK_NUM%
echo clean
echo rescan
) > "%TEMP%\dp_clean.txt"

diskpart /s "%TEMP%\dp_clean.txt" >nul
del "%TEMP%\dp_clean.txt"

timeout /t 2 /nobreak >nul

echo [2/3] Writing os.img (MBR + Protected Mode Kernel) to sector 0...
D:\msys64\usr\bin\dd.exe if=os.img of=\\.\PhysicalDrive%DISK_NUM% bs=512 status=progress

echo.
echo [3/3] Syncing storage caches...
powershell -Command "Update-HostStorageCache" >nul 2>&1

echo.
echo ===============================================================================
echo  [SUCCESS] A.A OS IS NOW 100%% FLASHED AND READY TO BOOT ON REAL HARDWARE!
echo ===============================================================================
echo.
echo Next Steps for Real PC / Laptop:
echo  1. Leave USB plugged in or plug into your target PC/Laptop.
echo  2. Restart PC and press Boot Key (F12, F9, F11, or F8).
echo  3. Ensure 'Secure Boot' is Disabled in BIOS.
echo  4. Select your USB drive and press Enter!
echo.
pause
