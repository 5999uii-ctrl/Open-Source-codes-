; =========================================================================
; A.A OS - Master Pure x86 Assembly Silicon Engine (silicon_engine.asm)
; 32-Bit Protected Mode Ring-0 Supercharged Silicon Mastery
; 
; Features implemented 100% in Pure Assembly:
; 1. Hardware CPU Register Snapshotter (All 8 GPRs, 6 Segments, 4 CRs, EFLAGS)
; 2. Serialized RDTSC Instruction Cycle & Silicon Latency Benchmark
; 3. Direct TLB Page Invalidation (INVLPG) & Pipeline Flush (WBINVD / CPUID)
; 4. Pure Hardware Fast Memory Operations (repe cmpsb, rep stosd, rep movsd)
;
; Strict x86 cdecl ABI Compliance (All callee-saved registers preserved)
; =========================================================================

[BITS 32]
section .text

GLOBAL _asm_capture_cpu_snapshot
GLOBAL _asm_measure_instruction_latencies
GLOBAL _asm_flush_tlb_page
GLOBAL _asm_full_pipeline_flush
GLOBAL _asm_pure_memcmp
GLOBAL _asm_pure_memcpy_fast
GLOBAL _asm_pure_memset_dword

; -------------------------------------------------------------------------
; 1. Direct Silicon Hardware CPU Register Snapshotter
; void asm_capture_cpu_snapshot(void* snapshot_struct_76_bytes)
; -------------------------------------------------------------------------
_asm_capture_cpu_snapshot:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov edi, [ebp + 8]          ; Output buffer pointer
    test edi, edi
    jz .done_snap

    ; Save GPRs
    mov [edi + 0],  eax
    mov [edi + 4],  ebx
    mov [edi + 8],  ecx
    mov [edi + 12], edx
    mov [edi + 16], esi
    mov [edi + 20], edi
    mov [edi + 24], ebp
    lea eax, [ebp + 8]
    mov [edi + 28], eax

    ; Save Segments (as 32-bit dwords for clean alignment)
    xor eax, eax
    mov ax, cs
    mov [edi + 32], eax
    mov ax, ds
    mov [edi + 36], eax
    mov ax, ss
    mov [edi + 40], eax
    mov ax, es
    mov [edi + 44], eax
    mov ax, fs
    mov [edi + 48], eax
    mov ax, gs
    mov [edi + 52], eax

    ; Save Control Registers
    mov eax, cr0
    mov [edi + 56], eax
    mov eax, cr2
    mov [edi + 60], eax
    mov eax, cr3
    mov [edi + 64], eax
    mov eax, cr4
    mov [edi + 68], eax

    ; Save EFLAGS
    pushfd
    pop eax
    mov [edi + 72], eax

.done_snap:
    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 2. Serialized RDTSC Instruction Cycle & Latency Benchmark Engine
; void asm_measure_instruction_latencies(uint32_t* results_array_8_elements)
; -------------------------------------------------------------------------
_asm_measure_instruction_latencies:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    ; Local variable for memory write test
    sub esp, 8

    ; [0] NOP Latency
    xor eax, eax
    cpuid
    rdtsc
    push eax                    ; [esp] = start TSC
    nop
    xor eax, eax
    cpuid
    rdtsc
    pop ecx                     ; ecx = start TSC
    sub eax, ecx
    mov edi, [ebp + 8]
    mov [edi + 0], eax

    ; [1] ADD reg, reg Latency
    xor eax, eax
    cpuid
    rdtsc
    push eax
    add edx, ecx
    xor eax, eax
    cpuid
    rdtsc
    pop ecx
    sub eax, ecx
    mov edi, [ebp + 8]
    mov [edi + 4], eax

    ; [2] IMUL reg, reg Latency
    xor eax, eax
    cpuid
    rdtsc
    push eax
    imul edx, ecx
    xor eax, eax
    cpuid
    rdtsc
    pop ecx
    sub eax, ecx
    mov edi, [ebp + 8]
    mov [edi + 8], eax

    ; [3] IDIV reg Latency
    xor eax, eax
    cpuid
    rdtsc
    push eax
    mov eax, 100
    xor edx, edx
    mov ebx, 3
    idiv ebx
    xor eax, eax
    cpuid
    rdtsc
    pop ecx
    sub eax, ecx
    mov edi, [ebp + 8]
    mov [edi + 12], eax

    ; [4] MOV [Physical RAM], reg (Memory Write) Latency
    lea ebx, [ebp - 4]
    xor eax, eax
    cpuid
    rdtsc
    push eax
    mov [ebx], eax
    clflush [ebx]
    xor eax, eax
    cpuid
    rdtsc
    pop ecx
    sub eax, ecx
    mov edi, [ebp + 8]
    mov [edi + 16], eax

    ; [5] IN AL, 0x80 (Port I/O) Latency
    xor eax, eax
    cpuid
    rdtsc
    push eax
    in al, 0x80
    xor eax, eax
    cpuid
    rdtsc
    pop ecx
    sub eax, ecx
    mov edi, [ebp + 8]
    mov [edi + 20], eax

    ; [6] CPUID Latency
    xor eax, eax
    cpuid
    rdtsc
    push eax
    xor eax, eax
    cpuid
    rdtsc
    pop ecx
    sub eax, ecx
    mov edi, [ebp + 8]
    mov [edi + 24], eax

    ; [7] WBINVD Hardware Cache Flush Latency
    xor eax, eax
    cpuid
    rdtsc
    push eax
    wbinvd
    xor eax, eax
    cpuid
    rdtsc
    pop ecx
    sub eax, ecx
    mov edi, [ebp + 8]
    mov [edi + 28], eax

    add esp, 8

    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 3. Direct TLB Page Invalidation
; void asm_flush_tlb_page(uint32_t virtual_addr)
; -------------------------------------------------------------------------
_asm_flush_tlb_page:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    invlpg [eax]
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 4. Full CPU Pipeline & Cache Flush
; void asm_full_pipeline_flush(void)
; -------------------------------------------------------------------------
_asm_full_pipeline_flush:
    push ebp
    mov ebp, esp
    wbinvd
    xor eax, eax
    cpuid
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 5. Pure Assembly Fast Memory Comparator
; int asm_pure_memcmp(const void* s1, const void* s2, uint32_t n)
; -------------------------------------------------------------------------
_asm_pure_memcmp:
    push ebp
    mov ebp, esp
    push esi
    push edi

    mov esi, [ebp + 8]
    mov edi, [ebp + 12]
    mov ecx, [ebp + 16]

    xor eax, eax
    test ecx, ecx
    jz .cmp_match

    cld
    repe cmpsb
    je .cmp_match

    mov eax, 1

.cmp_match:
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 6. Pure Assembly Fast Memory Copy
; void asm_pure_memcpy_fast(void* dest, const void* src, uint32_t count_bytes)
; -------------------------------------------------------------------------
_asm_pure_memcpy_fast:
    push ebp
    mov ebp, esp
    push esi
    push edi

    mov edi, [ebp + 8]
    mov esi, [ebp + 12]
    mov ecx, [ebp + 16]

    test ecx, ecx
    jz .cpy_done

    cld
    mov edx, ecx
    shr ecx, 2
    rep movsd

    mov ecx, edx
    and ecx, 3
    rep movsb

.cpy_done:
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 7. Pure Assembly Dword Memory Set
; void asm_pure_memset_dword(void* dest, uint32_t dword_val, uint32_t dword_count)
; -------------------------------------------------------------------------
_asm_pure_memset_dword:
    push ebp
    mov ebp, esp
    push edi

    mov edi, [ebp + 8]
    mov eax, [ebp + 12]
    mov ecx, [ebp + 16]

    cld
    rep stosd

    pop edi
    mov esp, ebp
    pop ebp
    ret
