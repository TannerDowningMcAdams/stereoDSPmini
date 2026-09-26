#pragma once
#include "dsp_math.hpp"
#include "frac_delay_line.hpp"
#include <cstdint>

namespace dsp {

// Delay-line pitch shifter for one channel: two taps sweep a window of the recent
// input at the rate that gives the new pitch, half a window apart, each faded by a
// smoothstep window that is zero where its tap jumps back. The windows sum to 1.
//
// It has no pitch detection, so it colours sustained tones with a flutter at the
// grain rate, |ratio - 1| * rate / window, and a comb between the two taps. Inside a
// reverb loop, as in FdnReverb's shimmer, both smear into the tail.
//
// The caller owns the buffer (an engine takes it from its arena). The reads are
// written out in process(): at -Og nothing is inlined, and FdnReverb runs four of
// these per sample.
class PitchShifter {
public:

    struct Config
    {
        float*   buffer;
        uint32_t size;          // floats; at least sizeFor(window)
        uint32_t window;        // samples each tap sweeps
        float    semitones;
    };

    static constexpr float kMaxSemitones = 24.0f;

    // Buffer size, in floats, for a window of `window` samples.
    static constexpr uint32_t sizeFor(uint32_t window)
    {
        return FracDelayLine::sizeFor(window + kMinDelay + 1u);
    }

    // Clears the buffer.
    void init(const Config& config);

    // Clamped to +-kMaxSemitones. Costs an exp2f when it changes.
    void setSemitones(float semitones);

    float process(float input)
    {
        pos_++;
        buffer_[pos_ & mask_] = input;

        phase_ += step_;
        if (phase_ < 0.0f)       { phase_ += 1.0f; }
        else if (phase_ >= 1.0f) { phase_ -= 1.0f; }
        float other = phase_ + 0.5f;
        if (other >= 1.0f) { other -= 1.0f; }

        // Smoothstep of a triangle: zero at phase 0 where this tap jumps, one at 0.5
        // where the other one does, and w(p) + w(p + 0.5) = 1.
        const float t = 1.0f - std::fabs(2.0f * phase_ - 1.0f);
        const float w = t * t * (3.0f - 2.0f * t);
        const float a = cubicAt(kMinDelay + phase_ * window_);
        const float b = cubicAt(kMinDelay + other * window_);
        return b + w * (a - b);
    }

    // Writes without reading, for when the output is not wanted: the window stays
    // current, so the output is clean again as soon as process() resumes.
    void feed(float input)
    {
        pos_++;
        buffer_[pos_ & mask_] = input;
    }

    float semitones() const { return semitones_; }
    float ratio()     const { return ratio_; }

private:

    // The cubic read's shortest delay.
    static constexpr uint32_t kMinDelay = 1u;

    // Four-point Lagrange at `delay` samples behind the newest, as
    // FracDelayLine::readCubic. The window keeps delay within the buffer.
    float cubicAt(float delay) const
    {
        const uint32_t whole = static_cast<uint32_t>(delay);
        const float    t     = delay - static_cast<float>(whole);
        const uint32_t i     = pos_ - whole;
        const float xm1 = buffer_[(i + 1u) & mask_];
        const float x0  = buffer_[i & mask_];
        const float x1  = buffer_[(i - 1u) & mask_];
        const float x2  = buffer_[(i - 2u) & mask_];
        const float c1 = x1 - 0.5f * x0 - (1.0f / 3.0f) * xm1 - (1.0f / 6.0f) * x2;
        const float c2 = 0.5f * (xm1 + x1) - x0;
        const float c3 = (1.0f / 6.0f) * (x2 - xm1) + 0.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

    float*   buffer_    = nullptr;
    uint32_t mask_      = 0;
    uint32_t pos_       = 0;        // index of the newest sample, before the mask
    float    window_    = 1.0f;
    float    semitones_ = 0.0f;
    float    ratio_     = 1.0f;
    float    step_      = 0.0f;     // phase per sample: (1 - ratio) / window
    float    phase_     = 0.0f;
};

} // namespace dsp
