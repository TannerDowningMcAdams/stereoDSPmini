#pragma once

#include <cstdint>
#include "frame.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif


// FrameBuffer is a non-owning view of a FloatFrame array
// Holds pointer and size, and indexes like array
// Can be passed by value and read/written in-place
// Usage:
// static FloatFrame frameBufferStorage[kBufferSize];
// FrameBuffer buffer = { frameBufferStorage, kBufferSize };

struct FrameBuffer {

    FloatFrame *data;
    uint16_t    size;

    FloatFrame&       operator[](uint16_t i)       { return data[i]; }
    const FloatFrame& operator[](uint16_t i) const { return data[i]; }

    FloatFrame* begin()             { return data; }
    FloatFrame* end()               { return data + size; }
    
    const FloatFrame* begin() const { return data; }
    const FloatFrame* end()   const { return data + size; }

};