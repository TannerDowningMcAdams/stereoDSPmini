#pragma once
#include "dsp_math.hpp"
#include <cstdint>

namespace dsp {

// Circular delay line read at fractional delays, for one channel. write() stores one
// sample, and each read returns the signal `delay` samples before the most recent
// write, so any number of taps can read one line. Delays are clamped to the range
// the chosen interpolation can reach inside the buffer.
//
// The caller owns the buffer (an engine takes it from its arena). The line uses the
// largest power of two that fits in it, so the index wraps with a mask.
class FracDelayLine {
public:

    struct Config
    {
        float*   buffer;
        uint32_t size;      // floats; at least 4
    };

    // Buffer size, in floats, for a longest delay of maxDelay samples.
    static constexpr uint32_t sizeFor(uint32_t maxDelay)
    {
        uint32_t size = 4u;
        while (size < maxDelay + kGuard) { size *= 2u; }
        return size;
    }

    // Clears the buffer.
    void init(const Config& config);
    void reset();

    void write(float input)
    {
        write_ = (write_ + 1u) & mask_;
        buffer_[write_] = input;
    }

    // Longest delay any read reaches.
    float maxDelay() const { return maxDelay_; }

    // Delay rounded to whole samples; for fixed taps.
    float readNearest(float delay) const
    {
        delay = clamp(delay, 0.0f, maxDelay_);
        return at(static_cast<uint32_t>(delay + 0.5f));
    }

    // Two points. Rolls off the top octave at half-sample delays, and that roll-off
    // moves with the fraction, so modulation adds a little tremolo in the treble.
    float readLinear(float delay) const
    {
        float frac;
        const uint32_t whole = split(delay, 0.0f, frac);
        const float x0 = at(whole);
        const float x1 = at(whole + 1u);
        return x0 + frac * (x1 - x0);
    }

    // Four-point third-order Lagrange, evaluated as a Farrow structure. The better
    // choice for modulated delays (chorus, vibrato). The newest point sits one
    // sample ahead of the read position, so the shortest delay is 1.
    float readCubic(float delay) const
    {
        float t;
        const uint32_t whole = split(delay, 1.0f, t);
        const float xm1 = at(whole - 1u);
        const float x0  = at(whole);
        const float x1  = at(whole + 1u);
        const float x2  = at(whole + 2u);
        const float c1 = x1 - 0.5f * x0 - (1.0f / 3.0f) * xm1 - (1.0f / 6.0f) * x2;
        const float c2 = 0.5f * (xm1 + x1) - x0;
        const float c3 = (1.0f / 6.0f) * (x2 - xm1) + 0.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

    // First-order allpass interpolation: flat magnitude at every delay, so nothing
    // accumulates when it sits inside a feedback loop (reverb, comb, waveguide). It
    // has state, so each tap owns one, and a fast-moving delay makes it ring briefly;
    // use readCubic for heavy modulation.
    class AllpassTap {
    public:

        // Once per write(): the state assumes the line moved on by one sample.
        float read(const FracDelayLine& line, float delay)
        {
            // The fractional part is kept in [0.5, 1.5), which keeps the coefficient
            // in (-0.2, 0.33] and the pole well inside the unit circle.
            delay = clamp(delay, 0.5f, line.maxDelay_);
            const uint32_t base = static_cast<uint32_t>(delay - 0.5f);
            const float    d    = delay - static_cast<float>(base);
            const float    a    = (1.0f - d) / (1.0f + d);
            state_ = a * line.at(base) + line.at(base + 1u) - a * state_;
            return state_;
        }

        void reset() { state_ = 0.0f; }

    private:
        float state_ = 0.0f;
    };

private:

    // The longest delay is size - kGuard: cubic reads two samples past the whole
    // delay, and the oldest sample held is at size - 1.
    static constexpr uint32_t kGuard = 3u;

    float at(uint32_t delay) const { return buffer_[(write_ - delay) & mask_]; }

    // Clamps delay to [minDelay, maxDelay_] and splits it into whole samples and a
    // fraction in [0, 1).
    uint32_t split(float delay, float minDelay, float& frac) const
    {
        delay = clamp(delay, minDelay, maxDelay_);
        const uint32_t whole = static_cast<uint32_t>(delay);
        frac = delay - static_cast<float>(whole);
        return whole;
    }

    float*   buffer_   = nullptr;
    uint32_t mask_     = 0;
    uint32_t write_    = 0;
    float    maxDelay_ = 0.0f;
};

} // namespace dsp
