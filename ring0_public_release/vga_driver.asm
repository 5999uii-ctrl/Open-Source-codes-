; =========================================================================
; A.A OS - Dedicated VGA Video Memory & CRTC Hardware Assembly Engine
; Direct 0xB8000 Framebuffer Blitter, 960-Dword Hardware Scroll, CRTC Ports
; =========================================================================

[BITS 32]

section .text

; -------------------------------------------------------------------------
; Exported Global Symbols
; -------------------------------------------------------------------------
[GLOBAL _asm_vga_clear_screen]
[GLOBAL _asm_vga_scroll_screen]
[GLOBAL _asm_vga_write_char_at]
[GLOBAL _asm_vga_update_hardware_cursor]
[GLOBAL _asm_vga_disable_hardware_cursor]
[GLOBAL _asm_vga_enable_hardware_cursor]
[GLOBAL _asm_vga_write_string_raw]
[GLOBAL _asm_video_blit_lfb]
[GLOBAL _asm_video_clear_lfb]
[GLOBAL _asm_video_draw_rect_lfb]

; -------------------------------------------------------------------------
; 1. Direct VGA Text Framebuffer Operations (Physical 0xB8000)
; -------------------------------------------------------------------------

; void asm_vga_clear_screen(uint8_t color_attr)
_asm_vga_clear_screen:
    push ebp
    mov ebp, esp
    push edi
    push ecx

    movzx eax, byte [ebp + 8]   ; color_attr
    shl eax, 8
    mov al, 0x20                ; Space character (' ')
    ; Duplicate word to 32-bit dword: [attr | ' ' | attr | ' ']
    mov edx, eax
    shl eax, 16
    mov ax, dx

    mov edi, 0x000B8000         ; Physical VGA Text Buffer Base
    mov ecx, 1000               ; 80 * 25 / 2 = 1000 dwords
    cld
    rep stosd

    pop ecx
    pop edi
    mov esp, ebp
    pop ebp
    ret

; void asm_vga_scroll_screen(uint8_t color_attr)
_asm_vga_scroll_screen:
    push ebp
    mov ebp, esp
    push edi
    push esi
    push ecx

    ; 1. Copy rows 1..24 to rows 0..23 (80 cols * 24 rows * 2 bytes = 3,840 bytes = 960 dwords)
    mov edi, 0x000B8000         ; Row 0
    mov esi, 0x000B80A0         ; Row 1 (80 * 2 = 160 = 0xA0)
    mov ecx, 960
    cld
    rep movsd

    ; 2. Clear bottom row 24 (80 cols * 2 bytes = 160 bytes = 40 dwords)
    movzx eax, byte [ebp + 8]   ; color_attr
    shl eax, 8
    mov al, 0x20
    mov edx, eax
    shl eax, 16
    mov ax, dx

    mov ecx, 40
    rep stosd

    pop ecx
    pop esi
    pop edi
    mov esp, ebp
    pop ebp
    ret

; void asm_vga_write_char_at(uint32_t col, uint32_t row, char ch, uint8_t attr)
_asm_vga_write_char_at:
    push ebp
    mov ebp, esp
    push ebx

    mov eax, [ebp + 12]         ; row (0-24)
    mov ecx, 80
    mul ecx                     ; EAX = row * 80
    add eax, [ebp + 8]          ; EAX = row * 80 + col
    shl eax, 1                  ; EAX = (row * 80 + col) * 2 bytes
    add eax, 0x000B8000         ; Physical memory address

    mov dl, [ebp + 16]          ; ch
    mov dh, [ebp + 20]          ; attr
    mov [eax], dx               ; Direct 16-bit write to physical VGA buffer

    pop ebx
    mov esp, ebp
    pop ebp
    ret

; void asm_vga_write_string_raw(uint32_t col, uint32_t row, const char* str, uint8_t attr)
_asm_vga_write_string_raw:
    push ebp
    mov ebp, esp
    push esi
    push edi
    push ebx

    mov eax, [ebp + 12]         ; row
    mov ecx, 80
    mul ecx
    add eax, [ebp + 8]          ; col
    shl eax, 1
    add eax, 0x000B8000
    mov edi, eax                ; Video buffer dest

    mov esi, [ebp + 16]         ; string pointer
    mov ah, [ebp + 20]          ; attr

.loop:
    lodsb
    test al, al
    jz .done
    stosw                       ; Store AL (char) and AH (attr)
    jmp .loop

.done:
    pop ebx
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 2. Hardware VGA CRTC Register Control (Ports 0x3D4 / 0x3D5)
; -------------------------------------------------------------------------

; void asm_vga_update_hardware_cursor(uint16_t cursor_offset)
_asm_vga_update_hardware_cursor:
    push ebp
    mov ebp, esp
    push ebx

    mov bx, [ebp + 8]           ; cursor_offset (row * 80 + col)

    ; Send High Byte (Index 0x0E)
    mov dx, 0x03D4
    mov al, 0x0E
    out dx, al
    mov dx, 0x03D5
    mov al, bh
    out dx, al

    ; Send Low Byte (Index 0x0F)
    mov dx, 0x03D4
    mov al, 0x0F
    out dx, al
    mov dx, 0x03D5
    mov al, bl
    out dx, al

    pop ebx
    mov esp, ebp
    pop ebp
    ret

; void asm_vga_disable_hardware_cursor(void)
_asm_vga_disable_hardware_cursor:
    mov dx, 0x03D4
    mov al, 0x0A
    out dx, al
    mov dx, 0x03D5
    mov al, 0x20                ; Set bit 5 to disable cursor
    out dx, al
    ret

; void asm_vga_enable_hardware_cursor(uint8_t cursor_start, uint8_t cursor_end)
_asm_vga_enable_hardware_cursor:
    push ebp
    mov ebp, esp

    mov dx, 0x03D4
    mov al, 0x0A
    out dx, al
    mov dx, 0x03D5
    in al, dx
    and al, 0xC0
    or al, [ebp + 8]            ; cursor_start
    out dx, al

    mov dx, 0x03D4
    mov al, 0x0B
    out dx, al
    mov dx, 0x03D5
    in al, dx
    and al, 0xE0
    or al, [ebp + 12]           ; cursor_end
    out dx, al

    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 3. High-Performance Linear Framebuffer (LFB) Video Blitting Engine
; -------------------------------------------------------------------------

; void asm_video_blit_lfb(void* dest_vram, const void* src_dram, uint32_t dword_count)
_asm_video_blit_lfb:
    push ebp
    mov ebp, esp
    push esi
    push edi
    push ecx

    mov edi, [ebp + 8]   ; dest_vram (VRAM MMIO Base e.g. 0xFD000000)
    mov esi, [ebp + 12]  ; src_dram (DRAM Backbuffer Base 0x00800000)
    mov ecx, [ebp + 16]  ; dword_count (width * height)

    test ecx, ecx
    jz .done_blit

    cld                  ; Forward direction flag
    rep movsd            ; Hardware accelerated 32-bit streaming transfer

.done_blit:
    pop ecx
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret

; void asm_video_clear_lfb(void* dest_buf, uint32_t color, uint32_t dword_count)
_asm_video_clear_lfb:
    push ebp
    mov ebp, esp
    push edi
    push ecx

    mov edi, [ebp + 8]   ; dest_buf
    mov eax, [ebp + 12]  ; color (32-bit 0x00RRGGBB)
    mov ecx, [ebp + 16]  ; dword_count

    test ecx, ecx
    jz .done_clear

    cld
    rep stosd

.done_clear:
    pop ecx
    pop edi
    mov esp, ebp
    pop ebp
    ret

; void asm_video_draw_rect_lfb(void* dest_buf, uint32_t pitch_dwords, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
_asm_video_draw_rect_lfb:
    push ebp
    mov ebp, esp
    push edi
    push ebx
    push esi
    push ecx

    mov edi, [ebp + 8]   ; dest_buf
    mov edx, [ebp + 12]  ; pitch_dwords (screen width)
    mov eax, [ebp + 20]  ; y
    mul edx              ; eax = y * pitch_dwords
    add eax, [ebp + 16]  ; eax = (y * pitch_dwords + x)
    shl eax, 2           ; byte offset
    add edi, eax         ; edi = pointer to top-left pixel

    mov ebx, [ebp + 28]  ; h (rows)
    mov eax, [ebp + 32]  ; color
    mov esi, [ebp + 24]  ; w (columns to fill per row)
    shl edx, 2           ; pitch_bytes = pitch_dwords * 4
    mov ecx, esi
    shl ecx, 2           ; w * 4
    sub edx, ecx         ; row_stride = pitch_bytes - (w * 4)

    test ebx, ebx
    jz .done_rect
    test esi, esi
    jz .done_rect

    cld
.row_loop:
    mov ecx, esi
    rep stosd            ; fill row
    add edi, edx         ; advance to next row start
    dec ebx
    jnz .row_loop

.done_rect:
    pop ecx
    pop esi
    pop ebx
    pop edi
    mov esp, ebp
    pop ebp
    ret

