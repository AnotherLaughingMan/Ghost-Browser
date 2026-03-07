; Ghost Browser — x64 Assembly (MASM / Windows)
; Calling convention: Microsoft x64 (rcx, rdx, r8, r9 + stack)
;
; Example: fast_memset_u32 — fills a 32-bit aligned buffer with a constant.
; C++ fallback: src/core/asm_fallbacks.cpp :: ghost_memset_u32()
;
; This is a placeholder demonstrating the build pipeline.
; Real routines added only when profiling justifies them.

.code

; void ghost_memset_u32_asm(uint32_t* dst, uint32_t value, size_t count)
;   rcx = dst (aligned pointer)
;   edx = value (32-bit fill value)
;   r8  = count (number of uint32_t elements)
ghost_memset_u32_asm PROC
    test    r8, r8
    jz      done
    movd    xmm0, edx
    pshufd  xmm0, xmm0, 0          ; broadcast value to all 4 lanes

    ; Check if we can do 16-byte aligned stores
    cmp     r8, 4
    jb      scalar_loop

vector_loop:
    movdqa  [rcx], xmm0
    add     rcx, 16
    sub     r8, 4
    cmp     r8, 4
    jae     vector_loop

scalar_loop:
    test    r8, r8
    jz      done
    mov     [rcx], edx
    add     rcx, 4
    dec     r8
    jnz     scalar_loop

done:
    ret
ghost_memset_u32_asm ENDP

END
