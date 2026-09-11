[BITS 32]
[GLOBAL _start]
[GLOBAL _load_idt]
[EXTERN _kernel_main]
[EXTERN _c_exception_handler]
[EXTERN _bss_start]
[EXTERN _bss_end]

section .text
_start:
    ; 1. Genuine Silicon FPU & SSE Hardware Initialization (CR0 / CR4)
    mov eax, cr0
    and ax, 0xFFFB      ; Clear CR0.EM (coprocessor emulation)
    or ax, 0x0002       ; Set CR0.MP (monitor coprocessor)
    mov cr0, eax

    ; Guarded SSE enable: check if CPUID and SSE feature flag (EDX bit 25) are supported
    pushfd
    pop eax
    mov ecx, eax
    xor eax, (1 << 21)  ; Toggle ID bit (bit 21)
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    xor eax, ecx
    jz .skip_sse        ; CPUID not supported

    mov eax, 1
    cpuid
    test edx, (1 << 25) ; Test SSE support
    jz .skip_sse

    mov eax, cr4
    or eax, (1 << 9) | (1 << 10) ; Set CR4.OSFXSR and CR4.OSXMMEXCPT
    mov cr4, eax

.skip_sse:
    fninit              ; Initialize physical FPU state

    ; 2. Zero-out the .BSS Section in Physical RAM (Eliminates warm-reboot & uninitialized RAM garbage)
    mov edi, _bss_start
    mov ecx, _bss_end
    sub ecx, _bss_start
    cld
    xor eax, eax
    rep stosb

    ; 3. Initialize 32-bit Protected Mode IDT Gates
    call init_idt_gates
    lidt [idt_descriptor]

    ; 4. Call C Kernel Main
    call _kernel_main

    ; 5. Halt CPU if kernel returns
    cli
    hlt
.hang:
    jmp .hang

_load_idt:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; Common ISR Stubs Macro
%macro ISR_NOERRCODE 1
[GLOBAL isr%1]
isr%1:
    push dword 0        ; Push 0 placeholder (CPU pushes no error code for this interrupt)
    push dword %1       ; Interrupt vector number
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
[GLOBAL isr%1]
isr%1:
    push dword %1       ; Interrupt number (error code already pushed by CPU)
    jmp isr_common_stub
%endmacro

; CPU Exceptions 0-31
ISR_NOERRCODE 0   ; 0: Divide by Zero (#DE)
ISR_NOERRCODE 1   ; 1: Debug (#DB)
ISR_NOERRCODE 2   ; 2: NMI
ISR_NOERRCODE 3   ; 3: Breakpoint (#BP)
ISR_NOERRCODE 4   ; 4: Overflow (#OF)
ISR_NOERRCODE 5   ; 5: Bound Range Exceeded (#BR)
ISR_NOERRCODE 6   ; 6: Invalid Opcode (#UD)
ISR_NOERRCODE 7   ; 7: Device Not Available (#NM)
ISR_ERRCODE   8   ; 8: Double Fault (#DF)
ISR_NOERRCODE 9   ; 9: Coprocessor Segment Overrun
ISR_ERRCODE   10  ; 10: Invalid TSS (#TS)
ISR_ERRCODE   11  ; 11: Segment Not Present (#NP)
ISR_ERRCODE   12  ; 12: Stack-Segment Fault (#SS)
ISR_ERRCODE   13  ; 13: General Protection Fault (#GP)
ISR_ERRCODE   14  ; 14: Page Fault (#PF)
ISR_NOERRCODE 15  ; 15: Reserved
ISR_NOERRCODE 16  ; 16: x87 FPU Floating-Point Error (#MF)
ISR_ERRCODE   17  ; 17: Alignment Check (#AC)
ISR_NOERRCODE 18  ; 18: Machine Check (#MC)
ISR_NOERRCODE 19  ; 19: SIMD Floating-Point Exception (#XM)
ISR_NOERRCODE 20  ; 20: Virtualization Exception (#VE)
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30  ; Security Exception (#SX)
ISR_NOERRCODE 31

isr_common_stub:
    pusha               ; Pushes EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX

    push ds
    push es
    push fs
    push gs

    mov ax, 0x10        ; Kernel Data Segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp            ; Pass pointer to trap_frame struct to C
    call _c_exception_handler
    add esp, 4          ; Clean up parameter

    pop gs
    pop fs
    pop es
    pop ds

    popa                ; Pops EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX
    add esp, 8          ; Clean up error_code and int_no
    iret                ; Return from interrupt

[GLOBAL asm_irq0_timer_wrapper]
[GLOBAL asm_irq1_keyboard_wrapper]
[GLOBAL asm_default_irq_wrapper]
[EXTERN _c_irq0_timer_handler]
[EXTERN _c_irq0_timer_handler_preempt]
[EXTERN _c_irq1_keyboard_handler]
[EXTERN _c_default_irq_handler]

init_idt_gates:
    %macro SET_IDT_GATE 2
    mov eax, isr%1
    mov word [idt_entries + (%1 * 8)], ax
    mov word [idt_entries + (%1 * 8) + 2], 0x08 ; Kernel Code Segment Selector
    mov byte [idt_entries + (%1 * 8) + 4], 0    ; Reserved
    mov byte [idt_entries + (%1 * 8) + 5], %2   ; Type & Attributes (0x8E = 32-bit Interrupt Gate, Ring 0)
    shr eax, 16
    mov word [idt_entries + (%1 * 8) + 6], ax
    %endmacro

    ; Exceptions 0..31
    %assign i 0
    %rep 32
    SET_IDT_GATE i, 0x8E
    %assign i i+1
    %endrep

    ; Universal Spurious & Hardware IRQ Gates for Vectors 32 to 255
    %assign i 32
    %rep 224
    mov eax, asm_default_irq_wrapper
    mov word [idt_entries + (i * 8)], ax
    mov word [idt_entries + (i * 8) + 2], 0x08
    mov byte [idt_entries + (i * 8) + 4], 0
    mov byte [idt_entries + (i * 8) + 5], 0x8E
    shr eax, 16
    mov word [idt_entries + (i * 8) + 6], ax
    %assign i i+1
    %endrep

    ; Vector 32 (Timer)
    mov eax, asm_irq0_timer_wrapper
    mov word [idt_entries + (32 * 8)], ax
    mov word [idt_entries + (32 * 8) + 2], 0x08
    mov byte [idt_entries + (32 * 8) + 4], 0
    mov byte [idt_entries + (32 * 8) + 5], 0x8E
    shr eax, 16
    mov word [idt_entries + (32 * 8) + 6], ax

    ; Vector 33 (Keyboard)
    mov eax, asm_irq1_keyboard_wrapper
    mov word [idt_entries + (33 * 8)], ax
    mov word [idt_entries + (33 * 8) + 2], 0x08
    mov byte [idt_entries + (33 * 8) + 4], 0
    mov byte [idt_entries + (33 * 8) + 5], 0x8E
    shr eax, 16
    mov word [idt_entries + (33 * 8) + 6], ax

    ret

asm_default_irq_wrapper:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld
    push dword 0xFF ; Default spurious vector indicator
    call _c_default_irq_handler
    add esp, 4
    pop gs
    pop fs
    pop es
    pop ds
    popa
    iret

asm_irq0_timer_wrapper:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld
    push esp
    call _c_irq0_timer_handler_preempt
    mov esp, eax
    pop gs
    pop fs
    pop es
    pop ds
    popa
    iret

asm_irq1_keyboard_wrapper:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld
    call _c_irq1_keyboard_handler
    pop gs
    pop fs
    pop es
    pop ds
    popa
    iret

section .data
align 8
idt_entries:
    times 256 * 8 db 0

idt_descriptor:
    dw (256 * 8) - 1    ; IDT limit (256 gates * 8 bytes - 1)
    dd idt_entries      ; IDT base address






[GLOBAL _asm_vmx_launch_helper]
[GLOBAL _asm_vmx_resume_helper]
[GLOBAL _vmx_vmexit_asm_entry]
[GLOBAL _vmx_host_saved_esp]
[EXTERN _vmx_vmexit_c_handler]

_vmx_host_saved_esp: dd 0

_asm_vmx_launch_helper:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    push ds
    push es
    push fs
    push gs

    ; Save active kernel host stack pointer before launch
    mov [_vmx_host_saved_esp], esp

    ; Execute VMLAUNCH instruction on silicon
    vmlaunch

    ; If vmlaunch immediately fails (CF/ZF set), CPU falls through here
    pop gs
    pop fs
    pop es
    pop ds
    pop edi
    pop esi
    pop ebx
    pop ebp
    mov eax, 1 ; Error return code
    ret

_asm_vmx_resume_helper:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    push ds
    push es
    push fs
    push gs

    ; Save active kernel host stack pointer before resume
    mov [_vmx_host_saved_esp], esp

    ; Execute VMRESUME instruction on silicon
    vmresume

    ; If vmresume immediately fails (CF/ZF set), CPU falls through here
    pop gs
    pop fs
    pop es
    pop ds
    pop edi
    pop esi
    pop ebx
    pop ebp
    mov eax, 1 ; Error return code
    ret

; When hardware VM-Exit occurs, CPU jumps here with Host_RSP and Host_RIP
_vmx_vmexit_asm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld

    ; Call C handler to read VMCS exit information
    call _vmx_vmexit_c_handler

    ; Restore host stack pointer saved before vmlaunch/vmresume
    mov esp, [_vmx_host_saved_esp]

    ; Restore host callee-saved registers
    pop gs
    pop fs
    pop es
    pop ds
    pop edi
    pop esi
    pop ebx
    pop ebp

    ; Return 0 to caller (successful execution + clean VM-Exit)
    xor eax, eax
    ret

; =========================================================================
; High Hardware Access: Pure Assembly Primitives (Direct Silicon Execution)
; =========================================================================

[GLOBAL _asm_fast_memcpy]
[GLOBAL _asm_fast_memset]
[GLOBAL _asm_fast_memzero]
[GLOBAL _asm_fast_memmove]
[GLOBAL _asm_fast_memcmp]
[GLOBAL _asm_insw_stream]
[GLOBAL _asm_outsw_stream]
[GLOBAL _asm_insl_stream]
[GLOBAL _asm_outsl_stream]
[GLOBAL _asm_wbinvd_pure]
[GLOBAL _asm_invlpg_pure]
[GLOBAL _asm_clflush_pure]
[GLOBAL _asm_read_cr0_pure]
[GLOBAL _asm_write_cr0_pure]
[GLOBAL _asm_read_cr2_pure]
[GLOBAL _asm_read_cr3_pure]
[GLOBAL _asm_write_cr3_pure]
[GLOBAL _asm_read_cr4_pure]
[GLOBAL _asm_write_cr4_pure]
[GLOBAL _asm_read_eflags_pure]
[GLOBAL _asm_write_eflags_pure]
[GLOBAL _asm_rdtsc_pure]
[GLOBAL _asm_rdmsr_pure]
[GLOBAL _asm_wrmsr_pure]
[GLOBAL _asm_load_gdt]
[GLOBAL _asm_load_tr]

; void asm_fast_memcpy(void* dest, const void* src, uint32_t count)
_asm_fast_memcpy:
    push ebp
    mov ebp, esp
    push esi
    push edi
    push ecx

    mov edi, [ebp + 8]   ; dest
    mov esi, [ebp + 12]  ; src
    mov ecx, [ebp + 16]  ; count
    test ecx, ecx
    jz .done

    cld
    mov edx, ecx
    shr ecx, 2           ; dwords
    rep movsd
    mov ecx, edx
    and ecx, 3           ; remaining bytes
    rep movsb

.done:
    pop ecx
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret

; void asm_fast_memset(void* dest, uint8_t val, uint32_t count)
_asm_fast_memset:
    push ebp
    mov ebp, esp
    push edi
    push ecx

    mov edi, [ebp + 8]   ; dest
    movzx eax, byte [ebp + 12] ; val
    mov ecx, [ebp + 16]  ; count
    test ecx, ecx
    jz .done

    ; Broadcast byte to 4 bytes in EAX
    mov ah, al
    mov edx, eax
    shl eax, 16
    mov ax, dx

    cld
    mov edx, ecx
    shr ecx, 2
    rep stosd
    mov ecx, edx
    and ecx, 3
    rep stosb

.done:
    pop ecx
    pop edi
    mov esp, ebp
    pop ebp
    ret

; void asm_fast_memzero(void* dest, uint32_t count)
_asm_fast_memzero:
    push ebp
    mov ebp, esp
    push edi
    push ecx

    mov edi, [ebp + 8]
    mov ecx, [ebp + 12]
    xor eax, eax
    test ecx, ecx
    jz .done

    cld
    mov edx, ecx
    shr ecx, 2
    rep stosd
    mov ecx, edx
    and ecx, 3
    rep stosb

.done:
    pop ecx
    pop edi
    mov esp, ebp
    pop ebp
    ret

; void asm_fast_memmove(void* dest, const void* src, uint32_t count)
_asm_fast_memmove:
    push ebp
    mov ebp, esp
    push esi
    push edi
    push ecx

    mov edi, [ebp + 8]
    mov esi, [ebp + 12]
    mov ecx, [ebp + 16]
    test ecx, ecx
    jz .done

    cmp edi, esi
    jb .forward
    je .done

    ; Backward copy (STD)
    std
    add edi, ecx
    dec edi
    add esi, ecx
    dec esi
    rep movsb
    cld
    jmp .done

.forward:
    cld
    mov edx, ecx
    shr ecx, 2
    rep movsd
    mov ecx, edx
    and ecx, 3
    rep movsb

.done:
    pop ecx
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret

; int asm_fast_memcmp(const void* s1, const void* s2, uint32_t count)
_asm_fast_memcmp:
    push ebp
    mov ebp, esp
    push esi
    push edi
    push ecx

    mov esi, [ebp + 8]
    mov edi, [ebp + 12]
    mov ecx, [ebp + 16]
    xor eax, eax
    test ecx, ecx
    jz .done

    cld
    repe cmpsb
    je .done

    movzx eax, byte [esi - 1]
    movzx edx, byte [edi - 1]
    sub eax, edx

.done:
    pop ecx
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret

; void asm_insw_stream(uint16_t port, void* addr, uint32_t count)
_asm_insw_stream:
    push ebp
    mov ebp, esp
    push edi
    push ecx

    mov dx, [ebp + 8]
    mov edi, [ebp + 12]
    mov ecx, [ebp + 16]
    cld
    rep insw

    pop ecx
    pop edi
    mov esp, ebp
    pop ebp
    ret

; void asm_outsw_stream(uint16_t port, const void* addr, uint32_t count)
_asm_outsw_stream:
    push ebp
    mov ebp, esp
    push esi
    push ecx

    mov dx, [ebp + 8]
    mov esi, [ebp + 12]
    mov ecx, [ebp + 16]
    cld
    rep outsw

    pop ecx
    pop esi
    mov esp, ebp
    pop ebp
    ret

; void asm_insl_stream(uint16_t port, void* addr, uint32_t count)
_asm_insl_stream:
    push ebp
    mov ebp, esp
    push edi
    push ecx

    mov dx, [ebp + 8]
    mov edi, [ebp + 12]
    mov ecx, [ebp + 16]
    cld
    rep insd

    pop ecx
    pop edi
    mov esp, ebp
    pop ebp
    ret

; void asm_outsl_stream(uint16_t port, const void* addr, uint32_t count)
_asm_outsl_stream:
    push ebp
    mov ebp, esp
    push esi
    push ecx

    mov dx, [ebp + 8]
    mov esi, [ebp + 12]
    mov ecx, [ebp + 16]
    cld
    rep outsd

    pop ecx
    pop esi
    mov esp, ebp
    pop ebp
    ret

; Direct MMU / CPU Silicon Operations
_asm_wbinvd_pure:
    wbinvd
    ret

_asm_invlpg_pure:
    mov eax, [esp + 4]
    invlpg [eax]
    ret

_asm_clflush_pure:
    mov eax, [esp + 4]
    clflush [eax]
    ret

_asm_read_cr0_pure:
    mov eax, cr0
    ret

_asm_write_cr0_pure:
    mov eax, [esp + 4]
    mov cr0, eax
    ret

_asm_read_cr2_pure:
    mov eax, cr2
    ret

_asm_read_cr3_pure:
    mov eax, cr3
    ret

_asm_write_cr3_pure:
    mov eax, [esp + 4]
    mov cr3, eax
    ret

_asm_read_cr4_pure:
    mov eax, cr4
    ret

_asm_write_cr4_pure:
    mov eax, [esp + 4]
    mov cr4, eax
    ret

_asm_read_eflags_pure:
    pushfd
    pop eax
    ret

_asm_write_eflags_pure:
    mov eax, [esp + 4]
    push eax
    popfd
    ret

_asm_rdtsc_pure:
    mov ecx, [esp + 4] ; lo ptr
    mov edx, [esp + 8] ; hi ptr
    rdtsc
    mov [ecx], eax
    mov [edx], edx
    ret

_asm_rdmsr_pure:
    push ebx
    mov ecx, [esp + 8]  ; msr
    mov ebx, [esp + 12] ; lo ptr
    rdmsr
    mov [ebx], eax
    mov ebx, [esp + 16] ; hi ptr
    mov [ebx], edx
    pop ebx
    ret

_asm_wrmsr_pure:
    mov ecx, [esp + 4] ; msr
    mov eax, [esp + 8] ; lo
    mov edx, [esp + 12]; hi
    wrmsr
    ret

_asm_load_gdt:
    mov eax, [esp + 4]
    lgdt [eax]
    ret

_asm_load_tr:
    mov ax, [esp + 4]
    ltr ax
    ret
