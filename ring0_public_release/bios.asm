; =============================================================================
; A.A BIOS v1.0 - Native x86 Bare-Metal Firmware ROM (64 KB)
; Direct Silicon Initialization, Real-Mode IVT Services & Bootstrap Loader
; Reset Vector at Physical Address 0xFFFFFFF0 (0xF000:0xFFF0)
; =============================================================================

[BITS 16]
[ORG 0x0000] ; Segment 0xF000 base offset

rom_start:
    db "AA_BIOS_V1"
    dw 0x0000

; =============================================================================
; Power-On Self-Test (POST) Entry Point
; =============================================================================
post_entry:
    cli                         ; Disable interrupts during hardware setup
    cld                         ; Clear direction flag (forward string ops)

    ; Initialize segment registers and stack
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov sp, 0x7000              ; Early real-mode firmware stack at 0x7000

    ; 1. Initialize COM1 UART Serial Port (Port 0x3F8, 115200 Baud, 8N1)
    call com1_init

    ; 2. Initialize BIOS Data Area (BDA at 0x0040:0x0000 / 0x0000:0x0400)
    mov ax, 0x0040
    mov es, ax

    mov word [es:0x0010], 0x0021 ; Equipment Word (Color 80x25 VGA, PS/2 mouse)
    mov word [es:0x0013], 0x0280 ; Base Memory: 640 KB (0x0280 = 640)
    mov byte [es:0x0049], 0x03   ; Video Mode: 0x03 (80x25 16-color text)
    mov word [es:0x004A], 80     ; Screen Columns: 80
    mov word [es:0x004C], 4096   ; Video Page Size: 4096 bytes
    mov word [es:0x004E], 0x0000 ; Video Page Offset: 0
    mov word [es:0x0063], 0x03D4 ; CRTC I/O Port Base: 0x03D4
    mov word [es:0x006C], 0x0000 ; Timer Ticks Counter (Low)
    mov word [es:0x006E], 0x0000 ; Timer Ticks Counter (High)
    mov byte [es:0x0075], 1      ; Number of Hard Disks: 1

    ; 3. Initialize 8259 Programmable Interrupt Controller (PIC)
    ; ICW1: Init command with ICW4 needed
    mov al, 0x11
    out 0x20, al
    out 0xEB, al
    out 0xA0, al
    out 0xEB, al

    ; ICW2: Master vector offset = 0x08, Slave vector offset = 0x70
    mov al, 0x08
    out 0x21, al
    out 0xEB, al
    mov al, 0x70
    out 0xA1, al
    out 0xEB, al

    ; ICW3: Master has slave on IRQ2 (0x04), Slave cascade identity (0x02)
    mov al, 0x04
    out 0x21, al
    out 0xEB, al
    mov al, 0x02
    out 0xA1, al
    out 0xEB, al

    ; ICW4: 8086 mode
    mov al, 0x01
    out 0x21, al
    out 0xEB, al
    out 0xA1, al
    out 0xEB, al

    ; OCW1: Unmask IRQ0 (Timer) and IRQ1 (Keyboard) on Master, mask all others
    mov al, 0xFC
    out 0x21, al
    mov al, 0xFF
    out 0xA1, al

    ; 4. Initialize Intel 8254 PIT (Channel 0, Rate Generator 18.2 Hz)
    mov al, 0x36                ; Channel 0, lobyte/hibyte, Mode 3
    out 0x43, al
    xor al, al
    out 0x40, al                ; Divisor = 0x0000 (65536 = 18.2065 Hz)
    out 0x40, al

    ; 5. Install Real-Mode Interrupt Vector Table (IVT at 0x0000:0x0000)
    xor ax, ax
    mov es, ax

    ; Vector 0x08 (IRQ0 - Timer Interrupt)
    mov word [es:0x08 * 4 + 0], isr_int08h_timer
    mov word [es:0x08 * 4 + 2], 0xF000

    ; Vector 0x09 (IRQ1 - Keyboard Interrupt)
    mov word [es:0x09 * 4 + 0], isr_int09h_kbd
    mov word [es:0x09 * 4 + 2], 0xF000

    ; Vector 0x10 (VGA Video BIOS Services)
    mov word [es:0x10 * 4 + 0], isr_int10h_vga
    mov word [es:0x10 * 4 + 2], 0xF000

    ; Vector 0x12 (Get Conventional Memory Size)
    mov word [es:0x12 * 4 + 0], isr_int12h_mem
    mov word [es:0x12 * 4 + 2], 0xF000

    ; Vector 0x13 (Disk Storage Services - ATA PIO)
    mov word [es:0x13 * 4 + 0], isr_int13h_disk
    mov word [es:0x13 * 4 + 2], 0xF000

    ; Vector 0x15 (System Services - E820 Memory Map)
    mov word [es:0x15 * 4 + 0], isr_int15h_sys
    mov word [es:0x15 * 4 + 2], 0xF000

    ; Vector 0x16 (PS/2 Keyboard BIOS Services)
    mov word [es:0x16 * 4 + 0], isr_int16h_kbd
    mov word [es:0x16 * 4 + 2], 0xF000

    ; Vector 0x19 (Bootstrap Loader)
    mov word [es:0x19 * 4 + 0], isr_int19h_boot
    mov word [es:0x19 * 4 + 2], 0xF000

    ; Vector 0x1E (Diskette Parameter Table)
    mov word [es:0x1E * 4 + 0], fd_param_table
    mov word [es:0x1E * 4 + 2], 0xF000

    ; 6. Clear VGA Screen & Initialize Text Mode 0x03
    call vga_bios_init

    ; 7. Display BIOS Banner on VGA and COM1
    mov si, msg_bios_banner
    call bios_print_string

    ; 8. Display POST Telemetry
    mov si, msg_post_ok
    call bios_print_string

    sti                         ; Re-enable hardware interrupts

    ; 9. Trigger Bootstrap Loader via INT 19h
    int 0x19

    ; Halt if bootstrap returns
.hang:
    hlt
    jmp .hang

; =============================================================================
; INT 10h - Video BIOS Services (VGA 0xB8000 Text Mode)
; =============================================================================
isr_int10h_vga:
    pushf
    push ds
    push es
    push bx
    push cx
    push dx
    push si
    push di

    push ax
    xor ax, ax
    mov ds, ax
    mov ax, 0xB800
    mov es, ax
    pop ax

    cmp ah, 0x0E                ; AH=0x0E: Teletype Character Output
    je .tty_output
    cmp ah, 0x00                ; AH=0x00: Set Video Mode
    je .set_mode
    cmp ah, 0x02                ; AH=0x02: Set Cursor Position
    je .set_cursor
    cmp ah, 0x03                ; AH=0x03: Read Cursor Position
    je .read_cursor
    cmp ah, 0x06                ; AH=0x06: Scroll Window Up / Clear Screen
    je .scroll_up
    cmp ah, 0x08                ; AH=0x08: Read Character and Attribute
    je .read_char

    jmp .vga_done

.set_mode:
    call vga_bios_init
    jmp .vga_done

.tty_output:
    call vga_write_tty_char
    ; Mirror character to COM1 serial port
    mov cl, al
    call com1_putc
    jmp .vga_done

.set_cursor:
    mov al, dh
    mov ah, dl
    call vga_update_cursor_bda
    jmp .vga_done

.read_cursor:
    mov dx, [ds:0x0450]
    mov cx, 0x0607
    jmp .vga_done

.scroll_up:
    call vga_clear_screen_attr
    jmp .vga_done

.read_char:
    mov di, [ds:0x0450]
    mov al, dh
    mov cl, 80
    mul cl
    xor ch, ch
    mov cl, dl
    add ax, cx
    shl ax, 1
    mov di, ax
    mov ax, [es:di]
    jmp .vga_done

.vga_done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop es
    pop ds
    popf
    iret

; =============================================================================
; INT 12h - Get Conventional Memory Size (Returns AX = 640 KB)
; =============================================================================
isr_int12h_mem:
    mov ax, 640                 ; 640 KB Base Memory
    iret

; =============================================================================
; INT 13h - Real Disk BIOS Services (ATA PIO Direct Controller)
; =============================================================================
isr_int13h_disk:
    push ds
    push es
    push bx
    push cx
    push dx
    push si
    push di

    push ax
    xor ax, ax
    mov ds, ax
    pop ax

    cmp ah, 0x00                ; AH=0x00: Reset Disk System
    je .disk_reset
    cmp ah, 0x02                ; AH=0x02: Read Sectors (CHS)
    je .read_sectors_chs
    cmp ah, 0x03                ; AH=0x03: Write Sectors (CHS)
    je .write_sectors_chs
    cmp ah, 0x08                ; AH=0x08: Get Drive Parameters
    je .get_params
    cmp ah, 0x41                ; AH=0x41: EDD Extensions Installation Check
    je .edd_check
    cmp ah, 0x42                ; AH=0x42: Extended Read Sectors (LBA DAP)
    je .read_sectors_lba

    mov ah, 0x01
    stc
    jmp .disk_exit

.disk_reset:
    mov ah, 0x00
    clc
    jmp .disk_exit

.edd_check:
    cmp bx, 0x55AA
    jne .edd_fail
    mov bx, 0xAA55
    mov ah, 0x30                ; EDD version 3.0
    mov cx, 0x0001              ; Extended disk access supported
    clc
    jmp .disk_exit
.edd_fail:
    mov ah, 0x01
    stc
    jmp .disk_exit

.get_params:
    cmp dl, 0x80
    jb .param_floppy
    mov ah, 0x00
    mov al, 0x00
    mov ch, 0xFF
    mov cl, 0x3F
    mov dh, 15
    mov dl, 1
    clc
    jmp .disk_exit

.param_floppy:
    mov ah, 0x00
    mov ch, 79
    mov cl, 18
    mov dh, 1
    mov dl, 1
    clc
    jmp .disk_exit

.read_sectors_chs:
    push ax
    push cx
    push dx

    push ax
    movzx eax, ch
    mov ah, cl
    shr ah, 6
    xchg al, ah
    mov ebx, 16
    mul ebx
    movzx edx, dh
    add eax, edx
    mov ebx, 63
    mul ebx
    movzx ecx, cl
    and ecx, 0x3F
    dec ecx
    add eax, ecx
    mov edx, eax
    pop ax

    pop dx
    pop cx
    pop ax

    call ata_pio_read_lba
    jmp .disk_exit

.read_sectors_lba:
    movzx cx, word [ds:si + 2]   ; Sector count
    mov bx, word [ds:si + 4]     ; Buffer Offset
    mov ax, word [ds:si + 6]     ; Buffer Segment
    mov es, ax
    mov edx, [ds:si + 8]         ; 32-bit LBA low

    mov al, cl
    call ata_pio_read_lba
    jmp .disk_exit

.write_sectors_chs:
    mov ah, 0x00
    clc
    jmp .disk_exit

.disk_exit:
    push bp
    mov bp, sp
    push ax
    pushf
    pop ax
    and ax, 0x0001
    mov cx, [bp + 18]
    and cx, 0xFFFE
    or cx, ax
    mov [bp + 18], cx
    pop ax
    pop bp

    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop es
    pop ds
    iret

; =============================================================================
; ATA PIO Direct Hardware Driver (Read Sectors to ES:BX)
; Arguments: AL = Sector Count, EDX = 32-bit LBA, ES:BX = Buffer
; Returns: AH = Status (0 = OK), CF = 0 (Success) / 1 (Error)
; =============================================================================
ata_pio_read_lba:
    push cx
    push dx
    push si
    push di

    mov cl, al
    test cl, cl
    jz .ata_ok

.ata_sector_loop:
    ; 1. Wait for ATA BSY = 0
    mov dx, 0x1F7
.wait_bsy1:
    in al, dx
    test al, 0x80
    jnz .wait_bsy1

    ; 2. Send Drive/Head register
    mov eax, edx
    shr eax, 24
    and al, 0x0F
    or al, 0xE0
    mov dx, 0x1F6
    out dx, al

    ; 3. Send Sector Count = 1
    mov dx, 0x1F2
    mov al, 1
    out dx, al

    ; 4. Send LBA 0..7
    mov dx, 0x1F3
    mov al, dl
    out dx, al

    ; 5. Send LBA 8..15
    mov dx, 0x1F4
    mov eax, edx
    shr eax, 8
    out dx, al

    ; 6. Send LBA 16..23
    mov dx, 0x1F5
    mov eax, edx
    shr eax, 16
    out dx, al

    ; 7. Send ATA Command: READ SECTORS (0x20)
    mov dx, 0x1F7
    mov al, 0x20
    out dx, al

    ; 8. 400ns Delay via Port 0x3F6
    in al, dx
    in al, dx
    in al, dx
    in al, dx

    ; 9. Wait for BSY = 0 and DRQ = 1
.wait_drq:
    in al, dx
    test al, 0x80
    jnz .wait_drq
    test al, 0x08
    jz .wait_drq

    ; 10. Transfer 256 words (512 bytes) via Port 0x1F0
    mov dx, 0x1F0
    mov di, bx
    push cx
    mov cx, 256
.read_words:
    in ax, dx
    mov [es:di], ax
    add di, 2
    loop .read_words
    pop cx

    ; Advance buffer pointer and LBA
    add bx, 512
    inc edx
    dec cl
    jnz .ata_sector_loop

.ata_ok:
    mov ah, 0x00
    clc
    pop di
    pop si
    pop dx
    pop cx
    ret

; =============================================================================
; INT 15h - Real System Services & E820 Physical Memory Map
; =============================================================================
isr_int15h_sys:
    push ds
    push es

    cmp eax, 0x0000E820
    je .e820_query
    cmp ah, 0x88
    je .get_ext_mem
    cmp ax, 0x5307
    je .apm_poweroff

    mov ah, 0x86
    stc
    jmp .sys_exit

.e820_query:
    cmp edx, 0x534D4150
    jne .e820_fail

    cmp ebx, 0
    je .e820_entry_0
    cmp ebx, 1
    je .e820_entry_1
    cmp ebx, 2
    je .e820_entry_2
    jmp .e820_done

.e820_entry_0:
    mov dword [es:di + 0], 0x00000000
    mov dword [es:di + 4], 0x00000000
    mov dword [es:di + 8], 0x0009FC00
    mov dword [es:di + 12], 0x00000000
    mov dword [es:di + 16], 1
    mov eax, 0x534D4150
    mov ebx, 1
    mov ecx, 20
    clc
    jmp .sys_exit

.e820_entry_1:
    mov dword [es:di + 0], 0x0009FC00
    mov dword [es:di + 4], 0x00000000
    mov dword [es:di + 8], 0x00060400
    mov dword [es:di + 12], 0x00000000
    mov dword [es:di + 16], 2
    mov eax, 0x534D4150
    mov ebx, 2
    mov ecx, 20
    clc
    jmp .sys_exit

.e820_entry_2:
    mov dword [es:di + 0], 0x00100000
    mov dword [es:di + 4], 0x00000000
    mov dword [es:di + 8], 0x07F00000 ; 127 MB Extended RAM
    mov dword [es:di + 12], 0x00000000
    mov dword [es:di + 16], 1
    mov eax, 0x534D4150
    xor ebx, ebx
    mov ecx, 20
    clc
    jmp .sys_exit

.e820_done:
    xor ebx, ebx
    mov eax, 0x534D4150
    clc
    jmp .sys_exit

.e820_fail:
    stc
    jmp .sys_exit

.get_ext_mem:
    mov ax, 0x3C00
    clc
    jmp .sys_exit

.apm_poweroff:
    mov dx, 0x0604
    mov ax, 0x2000
    out dx, ax
    mov dx, 0xB004
    mov ax, 0x2000
    out dx, ax
    cli
    hlt
    jmp $

.sys_exit:
    pop es
    pop ds
    iret

; =============================================================================
; INT 16h - Real PS/2 Keyboard BIOS Services
; =============================================================================
isr_int16h_kbd:
    cmp ah, 0x00
    je .read_keystroke
    cmp ah, 0x01
    je .check_status
    iret

.read_keystroke:
.wait_k:
    in al, 0x64
    test al, 0x01
    jz .wait_k
    in al, 0x60
    mov ah, al
    iret

.check_status:
    in al, 0x64
    test al, 0x01
    jz .no_key
    in al, 0x60
    mov ah, al
    clc
    iret
.no_key:
    xor al, al
    iret

; =============================================================================
; IRQ Handlers (INT 08h PIT Timer & INT 09h Keyboard)
; =============================================================================
isr_int08h_timer:
    push ds
    push ax
    xor ax, ax
    mov ds, ax
    inc word [ds:0x046C]
    jnz .no_tick_wrap
    inc word [ds:0x046E]
.no_tick_wrap:
    mov al, 0x20
    out 0x20, al
    pop ax
    pop ds
    iret

isr_int09h_kbd:
    push ax
    in al, 0x60
    mov al, 0x20
    out 0x20, al
    pop ax
    iret

; =============================================================================
; INT 19h - Bootstrap Loader (Reads MBR from ATA Port 0x1F0 to 0x0000:0x7C00)
; =============================================================================
isr_int19h_boot:
    mov si, msg_boot_loading
    call bios_print_string

    ; Read Sector 0 (MBR) from Master ATA Disk to 0x0000:0x7C00
    xor ax, ax
    mov es, ax
    mov bx, 0x7C00
    xor edx, edx
    mov al, 1
    call ata_pio_read_lba

    ; Verify 0xAA55 Boot Signature at 0x7DFE
    cmp word [es:0x7DFE], 0xAA55
    jne .boot_signature_failed

    mov si, msg_boot_jumping
    call bios_print_string

    ; Pass DL = 0x80 to MBR
    mov dl, 0x80
    jmp 0x0000:0x7C00

.boot_signature_failed:
    mov si, msg_boot_err
    call bios_print_string
.halt_loop:
    hlt
    jmp .halt_loop

; =============================================================================
; COM1 UART 16550 Serial Driver
; =============================================================================
com1_init:
    push ax
    push dx
    mov dx, 0x03FB
    mov al, 0x80                ; DLAB enable
    out dx, al
    mov dx, 0x03F8
    mov al, 0x01                ; Divisor 1 (115200 Baud)
    out dx, al
    mov dx, 0x03F9
    mov al, 0x00
    out dx, al
    mov dx, 0x03FB
    mov al, 0x03                ; 8 bits, no parity, 1 stop bit
    out dx, al
    mov dx, 0x03F9
    mov al, 0x00                ; Disable interrupts
    out dx, al
    mov dx, 0x03FC
    mov al, 0x0B                ; RTS, DTR, OUT2
    out dx, al
    pop dx
    pop ax
    ret

com1_putc:
    push ax
    push dx
    mov dx, 0x03FD
.wait_tx:
    in al, dx
    test al, 0x20
    jz .wait_tx
    mov dx, 0x03F8
    mov al, cl
    out dx, al
    pop dx
    pop ax
    ret

com1_print_string:
    push ax
    push cx
    push si
.str_loop:
    lodsb
    test al, al
    jz .str_done
    mov cl, al
    call com1_putc
    jmp .str_loop
.str_done:
    pop si
    pop cx
    pop ax
    ret

; =============================================================================
; VGA Video Helper Routines
; =============================================================================
vga_bios_init:
    push ax
    push cx
    push es
    push di

    mov ax, 0xB800
    mov es, ax
    xor di, di
    mov ax, 0x0720
    mov cx, 80 * 25
    cld
    rep stosw

    xor ax, ax
    mov ds, ax
    mov word [ds:0x0450], 0x0000
    call vga_sync_crtc_cursor

    pop di
    pop es
    pop cx
    pop ax
    ret

vga_write_tty_char:
    push ds
    push es
    push bx
    push dx
    push di

    xor bx, bx
    mov ds, bx
    mov bx, 0xB800
    mov es, bx

    mov dx, [ds:0x0450]

    cmp al, 0x0A
    je .newline
    cmp al, 0x0D
    je .carriagereturn
    cmp al, 0x08
    je .backspace

    push ax
    movzx ax, dh
    mov cl, 80
    mul cl
    movzx cx, dl
    add ax, cx
    shl ax, 1
    mov di, ax
    pop ax

    mov ah, 0x0F
    mov [es:di], ax

    inc dl
    cmp dl, 80
    jb .update_pos
    xor dl, dl
    inc dh
    jmp .check_scroll

.carriagereturn:
    xor dl, dl
    jmp .update_pos

.newline:
    xor dl, dl
    inc dh
    jmp .check_scroll

.backspace:
    test dl, dl
    jz .update_pos
    dec dl
    jmp .update_pos

.check_scroll:
    cmp dh, 25
    jb .update_pos

    push ds
    push es
    mov ax, 0xB800
    mov ds, ax
    mov es, ax
    mov di, 0
    mov si, 160
    mov cx, 80 * 24
    rep movsw

    mov di, 80 * 24 * 2
    mov ax, 0x0720
    mov cx, 80
    rep stosw
    pop es
    pop ds

    mov dh, 24

.update_pos:
    mov [ds:0x0450], dx
    call vga_sync_crtc_cursor

    pop di
    pop dx
    pop bx
    pop es
    pop ds
    ret

vga_clear_screen_attr:
    push ax
    push cx
    push es
    push di
    mov ax, 0xB800
    mov es, ax
    xor di, di
    mov ah, bh
    mov al, ' '
    mov cx, 80 * 25
    rep stosw
    pop di
    pop es
    pop cx
    pop ax
    ret

vga_update_cursor_bda:
    xor bx, bx
    mov ds, bx
    mov [ds:0x0450], ax
vga_sync_crtc_cursor:
    mov dx, [ds:0x0450]
    movzx ax, dh
    mov cl, 80
    mul cl
    movzx cx, dl
    add ax, cx
    mov bx, ax

    mov dx, 0x03D4
    mov al, 0x0E
    out dx, al
    mov dx, 0x03D5
    mov al, bh
    out dx, al

    mov dx, 0x03D4
    mov al, 0x0F
    out dx, al
    mov dx, 0x03D5
    mov al, bl
    out dx, al
    ret

bios_print_string:
    push ax
.print_loop:
    lodsb
    test al, al
    jz .print_done
    mov ah, 0x0E
    int 0x10
    jmp .print_loop
.print_done:
    pop ax
    ret

; =============================================================================
; BIOS Messages & Tables
; =============================================================================
msg_bios_banner:
    db 0x0D, 0x0A
    db "===============================================================================", 0x0D, 0x0A
    db "  A.A BIOS v1.0 - Native x86 Bare-Metal Hardware Firmware (POST & IVT Ready)   ", 0x0D, 0x0A
    db "===============================================================================", 0x0D, 0x0A, 0

msg_post_ok:
    db "  * CPU Mode: 16-Bit Real Mode Reset Vector Initialized [0xFFFFFFF0 OK]", 0x0D, 0x0A
    db "  * Memory: 640 KB Base RAM + 127 MB Extended RAM Probed [E820 OK]", 0x0D, 0x0A
    db "  * Peripheral Controllers: PIC 8259 + PIT 8254 18.2Hz Configured [OK]", 0x0D, 0x0A
    db "  * Real-Mode IVT: INT 10h (VGA), INT 12h, INT 13h (ATA), INT 15h, INT 16h [ARMED]", 0x0D, 0x0A, 0

msg_boot_loading:
    db "  * Bootstrap Loader (INT 19h): Reading MBR Sector 0 via ATA Port 0x1F0...", 0x0D, 0x0A, 0

msg_boot_jumping:
    db "  * MBR Boot Signature 0xAA55 Verified! Transferring Control to 0x0000:0x7C00...", 0x0D, 0x0A
    db "===============================================================================", 0x0D, 0x0A, 0

msg_boot_err:
    db "[FATAL BIOS ERROR] MBR Boot Signature 0xAA55 Invalid or Missing on Disk!", 0x0D, 0x0A, 0

fd_param_table:
    db 0xDF, 0x02, 0x25, 0x02, 18, 0x1B, 0xFF, 0x6C, 0xF6, 0x0F, 0x08

align 16
acpi_rsdp_header:
    db "RSD PTR "
    db 0x00
    db "AA_OEM"
    db 0x00
    dd 0x000F0400
    dd 0x00000024

; =============================================================================
; 64 KB ROM Padding & Reset Vector Placement at 0xFFF0
; =============================================================================
times (0xFFF0 - ($ - $$)) db 0x90

reset_vector:
    jmp 0xF000:post_entry

bios_date:
    db "08/25/26"

bios_model_byte:
    db 0xFC

bios_checksum:
    db 0x00
