; =========================================================================
; A.A OS - Dedicated UART 16550 Serial Port Controller Assembly Engine
; Direct Silicon COM1/COM2 Register Access, Baud Rate Divisor, Loopback Test
; =========================================================================

[BITS 32]

section .text

; -------------------------------------------------------------------------
; Exported Global Symbols
; -------------------------------------------------------------------------
[GLOBAL _asm_serial_init]
[GLOBAL _asm_serial_is_transmit_empty]
[GLOBAL _asm_serial_write_char]
[GLOBAL _asm_serial_write_string]
[GLOBAL _asm_serial_is_data_ready]
[GLOBAL _asm_serial_read_char]
[GLOBAL _asm_serial_set_baud_rate]
[GLOBAL _asm_serial_run_loopback_test]

; -------------------------------------------------------------------------
; 1. 16550 UART Hardware Initialization
; -------------------------------------------------------------------------

; void asm_serial_init(uint16_t base_port, uint32_t baud_rate)
_asm_serial_init:
    push ebp
    mov ebp, esp
    push ebx

    mov bx, [ebp + 8]           ; base_port (e.g. 0x3F8 for COM1)
    mov eax, [ebp + 12]         ; baud_rate (e.g. 115200, 38400, 9600)
    test eax, eax
    jnz .calc_div
    mov eax, 115200
.calc_div:
    ; Base clock 115200 Hz
    mov ecx, eax
    mov eax, 115200
    xor edx, edx
    div ecx                     ; EAX = Divisor (115200 / baud)
    test eax, eax
    jnz .div_ok
    mov eax, 1                  ; Minimum divisor
.div_ok:
    mov ecx, eax                ; ECX = Divisor

    ; 1. Disable Interrupts: Port Base + 1 (IER = 0x00)
    mov dx, bx
    inc dx
    xor al, al
    out dx, al

    ; 2. Enable DLAB (Divisor Latch Access Bit): Port Base + 3 (LCR = 0x80)
    mov dx, bx
    add dx, 3
    mov al, 0x80
    out dx, al

    ; 3. Set Divisor Low Byte: Port Base + 0
    mov dx, bx
    mov al, cl
    out dx, al

    ; 4. Set Divisor High Byte: Port Base + 1
    mov dx, bx
    inc dx
    mov al, ch
    out dx, al

    ; 5. Configure 8 Data Bits, No Parity, 1 Stop Bit (8N1): Port Base + 3 (LCR = 0x03)
    mov dx, bx
    add dx, 3
    mov al, 0x03
    out dx, al

    ; 6. Enable FIFO, Clear TX/RX Queues, 14-byte threshold: Port Base + 2 (FCR = 0xC7)
    mov dx, bx
    add dx, 2
    mov al, 0xC7
    out dx, al

    ; 7. Set RTS/DSR Active, Auxiliary Output 2 (IRQs): Port Base + 4 (MCR = 0x0B)
    mov dx, bx
    add dx, 4
    mov al, 0x0B
    out dx, al

    pop ebx
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 2. UART Status & Transmit / Receive Routines (With Timeout Guards)
; -------------------------------------------------------------------------

; int asm_serial_is_transmit_empty(uint16_t base_port)
_asm_serial_is_transmit_empty:
    mov dx, [esp + 4]
    add dx, 5                   ; Line Status Register (LSR = Base + 5)
    in al, dx
    test al, 0x20               ; Bit 5: Empty Transmitter Holding Register
    jz .not_empty
    mov eax, 1
    ret
.not_empty:
    xor eax, eax
    ret

; void asm_serial_write_char(uint16_t base_port, char ch)
_asm_serial_write_char:
    push ebp
    mov ebp, esp

    mov dx, [ebp + 8]
    add dx, 5
    mov ecx, 100000             ; 100,000 cycle timeout guard

.wait_tx:
    in al, dx
    test al, 0x20
    jnz .ready
    dec ecx
    jnz .wait_tx
    jmp .done                   ; Prevent hang on faulty hardware

.ready:
    mov dx, [ebp + 8]           ; Data Port (Base + 0)
    mov al, [ebp + 12]          ; char
    out dx, al

.done:
    mov esp, ebp
    pop ebp
    ret

; void asm_serial_write_string(uint16_t base_port, const char* str)
_asm_serial_write_string:
    push ebp
    mov ebp, esp
    push esi
    push ebx

    mov bx, [ebp + 8]           ; base_port
    mov esi, [ebp + 12]         ; str

.loop:
    lodsb
    test al, al
    jz .done

    ; Wait until transmit buffer is empty (with timeout)
    mov dx, bx
    add dx, 5
    mov ecx, 100000
.wait:
    in al, dx
    test al, 0x20
    jnz .send
    dec ecx
    jnz .wait
    jmp .done                   ; Timeout protection

.send:
    mov dx, bx
    mov al, [esi - 1]
    out dx, al
    jmp .loop

.done:
    pop ebx
    pop esi
    mov esp, ebp
    pop ebp
    ret

; int asm_serial_is_data_ready(uint16_t base_port)
_asm_serial_is_data_ready:
    mov dx, [esp + 4]
    add dx, 5
    in al, dx
    test al, 0x01               ; Bit 0: Data Ready
    jz .no_data
    mov eax, 1
    ret
.no_data:
    xor eax, eax
    ret

; int asm_serial_read_char(uint16_t base_port)
_asm_serial_read_char:
    push ebp
    mov ebp, esp

    mov dx, [ebp + 8]
    add dx, 5
    mov ecx, 100000             ; Non-blocking timeout guard

.wait_rx:
    in al, dx
    test al, 0x01
    jnz .ready
    dec ecx
    jnz .wait_rx
    mov eax, -1                 ; Timeout / No data
    mov esp, ebp
    pop ebp
    ret

.ready:
    mov dx, [ebp + 8]
    in al, dx
    movzx eax, al
    mov esp, ebp
    pop ebp
    ret

; void asm_serial_set_baud_rate(uint16_t base_port, uint32_t baud_rate)
_asm_serial_set_baud_rate:
    jmp _asm_serial_init

; -------------------------------------------------------------------------
; 3. Internal Hardware Loopback Diagnostic Test
; -------------------------------------------------------------------------

; int asm_serial_run_loopback_test(uint16_t base_port)
_asm_serial_run_loopback_test:
    push ebp
    mov ebp, esp
    push ebx

    mov bx, [ebp + 8]

    ; 1. Enable Loopback Mode: Port Base + 4 (MCR = 0x1E: RTS, DTR, OUT1, OUT2, LOOP)
    mov dx, bx
    add dx, 4
    mov al, 0x1E
    out dx, al

    ; 2. Send Test Byte 0xAE to Port Base + 0
    mov dx, bx
    mov al, 0xAE
    out dx, al

    ; 3. Wait for data in receiver with timeout
    mov dx, bx
    add dx, 5
    mov ecx, 50000
.wait_loop:
    in al, dx
    test al, 0x01
    jnz .data_arrived
    dec ecx
    jnz .wait_loop

    ; Timeout: Restore normal mode and fail
    mov dx, bx
    add dx, 4
    mov al, 0x0F
    out dx, al
    xor eax, eax
    jmp .done

.data_arrived:
    ; 4. Read byte from Port Base + 0
    mov dx, bx
    in al, dx

    ; 5. Restore Normal Operational Mode (MCR = 0x0F)
    mov dx, bx
    add dx, 4
    mov ah, al                  ; Save read byte in AH
    mov al, 0x0F
    out dx, al

    ; Check if received byte matches 0xAE
    cmp ah, 0xAE
    jne .fail
    mov eax, 1                  ; Loopback Pass
    jmp .done

.fail:
    xor eax, eax

.done:
    pop ebx
    mov esp, ebp
    pop ebp
    ret
