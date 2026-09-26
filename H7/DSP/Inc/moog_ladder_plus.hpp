#pragma once
#include "dsp_math.hpp"
#include <cstdint>

namespace dsp {

// Four-pole transistor ladder lowpass with a saturator in every stage, for one
// channel: the large-signal model of D'Angelo and Valimaki, "Generalized Moog Ladder
// Filter: Part II" (IEEE/ACM TASLP 22(12), 2014), made explicit with that paper's
// delay-free loop method. Each stage integrates tanh of its input minus tanh of its
// own output, as its transistor pair does:
//
//   dv_i/dt = 2 pi fc [tanh(v_(i-1)) - tanh(v_i)],   v_0 = drive * x - k * v_4
//
// Signals are in units of twice the thermal voltage, so drive sets how far a
// full-scale input reaches into the curves. Each delay-free term is predicted from
// past values with the curves' slope at rest (1), which keeps the small-signal
// response exactly the bilinear transform of the linear ladder: cutoff and resonant
// peak land on the set values with no tuning tables. No iteration: five saturator
// calls per sample, against one for MoogLadder.
//
// Loud inputs pull the resonance down and flatten it, and self-oscillation settles
// at a level the curves set. Every signal the loop reuses has passed through a
// saturator or a stage, so the output stays bounded at any setting.
//
// The saturator is a template argument of process() (see sat:: in dsp_math.hpp), so
// the curve can change between blocks without touching the filter state.
//
// DC gain is 1 / (1 + 4 * resonance), as in the analogue ladder.
class MoogLadderPlus {
public:

    struct Config
    {
        uint32_t sampleRate;
        float    frequency;     // Hz
        float    resonance;     // 0..kMaxResonance; 1 is the edge of self-oscillation
        float    drive;         // input gain into the model; 1 saturates a full-scale input gently
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
    // Small signals see unity gain at any drive: only the saturation moves.
    void setDrive(float drive);

    void reset();

    template <typename Sat = sat::FastTanh>
    float process(float input)
    {
        // Dropped here, a NaN never reaches the state.
        if (input != input) { input = 0.0f; }

        // Global loop: the feedback term k * v_4[n] is not known yet. The linear
        // ladder's output, predicted from past ladder inputs and outputs, stands in
        // for it; loopScale_ folds in the part that depends on this sample's input.
        const float u = loopScale_ * (drive_ * input
                      + fbY_[0] * y_[0] + fbY_[1] * y_[1] + fbY_[2] * y_[2] + fbY_[3] * y_[3]
                      - fbX_[0] * x_[0] - fbX_[1] * x_[1] - fbX_[2] * x_[2] - fbX_[3] * x_[3]);

        float x     = Sat::value(u);
        float xPrev = x_[0];
        x_[3] = x_[2];  x_[2] = x_[1];  x_[1] = x_[0];  x_[0] = x;

        // Each stage: trapezoidal integration with tanh(v[n]) predicted as
        // tanh(v[n-1]) + (v[n] - v[n-1]), solved for v[n].
        for (int i = 0; i < 4; i++)
        {
            const float tPrev = t_[i];
            v_[i] += gain_ * (x + xPrev) - twoGain_ * tPrev;
            t_[i]  = Sat::value(v_[i]);
            xPrev  = tPrev;     // the next stage's input one sample ago
            x      = t_[i];
        }

        const float y = v_[3];
        y_[3] = y_[2];  y_[2] = y_[1];  y_[1] = y_[0];  y_[0] = y;
        return y * invDrive_;
    }

    // output may equal input.
    template <typename Sat = sat::FastTanh>
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

    void updateLoop();

    float piOverRate_ = 0.0f;
    float maxHz_      = 0.0f;
    float hz_         = 0.0f;
    float resonance_  = 0.0f;
    float drive_      = 1.0f;
    float invDrive_   = 1.0f;

    float gain_       = 0.0f;   // G = g / (1 + g), g = tan(pi * hz / rate)
    float twoGain_    = 0.0f;
    float k_          = 0.0f;   // 4 * resonance
    float loopScale_  = 1.0f;   // 1 / (1 + k * G^4)
    // Loop prediction weights for lags 1..4: k * C(4, m) * G^4 on the saturated
    // ladder input, k * C(4, m) * (-(1 - g) / (1 + g))^m on the ladder output.
    float fbX_[4]     = {};
    float fbY_[4]     = {};

    float v_[4]       = {};     // stage outputs
    float t_[4]       = {};     // Sat::value of each stage output
    float x_[4]       = {};     // saturated ladder input, newest first
    float y_[4]       = {};     // ladder output v_[3], newest first
};

} // namespace dsp
