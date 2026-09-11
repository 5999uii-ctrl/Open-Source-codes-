; =========================================================================
; A.A OS - Dedicated Intel 8042 PS/2 Keyboard & Mouse Hardware Assembly Engine
; Direct Silicon 8042 Controller Ports (0x60/0x64), Scancodes, IntelliMouse
; =========================================================================

[BITS 32]

section .text

; -------------------------------------------------------------------------
; Exported Global Symbols
; -------------------------------------------------------------------------
[GLOBAL _asm_ps2_wait_input_empty]
[GLOBAL _asm_ps2_wait_output_full]
[GLOBAL _asm_ps2_read_data]
[GLOBAL _asm_ps2_write_data]
[GLOBAL _asm_ps2_write_command]
[GLOBAL _asm_ps2_flush_output_buffer]

[GLOBAL _asm_ps2_set_keyboard_leds]
[GLOBAL _asm_ps2_read_scancode_poll]
[GLOBAL _asm_ps2_mouse_write]
[GLOBAL _asm_ps2_mouse_read_byte]
[GLOBAL _asm_ps2_mouse_enable_streaming]
[GLOBAL _asm_ps2_mouse_intellimouse_knock]

; -------------------------------------------------------------------------
; 1. Intel 8042 Controller Low-Level Port Handshaking (0x60 / 0x64)
; -------------------------------------------------------------------------

; int asm_ps2_wait_input_empty(void)
_asm_ps2_wait_input_empty:
    push ebp
    mov ebp, esp

    mov ecx, 100000             ; Timeout counter

.poll:
    in al, 0x64                 ; Read 8042 Status Register
    test al, 0x02               ; Bit 1: Input Buffer Status (0 = Empty, 1 = Full)
    jz .empty
    dec ecx
    jnz .poll
    xor eax, eax                ; Timeout (0)
    mov esp, ebp
    pop ebp
    ret

.empty:
    mov eax, 1                  ; Success (Input buffer empty)
    mov esp, ebp
    pop ebp
    ret

; int asm_ps2_wait_output_full(void)
_asm_ps2_wait_output_full:
    push ebp
    mov ebp, esp

    mov ecx, 100000

.poll:
    in al, 0x64
    test al, 0x01               ; Bit 0: Output Buffer Status (1 = Full/Data Ready)
    jnz .full
    dec ecx
    jnz .poll
    xor eax, eax                ; Timeout
    mov esp, ebp
    pop ebp
    ret

.full:
    mov eax, 1
    mov esp, ebp
    pop ebp
    ret

; uint8_t asm_ps2_read_data(void)
_asm_ps2_read_data:
    in al, 0x60                 ; Read Data Port
    ret

; int asm_ps2_write_data(uint8_t data)
_asm_ps2_write_data:
    push ebp
    mov ebp, esp

    call _asm_ps2_wait_input_empty
    test eax, eax
    jz .fail

    mov al, [ebp + 8]           ; data
    out 0x60, al
    mov eax, 1
    mov esp, ebp
    pop ebp
    ret

.fail:
    xor eax, eax
    mov esp, ebp
    pop ebp
    ret

; int asm_ps2_write_command(uint8_t cmd)
_asm_ps2_write_command:
    push ebp
    mov ebp, esp

    call _asm_ps2_wait_input_empty
    test eax, eax
    jz .fail

    mov al, [ebp + 8]           ; cmd
    out 0x64, al
    mov eax, 1
    mov esp, ebp
    pop ebp
    ret

.fail:
    xor eax, eax
    mov esp, ebp
    pop ebp
    ret

; void asm_ps2_flush_output_buffer(void)
_asm_ps2_flush_output_buffer:
    mov ecx, 32                 ; Up to 32 drain attempts

.drain:
    in al, 0x64
    test al, 0x01
    jz .done
    in al, 0x60                 ; Discard byte
    dec ecx
    jnz .drain

.done:
    ret

; -------------------------------------------------------------------------
; 2. PS/2 Keyboard Silicon Commands
; -------------------------------------------------------------------------

; int asm_ps2_set_keyboard_leds(uint8_t led_mask)
_asm_ps2_set_keyboard_leds:
    push ebp
    mov ebp, esp

    ; Step 1: Send Command 0xED (Set LEDs)
    call _asm_ps2_wait_input_empty
    test eax, eax
    jz .fail

    mov al, 0xED
    out 0x60, al

    ; Step 2: Wait for Keyboard ACK (0xFA)
    call _asm_ps2_wait_output_full
    test eax, eax
    jz .fail

    in al, 0x60
    cmp al, 0xFA
    jne .fail

    ; Step 3: Send LED Mask Byte (Scroll=1, Num=2, Caps=4)
    call _asm_ps2_wait_input_empty
    test eax, eax
    jz .fail

    mov al, [ebp + 8]           ; led_mask
    out 0x60, al

    ; Step 4: Wait for Final ACK (0xFA)
    call _asm_ps2_wait_output_full
    test eax, eax
    jz .fail

    in al, 0x60
    cmp al, 0xFA
    jne .fail

    mov eax, 1                  ; Success
    mov esp, ebp
    pop ebp
    ret

.fail:
    xor eax, eax                ; Failure
    mov esp, ebp
    pop ebp
    ret

; int asm_ps2_read_scancode_poll(void)
_asm_ps2_read_scancode_poll:
    in al, 0x64
    test al, 0x01
    jz .no_data

    ; Verify byte is from keyboard (Bit 5 of status port is 0 for keyboard, 1 for mouse)
    test al, 0x20
    jnz .no_data

    in al, 0x60
    movzx eax, al
    ret

.no_data:
    mov eax, -1
    ret

; -------------------------------------------------------------------------
; 3. PS/2 Mouse (Auxiliary Device) Hardware Engine
; -------------------------------------------------------------------------

; int asm_ps2_mouse_write(uint8_t byte_val)
_asm_ps2_mouse_write:
    push ebp
    mov ebp, esp

    ; Tell 8042 controller that next byte goes to Mouse: Command 0xD4
    call _asm_ps2_wait_input_empty
    test eax, eax
    jz .fail

    mov al, 0xD4
    out 0x64, al

    ; Write byte to Data Port
    call _asm_ps2_wait_input_empty
    test eax, eax
    jz .fail

    mov al, [ebp + 8]           ; byte_val
    out 0x60, al

    ; Wait for Mouse ACK (0xFA)
    call _asm_ps2_wait_output_full
    test eax, eax
    jz .fail

    in al, 0x60
    cmp al, 0xFA
    jne .fail

    mov eax, 1                  ; Success
    mov esp, ebp
    pop ebp
    ret

.fail:
    xor eax, eax
    mov esp, ebp
    pop ebp
    ret

; int asm_ps2_mouse_read_byte(uint8_t* out_byte)
_asm_ps2_mouse_read_byte:
    push ebp
    mov ebp, esp

    call _asm_ps2_wait_output_full
    test eax, eax
    jz .fail

    ; Check if data is from Aux/Mouse (Bit 5 = 1)
    in al, 0x64
    test al, 0x20
    jz .fail

    in al, 0x60
    mov edx, [ebp + 8]          ; out_byte pointer
    test edx, edx
    jz .done_read
    mov [edx], al

.done_read:
    mov eax, 1
    mov esp, ebp
    pop ebp
    ret

.fail:
    xor eax, eax
    mov esp, ebp
    pop ebp
    ret

; int asm_ps2_mouse_enable_streaming(void)
_asm_ps2_mouse_enable_streaming:
    push dword 0xF4
    call _asm_ps2_mouse_write
    add esp, 4
    ret

; int asm_ps2_mouse_intellimouse_knock(void)
; Magic knocking sample rate sequence: 200 -> 100 -> 80
; If device returns ID 3, IntelliMouse 3-button + Z-axis Scroll Wheel is active!
_asm_ps2_mouse_intellimouse_knock:
    push ebp
    mov ebp, esp

    ; 1. Set Sample Rate 200: (0xF3, 200)
    push dword 0xF3
    call _asm_ps2_mouse_write
    add esp, 4
    push dword 200
    call _asm_ps2_mouse_write
    add esp, 4

    ; 2. Set Sample Rate 100: (0xF3, 100)
    push dword 0xF3
    call _asm_ps2_mouse_write
    add esp, 4
    push dword 100
    call _asm_ps2_mouse_write
    add esp, 4

    ; 3. Set Sample Rate 80: (0xF3, 80)
    push dword 0xF3
    call _asm_ps2_mouse_write
    add esp, 4
    push dword 80
    call _asm_ps2_mouse_write
    add esp, 4

    ; 4. Query Device ID: 0xF2
    push dword 0xF2
    call _asm_ps2_mouse_write
    add esp, 4

    ; Read returned ID byte
    call _asm_ps2_wait_output_full
    test eax, eax
    jz .std_mouse

    in al, 0x60
    movzx eax, al
    mov esp, ebp
    pop ebp
    ret

.std_mouse:
    xor eax, eax
    mov esp, ebp
    pop ebp
    ret
