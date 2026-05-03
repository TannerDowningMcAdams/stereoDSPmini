#pragma once
#include "dsp.hpp"
#include <cstdint>
#include <cassert>

namespace dsp {

// Non-owning planar stereo audio buffer.
// Backing storage is managed by the caller (typically static or DMA-adjacent).
// Passed by value between Audio, Processor, and System — cheap, no heap.
class AudioBuffer {
public:
    AudioBuffer() = default;

    AudioBuffer(float* left, float* right, uint16_t size)
        : left_(left), right_(right), size_(size) {}

    float*       left()        { return left_; }
    const float* left()  const { return left_; }
    float*       right()       { return right_; }
    const float* right() const { return right_; }
    uint16_t     size()  const { return size_; }

    // --- Per-sample access ---
    float&       leftAt(uint16_t i)        { return left_[i]; }
    const float& leftAt(uint16_t i)  const { return left_[i]; }
    float&       rightAt(uint16_t i)       { return right_[i]; }
    const float& rightAt(uint16_t i) const { return left_[i]; }

    void setLeft(uint16_t i,  float value) { left_[i]  = value; }
    void setRight(uint16_t i, float value) { right_[i] = value; }

    // --- Validity ---
    bool valid() const { return left_ && right_ && size_ > 0; }

    // Deinterleave int32 DMA input into planar float
    void fromInterleaved(const int32_t* src, float scale, uint16_t shift) {
        for (uint16_t i = 0; i < size_; i++) {
            left_[i]  = (src[i * 2]     >> shift) * scale;
            right_[i] = (src[i * 2 + 1] >> shift) * scale;
        }
    }

    // Interleave planar float back to int32 DMA output.
    void toInterleaved(int32_t* dst, float scale, uint16_t shift) const {
        for (uint16_t i = 0; i < size_; i++) {
            dst[i * 2]     = (static_cast<int32_t>(left_[i]  * scale)) << shift;
            dst[i * 2 + 1] = (static_cast<int32_t>(right_[i] * scale)) << shift;
        }
    }

private:
    float*   left_  = nullptr;
    float*   right_ = nullptr;
    uint16_t size_  = 0;
};

} // namespace dsp
