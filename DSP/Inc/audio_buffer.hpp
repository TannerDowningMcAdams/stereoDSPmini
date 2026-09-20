#pragma once
#include "dsp.hpp"
#include <cstdint>
#include <cassert>

namespace dsp {

// Non-owning planar stereo audio buffer.
// Backing storage is managed by the caller (typically static or DMA-adjacent).
// Passed by value between Audio, Processor, and System
class AudioBuffer {
public:
    AudioBuffer() = default;

    AudioBuffer(float* left, float* right, uint16_t size)
        : left_(left), right_(right), size_(size) {}

    // Pointer-level buffer access
    float*       left()        { return left_; }
    const float* left()  const { return left_; }
    float*       right()       { return right_; }
    const float* right() const { return right_; }
    uint16_t     size()  const { return size_; }

    // Per-sample buffer access
    float&       leftAt(uint16_t i)        { return left_[i]; }
    const float& leftAt(uint16_t i)  const { return left_[i]; }
    float&       rightAt(uint16_t i)       { return right_[i]; }
    const float& rightAt(uint16_t i) const { return right_[i]; }

    void setLeft(uint16_t i,  float value) { left_[i]  = value; }
    void setRight(uint16_t i, float value) { right_[i] = value; }

    // Check for a valid buffer
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
            dst[i * 2]     = packSample(left_[i],  scale, shift);
            dst[i * 2 + 1] = packSample(right_[i], scale, shift);
        }
    }

private:
    // Largest sample magnitude that survives the conversion to a DMA word.
    // This is stmlib's FBIPMAX.
    static constexpr float kFullScale = 0.999985f;

    // Clamp, scale, and left-align one sample into a DMA word.
    // The shift goes through uint32_t because left-shifting a negative signed
    // value is undefined behaviour.
    static int32_t packSample(float sample, float scale, uint16_t shift) {
        // NaN fails both clamp comparisons, so it is handled here.
        if (sample != sample) { sample = 0.0f; }
        if (sample >  kFullScale) { sample =  kFullScale; }
        if (sample < -kFullScale) { sample = -kFullScale; }
        const uint32_t word = static_cast<uint32_t>(static_cast<int32_t>(sample * scale));
        return static_cast<int32_t>(word << shift);
    }

    float*   left_  = nullptr;
    float*   right_ = nullptr;
    uint16_t size_  = 0;
};

} // namespace dsp
