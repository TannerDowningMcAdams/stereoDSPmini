#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "frame.hpp"

#ifdef __cplusplus
}
#endif

struct AudioBuffer
{
    FloatFrame *data;
    uint16_t    size;

    FloatFrame&       operator[](uint16_t i)       { return data[i]; }
    const FloatFrame& operator[](uint16_t i) const { return data[i]; }

    FloatFrame* begin()             { return data; }
    FloatFrame* end()               { return data + size; }
    
    const FloatFrame* begin() const { return data; }
    const FloatFrame* end()   const { return data + size; }
};