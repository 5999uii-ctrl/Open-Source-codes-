; =========================================================================
; A.A OS - Dedicated Hardware Silicon Debugger & Register Engine
; DR0-DR7 Breakpoints, EFLAGS Single-Step Trap Flag, CR0-CR4, MSRs, RDPMC
; =========================================================================

[BITS 32]

section .text

; -------------------------------------------------------------------------
; Exported Global Symbols
; -------------------------------------------------------------------------
[GLOBAL _asm_read_dr0]
[GLOBAL _asm_read_dr1]
[GLOBAL _asm_read_dr2]
[GLOBAL _asm_read_dr3]
[GLOBAL _asm_read_dr6]
[GLOBAL _asm_read_dr7]

[GLOBAL _asm_write_dr0]
[GLOBAL _asm_write_dr1]
[GLOBAL _asm_write_dr2]
[GLOBAL _asm_write_dr3]
[GLOBAL _asm_write_dr6]
[GLOBAL _asm_write_dr7]

[GLOBAL _asm_read_cr0]
[GLOBAL _asm_write_cr0]
[GLOBAL _asm_read_cr4]
[GLOBAL _asm_write_cr4]
[GLOBAL _asm_read_eflags]
[GLOBAL _asm_write_eflags]

[GLOBAL _asm_rdmsr_lohi]
[GLOBAL _asm_wrmsr_lohi]
[GLOBAL _asm_rdpmc]
[GLOBAL _asm_rdtsc_serialized]

[GLOBAL _asm_set_trap_flag]
[GLOBAL _asm_clear_trap_flag]
[GLOBAL _asm_arm_hardware_breakpoint]
[GLOBAL _asm_disarm_hardware_breakpoint]

; -------------------------------------------------------------------------
; 1. CPU Debug Registers DR0 - DR7 Direct Access
; -------------------------------------------------------------------------

_asm_read_dr0:
    mov eax, dr0
    ret

_asm_read_dr1:
    mov eax, dr1
    ret

_asm_read_dr2:
    mov eax, dr2
    ret

_asm_read_dr3:
    mov eax, dr3
    ret

_asm_read_dr6:
    mov eax, dr6
    ret

_asm_read_dr7:
    mov eax, dr7
    ret

_asm_write_dr0:
    mov eax, [esp + 4]
    mov dr0, eax
    ret

_asm_write_dr1:
    mov eax, [esp + 4]
    mov dr1, eax
    ret

_asm_write_dr2:
    mov eax, [esp + 4]
    mov dr2, eax
    ret

_asm_write_dr3:
    mov eax, [esp + 4]
    mov dr3, eax
    ret

_asm_write_dr6:
    mov eax, [esp + 4]
    mov dr6, eax
    ret

_asm_write_dr7:
    mov eax, [esp + 4]
    mov dr7, eax
    ret

; -------------------------------------------------------------------------
; 2. CPU Control Registers CR0, CR4, EFLAGS
; -------------------------------------------------------------------------

_asm_read_cr0:
    mov eax, cr0
    ret

_asm_write_cr0:
    mov eax, [esp + 4]
    mov cr0, eax
    ret

_asm_read_cr4:
    mov eax, cr4
    ret

_asm_write_cr4:
    mov eax, [esp + 4]
    mov cr4, eax
    ret

_asm_read_eflags:
    pushfd
    pop eax
    ret

_asm_write_eflags:
    mov eax, [esp + 4]
    push eax
    popfd
    ret

; -------------------------------------------------------------------------
; 3. MSRs & Performance Monitoring Counters (RDPMC / RDMSR / WRMSR)
; -------------------------------------------------------------------------

; void asm_rdmsr_lohi(uint32_t msr, uint32_t* lo, uint32_t* hi)
_asm_rdmsr_lohi:
    push ebp
    mov ebp, esp
    push ebx

    mov ecx, [ebp + 8]          ; MSR Index
    rdmsr                       ; EAX = Low 32-bit, EDX = High 32-bit

    mov ebx, [ebp + 12]         ; lo pointer
    test ebx, ebx
    jz .skip_lo
    mov [ebx], eax

.skip_lo:
    mov ebx, [ebp + 16]         ; hi pointer
    test ebx, ebx
    jz .skip_hi
    mov [ebx], edx

.skip_hi:
    pop ebx
    mov esp, ebp
    pop ebp
    ret

; void asm_wrmsr_lohi(uint32_t msr, uint32_t lo, uint32_t hi)
_asm_wrmsr_lohi:
    push ebp
    mov ebp, esp

    mov ecx, [ebp + 8]          ; MSR Index
    mov eax, [ebp + 12]         ; Low 32-bit
    mov edx, [ebp + 16]         ; High 32-bit
    wrmsr

    mov esp, ebp
    pop ebp
    ret

; uint64_t asm_rdpmc(uint32_t counter_idx)
_asm_rdpmc:
    mov ecx, [esp + 4]
    rdpmc                       ; Returns in EDX:EAX (64-bit return)
    ret

; uint64_t asm_rdtsc_serialized(void)
_asm_rdtsc_serialized:
    push ebx
    xor eax, eax
    cpuid                       ; Serializing instruction
    rdtsc                       ; Returns TSC in EDX:EAX
    pop ebx
    ret

; -------------------------------------------------------------------------
; 4. Single-Step Trap Flag (EFLAGS.TF) Control
; -------------------------------------------------------------------------

_asm_set_trap_flag:
    pushfd
    pop eax
    or eax, (1 << 8)            ; Set Trap Flag (TF) Bit 8
    push eax
    popfd
    ret

_asm_clear_trap_flag:
    pushfd
    pop eax
    and eax, ~(1 << 8)          ; Clear Trap Flag
    push eax
    popfd
    ret

; -------------------------------------------------------------------------
; 5. Hardware Silicon Breakpoint Configurator
; -------------------------------------------------------------------------

; void asm_arm_hardware_breakpoint(uint32_t bp_idx, uint32_t addr, uint32_t condition, uint32_t length_code)
_asm_arm_hardware_breakpoint:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov edx, [ebp + 8]          ; bp_idx (0-3)
    mov eax, [ebp + 12]         ; addr
    mov ebx, [ebp + 16]         ; condition (0=exec, 1=write, 3=readwrite)

    ; 1. Store Address in DR0 - DR3
    cmp edx, 0
    jne .check1
    mov dr0, eax
    jmp .update_dr7

.check1:
    cmp edx, 1
    jne .check2
    mov dr1, eax
    jmp .update_dr7

.check2:
    cmp edx, 2
    jne .check3
    mov dr2, eax
    jmp .update_dr7

.check3:
    cmp edx, 3
    jne .done
    mov dr3, eax

.update_dr7:
    ; 2. Configure DR7 Control Register
    mov eax, dr7

    ; Enable Local Breakpoint Bit: (1 << (bp_idx * 2))
    mov cl, dl
    shl cl, 1                   ; bp_idx * 2
    mov esi, 1
    shl esi, cl
    or eax, esi

    ; Clear Condition & Length field: (0x0F << (16 + (bp_idx * 4)))
    mov cl, dl
    shl cl, 2                   ; bp_idx * 4
    add cl, 16                  ; 16 + bp_idx * 4
    mov esi, 0x0F
    shl esi, cl
    not esi
    and eax, esi

    ; Set Condition & Length field: ((len << 2) | cond) << (16 + (bp_idx * 4))
    and ebx, 0x03               ; Cond: 2 bits
    mov edi, [ebp + 20]         ; Length code: 2 bits
    and edi, 0x03
    shl edi, 2
    or ebx, edi                 ; 4-bit config: (len << 2) | cond
    shl ebx, cl
    or eax, ebx

    mov dr7, eax

.done:
    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret

; void asm_disarm_hardware_breakpoint(uint32_t bp_idx)
_asm_disarm_hardware_breakpoint:
    push ebp
    mov ebp, esp
    push esi

    mov edx, [ebp + 8]          ; bp_idx (0-3)

    ; Clear Address
    xor eax, eax
    cmp edx, 0
    jne .c1
    mov dr0, eax
    jmp .clr_dr7
.c1:
    cmp edx, 1
    jne .c2
    mov dr1, eax
    jmp .clr_dr7
.c2:
    cmp edx, 2
    jne .c3
    mov dr2, eax
    jmp .clr_dr7
.c3:
    cmp edx, 3
    jne .done
    mov dr3, eax

.clr_dr7:
    mov eax, dr7
    mov cl, dl
    shl cl, 1
    mov esi, 3
    shl esi, cl
    not esi
    and eax, esi                ; Disable Local & Global bits for this BP
    mov dr7, eax

.done:
    pop esi
    mov esp, ebp
    pop ebp
    ret
