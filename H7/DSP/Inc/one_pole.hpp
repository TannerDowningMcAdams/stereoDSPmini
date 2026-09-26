#pragma once
#include "dsp_math.hpp"
#include <cmath>
#include <cstdint>

namespace dsp {

// One-pole lowpass or highpass, 6 dB/octave, for one channel. Uses the
// topology-preserving (trapezoidal) form: the cutoff is prewarped so the -3 dB
// point lands on the set frequency at any rate, and the frequency can change every
// block without clicks because the state is the integrator output, independent of
// the coefficient. The highpass is the input minus the lowpass, from the same state.
class OnePole {
public:

    enum class Mode : uint8_t { Lowpass, Highpass };

    struct Config
    {
        uint32_t sampleRate;
        Mode     mode;
        float    frequency;     // Hz
    };

    void init(const Config& config)
    {
        piOverRate_ = kPi / static_cast<float>(config.sampleRate);
        maxHz_      = kMaxRatio * static_cast<float>(config.sampleRate);
        mode_       = config.mode;
        setFrequency(config.frequency);
        reset();
    }

    // Clamped to 0..kMaxRatio of the sample rate, where the prewarp is still finite.
    // Costs one tanf, so call it per block or on change, never per sample.
    void setFrequency(float hz)
    {
        hz = clamp(hz, 0.0f, maxHz_);
        hz_ = hz;
        const float g = std::tan(hz * piOverRate_);
        gain_ = g / (1.0f + g);
    }

    void setMode(Mode mode) { mode_ = mode; }

    // Settles the filter as if `value` had been the input forever: the lowpass then
    // outputs `value` and the highpass outputs zero.
    void reset(float value = 0.0f) { state_ = value; }

    float process(float input)
    {
        const float lowpass = tick(input);
        return (mode_ == Mode::Lowpass) ? lowpass : input - lowpass;
    }

    // output may equal input.
    void process(const float* input, float* output, uint16_t frames)
    {
        if (mode_ == Mode::Lowpass) {
            for (uint16_t i = 0; i < frames; i++) { output[i] = tick(input[i]); }
        } else {
            for (uint16_t i = 0; i < frames; i++) {
                const float x = input[i];
                output[i] = x - tick(x);
            }
        }
    }

    float frequency() const { return hz_; }
    Mode  mode()      const { return mode_; }

private:

    static constexpr float kMaxRatio = 0.49f;

    // Returns the lowpass output and advances the integrator.
    float tick(float input)
    {
        const float v       = (input - state_) * gain_;
        const float lowpass = v + state_;
        state_ = lowpass + v;
        return lowpass;
    }

    float piOverRate_ = kPi / 48000.0f;
    float maxHz_      = kMaxRatio * 48000.0f;
    float hz_         = 0.0f;
    float gain_       = 0.0f;
    float state_      = 0.0f;
    Mode  mode_       = Mode::Lowpass;
};

} // namespace dsp
