#pragma once
#include "dsp.hpp"
#include <cstdint>
#include <type_traits>

namespace dsp {

// Non-owning planar stereo view, like a span: T is float for a writable buffer
// and const float for a read-only one. Writable converts to read-only, never back.
template <typename T>
class BasicAudioBuffer {
public:
    BasicAudioBuffer() = default;

    BasicAudioBuffer(T* left, T* right, uint16_t size)
        : left_(left), right_(right), size_(size) {}

    // AudioBuffer -> ConstAudioBuffer, exactly as float* -> const float*.
    template <typename U, typename = std::enable_if_t<std::is_same<const U, T>::value &&
                                                      !std::is_same<U, T>::value>>
    BasicAudioBuffer(const BasicAudioBuffer<U>& other)
        : left_(other.left()), right_(other.right()), size_(other.size()) {}

    // Constness of the view itself is shallow, as with a span; T alone decides
    // whether the samples can be written.
    T*       left()  const { return left_; }
    T*       right() const { return right_; }
    uint16_t size()  const { return size_; }

    T& leftAt(uint16_t i)  const { return left_[i]; }
    T& rightAt(uint16_t i) const { return right_[i]; }

    void setLeft(uint16_t i, float value) const
    {
        static_assert(!std::is_const<T>::value, "ConstAudioBuffer is read-only");
        left_[i] = value;
    }
    void setRight(uint16_t i, float value) const
    {
        static_assert(!std::is_const<T>::value, "ConstAudioBuffer is read-only");
        right_[i] = value;
    }

    // Check for a valid buffer
    bool valid() const { return left_ && right_ && size_ > 0; }

    // Deinterleave int32 DMA input into planar float. Each word holds a
    // right-aligned sample above padBits of zero fill, so the sample's own sign
    // bit is extended over the fill.
    void fromInterleaved(const int32_t* src, float scale, uint16_t padBits) const
    {
        static_assert(!std::is_const<T>::value, "ConstAudioBuffer is read-only");
        for (uint16_t i = 0; i < size_; i++) {
            left_[i]  = unpackSample(src[i * 2],     padBits) * scale;
            right_[i] = unpackSample(src[i * 2 + 1], padBits) * scale;
        }
    }

    // Interleave planar float back to right-aligned int32 DMA output. The
    // transmitter sends only the low bits, so the sign extension above them is ignored.
    void toInterleaved(int32_t* dst, float scale) const
    {
        for (uint16_t i = 0; i < size_; i++) {
            dst[i * 2]     = packSample(left_[i],  scale);
            dst[i * 2 + 1] = packSample(right_[i], scale);
        }
    }

private:
    // Largest sample magnitude that survives the conversion to a DMA word.
    // This is stmlib's FBIPMAX.
    static constexpr float kFullScale = 0.999985f;

    // Sign-extend a right-aligned sample to the full word; on the M7 this is one
    // SBFX. The left shift goes through uint32_t to avoid undefined behavior.
    static int32_t unpackSample(int32_t word, uint16_t padBits) {
        return static_cast<int32_t>(static_cast<uint32_t>(word) << padBits) >> padBits;
    }

    // Clamp and scale one sample into a DMA word.
    static int32_t packSample(float sample, float scale) {
        // NaN fails both clamp comparisons, so it has to be caught on its own:
        // static_cast<int32_t>(NaN) is undefined behavior.
        if (sample != sample) { sample = 0.0f; }
        if (sample >  kFullScale) { sample =  kFullScale; }
        if (sample < -kFullScale) { sample = -kFullScale; }
        return static_cast<int32_t>(sample * scale);
    }

    T*       left_  = nullptr;
    T*       right_ = nullptr;
    uint16_t size_  = 0;
};

using AudioBuffer      = BasicAudioBuffer<float>;
using ConstAudioBuffer = BasicAudioBuffer<const float>;

} // namespace dsp
