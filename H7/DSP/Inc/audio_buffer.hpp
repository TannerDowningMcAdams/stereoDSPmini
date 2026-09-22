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

    // Deinterleave int32 DMA input into planar float
    void fromInterleaved(const int32_t* src, float scale, uint16_t shift) const
    {
        static_assert(!std::is_const<T>::value, "ConstAudioBuffer is read-only");
        for (uint16_t i = 0; i < size_; i++) {
            left_[i]  = (src[i * 2]     >> shift) * scale;
            right_[i] = (src[i * 2 + 1] >> shift) * scale;
        }
    }

    // Interleave planar float back to int32 DMA output.
    void toInterleaved(int32_t* dst, float scale, uint16_t shift) const
    {
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
        // NaN fails both clamp comparisons, so it has to be caught on its own:
        // static_cast<int32_t>(NaN) is undefined behaviour. One silent sample is
        // the mildest available failure -- a NaN here means a bug upstream, and
        // emitting silence keeps it from becoming a full-scale slam.
        if (sample != sample) { sample = 0.0f; }
        if (sample >  kFullScale) { sample =  kFullScale; }
        if (sample < -kFullScale) { sample = -kFullScale; }
        const uint32_t word = static_cast<uint32_t>(static_cast<int32_t>(sample * scale));
        return static_cast<int32_t>(word << shift);
    }

    T*       left_  = nullptr;
    T*       right_ = nullptr;
    uint16_t size_  = 0;
};

using AudioBuffer      = BasicAudioBuffer<float>;
using ConstAudioBuffer = BasicAudioBuffer<const float>;

} // namespace dsp
