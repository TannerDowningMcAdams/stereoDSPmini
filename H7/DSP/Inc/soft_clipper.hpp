#pragma once
#include "dsp_math.hpp"
#include "param_ramp.hpp"
#include <cstdint>

namespace dsp {

// Memoryless saturator for one channel: drive into one of the sat:: curves, the
// curve chosen at run time. Every curve has unity slope at zero and saturates at
// +-1, so small signals come out at drive times their level and the output stays
// within +-1, to one float step of rounding, whichever curve is chosen.
//
// The block form picks the curve once per block and ramps the drive across it, so
// a pot or MIDI step in drive does not zipper, and it crossfades a curve change
// across one block, since two curves can differ by 0.1 at the same input. Nothing
// here band-limits: harmonics above Nyquist fold back, more so with more drive and
// with the harder knees.
class SoftClipper {
public:

    enum class Curve : uint8_t
    {
        Tanh,       // std::tanh: the reference, and the slowest
        FastTanh,   // tanh to within 4e-6
        Rational,   // knee at 3
        Sine,       // knee at pi/2
        Cubic,      // knee at 1.5; the cheapest
    };

    struct Config
    {
        Curve curve;
        float drive;        // input gain, and the small-signal gain
    };

    static constexpr float kMinDrive = 0.1f;
    static constexpr float kMaxDrive = 100.0f;      // +40 dB

    // Starts at the configured drive, with no ramp.
    void init(const Config& config);

    // The block form moves to the new curve over the next block.
    void setCurve(Curve curve) { next_ = curve; }
    // Clamped to kMinDrive..kMaxDrive. The block form ramps to it over the next block.
    void setDrive(float drive);

    // One sample at the curve and drive as set, with no ramp or crossfade; for
    // callers without blocks.
    float process(float input) const { return shape(next_, drive_ * input); }

    // output may equal input.
    void process(const float* input, float* output, uint16_t frames);

    // Any curve at any input, without an object.
    static float shape(Curve curve, float x)
    {
        switch (curve)
        {
        case Curve::Tanh:     return sat::Tanh::value(x);
        case Curve::FastTanh: return sat::FastTanh::value(x);
        case Curve::Rational: return sat::Rational::value(x);
        case Curve::Sine:     return sat::Sine::value(x);
        case Curve::Cubic:    return sat::Cubic::value(x);
        }
        return 0.0f;
    }

    Curve curve() const { return next_; }
    float drive() const { return drive_; }

private:

    void crossfade(const float* input, float* output, uint16_t frames);

    Curve     curve_ = Curve::FastTanh;     // the block form's curve
    Curve     next_  = Curve::FastTanh;     // as set
    float     drive_ = 1.0f;                // as set
    ParamRamp ramp_;
};

} // namespace dsp
