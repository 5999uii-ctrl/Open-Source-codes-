; =============================================================
; A.A OS - Universal Master Boot Record (MBR) Bootloader
; Bulletproof Real Bare-Metal Hardware & USB Flash Drive Bootloader
; Segment-Advancing Multi-Sector Kernel Loader (GEMINI.md Invariant 4)
; Standard 512-Byte MBR with Active Partition Table at Offset 0x1BE
; =============================================================

[BITS 16]
[ORG 0x7C00]

KERNEL_OFFSET   equ 0x8000
SECTOR_COUNT    equ 896         ; 896 sectors = 458,752 bytes (448 KB headroom)

start:
    jmp 0x0000:boot_start       ; Far jump normalizes CS to 0x0000

    ; ---------------------------------------------------------
    ; Standard BIOS Parameter Block (BPB) for USB-HDD / FAT BIOS
    ; ---------------------------------------------------------
    OEM_Label           db "A.A-OS  "
    BytesPerSector      dw 512
    SectorsPerCluster   db 1
    ReservedSectors     dw 1
    TotalFATs           db 2
    MaxRootDirEntries   dw 224
    TotalSectorsSmall   dw 2880
    MediaDescriptor     db 0xF8     ; 0xF8 = Hard disk / USB-HDD
    SectorsPerFAT       dw 9
    SectorsPerTrack     dw 18
    NumHeads            dw 2
    HiddenSectors       dd 0
    TotalSectorsLarge   dd 0
    DriveNumber         db 0x80     ; 0x80 = Hard Drive / USB
    Flags               db 0x00
    Signature           db 0x29
    VolumeID            dd 0x12345678
    VolumeLabel         db "A.A OS     "
    FileSystem          db "FAT16   "

boot_start:
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [BOOT_DRIVE], dl        ; Save BIOS boot drive (0x80 for USB-HDD, 0x00 for USB-FDD)

    ; 1. Set 80x25 Color Text Mode
    mov ax, 0x0003
    int 0x10

    ; 2. Real Hardware Memory Detection (BIOS E820 / SMAP)
    call detect_memory

    ; 3. Load Kernel in 32-Sector Chunks with Segment Advancing
    call read_disk

    ; 4. Fast A20 Gate Enable (Safely masking bit 0 to PREVENT hardware reset!)
    in al, 0x92
    test al, 2
    jnz .a20_ok
    or al, 2
    and al, 0xFE                ; Clear bit 0 (Fast Reset) to prevent PC reboot
    out 0x92, al
.a20_ok:

    ; 5. Enter 32-bit Protected Mode
    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp 0x08:init_pm

; -------------------------------------------------------------
; Real Hardware Memory Detection (BIOS E820)
; -------------------------------------------------------------
detect_memory:
    pusha
    mov di, 0x1010              ; Buffer at 0x1010
    xor ebx, ebx                ; Continuation index
    xor bp, bp                  ; Entry count

.e820_loop:
    mov eax, 0xE820
    mov edx, 0x534D4150         ; 'SMAP'
    mov ecx, 24
    mov byte [di + 20], 1
    int 0x15
    jc .e820_end
    cmp eax, 0x534D4150
    jne .e820_end

    mov eax, [di + 8]
    or eax, [di + 12]
    jz .skip_e820

    inc bp
    add di, 24
    cmp bp, 32
    jge .e820_end

.skip_e820:
    test ebx, ebx
    jnz .e820_loop

.e820_end:
    movzx eax, bp
    mov [0x1000], eax

    ; Conventional Memory (INT 12h)
    int 0x12
    movzx eax, ax
    mov [0x1004], eax

    ; Extended Memory (INT 15h, AH=88h)
    mov ah, 0x88
    int 0x15
    jnc .ext_ok
    xor ax, ax
.ext_ok:
    movzx eax, ax
    mov [0x1008], eax
    mov dword [0x100C], 0x4D454D50 ; "PMEM"
    popa
    ret

; -------------------------------------------------------------
; Segment-Advancing Multi-Sector Disk Reader (INT 13h, AH=42h)
; Reads in 32-sector (16 KB) chunks with segment incrementing
; Prevents 16-bit real-mode segment wrap-around (GEMINI.md Invariant 4)
; -------------------------------------------------------------
read_disk:
    pusha
    mov ebp, 1                  ; Starting at LBA 1
    mov cx, 0x0800              ; Start segment (0x0800:0x0000 = physical 0x8000)
    mov bx, SECTOR_COUNT        ; Sectors left to read

.read_loop:
    mov ax, bx
    cmp ax, 32
    jbe .set_chunk
    mov ax, 32

.set_chunk:
    mov [dap_count], ax
    mov [dap_segment], cx
    mov [dap_lba], ebp

    mov ah, 0x42
    mov dl, [BOOT_DRIVE]
    mov si, dap
    int 0x13
    jnc .ok

    ; Retry once with reset
    xor ax, ax
    mov dl, [BOOT_DRIVE]
    int 0x13

    mov ah, 0x42
    mov dl, [BOOT_DRIVE]
    mov si, dap
    int 0x13
    jnc .ok

    ; Disk Error: Print 'D' and halt
    mov ax, 0x0E44
    int 0x10
    cli
    hlt
    jmp $

.ok:
    movzx eax, word [dap_count]
    add ebp, eax                ; Advance LBA
    sub bx, ax                  ; Decrement sectors remaining
    shl ax, 5                   ; ax = sectors * 32 (segment increment: 32 * 512 = 16384 bytes = 0x0400 segment)
    add cx, ax                  ; Advance segment (0x0800 -> 0x0C00 -> 0x1000...)

    test bx, bx
    jnz .read_loop

    popa
    ret

; -------------------------------------------------------------
; 32-bit Protected Mode Segment
; -------------------------------------------------------------
[BITS 32]
init_pm:
    mov ax, 0x10
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x90000            ; Protected Mode Stack (0x90000)
    mov ebp, esp

    call KERNEL_OFFSET          ; Jump to kernel_entry (0x8000)
    cli
    hlt
    jmp $

; -------------------------------------------------------------
; GDT & Disk Address Packet Tables
; -------------------------------------------------------------
[BITS 16]
align 8
gdt_start:
    dd 0, 0
    dw 0xFFFF, 0x0000, 0x9A00, 0x00CF   ; 32-bit Code (Base=0, Limit=4GB)
    dw 0xFFFF, 0x0000, 0x9200, 0x00CF   ; 32-bit Data (Base=0, Limit=4GB)
gdt_end:

gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start

align 4
dap:
    db 0x10, 0                  ; Size (16 bytes), Reserved
dap_count:
    dw 32                       ; Sectors per chunk
dap_offset:
    dw 0x0000                   ; Offset (always 0)
dap_segment:
    dw 0x0800                   ; Segment
dap_lba:
    dd 1                        ; LBA low
    dd 0                        ; LBA high

BOOT_DRIVE      db 0x80

; -------------------------------------------------------------
; Standard MBR Partition Table (Strictly at Offset 446 / 0x1BE)
; Required by HP / Dell / Lenovo / Asus / Real PC BIOS to boot USB
; -------------------------------------------------------------
times 446 - ($ - $$) db 0

; Partition 1: Active Bootable FAT32 Partition
db 0x80                         ; 0x80 = Active / Bootable Flag
db 0x00, 0x02, 0x00             ; Starting CHS (Head 0, Sector 2, Cylinder 0)
db 0x0C                         ; Type: FAT32 LBA
db 0x01, 0x12, 0x4F             ; Ending CHS (Head 1, Sector 18, Cylinder 79)
dd 1                            ; Starting LBA = 1
dd 2879                         ; Total Sectors

; Partitions 2, 3, 4: Empty (48 bytes)
times 48 db 0

; Boot Signature
dw 0xAA55
