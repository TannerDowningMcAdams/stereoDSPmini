#pragma once
#include "dsp_math.hpp"
#include <cstdint>

namespace dsp {

// Four-pole transistor ladder lowpass, 24 dB/octave, for one channel. Four
// trapezoidal one-poles with the resonance loop solved per sample (zero-delay
// feedback, Zavalishin, "The Art of VA Filter Design", ch. 5), so the cutoff and the
// resonant peak land on the set frequency at any rate. One saturator on the ladder
// input bounds the loop: resonance above 1 self-oscillates at a steady level
// instead of growing. The loop is solved for the linear filter and the saturator
// then applied to its input, so it costs one saturator call per sample. For
// saturation in every stage, see MoogLadderPlus.
//
// The saturator is a template argument of process() (see sat:: in dsp_math.hpp), so
// the curve can change between blocks without touching the filter state.
//
// DC gain is 1 / (1 + 4 * resonance), as in the analogue ladder; compensate after
// the filter if the level must hold.
class MoogLadder {
public:

    struct Config
    {
        uint32_t sampleRate;
        float    frequency;     // Hz
        float    resonance;     // 0..kMaxResonance; 1 is the edge of self-oscillation
        float    drive;         // input gain into the saturator; 1 saturates a full-scale input gently
    };

    static constexpr float kMaxRatio     = 0.49f;   // of the sample rate
    static constexpr float kMaxResonance = 1.2f;
    static constexpr float kMinDrive     = 0.25f;
    static constexpr float kMaxDrive     = 16.0f;

    // Starts from silence.
    void init(const Config& config);

    // Values are clamped to the limits above. setFrequency costs a tanf.
    void setFrequency(float hz);
    void setResonance(float resonance);
    // Small signals see unity gain at any drive: only the saturation point moves.
    void setDrive(float drive);

    void reset();

    template <typename Sat = sat::Rational>
    float process(float input)
    {
        // Dropped here, a NaN never reaches the state.
        if (input != input) { input = 0.0f; }
        // Ladder output from the states alone, with the input contribution
        // G^4 * u split off so the loop u = x - k * y can be solved for u.
        const float s = beta_ * (((gain_ * s_[0] + s_[1]) * gain_ + s_[2]) * gain_ + s_[3]);
        const float u = (input - k_ * s) * loopScale_;
        float y = Sat::value(drive_ * u) * invDrive_;
        for (float& state : s_)
        {
            y = tick(y, state);
        }
        return y;
    }

    // output may equal input.
    template <typename Sat = sat::Rational>
    void process(const float* input, float* output, uint16_t frames)
    {
        for (uint16_t i = 0; i < frames; i++)
        {
            output[i] = process<Sat>(input[i]);
        }
    }

    float frequency() const { return hz_; }
    float resonance() const { return resonance_; }
    float drive()     const { return drive_; }

private:

    // Trapezoidal one-pole lowpass stage.
    float tick(float x, float& state) const
    {
        const float v = (x - state) * gain_;
        const float y = v + state;
        state = y + v;
        return y;
    }

    void updateLoop();

    float piOverRate_ = 0.0f;
    float maxHz_      = 0.0f;
    float hz_         = 0.0f;
    float resonance_  = 0.0f;
    float drive_      = 1.0f;
    float invDrive_   = 1.0f;

    float gain_       = 0.0f;   // G = g / (1 + g), g = tan(pi * hz / rate)
    float beta_       = 1.0f;   // 1 - G
    float k_          = 0.0f;   // 4 * resonance
    float loopScale_  = 1.0f;   // 1 / (1 + k * G^4)
    float s_[4]       = {};
};

} // namespace dsp
