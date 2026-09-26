#pragma once
#include "dsp_math.hpp"
#include <cstdint>

namespace dsp {

// Low-frequency oscillator, bipolar -1..1. Phase is in cycles, 0..1, and every shape
// starts at 0 rising except the square, which starts high, so all of them line up
// with the sine. The shapes are not band-limited: fine for modulation, not for audio.
//
// It steps once per next() call. Init it with the block rate instead of the sample
// rate to step once per block.
class Lfo {
public:

    enum class Shape : uint8_t { Sine, Triangle, SawUp, SawDown, Square };

    struct Config
    {
        uint32_t sampleRate;    // calls to next() per second
        Shape    shape;
        float    frequency;     // Hz
    };

    // Starts at phase 0.
    void init(const Config& config);

    // Clamped to 0..kMaxRatio of the rate. The phase carries on, so a change is
    // continuous.
    void setFrequency(float hz);
    void setShape(Shape shape) { shape_ = shape; }

    // Wrapped into [0, 1). For sync: with the beat phase, the LFO follows the tempo.
    void setPhase(float cycles);

    // The value at the current phase, then one step on.
    float next()
    {
        const float out = shapeAt(shape_, phase_);
        phase_ += increment_;
        if (phase_ >= 1.0f) { phase_ -= 1.0f; }
        return out;
    }

    // Any shape at any phase in [0, 1), without an oscillator: for a modulation
    // that follows an external phase.
    static float shapeAt(Shape shape, float phase)
    {
        switch (shape)
        {
        case Shape::Sine:     return sine(phase);
        case Shape::Triangle: return triangle(phase);
        case Shape::SawUp:    return saw(phase);
        case Shape::SawDown:  return -saw(phase);
        case Shape::Square:   return (phase < 0.5f) ? 1.0f : -1.0f;
        }
        return 0.0f;
    }

    float frequency() const { return hz_; }
    float phase()     const { return phase_; }
    Shape shape()     const { return shape_; }

    static constexpr float kMaxRatio = 0.49f;

private:

    static float triangle(float phase)
    {
        float u = phase + 0.25f;
        if (u >= 1.0f) { u -= 1.0f; }
        const float d = u - 0.5f;
        return 1.0f - 4.0f * ((d < 0.0f) ? -d : d);
    }

    // The triangle already folds the phase into the quarter-wave range.
    static float sine(float phase) { return sinHalfPi(triangle(phase)); }

    static float saw(float phase)
    {
        float u = phase + 0.5f;
        if (u >= 1.0f) { u -= 1.0f; }
        return 2.0f * u - 1.0f;
    }

    float rate_      = 48000.0f;
    float hz_        = 0.0f;
    float phase_     = 0.0f;
    float increment_ = 0.0f;
    Shape shape_     = Shape::Sine;
};

} // namespace dsp
