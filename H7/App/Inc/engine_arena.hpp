#pragma once

#include <cstdint>

// Static bump allocators for engine memory (plan §4.4, H9). There is no individual
// free: the host resets both arenas whenever the engine changes, and an engine takes
// its worst case in activate() and never resizes.

static constexpr uint32_t kArenaAlign = 32u;    // one cache line

// Bytes an allocation of `bytes` takes from an arena, for EngineInfo's worst cases.
constexpr uint32_t arenaBytes(uint32_t bytes)
{
    return (bytes + kArenaAlign - 1u) & ~(kArenaAlign - 1u);
}

class Arena {
public:

    void init(uint8_t* base, uint32_t capacity);

    // kArenaAlign aligned, not cleared. nullptr when it does not fit, which the
    // registry's compile-time checks rule out for every engine.
    void* allocate(uint32_t bytes);

    template <typename T>
    T* allocate(uint32_t count) { return static_cast<T*>(allocate(count * sizeof(T))); }

    void reset() { used_ = 0u; }

    uint32_t used()     const { return used_; }
    uint32_t capacity() const { return capacity_; }

private:
    uint8_t* base_     = nullptr;
    uint32_t capacity_ = 0;
    uint32_t used_     = 0;
};

struct EngineArenas
{
    // DTCM, shared with .data, .bss and the stack: small hot state.
    static constexpr uint32_t kFastBytes  = 64u * 1024u;
    // AXI SRAM: delay lines, reverb buffers, model weights.
    static constexpr uint32_t kLargeBytes = 448u * 1024u;

    Arena fast;
    Arena large;

    // Binds both arenas to their static storage.
    void init();
    void reset()
    {
        fast.reset();
        large.reset();
    }
};
