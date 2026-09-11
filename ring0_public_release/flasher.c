#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <shlobj.h>
#include <shellapi.h>
#include <wchar.h>

#define MAX_DRIVES 16

typedef struct {
    int number;
    ULONGLONG size;
    wchar_t friendly_name[256];
} UsbDriveInfo;

// Function to check if a drive is a USB drive and get its info
BOOL GetUsbDriveInfo(int drive_num, UsbDriveInfo *info) {
    wchar_t dev_path[64];
    swprintf_s(dev_path, 64, L"\\\\.\\PhysicalDrive%d", drive_num);

    HANDLE hDevice = CreateFileW(
        dev_path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        // Try opening with 0 access rights (just for query)
        hDevice = CreateFileW(
            dev_path,
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
    }

    if (hDevice == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    STORAGE_PROPERTY_QUERY query;
    memset(&query, 0, sizeof(query));
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    BYTE buffer[1024];
    DWORD bytes_returned = 0;

    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &query,
        sizeof(query),
        buffer,
        sizeof(buffer),
        &bytes_returned,
        NULL
    );

    if (!result) {
        CloseHandle(hDevice);
        return FALSE;
    }

    PSTORAGE_DEVICE_DESCRIPTOR desc = (PSTORAGE_DEVICE_DESCRIPTOR)buffer;
    if (desc->BusType != BusTypeUsb) {
        CloseHandle(hDevice);
        return FALSE;
    }

    info->number = drive_num;
    info->size = 0;
    info->friendly_name[0] = L'\0';

    DISK_GEOMETRY_EX geom;
    result = DeviceIoControl(
        hDevice,
        IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
        NULL,
        0,
        &geom,
        sizeof(geom),
        &bytes_returned,
        NULL
    );

    if (result) {
        info->size = geom.DiskSize.QuadPart;
    }

    wchar_t vendor[128] = {0};
    wchar_t product[128] = {0};

    if (desc->VendorIdOffset != 0 && desc->VendorIdOffset < bytes_returned) {
        char *v = (char *)(buffer + desc->VendorIdOffset);
        while (*v == ' ') v++;
        MultiByteToWideChar(CP_ACP, 0, v, -1, vendor, 127);
    }
    if (desc->ProductIdOffset != 0 && desc->ProductIdOffset < bytes_returned) {
        char *p = (char *)(buffer + desc->ProductIdOffset);
        while (*p == ' ') p++;
        MultiByteToWideChar(CP_ACP, 0, p, -1, product, 127);
    }

    int len_v = lstrlenW(vendor);
    while (len_v > 0 && vendor[len_v - 1] == L' ') {
        vendor[len_v - 1] = L'\0';
        len_v--;
    }
    int len_p = lstrlenW(product);
    while (len_p > 0 && product[len_p - 1] == L' ') {
        product[len_p - 1] = L'\0';
        len_p--;
    }

    if (len_v > 0 && len_p > 0) {
        swprintf_s(info->friendly_name, 256, L"%s %s", vendor, product);
    } else if (len_p > 0) {
        swprintf_s(info->friendly_name, 256, L"%s", product);
    } else {
        swprintf_s(info->friendly_name, 256, L"Generic USB Disk");
    }

    CloseHandle(hDevice);
    return TRUE;
}

// Function to run diskpart clean to unmount and clear drive
BOOL RunDiskpartClean(int disk_num) {
    wchar_t temp_path[MAX_PATH];
    DWORD path_len = GetTempPathW(MAX_PATH, temp_path);
    if (path_len == 0) {
        return FALSE;
    }

    wchar_t script_path[MAX_PATH];
    swprintf_s(script_path, MAX_PATH, L"%s\\aa_clean_usb.txt", temp_path);

    FILE *f = NULL;
    if (_wfopen_s(&f, script_path, L"w, ccs=UTF-8") != 0 || f == NULL) {
        return FALSE;
    }

    fwprintf(f, L"select disk %d\nclean\nrescan\n", disk_num);
    fclose(f);

    wchar_t cmd_line[MAX_PATH * 2];
    swprintf_s(cmd_line, MAX_PATH * 2, L"diskpart.exe /s \"%s\"", script_path);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    BOOL success = CreateProcessW(
        NULL,
        cmd_line,
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (success) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    _wremove(script_path);
    return success;
}

// Function to flash os.img directly to physical sector 0 of selected USB disk
BOOL FlashImage(int disk_num, const wchar_t *img_path) {
    HANDLE hImg = CreateFileW(
        img_path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hImg == INVALID_HANDLE_VALUE) {
        wprintf(L"[ERROR] Could not open image file: %s\n", img_path);
        return FALSE;
    }

    LARGE_INTEGER img_size;
    if (!GetFileSizeEx(hImg, &img_size)) {
        wprintf(L"[ERROR] Could not get image size.\n");
        CloseHandle(hImg);
        return FALSE;
    }

    BYTE *buffer = (BYTE *)malloc(img_size.QuadPart);
    if (!buffer) {
        wprintf(L"[ERROR] Memory allocation failed for image buffer.\n");
        CloseHandle(hImg);
        return FALSE;
    }

    DWORD bytes_read = 0;
    if (!ReadFile(hImg, buffer, img_size.LowPart, &bytes_read, NULL) || bytes_read != img_size.LowPart) {
        wprintf(L"[ERROR] Failed to read image file.\n");
        free(buffer);
        CloseHandle(hImg);
        return FALSE;
    }
    CloseHandle(hImg);

    wchar_t dev_path[64];
    swprintf_s(dev_path, 64, L"\\\\.\\PhysicalDrive%d", disk_num);

    wprintf(L"  -> Running diskpart clean on Disk %d...\n", disk_num);
    if (!RunDiskpartClean(disk_num)) {
        wprintf(L"[WARNING] Diskpart clean command failed, proceeding with raw write...\n");
    }
    Sleep(1500);

    wprintf(L"  -> Opening direct write stream to %s...\n", dev_path);
    HANDLE hDisk = CreateFileW(
        dev_path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
        NULL
    );

    if (hDisk == INVALID_HANDLE_VALUE) {
        wprintf(L"[ERROR] Access denied. Make sure no programs are locking the drive and run as Administrator.\n");
        free(buffer);
        return FALSE;
    }

    DWORD bytes_returned = 0;
    DeviceIoControl(hDisk, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytes_returned, NULL);
    DeviceIoControl(hDisk, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &bytes_returned, NULL);

    wprintf(L"  -> Writing A.A OS (%d bytes) to physical sector 0...\n", (int)img_size.QuadPart);
    
    DWORD bytes_written = 0;
    BOOL write_result = WriteFile(
        hDisk,
        buffer,
        img_size.LowPart,
        &bytes_written,
        NULL
    );

    if (!write_result || bytes_written != img_size.LowPart) {
        wprintf(L"[ERROR] Failed to write sectors. Error code: %d\n", (int)GetLastError());
        DeviceIoControl(hDisk, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &bytes_returned, NULL);
        CloseHandle(hDisk);
        free(buffer);
        return FALSE;
    }

    wprintf(L"  -> Flushing hardware cache...\n");
    FlushFileBuffers(hDisk);

    DeviceIoControl(hDisk, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &bytes_returned, NULL);
    DeviceIoControl(hDisk, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &bytes_returned, NULL);
    
    CloseHandle(hDisk);
    free(buffer);
    return TRUE;
}

int main(int argc, char *argv[]) {
    SetConsoleTitleW(L"A.A OS - Direct USB Flasher");
    system("color 0F");

    if (!IsUserAnAdmin()) {
        wprintf(L"[*] Requesting Administrator elevation...\n");
        wchar_t szPath[MAX_PATH];
        if (GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath))) {
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.lpVerb = L"runas";
            sei.lpFile = szPath;
            sei.hwnd = NULL;
            sei.nShow = SW_NORMAL;
            if (ShellExecuteExW(&sei)) {
                return 0;
            }
        }
        wprintf(L"[ERROR] This program must be run as Administrator.\n");
        system("pause");
        return 1;
    }

    wprintf(L"===============================================================================\n");
    wprintf(L"                  A.A OS - DIRECT SECTOR USB FLASHING ENGINE                   \n");
    wprintf(L"===============================================================================\n\n");

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t *lastBackslash = wcsrchr(exePath, L'\\');
    if (lastBackslash) {
        *lastBackslash = L'\0';
    }

    wchar_t imgPath[MAX_PATH];
    swprintf_s(imgPath, MAX_PATH, L"%s\\os.img", exePath);

    if (GetFileAttributesW(imgPath) == INVALID_FILE_ATTRIBUTES) {
        swprintf_s(imgPath, MAX_PATH, L"c:\\Users\\ALLAH\\OneDrive\\Desktop\\my_os\\os.img");
        if (GetFileAttributesW(imgPath) == INVALID_FILE_ATTRIBUTES) {
            wprintf(L"[ERROR] os.img not found! Please build the OS image using build.ps1 first.\n");
            system("pause");
            return 1;
        }
    }

    wprintf(L"[1/4] Located OS image: %s\n", imgPath);

    UsbDriveInfo usbList[MAX_DRIVES];
    int usbCount = 0;

    while (1) {
        wprintf(L"[2/4] Scanning for connected USB flash drives...\n");
        usbCount = 0;
        for (int i = 0; i < MAX_DRIVES; i++) {
            if (GetUsbDriveInfo(i, &usbList[usbCount])) {
                usbCount++;
            }
        }

        if (usbCount > 0) {
            break;
        }

        wprintf(L"[!] No USB flash drive detected! Please insert your USB drive.\n");
        wprintf(L"Press [Enter] to scan again, or press Ctrl+C to exit...\n");
        
        wchar_t ch = getwchar();
        while (ch != L'\n' && ch != WEOF) {
            ch = getwchar();
        }
    }

    wprintf(L"\nDetected USB Flash Drives:\n");
    wprintf(L"--------------------------------------------------\n");
    for (int i = 0; i < usbCount; i++) {
        double gb = (double)usbList[i].size / (1024.0 * 1024.0 * 1024.0);
        wprintf(L"  Disk %d: %s (%.2f GB)\n", usbList[i].number, usbList[i].friendly_name, gb);
    }
    wprintf(L"--------------------------------------------------\n\n");

    int selected_disk = -1;
    if (usbCount == 1) {
        selected_disk = usbList[0].number;
        wprintf(L"Auto-selected Disk %d: %s\n", selected_disk, usbList[0].friendly_name);
    } else {
        while (1) {
            wprintf(L"Enter the USB Disk Number to flash (e.g. %d): ", usbList[0].number);
            wchar_t input[64];
            if (fgetws(input, 64, stdin)) {
                int num = -1;
                if (swscanf_s(input, L"%d", &num) == 1) {
                    BOOL isValid = FALSE;
                    for (int i = 0; i < usbCount; i++) {
                        if (usbList[i].number == num) {
                            isValid = TRUE;
                            break;
                        }
                    }
                    if (isValid) {
                        selected_disk = num;
                        break;
                    }
                }
            }
            wprintf(L"[ERROR] Invalid USB Disk Number selected. Please select from the list.\n");
        }
    }

    wprintf(L"\n[WARNING] All data on USB Disk %d will be PERMANENTLY WIPED!\n", selected_disk);
    wprintf(L"Type 'YES' to start flashing physical sector 0: ");
    wchar_t confirm[64] = {0};
    if (fgetws(confirm, 64, stdin)) {
        wchar_t *nl = wcschr(confirm, L'\n');
        if (nl) *nl = L'\0';
        nl = wcschr(confirm, L'\r');
        if (nl) *nl = L'\0';

        if (wcscmp(confirm, L"YES") != 0) {
            wprintf(L"[CANCELLED] Flashing aborted by user.\n");
            system("pause");
            return 0;
        }
    }

    wprintf(L"\n[3/4] Flashing A.A OS to Disk %d...\n", selected_disk);
    if (!FlashImage(selected_disk, imgPath)) {
        wprintf(L"\n[ERROR] Flashing failed!\n");
        system("pause");
        return 1;
    }

    wprintf(L"\n===============================================================================\n");
    wprintf(L"   [SUCCESS] A.A OS HAS BEEN SUCCESSFULLY FLASHED ONTO YOUR USB DRIVE!         \n");
    wprintf(L"===============================================================================\n");
    wprintf(L"   successfully boot                                                           \n");
    wprintf(L"===============================================================================\n\n");
    wprintf(L"How to boot A.A OS on your physical PC / Laptop:\n");
    wprintf(L"  1. Leave the USB flash drive plugged in.\n");
    wprintf(L"  2. Restart your computer and press the Boot Key (e.g. F12, F9, F11, F8).\n");
    wprintf(L"  3. Go to BIOS settings -> Disable 'Secure Boot' & Enable 'Legacy' or 'CSM'.\n");
    wprintf(L"  4. Select your USB drive from the boot menu and press Enter.\n\n");

    system("pause");
    return 0;
}
