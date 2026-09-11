; =========================================================================
; A.A OS - Dedicated Hardware Silicon Port I/O Assembly Engine
; Direct ATA/IDE PIO Hard Disk Controller, PCI Configuration Mechanism #1,
; CMOS RTC Controller, Intel 8254 PIT & Motherboard Speaker Modulation
; =========================================================================

[BITS 32]

section .text

; -------------------------------------------------------------------------
; Exported Global Symbols
; -------------------------------------------------------------------------
[GLOBAL _asm_inb]
[GLOBAL _asm_inw]
[GLOBAL _asm_inl]
[GLOBAL _asm_outb]
[GLOBAL _asm_outw]
[GLOBAL _asm_outl]
[GLOBAL _asm_io_wait]

[GLOBAL _asm_pci_read_dword]
[GLOBAL _asm_pci_write_dword]
[GLOBAL _asm_pci_read_word]
[GLOBAL _asm_pci_read_byte]

[GLOBAL _asm_cmos_read_register]
[GLOBAL _asm_cmos_write_register]

[GLOBAL _asm_pit_set_reload_value]
[GLOBAL _asm_speaker_tone_on]
[GLOBAL _asm_speaker_tone_off]

[GLOBAL _asm_ata_wait_bsy]
[GLOBAL _asm_ata_wait_drq]
[GLOBAL _asm_ata_read_sector_pio]
[GLOBAL _asm_ata_write_sector_pio]
[GLOBAL _asm_ata_identify_pio]

; -------------------------------------------------------------------------
; 1. Direct Port I/O Low-Level Instructions
; -------------------------------------------------------------------------

; uint8_t asm_inb(uint16_t port)
_asm_inb:
    mov dx, [esp + 4]
    in al, dx
    ret

; uint16_t asm_inw(uint16_t port)
_asm_inw:
    mov dx, [esp + 4]
    in ax, dx
    ret

; uint32_t asm_inl(uint16_t port)
_asm_inl:
    mov dx, [esp + 4]
    in eax, dx
    ret

; void asm_outb(uint16_t port, uint8_t val)
_asm_outb:
    mov dx, [esp + 4]
    mov al, [esp + 8]
    out dx, al
    ret

; void asm_outw(uint16_t port, uint16_t val)
_asm_outw:
    mov dx, [esp + 4]
    mov ax, [esp + 8]
    out dx, ax
    ret

; void asm_outl(uint16_t port, uint32_t val)
_asm_outl:
    mov dx, [esp + 4]
    mov eax, [esp + 8]
    out dx, eax
    ret

; void asm_io_wait(void)
_asm_io_wait:
    mov al, 0
    out 0x80, al         ; Diagnostic Port 0x80 delay (~1-4 microseconds)
    ret

; -------------------------------------------------------------------------
; 2. PCI Configuration Space Mechanism #1 (Ports 0xCF8 / 0xCFC)
; -------------------------------------------------------------------------

; uint32_t asm_pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
_asm_pci_read_dword:
    push ebp
    mov ebp, esp
    push ebx

    movzx eax, byte [ebp + 8]   ; bus
    shl eax, 16
    movzx ebx, byte [ebp + 12]  ; slot / device
    shl ebx, 11
    or eax, ebx
    movzx ebx, byte [ebp + 16]  ; func
    shl ebx, 8
    or eax, ebx
    movzx ebx, byte [ebp + 20]  ; offset
    and ebx, 0xFC               ; Align to 32-bit dword boundary
    or eax, ebx
    or eax, 0x80000000          ; Enable Bit 31

    mov dx, 0x0CF8
    out dx, eax

    mov dx, 0x0CFC
    in eax, dx

    pop ebx
    mov esp, ebp
    pop ebp
    ret

; void asm_pci_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val)
_asm_pci_write_dword:
    push ebp
    mov ebp, esp
    push ebx

    movzx eax, byte [ebp + 8]   ; bus
    shl eax, 16
    movzx ebx, byte [ebp + 12]  ; slot
    shl ebx, 11
    or eax, ebx
    movzx ebx, byte [ebp + 16]  ; func
    shl ebx, 8
    or eax, ebx
    movzx ebx, byte [ebp + 20]  ; offset
    and ebx, 0xFC
    or eax, ebx
    or eax, 0x80000000

    mov dx, 0x0CF8
    out dx, eax

    mov dx, 0x0CFC
    mov eax, [ebp + 24]         ; val
    out dx, eax

    pop ebx
    mov esp, ebp
    pop ebp
    ret

; uint16_t asm_pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
_asm_pci_read_word:
    push ebp
    mov ebp, esp
    push dword [ebp + 20]
    push dword [ebp + 16]
    push dword [ebp + 12]
    push dword [ebp + 8]
    call _asm_pci_read_dword
    add esp, 16

    mov cl, [ebp + 20]
    and cl, 2
    shl cl, 3                   ; (offset & 2) * 8
    shr eax, cl
    and eax, 0xFFFF
    mov esp, ebp
    pop ebp
    ret

; uint8_t asm_pci_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
_asm_pci_read_byte:
    push ebp
    mov ebp, esp
    push dword [ebp + 20]
    push dword [ebp + 16]
    push dword [ebp + 12]
    push dword [ebp + 8]
    call _asm_pci_read_dword
    add esp, 16

    mov cl, [ebp + 20]
    and cl, 3
    shl cl, 3                   ; (offset & 3) * 8
    shr eax, cl
    and eax, 0xFF
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 3. CMOS RTC Controller (Ports 0x70 / 0x71)
; -------------------------------------------------------------------------

; uint8_t asm_cmos_read_register(uint8_t reg)
_asm_cmos_read_register:
    push ebp
    mov ebp, esp

    mov al, [ebp + 8]
    and al, 0x7F                ; Preserve NMI enable (Bit 7 = 0)
    out 0x70, al
    call _asm_io_wait
    in al, 0x71

    mov esp, ebp
    pop ebp
    ret

; void asm_cmos_write_register(uint8_t reg, uint8_t val)
_asm_cmos_write_register:
    push ebp
    mov ebp, esp

    mov al, [ebp + 8]
    and al, 0x7F
    out 0x70, al
    call _asm_io_wait
    mov al, [ebp + 12]
    out 0x71, al

    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 4. Intel 8254 PIT Timer & PC Speaker Modulation
; -------------------------------------------------------------------------

; void asm_pit_set_reload_value(uint16_t divisor)
_asm_pit_set_reload_value:
    push ebp
    mov ebp, esp

    mov al, 0x36                ; Channel 0, Lobyte/Hibyte, Mode 3 (Square Wave), Binary
    out 0x43, al
    mov ax, [ebp + 8]           ; divisor
    out 0x40, al                ; Low byte
    mov al, ah
    out 0x40, al                ; High byte

    mov esp, ebp
    pop ebp
    ret

; void asm_speaker_tone_on(uint32_t frequency_hz)
_asm_speaker_tone_on:
    push ebp
    mov ebp, esp
    push ebx

    mov ebx, [ebp + 8]          ; frequency_hz
    test ebx, ebx
    jz .done

    ; PIT Base Clock: 1193182 Hz
    mov eax, 1193182
    xor edx, edx
    div ebx                     ; EAX = Divisor (1193182 / freq)
    cmp eax, 65535
    jbe .valid_div
    mov eax, 65535
.valid_div:
    mov ebx, eax

    ; Program Channel 2 for PC Speaker (Mode 3 Square Wave)
    mov al, 0xB6
    out 0x43, al
    mov al, bl
    out 0x42, al                ; Low byte
    mov al, bh
    out 0x42, al                ; High byte

    ; Enable Speaker Gate on Port 0x61 (Bits 0 & 1)
    in al, 0x61
    or al, 0x03
    out 0x61, al

.done:
    pop ebx
    mov esp, ebp
    pop ebp
    ret

; void asm_speaker_tone_off(void)
_asm_speaker_tone_off:
    in al, 0x61
    and al, 0xFC                ; Clear bits 0 & 1
    out 0x61, al
    ret

; -------------------------------------------------------------------------
; 5. ATA/IDE Hard Disk Controller PIO Subsystem (Primary 0x1F0-0x1F7)
; -------------------------------------------------------------------------

; int asm_ata_wait_bsy(uint16_t base_port)
_asm_ata_wait_bsy:
    push ebp
    mov ebp, esp

    mov dx, [ebp + 8]
    add dx, 7                   ; Status Port (Base + 7)
    mov ecx, 1000000            ; Timeout counter

.poll:
    in al, dx
    test al, 0x80               ; BSY bit (Bit 7)
    jz .ready
    dec ecx
    jnz .poll
    xor eax, eax                ; Timeout (0)
    mov esp, ebp
    pop ebp
    ret

.ready:
    mov eax, 1                  ; Ready (1)
    mov esp, ebp
    pop ebp
    ret

; int asm_ata_wait_drq(uint16_t base_port)
_asm_ata_wait_drq:
    push ebp
    mov ebp, esp

    mov dx, [ebp + 8]
    add dx, 7
    mov ecx, 1000000

.poll:
    in al, dx
    test al, 0x01               ; ERR bit (Bit 0)
    jnz .error
    test al, 0x20               ; DF bit (Bit 5 Device Fault)
    jnz .error
    test al, 0x08               ; DRQ bit (Bit 3 Data Request)
    jnz .ready
    dec ecx
    jnz .poll

.error:
    xor eax, eax                ; Error / Timeout
    mov esp, ebp
    pop ebp
    ret

.ready:
    mov eax, 1                  ; DRQ Asserted
    mov esp, ebp
    pop ebp
    ret

; int asm_ata_read_sector_pio(uint16_t base_port, uint8_t drive, uint32_t lba, void* dest_buf)
_asm_ata_read_sector_pio:
    push ebp
    mov ebp, esp
    push edi
    push ebx

    mov bx, [ebp + 8]           ; base_port (e.g. 0x1F0)

    ; 1. Wait for BSY to clear
    push dword [ebp + 8]
    call _asm_ata_wait_bsy
    add esp, 4
    test eax, eax
    jz .fail

    ; 2. Reload arguments safely after function call
    mov bx, [ebp + 8]
    mov cl, [ebp + 12]          ; drive (0=Master, 1=Slave)
    mov edi, [ebp + 20]         ; dest_buf

    ; 3. Send Drive / Head / LBA 24-27 (Port Base + 6)
    mov dx, bx
    add dx, 6
    mov al, [ebp + 19]          ; LBA bits 24-27
    and al, 0x0F
    or al, 0xE0                 ; LBA Mode
    test cl, cl
    jz .is_master
    or al, 0x10                 ; Slave drive bit 4
.is_master:
    out dx, al

    ; 4. Send Sector Count = 1 (Port Base + 2)
    mov dx, bx
    add dx, 2
    mov al, 1
    out dx, al

    ; 5. Send LBA bits 0-7 (Port Base + 3)
    mov dx, bx
    add dx, 3
    mov al, [ebp + 16]
    out dx, al

    ; 6. Send LBA bits 8-15 (Port Base + 4)
    mov dx, bx
    add dx, 4
    mov al, [ebp + 17]
    out dx, al

    ; 7. Send LBA bits 16-23 (Port Base + 5)
    mov dx, bx
    add dx, 5
    mov al, [ebp + 18]
    out dx, al

    ; 8. Send Command 0x20 (READ SECTORS) to Command Port (Base + 7)
    mov dx, bx
    add dx, 7
    mov al, 0x20
    out dx, al

    ; 9. Wait for DRQ to assert
    push dword [ebp + 8]
    call _asm_ata_wait_drq
    add esp, 4
    test eax, eax
    jz .fail

    ; 10. Read 256 Words (512 Bytes) from Data Port (Base + 0) via REP INSW
    mov dx, [ebp + 8]
    mov edi, [ebp + 20]
    mov ecx, 256
    cld
    rep insw

    mov eax, 1                  ; Success
    jmp .done

.fail:
    xor eax, eax

.done:
    pop ebx
    pop edi
    mov esp, ebp
    pop ebp
    ret

; int asm_ata_write_sector_pio(uint16_t base_port, uint8_t drive, uint32_t lba, const void* src_buf)
_asm_ata_write_sector_pio:
    push ebp
    mov ebp, esp
    push esi
    push ebx

    mov bx, [ebp + 8]

    push dword [ebp + 8]
    call _asm_ata_wait_bsy
    add esp, 4
    test eax, eax
    jz .fail

    ; Reload arguments safely
    mov bx, [ebp + 8]
    mov cl, [ebp + 12]          ; drive
    mov esi, [ebp + 20]         ; src_buf

    ; Select Drive & LBA high bits
    mov dx, bx
    add dx, 6
    mov al, [ebp + 19]
    and al, 0x0F
    or al, 0xE0
    test cl, cl
    jz .w_is_master
    or al, 0x10
.w_is_master:
    out dx, al

    ; Sector Count = 1
    mov dx, bx
    add dx, 2
    mov al, 1
    out dx, al

    ; LBA 0-7, 8-15, 16-23
    mov dx, bx
    add dx, 3
    mov al, [ebp + 16]
    out dx, al
    mov dx, bx
    add dx, 4
    mov al, [ebp + 17]
    out dx, al
    mov dx, bx
    add dx, 5
    mov al, [ebp + 18]
    out dx, al

    ; Send Command 0x30 (WRITE SECTORS)
    mov dx, bx
    add dx, 7
    mov al, 0x30
    out dx, al

    push dword [ebp + 8]
    call _asm_ata_wait_drq
    add esp, 4
    test eax, eax
    jz .fail

    ; Write 256 Words (512 Bytes) via REP OUTSW
    mov dx, [ebp + 8]
    mov esi, [ebp + 20]
    mov ecx, 256
    cld
    rep outsw

    ; Flush Cache Command (0xE7)
    mov dx, [ebp + 8]
    add dx, 7
    mov al, 0xE7
    out dx, al

    push dword [ebp + 8]
    call _asm_ata_wait_bsy
    add esp, 4

    mov eax, 1
    jmp .done

.fail:
    xor eax, eax

.done:
    pop ebx
    pop esi
    mov esp, ebp
    pop ebp
    ret

; int asm_ata_identify_pio(uint16_t base_port, uint8_t drive, void* dest_buf)
_asm_ata_identify_pio:
    push ebp
    mov ebp, esp
    push edi
    push ebx

    push dword [ebp + 8]
    call _asm_ata_wait_bsy
    add esp, 4
    test eax, eax
    jz .fail

    ; Reload arguments safely
    mov bx, [ebp + 8]
    mov cl, [ebp + 12]          ; drive
    mov edi, [ebp + 16]         ; dest_buf

    ; Select Drive
    mov dx, bx
    add dx, 6
    mov al, (0xA0 | (0 << 4))
    test cl, cl
    jz .id_is_master
    mov al, (0xA0 | (1 << 4))
.id_is_master:
    out dx, al

    ; Zero out sector count and LBA registers
    mov dx, bx
    add dx, 2
    xor al, al
    out dx, al
    mov dx, bx
    add dx, 3
    out dx, al
    mov dx, bx
    add dx, 4
    out dx, al
    mov dx, bx
    add dx, 5
    out dx, al

    ; Send 0xEC (IDENTIFY)
    mov dx, bx
    add dx, 7
    mov al, 0xEC
    out dx, al

    ; Check status
    in al, dx
    test al, al
    jz .fail                    ; Drive does not exist

    push dword [ebp + 8]
    call _asm_ata_wait_drq
    add esp, 4
    test eax, eax
    jz .fail

    ; Read 256 Words IDENTIFY parameters
    mov dx, [ebp + 8]
    mov edi, [ebp + 16]
    mov ecx, 256
    cld
    rep insw

    mov eax, 1
    jmp .done

.fail:
    xor eax, eax

.done:
    pop ebx
    pop edi
    mov esp, ebp
    pop ebp
    ret
