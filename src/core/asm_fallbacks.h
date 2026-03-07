#pragma once

// Portable C++ fallbacks for every x64 ASM routine.
// These are the DEFAULT code paths on non-x64 or when ASM is disabled.
// On Windows x64, the real implementations are in asm/win/*.asm .

#include <cstddef>
#include <cstdint>

// ── ASM declarations (Windows x64 MASM, extern "C") ─────────────────────────
#if defined(_WIN64) && !defined(GHOST_NO_ASM)
extern "C" {
    uint32_t ghost_crc32c_asm(const uint8_t *data, size_t len);
    void     ghost_prefetch_range_asm(const uint8_t *data, size_t len);
}
#endif

namespace ghost {

// ── CRC32C (Castagnoli) ───────────────────────────────────────────────────────
// Hardware path: SSE4.2 CRC32 instruction (~1 cycle/8 bytes throughput).
// Software path: byte-at-a-time table lookup (asm_fallbacks.cpp).
//
// Used for URL string hashing in HistoryManager deduplication.
uint32_t crc32c_sw(const uint8_t *data, size_t len);

inline uint32_t crc32c(const uint8_t *data, size_t len)
{
#if defined(_WIN64) && !defined(GHOST_NO_ASM)
    return ghost_crc32c_asm(data, len);
#else
    return crc32c_sw(data, len);
#endif
}

// Convenience overload for Qt byte arrays / string data.
inline uint32_t crc32c(const char *data, size_t len)
{
    return crc32c(reinterpret_cast<const uint8_t *>(data), len);
}

// ── Cache prefetch ────────────────────────────────────────────────────────────
// Warms CPU cache lines for a buffer before a sequential read.
// Hardware path: PREFETCHT0 every 64 bytes.
// Software path: no-op (the read itself will fault the pages in).
inline void prefetch_range(const void *data, size_t len)
{
#if defined(_WIN64) && !defined(GHOST_NO_ASM)
    ghost_prefetch_range_asm(static_cast<const uint8_t *>(data), len);
#else
    (void)data; (void)len;
#endif
}

// ── Fill a 32-bit aligned buffer with a constant value ───────────────────────
// ASM counterpart: asm/win/memset_u32.asm :: ghost_memset_u32_asm
inline void ghost_memset_u32(uint32_t *dst, uint32_t value, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        dst[i] = value;
}

} // namespace ghost
