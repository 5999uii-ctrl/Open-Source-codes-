; =========================================================================
; A.A OS - Dedicated Physical Memory Management Assembly Engine
; Direct Silicon RAM Manipulation, Bitwise Transformations, Hardware Paging,
; Physical Bitmap Allocator, 5-Pass Wash, Range Flush, Search & RDTSC Latency
; =========================================================================

[BITS 32]

section .text

; -------------------------------------------------------------------------
; Exported Global Symbols
; -------------------------------------------------------------------------
[GLOBAL _asm_mem_copy_32]
[GLOBAL _asm_mem_move_32]
[GLOBAL _asm_mem_fill_32]
[GLOBAL _asm_mem_zero_32]
[GLOBAL _asm_mem_cmp_32]
[GLOBAL _asm_mem_chr_32]
[GLOBAL _asm_mem_xor_mask]
[GLOBAL _asm_mem_and_mask]
[GLOBAL _asm_mem_or_mask]
[GLOBAL _asm_mem_not_mask]
[GLOBAL _asm_mem_reverse_endian_32]
[GLOBAL _asm_mem_search_byte]
[GLOBAL _asm_mem_search_dword]
[GLOBAL _asm_mem_count_byte]
[GLOBAL _asm_dram_5pass_wash]
[GLOBAL _asm_mem_benchmark_latency]
[GLOBAL _asm_mem_flush_range]
[GLOBAL _asm_mem_invlpg_range]
[GLOBAL _asm_mem_enable_pse]
[GLOBAL _asm_mem_enable_global_pages]
[GLOBAL _asm_paging_enable]
[GLOBAL _asm_paging_disable]
[GLOBAL _asm_load_page_directory]
[GLOBAL _asm_read_cr3]
[GLOBAL _asm_read_cr2]
[GLOBAL _asm_bitmap_set_bit]
[GLOBAL _asm_bitmap_clear_bit]
[GLOBAL _asm_bitmap_test_bit]
[GLOBAL _asm_bitmap_find_first_free]

; -------------------------------------------------------------------------
; 1. High-Speed Block Memory Operations
; -------------------------------------------------------------------------

; void asm_mem_copy_32(void* dest, const void* src, uint32_t count)
_asm_mem_copy_32:
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
    shr ecx, 2           ; 32-bit dwords count
    rep movsd
    mov ecx, edx
    and ecx, 3           ; Remaining bytes (0-3)
    rep movsb

.done:
    pop ecx
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret

; void asm_mem_move_32(void* dest, const void* src, uint32_t count)
_asm_mem_move_32:
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

    cmp edi, esi
    jb .forward
    je .done

    ; Overlapping buffers (dest > src): reverse copy using STD
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

; void asm_mem_fill_32(void* dest, uint8_t byte_val, uint32_t count)
_asm_mem_fill_32:
    push ebp
    mov ebp, esp
    push edi
    push ecx

    mov edi, [ebp + 8]   ; dest
    movzx eax, byte [ebp + 12] ; byte_val
    mov ecx, [ebp + 16]  ; count
    test ecx, ecx
    jz .done

    ; Broadcast byte to all 4 bytes of EAX
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

; void asm_mem_zero_32(void* dest, uint32_t count)
_asm_mem_zero_32:
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

; int asm_mem_cmp_32(const void* s1, const void* s2, uint32_t count)
_asm_mem_cmp_32:
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

; void* asm_mem_chr_32(const void* src, uint8_t byte_target, uint32_t count)
_asm_mem_chr_32:
    push ebp
    mov ebp, esp
    push edi
    push ecx

    mov edi, [ebp + 8]
    movzx eax, byte [ebp + 12]
    mov ecx, [ebp + 16]
    test ecx, ecx
    jz .not_found

    cld
    repne scasb
    jne .not_found

    dec edi
    mov eax, edi
    jmp .done

.not_found:
    xor eax, eax

.done:
    pop ecx
    pop edi
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 2. Silicon Direct Memory Bitwise Transformations
; -------------------------------------------------------------------------

; void asm_mem_xor_mask(void* addr, uint32_t mask, uint32_t count_bytes)
_asm_mem_xor_mask:
    push ebp
    mov ebp, esp
    push esi
    push edi
    push ebx
    push ecx

    mov edi, [ebp + 8]
    mov esi, edi
    mov ebx, [ebp + 12]
    mov ecx, [ebp + 16]
    test ecx, ecx
    jz .done

    cld
    mov edx, ecx
    shr ecx, 2
    jz .bytes

.dwords:
    lodsd
    xor eax, ebx
    stosd
    dec ecx
    jnz .dwords

.bytes:
    mov ecx, edx
    and ecx, 3
    jz .done

.byte_loop:
    lodsb
    xor al, bl
    stosb
    dec ecx
    jnz .byte_loop

.done:
    pop ecx
    pop ebx
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret

; void asm_mem_and_mask(void* addr, uint32_t mask, uint32_t count_bytes)
_asm_mem_and_mask:
    push ebp
    mov ebp, esp
    push esi
    push edi
    push ebx
    push ecx

    mov edi, [ebp + 8]
    mov esi, edi
    mov ebx, [ebp + 12]
    mov ecx, [ebp + 16]
    test ecx, ecx
    jz .done

    cld
    mov edx, ecx
    shr ecx, 2
    jz .bytes

.dwords:
    lodsd
    and eax, ebx
    stosd
    dec ecx
    jnz .dwords

.bytes:
    mov ecx, edx
    and ecx, 3
    jz .done

.byte_loop:
    lodsb
    and al, bl
    stosb
    dec ecx
    jnz .byte_loop

.done:
    pop ecx
    pop ebx
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret

; void asm_mem_or_mask(void* addr, uint32_t mask, uint32_t count_bytes)
_asm_mem_or_mask:
    push ebp
    mov ebp, esp
    push esi
    push edi
    push ebx
    push ecx

    mov edi, [ebp + 8]
    mov esi, edi
    mov ebx, [ebp + 12]
    mov ecx, [ebp + 16]
    test ecx, ecx
    jz .done

    cld
    mov edx, ecx
    shr ecx, 2
    jz .bytes

.dwords:
    lodsd
    or eax, ebx
    stosd
    dec ecx
    jnz .dwords

.bytes:
    mov ecx, edx
    and ecx, 3
    jz .done

.byte_loop:
    lodsb
    or al, bl
    stosb
    dec ecx
    jnz .byte_loop

.done:
    pop ecx
    pop ebx
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret

; void asm_mem_not_mask(void* addr, uint32_t count_bytes)
_asm_mem_not_mask:
    push ebp
    mov ebp, esp
    push esi
    push edi
    push ecx

    mov edi, [ebp + 8]
    mov esi, edi
    mov ecx, [ebp + 12]
    test ecx, ecx
    jz .done

    cld
    mov edx, ecx
    shr ecx, 2
    jz .bytes

.dwords:
    lodsd
    not eax
    stosd
    dec ecx
    jnz .dwords

.bytes:
    mov ecx, edx
    and ecx, 3
    jz .done

.byte_loop:
    lodsb
    not al
    stosb
    dec ecx
    jnz .byte_loop

.done:
    pop ecx
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret

; void asm_mem_reverse_endian_32(void* addr, uint32_t count_dwords)
_asm_mem_reverse_endian_32:
    push ebp
    mov ebp, esp
    push esi
    push edi
    push ecx

    mov edi, [ebp + 8]
    mov esi, edi
    mov ecx, [ebp + 12]
    test ecx, ecx
    jz .done

    cld
.loop:
    lodsd
    bswap eax            ; x86 Byte-Swap instruction (Endianness reversal)
    stosd
    dec ecx
    jnz .loop

.done:
    pop ecx
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 3. Advanced Physical Memory Search & Frequency Count
; -------------------------------------------------------------------------

; uint32_t asm_mem_search_byte(uint32_t start_addr, uint32_t end_addr, uint8_t target)
_asm_mem_search_byte:
    push ebp
    mov ebp, esp
    push edi
    push ecx

    mov edi, [ebp + 8]
    mov edx, [ebp + 12]
    movzx eax, byte [ebp + 16]

    cmp edi, edx
    jae .not_found

    mov ecx, edx
    sub ecx, edi

    cld
    repne scasb
    jne .not_found

    dec edi
    mov eax, edi
    jmp .done

.not_found:
    xor eax, eax

.done:
    pop ecx
    pop edi
    mov esp, ebp
    pop ebp
    ret

; uint32_t asm_mem_search_dword(uint32_t start_addr, uint32_t end_addr, uint32_t target)
_asm_mem_search_dword:
    push ebp
    mov ebp, esp
    push edi
    push ecx

    mov edi, [ebp + 8]
    mov edx, [ebp + 12]
    mov eax, [ebp + 16]

    cmp edi, edx
    jae .not_found

    mov ecx, edx
    sub ecx, edi
    shr ecx, 2
    jz .not_found

    cld
    repne scasd
    jne .not_found

    sub edi, 4
    mov eax, edi
    jmp .done

.not_found:
    xor eax, eax

.done:
    pop ecx
    pop edi
    mov esp, ebp
    pop ebp
    ret

; uint32_t asm_mem_count_byte(uint32_t start_addr, uint32_t length_bytes, uint8_t target)
_asm_mem_count_byte:
    push ebp
    mov ebp, esp
    push esi
    push ebx
    push ecx

    mov esi, [ebp + 8]   ; start_addr
    mov ecx, [ebp + 12]  ; length_bytes
    movzx ebx, byte [ebp + 16] ; target
    xor eax, eax         ; Match counter

    test ecx, ecx
    jz .done

.loop:
    mov dl, [esi]
    cmp dl, bl
    jne .skip
    inc eax
.skip:
    inc esi
    dec ecx
    jnz .loop

.done:
    pop ecx
    pop ebx
    pop esi
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 4. Master DRAM 5-Pass Wash & Capacitor Retention Sanitizer
; -------------------------------------------------------------------------

; void asm_dram_5pass_wash(uint32_t start_addr, uint32_t length_bytes)
_asm_dram_5pass_wash:
    push ebp
    mov ebp, esp
    push esi
    push edi
    push ebx
    push ecx

    mov ebx, [ebp + 8]
    mov ecx, [ebp + 12]
    test ecx, ecx
    jz .done

    shr ecx, 2

    ; PASS 1: All Zeros (0x00000000)
    mov edi, ebx
    mov edx, ecx
    xor eax, eax
    cld
    rep stosd
    wbinvd

    ; PASS 2: All Ones (0xFFFFFFFF)
    mov edi, ebx
    mov ecx, edx
    mov eax, 0xFFFFFFFF
    rep stosd
    wbinvd

    ; PASS 3: Alternating Pattern (0xAAAAAAAA)
    mov edi, ebx
    mov ecx, edx
    mov eax, 0xAAAAAAAA
    rep stosd
    wbinvd

    ; PASS 4: Inverted Pattern (0x55555555)
    mov edi, ebx
    mov ecx, edx
    mov eax, 0x55555555
    rep stosd
    wbinvd

    ; PASS 5: Calibration (0xAA55AA55)
    mov edi, ebx
    mov ecx, edx
    mov eax, 0xAA55AA55
    rep stosd
    wbinvd

.done:
    pop ecx
    pop ebx
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 5. High-Precision RDTSC Physical Memory Latency Benchmark (Bug Fixed)
; -------------------------------------------------------------------------

; void asm_mem_benchmark_latency(uint32_t addr, uint32_t* out_read_cycles, uint32_t* out_write_cycles)
_asm_mem_benchmark_latency:
    push ebp
    mov ebp, esp
    sub esp, 8           ; Local variables for Start TSC [ebp - 4] and End TSC [ebp - 8]
    push esi
    push edi
    push ebx

    mov esi, [ebp + 8]

    ; 10,000 Reads with RDTSC
    xor eax, eax
    cpuid
    rdtsc
    mov [ebp - 4], eax   ; Save Start TSC safely on stack

    mov ecx, 10000
.read_loop:
    mov edx, [esi]
    dec ecx
    jnz .read_loop

    xor eax, eax
    cpuid
    rdtsc
    sub eax, [ebp - 4]   ; EAX = Elapsed TSC cycles (End - Start)
    xor edx, edx
    mov ecx, 10000
    div ecx              ; Average cycles per read

    mov edi, [ebp + 12]
    test edi, edi
    jz .bench_writes
    mov [edi], eax

.bench_writes:
    ; 10,000 Writes with RDTSC
    xor eax, eax
    cpuid
    rdtsc
    mov [ebp - 4], eax   ; Save Start TSC

    mov ecx, 10000
.write_loop:
    mov [esi], ecx
    dec ecx
    jnz .write_loop

    xor eax, eax
    cpuid
    rdtsc
    sub eax, [ebp - 4]
    xor edx, edx
    mov ecx, 10000
    div ecx              ; Average cycles per write

    mov edi, [ebp + 16]
    test edi, edi
    jz .finish
    mov [edi], eax

.finish:
    pop ebx
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret

; -------------------------------------------------------------------------
; 6. Cache Line Eviction & Page Invalidation
; -------------------------------------------------------------------------

; void asm_mem_flush_range(uint32_t start_addr, uint32_t length_bytes)
_asm_mem_flush_range:
    push ebp
    mov ebp, esp
    push eax
    push ecx

    mov eax, [ebp + 8]
    mov ecx, [ebp + 12]
    test ecx, ecx
    jz .done

.loop:
    clflush [eax]        ; Flush 64-byte cache line
    add eax, 64
    sub ecx, 64
    ja .loop

.done:
    pop ecx
    pop eax
    mov esp, ebp
    pop ebp
    ret

; void asm_mem_invlpg_range(uint32_t start_addr, uint32_t count_pages)
_asm_mem_invlpg_range:
    push ebp
    mov ebp, esp
    push eax
    push ecx

    mov eax, [ebp + 8]
    mov ecx, [ebp + 12]
    test ecx, ecx
    jz .done

.loop:
    invlpg [eax]         ; Invalidate TLB entry for 4KB page
    add eax, 4096
    dec ecx
    jnz .loop

.done:
    pop ecx
    pop eax
    mov esp, ebp
    pop ebp
    ret

; void asm_mem_enable_pse(void)
_asm_mem_enable_pse:
    mov eax, cr4
    or eax, (1 << 4)     ; CR4.PSE = 4MB Pages
    mov cr4, eax
    ret

; void asm_mem_enable_global_pages(void)
_asm_mem_enable_global_pages:
    mov eax, cr4
    or eax, (1 << 7)     ; CR4.PGE = Page Global Enable
    mov cr4, eax
    ret

; -------------------------------------------------------------------------
; 7. Hardware Paging & CR3 Master Control
; -------------------------------------------------------------------------

; void asm_load_page_directory(uint32_t page_dir_phys)
_asm_load_page_directory:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    mov cr3, eax         ; Load physical address of Page Directory into CR3
    mov esp, ebp
    pop ebp
    ret

; void asm_paging_enable(void)
_asm_paging_enable:
    mov eax, cr0
    or eax, 0x80000000   ; Set CR0.PG (bit 31)
    mov cr0, eax
    jmp .flush_pipeline  ; Serializing jump
.flush_pipeline:
    ret

; void asm_paging_disable(void)
_asm_paging_disable:
    mov eax, cr0
    and eax, 0x7FFFFFFF  ; Clear CR0.PG
    mov cr0, eax
    jmp .flush_pipeline
.flush_pipeline:
    ret

; uint32_t asm_read_cr3(void)
_asm_read_cr3:
    mov eax, cr3
    ret

; uint32_t asm_read_cr2(void)
_asm_read_cr2:
    mov eax, cr2         ; Read Page Fault Linear Address
    ret

; -------------------------------------------------------------------------
; 8. Physical Memory Bitmap Allocator Helpers (Bug Fixed with Remainder)
; -------------------------------------------------------------------------

; void asm_bitmap_set_bit(uint32_t* bitmap, uint32_t bit_index)
_asm_bitmap_set_bit:
    push ebp
    mov ebp, esp
    mov edx, [ebp + 8]   ; bitmap pointer
    mov ecx, [ebp + 12]  ; bit_index
    bts [edx], ecx       ; x86 Bit Test and Set
    mov esp, ebp
    pop ebp
    ret

; void asm_bitmap_clear_bit(uint32_t* bitmap, uint32_t bit_index)
_asm_bitmap_clear_bit:
    push ebp
    mov ebp, esp
    mov edx, [ebp + 8]
    mov ecx, [ebp + 12]
    btr [edx], ecx       ; x86 Bit Test and Reset
    mov esp, ebp
    pop ebp
    ret

; int asm_bitmap_test_bit(const uint32_t* bitmap, uint32_t bit_index)
_asm_bitmap_test_bit:
    push ebp
    mov ebp, esp
    mov edx, [ebp + 8]
    mov ecx, [ebp + 12]
    bt [edx], ecx        ; x86 Bit Test
    jc .is_set
    xor eax, eax
    mov esp, ebp
    pop ebp
    ret
.is_set:
    mov eax, 1
    mov esp, ebp
    pop ebp
    ret

; int asm_bitmap_find_first_free(const uint32_t* bitmap, uint32_t max_bits)
_asm_bitmap_find_first_free:
    push ebp
    mov ebp, esp
    push esi
    push ebx

    mov esi, [ebp + 8]   ; bitmap
    mov ecx, [ebp + 12]  ; max_bits
    test ecx, ecx
    jz .not_found

    mov edx, ecx
    shr edx, 5           ; full dwords (32 bits each)
    xor ebx, ebx         ; dword index

.dword_loop:
    cmp ebx, edx
    jae .check_remainder

    mov eax, [esi + ebx * 4]
    cmp eax, 0xFFFFFFFF  ; All 32 bits allocated?
    jne .found_in_dword

    inc ebx
    jmp .dword_loop

.found_in_dword:
    not eax              ; Invert: 1 represents free bit
    bsf eax, eax         ; x86 Bit Scan Forward
    shl ebx, 5
    add eax, ebx         ; Global bit index
    jmp .done

.check_remainder:
    mov ecx, [ebp + 12]
    and ecx, 31          ; Remainder bits (max_bits % 32)
    jz .not_found

    mov eax, [esi + ebx * 4]
    not eax              ; Free bits are 1s
    ; Mask out bits beyond remainder
    mov edx, 1
    shl edx, cl
    dec edx              ; Mask = (1 << rem) - 1
    and eax, edx
    jz .not_found

    bsf eax, eax
    shl ebx, 5
    add eax, ebx
    jmp .done

.not_found:
    mov eax, -1

.done:
    pop ebx
    pop esi
    mov esp, ebp
    pop ebp
    ret
