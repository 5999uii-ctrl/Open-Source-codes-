; =========================================================================
; A.A OS - Pure x86 32-Bit Assembly Task Switching Module
; Cooperative Yield Gate & Task Trampoline Wrapper
; =========================================================================

[BITS 32]
section .text

[GLOBAL _asm_task_yield_gate]
[GLOBAL asm_task_yield_gate]
[GLOBAL _task_entry_wrapper]
[GLOBAL task_entry_wrapper]
[EXTERN _c_task_yield_handler]
[EXTERN _task_run_entry]
[EXTERN _task_exit]

; -------------------------------------------------------------------------
; asm_task_yield_gate:
; Simulates an interrupt stack frame (EFLAGS, CS, EIP) followed by
; general registers (pusha) and segment registers, then invokes the C
; scheduler yield handler to switch context cooperatively.
; -------------------------------------------------------------------------
align 4
_asm_task_yield_gate:
asm_task_yield_gate:
    ; 1. Forge interrupt stack frame so context matches struct irq_context exactly
    pushfd
    push dword 0x08             ; Kernel Code Segment Selector (CS)
    push dword .yield_return    ; Return address for iret to pop into EIP

    ; 2. Push general registers (EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX)
    pusha

    ; 3. Push segment registers
    push ds
    push es
    push fs
    push gs

    ; 4. Set kernel data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld

    ; 5. Invoke scheduler with pointer to current irq_context
    push esp
    call _c_task_yield_handler
    ; Switch to returned stack pointer (new or same task)
    mov esp, eax

    ; 6. Restore segment registers from target task stack
    pop gs
    pop fs
    pop es
    pop ds

    ; 7. Restore general registers
    popa

    ; 8. Return via iret (pops EIP=.yield_return, CS=0x08, EFLAGS)
    iret

.yield_return:
    ret

; -------------------------------------------------------------------------
; task_entry_wrapper:
; Initial execution trampoline for freshly spawned tasks.
; Switches CPU into task function with interrupts enabled, and ensures
; graceful task termination if the entry function ever returns.
; -------------------------------------------------------------------------
align 4
_task_entry_wrapper:
task_entry_wrapper:
    ; Task starts here with EFLAGS.IF=1 (interrupts enabled)
    call _task_run_entry
    ; If entry function returns, terminate gracefully
    call _task_exit

.hang_loop:
    sti
    hlt
    jmp .hang_loop
