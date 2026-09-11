; =========================================================================
; A.A OS - Direct Silicon Chip & Hardware Controller Power Engine
; Pure x86 32-Bit Protected Mode Ring-0 Assembly Module
; Direct control over: 8259 PIC, Local APIC, 8042 PS/2, CPU Caches, 
; PIT/Speaker, PCI-PM D0/D3 Chip Power States, and Physical Memory Remapping
; Strict x86 cdecl ABI Compliance (All callee-saved registers preserved)
; =========================================================================

[BITS 32]
section .text

GLOBAL _asm_chip_pic_set_state
GLOBAL _asm_chip_apic_set_state
GLOBAL _asm_chip_ps2_set_state
GLOBAL _asm_chip_cache_set_state
GLOBAL _asm_chip_speaker_set_state
GLOBAL _asm_chip_pci_set_power_state
GLOBAL _asm_silicon_atomic_swap_mem
GLOBAL _asm_silicon_wrmsr_safe
GLOBAL _asm_silicon_pit_set_rate
GLOBAL _asm_silicon_get_cr0
GLOBAL _asm_silicon_get_cr4
GLOBAL _asm_silicon_set_wp
GLOBAL _asm_silicon_wrmsr_raw
GLOBAL _asm_silicon_cli
GLOBAL _asm_silicon_sti
GLOBAL _asm_silicon_write_phys_dword
GLOBAL _asm_silicon_read_phys_dword
GLOBAL _asm_silicon_triple_fault

; -------------------------------------------------------------------------
; Helper: I/O Wait (writes to port 0x80 to allow bus settling)
; -------------------------------------------------------------------------
_asm_hw_io_wait:
    out 0x80, al
    ret

; -------------------------------------------------------------------------
; 1. Intel 8259 PIC Chips Power / Interrupt Enable Controller
; void asm_chip_pic_set_state(uint32_t enable)
; enable=1 -> Unmask operational IRQs, enable=0 -> Mask all IRQs (power down)
; -------------------------------------------------------------------------
_asm_chip_pic_set_state:
    push ebp
    mov ebp, esp

    mov eax, [ebp + 8]          ; enable flag
    test eax, eax
    jz .disable_pic

.enable_pic:
    mov al, 0xF8                ; Master PIC: Unmask IRQ0(PIT), IRQ1(KBD), IRQ2(Cascade)
    out 0x21, al
    call _asm_hw_io_wait
    mov al, 0xAF                ; Slave PIC: Unmask IRQ12(Mouse), IRQ14(ATA HDD)
    out 0xA1, al
    call _asm_hw_io_wait
    jmp .done_pic

.disable_pic:
    mov al, 0xFF                ; Master PIC: Mask ALL 8 IRQ lines
    out 0x21, al
    call _asm_hw_io_wait
    mov al, 0xFF                ; Slave PIC: Mask ALL 8 IRQ lines
    out 0xA1, al
    call _asm_hw_io_wait

.done_pic:
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 2. On-Die Local APIC Chip Power & Execution State
; uint32_t asm_chip_apic_set_state(uint32_t enable)
; enable=1 -> Enable Local APIC, enable=0 -> Disable Local APIC core
; Returns: 1 if success, 0 if APIC not physically present on CPU
; -------------------------------------------------------------------------
_asm_chip_apic_set_state:
    push ebp
    mov ebp, esp
    push ebx
    push ecx
    push edx

    ; 1. Verify APIC CPUID feature flag (Leaf 1, EDX bit 9)
    mov eax, 1
    cpuid
    test edx, (1 << 9)
    jz .no_apic_hardware

    mov ebx, [ebp + 8]          ; enable flag

    ; 2. Read IA32_APIC_BASE MSR (0x1B)
    mov ecx, 0x1B
    rdmsr

    test ebx, ebx
    jz .disable_apic

.enable_apic:
    or eax, (1 << 11)           ; Set Global Enable Bit (bit 11)
    mov ecx, 0x1B
    wrmsr

    ; Access SVR (Spurious Interrupt Vector Register) at MMIO 0xFEE000F0
    ; Set bit 8 (APIC Software Enable)
    mov edx, 0xFEE000F0
    mov eax, [edx]
    or eax, (1 << 8) | 0xFF     ; Software enable + vector 0xFF
    mov [edx], eax
    mov eax, 1
    jmp .done_apic

.disable_apic:
    ; Clear bit 8 in SVR
    mov edx, 0xFEE000F0
    mov eax, [edx]
    and eax, ~(1 << 8)
    mov [edx], eax

    ; Clear bit 11 in IA32_APIC_BASE MSR (0x1B)
    mov ecx, 0x1B
    rdmsr
    and eax, ~(1 << 11)
    wrmsr
    mov eax, 1
    jmp .done_apic

.no_apic_hardware:
    xor eax, eax

.done_apic:
    pop edx
    pop ecx
    pop ebx
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 3. Intel 8042 PS/2 Keyboard/Mouse Microcontroller Chip
; void asm_chip_ps2_set_state(uint32_t device, uint32_t enable)
; device: 0=Keyboard, 1=Mouse; enable: 0=Disable, 1=Enable
; -------------------------------------------------------------------------
_asm_chip_ps2_set_state:
    push ebp
    mov ebp, esp
    push ecx

    mov ecx, [ebp + 8]          ; device (0=kbd, 1=mouse)
    mov eax, [ebp + 12]         ; enable (0=disable, 1=enable)

    ; Wait for 8042 input buffer empty
    mov edx, 100000
.wait_8042:
    in al, 0x64
    test al, 0x02
    jz .ready_8042
    dec edx
    jnz .wait_8042

.ready_8042:
    test ecx, ecx
    jnz .handle_mouse

.handle_kbd:
    mov eax, [ebp + 12]
    test eax, eax
    jz .kbd_off
    mov al, 0xAE                ; 0xAE = Enable Keyboard Interface
    out 0x64, al
    jmp .done_ps2
.kbd_off:
    mov al, 0xAD                ; 0xAD = Disable Keyboard Interface
    out 0x64, al
    jmp .done_ps2

.handle_mouse:
    mov eax, [ebp + 12]
    test eax, eax
    jz .mouse_off
    mov al, 0xA8                ; 0xA8 = Enable Auxiliary Mouse Interface
    out 0x64, al
    jmp .done_ps2
.mouse_off:
    mov al, 0xA7                ; 0xA7 = Disable Auxiliary Mouse Interface
    out 0x64, al

.done_ps2:
    pop ecx
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 4. CPU Hardware L1/L2/L3 SRAM Cache Controller
; void asm_chip_cache_set_state(uint32_t enable)
; enable=1 -> Caches Enabled, enable=0 -> Caches Disabled (CR0.CD + CR0.NW + wbinvd)
; -------------------------------------------------------------------------
_asm_chip_cache_set_state:
    push ebp
    mov ebp, esp

    mov eax, [ebp + 8]          ; enable flag
    test eax, eax
    jz .cache_off

.cache_on:
    wbinvd                      ; Write back and invalidate existing cache
    mov eax, cr0
    and eax, ~((1 << 30) | (1 << 29)) ; Clear CR0.CD (bit 30) and CR0.NW (bit 29)
    mov cr0, eax
    wbinvd
    jmp .done_cache

.cache_off:
    wbinvd
    mov eax, cr0
    or eax, (1 << 30) | (1 << 29)    ; Set CR0.CD and CR0.NW
    mov cr0, eax
    wbinvd

.done_cache:
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 5. Motherboard Speaker Gate Circuit Power Controller
; void asm_chip_speaker_set_state(uint32_t enable)
; -------------------------------------------------------------------------
_asm_chip_speaker_set_state:
    push ebp
    mov ebp, esp

    mov eax, [ebp + 8]
    test eax, eax
    jz .speaker_off

.speaker_on:
    in al, 0x61
    or al, 0x03                 ; Turn on timer gate and speaker data line
    out 0x61, al
    jmp .done_speaker

.speaker_off:
    in al, 0x61
    and al, 0xFC                ; Turn off timer gate and speaker line
    out 0x61, al

.done_speaker:
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 6. PCI Silicon Chip Power Management (D0 Active / D3 Sleep State)
; uint32_t asm_chip_pci_set_power_state(uint8_t bus, uint8_t slot, uint8_t func, uint8_t state)
; state: 0 = D0 (Full Power ON), 3 = D3hot (Powered OFF / Deep Sleep)
; Returns: 1 if PM capability found and updated, 0 if PM not supported on chip
; -------------------------------------------------------------------------
_asm_chip_pci_set_power_state:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    movzx eax, byte [ebp + 8]   ; bus
    shl eax, 16
    movzx ebx, byte [ebp + 12]  ; slot
    shl ebx, 11
    or eax, ebx
    movzx ebx, byte [ebp + 16]  ; func
    shl ebx, 8
    or eax, ebx
    or eax, 0x80000000          ; Enable bit
    mov esi, eax                ; Base PCI Config Address

    ; Read Status Register (Offset 0x04) to check if Capabilities List is supported (Bit 4)
    mov eax, esi
    or eax, 0x04
    mov dx, 0x0CF8
    out dx, eax
    mov dx, 0x0CFC
    in eax, dx
    test eax, (1 << 20)         ; Status bit 4: Capabilities List Present
    jz .no_pm_cap

    ; Read Capabilities Pointer (Offset 0x34)
    mov eax, esi
    or eax, 0x34
    mov dx, 0x0CF8
    out dx, eax
    mov dx, 0x0CFC
    in eax, dx
    and eax, 0xFC               ; Cap pointer (byte 0)
    mov edi, eax                ; Current Cap Pointer

.cap_loop:
    test edi, edi
    jz .no_pm_cap
    cmp edi, 0xFF
    jae .no_pm_cap

    ; Read Capability Header at offset EDI
    mov eax, esi
    or eax, edi
    mov dx, 0x0CF8
    out dx, eax
    mov dx, 0x0CFC
    in eax, dx

    movzx ebx, al               ; Capability ID
    cmp bl, 0x01                ; 0x01 = PCI Power Management Capability ID
    je .found_pm_cap

    ; Next pointer is in byte 1 (AH)
    shr eax, 8
    and eax, 0xFC
    mov edi, eax
    jmp .cap_loop

.found_pm_cap:
    ; PMCSR (Power Management Control/Status Register) is at cap_offset + 4
    add edi, 4
    mov eax, esi
    or eax, edi
    mov dx, 0x0CF8
    out dx, eax
    mov dx, 0x0CFC
    in eax, dx                  ; Read current PMCSR dword

    ; Mask PowerState bits 0-1
    and eax, ~0x00000003
    movzx ebx, byte [ebp + 20]  ; new state (0 = D0, 3 = D3)
    and ebx, 0x03
    or eax, ebx

    ; Write updated PMCSR
    push eax
    mov eax, esi
    or eax, edi
    mov dx, 0x0CF8
    out dx, eax
    pop eax
    mov dx, 0x0CFC
    out dx, eax

    mov eax, 1                  ; Success
    jmp .done_pci_pm

.no_pm_cap:
    xor eax, eax                ; No PM capability on this chip

.done_pci_pm:
    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 7. Atomic Physical Memory Dword Swap Engine
; void asm_silicon_atomic_swap_mem(uint32_t addr1, uint32_t addr2, uint32_t dword_count)
; -------------------------------------------------------------------------
_asm_silicon_atomic_swap_mem:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov esi, [ebp + 8]          ; addr1
    mov edi, [ebp + 12]         ; addr2
    mov ecx, [ebp + 16]         ; dword_count

    test ecx, ecx
    jz .done_swap

.swap_loop:
    mov eax, [esi]
    xchg eax, [edi]             ; Pure x86 atomic bus exchange
    mov [esi], eax
    add esi, 4
    add edi, 4
    dec ecx
    jnz .swap_loop

    wbinvd                      ; Flush and invalidate cache to physical RAM

.done_swap:
    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 8. Intel 8254 PIT High-Speed Frequency Rate Writer
; void asm_silicon_pit_set_rate(uint32_t freq_hz)
; -------------------------------------------------------------------------
_asm_silicon_pit_set_rate:
    push ebp
    mov ebp, esp
    push ebx
    push edx

    mov ebx, [ebp + 8]          ; freq_hz
    test ebx, ebx
    jz .done_pit

    mov eax, 1193182
    xor edx, edx
    div ebx                     ; Divisor = 1193182 / freq
    cmp eax, 65535
    jbe .valid_pit
    mov eax, 65535
.valid_pit:
    test eax, eax
    jnz .write_pit
    mov eax, 1
.write_pit:
    mov ebx, eax

    mov al, 0x36                ; Channel 0, Lobyte/Hibyte, Mode 3 Square Wave
    out 0x43, al
    call _asm_hw_io_wait
    mov al, bl
    out 0x40, al                ; Low byte
    call _asm_hw_io_wait
    mov al, bh
    out 0x40, al                ; High byte

.done_pit:
    pop edx
    pop ebx
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 9. Silicon Register Inspection Helpers
; uint32_t asm_silicon_get_cr0(void)
; uint32_t asm_silicon_get_cr4(void)
; -------------------------------------------------------------------------
_asm_silicon_get_cr0:
    mov eax, cr0
    ret

_asm_silicon_get_cr4:
    mov eax, cr4
    ret

; -------------------------------------------------------------------------
; 10. Zero-Security Supervisor Write-Protection Controller (CR0.WP)
; void asm_silicon_set_wp(uint32_t enable)
; enable=0 -> Sets CR0.WP=0: Ring-0 can overwrite ANY read-only RAM/Code/Page Tables
; -------------------------------------------------------------------------
_asm_silicon_set_wp:
    push ebp
    mov ebp, esp
    mov eax, cr0
    mov edx, [ebp + 8]
    test edx, edx
    jz .disable_wp
    or eax, (1 << 16)           ; CR0.WP = 1 (Protection Active)
    jmp .done_wp
.disable_wp:
    and eax, ~(1 << 16)          ; CR0.WP = 0 (ZERO SECURITY: Write Protection Disabled)
.done_wp:
    mov cr0, eax
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 11. Zero-Security Direct MSR Writer (WRMSR)
; void asm_silicon_wrmsr_raw(uint32_t msr, uint32_t lo, uint32_t hi)
; -------------------------------------------------------------------------
_asm_silicon_wrmsr_raw:
    push ebp
    mov ebp, esp
    push ebx
    mov ecx, [ebp + 8]          ; MSR register number
    mov eax, [ebp + 12]         ; Low 32 bits
    mov edx, [ebp + 16]         ; High 32 bits
    wrmsr                       ; Direct raw silicon MSR write
    pop ebx
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 12. Global Hardware Interrupt Lock (CLI / STI)
; -------------------------------------------------------------------------
_asm_silicon_cli:
    cli
    ret

_asm_silicon_sti:
    sti
    ret

; -------------------------------------------------------------------------
; 13. Direct Zero-Security Physical Memory Dword Access
; void asm_silicon_write_phys_dword(uint32_t addr, uint32_t val)
; uint32_t asm_silicon_read_phys_dword(uint32_t addr)
; -------------------------------------------------------------------------
_asm_silicon_write_phys_dword:
    push ebp
    mov ebp, esp
    mov edx, [ebp + 8]          ; Physical Address
    mov eax, [ebp + 12]         ; Value to write
    mov [edx], eax              ; Direct uninhibited physical write
    clflush [edx]               ; Direct cacheline flush
    mov esp, ebp
    pop ebp
    ret

_asm_silicon_read_phys_dword:
    push ebp
    mov ebp, esp
    mov edx, [ebp + 8]
    mov eax, [edx]
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 14. Hardware Triple Fault (Instant Hardware Silicon Reset)
; void asm_silicon_triple_fault(void)
; -------------------------------------------------------------------------
_asm_silicon_triple_fault:
    push dword 0
    push dword 0
    lidt [esp]                  ; Zero-out IDT descriptor limit to 0
    int 3                       ; Interrupt 3 -> Double Fault -> Triple Fault -> Hard CPU Reset
    hlt

