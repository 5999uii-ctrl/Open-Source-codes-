; =========================================================================
; A.A OS - Dedicated Pure x86 Assembly Storage & Memory Master Editor
; 32-Bit Protected Mode Ring-0 Engine
; Direct physical ATA PIO and physical RAM byte/sector editing, clearing,
; writing, copying, and erasing with ZERO abstraction layers.
; Strict x86 cdecl ABI Compliance (All callee-saved registers preserved)
; =========================================================================

[BITS 32]
section .text

GLOBAL _asm_storage_edit_sector_byte
GLOBAL _asm_storage_fill_sectors_raw
GLOBAL _asm_storage_copy_sector_raw
GLOBAL _asm_mem_edit_byte_raw
GLOBAL _asm_mem_fill_raw
GLOBAL _asm_mem_erase_raw

; External hardware primitives from hardware_io.asm
EXTERN _asm_ata_wait_bsy
EXTERN _asm_ata_wait_drq
EXTERN _asm_ata_read_sector_pio
EXTERN _asm_ata_write_sector_pio

; -------------------------------------------------------------------------
; 1. Edit a Single Byte in Physical Storage Sector
; uint32_t asm_storage_edit_sector_byte(uint32_t lba, uint32_t offset, uint8_t new_val, void* temp_buf)
; Returns: 1 on success, 0 on failure
; -------------------------------------------------------------------------
_asm_storage_edit_sector_byte:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov eax, [ebp + 8]          ; lba
    mov ecx, [ebp + 12]         ; offset (0-511)
    mov dl,  [ebp + 16]         ; new_val
    mov edi, [ebp + 20]         ; temp_buf (512 bytes)

    test edi, edi
    jz .fail

    cmp ecx, 511
    ja .fail

    ; Step 1: Read the sector from ATA Primary Master (Port 0x1F0, Drive 0)
    push edi                    ; dest_buf
    push eax                    ; lba
    push dword 0                ; drive 0 (Master)
    push dword 0x1F0            ; base_port
    call _asm_ata_read_sector_pio
    add esp, 16
    test eax, eax
    jz .fail

    ; Step 2: Modify the exact byte in memory buffer
    mov edi, [ebp + 20]
    mov ecx, [ebp + 12]
    mov dl,  [ebp + 16]
    mov [edi + ecx], dl

    ; Step 3: Write modified 512 bytes back to disk via ATA PIO
    push edi                    ; src_buf
    push dword [ebp + 8]        ; lba
    push dword 0                ; drive 0
    push dword 0x1F0            ; base_port
    call _asm_ata_write_sector_pio
    add esp, 16
    test eax, eax
    jz .fail

    mov eax, 1                  ; Success
    jmp .done

.fail:
    xor eax, eax

.done:
    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 2. Fill Range of Sectors with Pattern or Zeroes (Erase / Write)
; uint32_t asm_storage_fill_sectors_raw(uint32_t start_lba, uint32_t count, uint8_t byte_val, void* temp_buf)
; Returns: Number of sectors successfully written
; -------------------------------------------------------------------------
_asm_storage_fill_sectors_raw:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov ebx, [ebp + 8]          ; start_lba
    mov esi, [ebp + 12]         ; count
    mov al,  [ebp + 16]         ; byte_val
    mov edi, [ebp + 20]         ; temp_buf (512 bytes)

    test edi, edi
    jz .fill_fail
    test esi, esi
    jz .fill_fail

    ; Prepare 512-byte buffer with byte_val
    mov ah, al
    mov dx, ax
    shl eax, 16
    mov ax, dx                  ; EAX = 4 duplicate bytes
    mov ecx, 128                ; 128 dwords = 512 bytes
    push edi
    cld
    rep stosd
    pop edi

    xor ecx, ecx                ; ecx = sectors written count

.sector_loop:
    cmp ecx, esi
    jae .fill_done

    push ecx                    ; save loop counter

    ; Write sector
    push edi                    ; src_buf
    mov eax, ebx
    add eax, ecx                ; current lba = start_lba + ecx
    push eax                    ; lba
    push dword 0                ; drive 0 (Master)
    push dword 0x1F0            ; base_port
    call _asm_ata_write_sector_pio
    add esp, 16

    pop ecx
    test eax, eax
    jz .fill_done               ; stop on write failure

    inc ecx
    jmp .sector_loop

.fill_done:
    mov eax, ecx                ; Return written count
    jmp .ret_fill

.fill_fail:
    xor eax, eax

.ret_fill:
    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 3. Clone / Copy Sector from Source LBA to Destination LBA
; uint32_t asm_storage_copy_sector_raw(uint32_t src_lba, uint32_t dst_lba, void* temp_buf)
; Returns: 1 on success, 0 on failure
; -------------------------------------------------------------------------
_asm_storage_copy_sector_raw:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov eax, [ebp + 8]          ; src_lba
    mov ebx, [ebp + 12]         ; dst_lba
    mov edi, [ebp + 16]         ; temp_buf

    test edi, edi
    jz .copy_fail

    ; Read source sector
    push edi
    push eax
    push dword 0
    push dword 0x1F0
    call _asm_ata_read_sector_pio
    add esp, 16
    test eax, eax
    jz .copy_fail

    ; Write to destination sector
    push edi
    push ebx
    push dword 0
    push dword 0x1F0
    call _asm_ata_write_sector_pio
    add esp, 16
    test eax, eax
    jz .copy_fail

    mov eax, 1
    jmp .copy_done

.copy_fail:
    xor eax, eax

.copy_done:
    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 4. Direct Byte Editor for Physical RAM
; void asm_mem_edit_byte_raw(uint32_t addr, uint8_t val)
; -------------------------------------------------------------------------
_asm_mem_edit_byte_raw:
    push ebp
    mov ebp, esp
    mov edx, [ebp + 8]          ; Physical Address
    mov al,  [ebp + 12]         ; Byte Value
    mov [edx], al               ; Direct volatile write
    clflush [edx]               ; Direct cacheline flush
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 5. Direct Physical RAM Fill Routine (Pure Assembly 'rep stosb')
; void asm_mem_fill_raw(uint32_t addr, uint32_t count, uint8_t val)
; -------------------------------------------------------------------------
_asm_mem_fill_raw:
    push ebp
    mov ebp, esp
    push edi

    mov edi, [ebp + 8]          ; Destination address
    mov ecx, [ebp + 12]         ; Count in bytes
    mov al,  [ebp + 16]         ; Value to fill

    cld
    rep stosb                   ; High-speed pure hardware fill
    wbinvd                      ; Cache flush

    pop edi
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 6. Direct Physical RAM Erase / Zero-Out
; void asm_mem_erase_raw(uint32_t addr, uint32_t count)
; -------------------------------------------------------------------------
_asm_mem_erase_raw:
    push ebp
    mov ebp, esp
    push edi

    mov edi, [ebp + 8]          ; Destination address
    mov ecx, [ebp + 12]         ; Count in bytes
    xor al, al                  ; 0x00

    cld
    rep stosb                   ; Zero out memory
    wbinvd                      ; Invalidate and flush caches

    pop edi
    mov esp, ebp
    pop ebp
    ret
