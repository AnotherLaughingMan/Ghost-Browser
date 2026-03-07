# Ghost — x64 Assembly Hot Paths

Platform-specific optimized routines for profiling-justified hot spots.

## Directory Layout

- `win/` — MASM (`.asm`) sources for Windows x64 (Microsoft calling convention)
- `linux/` — GAS (`.s`) sources for Linux x64 (System V AMD64 ABI)
- `macos/` — GAS (`.s`) sources for macOS x64 (System V AMD64 ABI)

## Rules

1. **Every ASM routine MUST have a portable C++ fallback** in `src/core/asm_fallbacks.h/.cpp`.
2. ASM is the opt-in fast path; C++ is the default.
3. Routines must be small, self-contained, and leaf-only (no Qt/framework calls).
4. No heap allocations — pass pre-allocated buffers from C++.
5. Document calling convention, register assumptions, and alignment per routine.
6. Test parity between ASM and C++ fallback outputs.
