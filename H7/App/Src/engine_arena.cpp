#include "engine_arena.hpp"

// The CubeMX linker script has no NOLOAD section for AXI SRAM, so the large arena is
// declared %nobits: loadable contents would put 448 KB of zeros into the image. The
// startup code does not clear AXI SRAM, which powers up with random data and ECC words
// (AN5342), so engines clear what they allocate.
#if defined(__arm__)
#define AXI_NOINIT __attribute__((section(".RAM_AXI0,\"aw\",%nobits@")))
#else
#define AXI_NOINIT
#endif

// The fast arena is ordinary .bss, so the linker checks it against RAM with the stack.
alignas(kArenaAlign) static uint8_t fastStorage[EngineArenas::kFastBytes];
alignas(kArenaAlign) AXI_NOINIT static uint8_t largeStorage[EngineArenas::kLargeBytes];

void Arena::init(uint8_t* base, uint32_t capacity)
{
    base_     = base;
    capacity_ = capacity;
    used_     = 0u;
}

void* Arena::allocate(uint32_t bytes)
{
    const uint32_t size = arenaBytes(bytes);
    if (size > capacity_ - used_) { return nullptr; }
    void* block = base_ + used_;
    used_ += size;
    return block;
}

void EngineArenas::init()
{
    fast.init(fastStorage, kFastBytes);
    large.init(largeStorage, kLargeBytes);
}
