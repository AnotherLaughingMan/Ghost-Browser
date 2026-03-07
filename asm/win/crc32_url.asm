; Ghost Browser — x64 MASM (Windows / Microsoft x64 ABI)
;
; Routine : ghost_crc32c_asm
; Purpose : Hardware CRC32C (Castagnoli polynomial 0x1EDC6F41) using the
;           SSE4.2 CRC32 instruction.  Used for URL string hashing so that
;           HistoryManager can deduplicate visits in O(1) with a hash set
;           instead of an O(n) linear scan.
;
; Signature:
;   uint32_t ghost_crc32c_asm(const uint8_t* data, size_t len)
;   rcx = data  (pointer; may be unaligned — CRC32 handles unaligned reads)
;   rdx = len   (byte count)
;   rax = CRC32C result (32-bit, zero-extended to 64-bit)
;
; Requires: SSE4.2 (Intel Sandy Bridge / AMD Bulldozer, ~2011+)
; Clobbers: rax, rcx, rdx  (all caller-saved per Microsoft ABI; no XMM used)
; This is a leaf function — no stack frame needed.
;
; Algorithm:
;   seed = 0xFFFFFFFF
;   Process 8 bytes/iter (CRC32 r64, m64) while len >= 8
;   Process 4 bytes/iter (CRC32 r32, m32) while len >= 4
;   Process 1 byte/iter  (CRC32 r32, m8)  for tail
;   return ~crc (final XOR 0xFFFFFFFF)
;
; C++ fallback: src/core/asm_fallbacks.cpp :: ghost::crc32c_sw()

.code

ghost_crc32c_asm PROC
    ; ── Seed ─────────────────────────────────────────────────────────────────
    xor     eax, eax
    not     eax                         ; eax = 0xFFFFFFFF (CRC32C seed)

    ; ── Zero-length guard ─────────────────────────────────────────────────
    test    rdx, rdx
    jz      done

    ; ── 8-byte (qword) loop ──────────────────────────────────────────────
    cmp     rdx, 8
    jb      tail4

qword_loop:
    crc32   rax, qword ptr [rcx]
    add     rcx, 8
    sub     rdx, 8
    cmp     rdx, 8
    jae     qword_loop

    ; ── 4-byte (dword) tail ──────────────────────────────────────────────
tail4:
    cmp     rdx, 4
    jb      byte_tail

    crc32   eax, dword ptr [rcx]
    add     rcx, 4
    sub     rdx, 4

    ; ── 1-byte tail ──────────────────────────────────────────────────────
byte_tail:
    test    rdx, rdx
    jz      done

byte_loop:
    crc32   eax, byte ptr [rcx]
    inc     rcx
    dec     rdx
    jnz     byte_loop

done:
    ; Final XOR — standard CRC32C finalisation
    not     eax
    ret
ghost_crc32c_asm ENDP

; ─────────────────────────────────────────────────────────────────────────────
; Routine : ghost_prefetch_range_asm
; Purpose : Software prefetch a contiguous byte range into L1/L2 cache using
;           PREFETCHT0 (prefetch to all cache levels).  Called before reading
;           large JSON buffers (history.json, settings.json) so cache lines
;           are hot when the Qt JSON parser iterates the data.
;
; Signature:
;   void ghost_prefetch_range_asm(const uint8_t* data, size_t len)
;   rcx = data pointer
;   rdx = byte count
;
; Notes:
;   Cache line size on Windows x64 targets is 64 bytes.
;   We step 64 bytes per PREFETCHT0 — exactly one cache line per issue.
;   The CPU reorder buffer will hide the ~4 cycle issue latency.
;   Non-faulting: PREFETCHT0 to unmapped addresses is a no-op.

ghost_prefetch_range_asm PROC
    test    rdx, rdx
    jz      pf_done

    ; Walk the range in 64-byte strides (one cache line per PREFETCHT0).
    ; We check BEFORE decrementing so rdx never underflows — the previous
    ; implementation used "sub rdx,64 ; ja" which wraps unsigned on the
    ; final partial stride and loops for 2^58 iterations.
pf_loop:
    cmp     rdx, 64
    jb      pf_tail              ; fewer than 64 bytes remain
    prefetcht0 [rcx]
    add     rcx, 64
    sub     rdx, 64
    jmp     pf_loop

pf_tail:
    ; Prefetch the final partial cache line (non-faulting if partially mapped).
    prefetcht0 [rcx]

pf_done:
    ret
ghost_prefetch_range_asm ENDP

END
