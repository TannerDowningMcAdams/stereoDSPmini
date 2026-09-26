#pragma once
#include "dsp_math.hpp"
#include "wright_omega.hpp"
#include <cstdint>

namespace dsp {

// The Q-Tron detector of EnvelopeFollower with the diode modelled by the Shockley
// equation, for one channel: a 1N4148 (SPICE model IS=4.352n N=1.906 RS=0.6458) and
// the attack resistor charging the 4u7 cap, the release resistor bleeding it to
// ground at all times, as in the circuit. The diode's knee is soft and its drop
// grows with current, so small signals charge the cap partway and a hard pick
// charges it harder than a fixed drop would.
//
// Each sample takes a backward Euler step. The cap and bleed resistor reduce to a
// Thevenin source behind a fixed resistance, which leaves one diode in series with
// one resistor: I = (N Vt / R) w(x) - Is, with w the Wright omega function and
// x = ln(Is R / N Vt) + (dV + Is R) / N Vt. Everything but dV is set when the
// settings change, so the step is explicit: one table read for w.
//
// Levels are in volts: the input is taken at the converter, where full scale 1.0 is
// 4 V, and gain is the inverting stage's voltage gain (EnvelopeFollower's Q-Tron
// values apply). The output is the cap voltage over its settled value for a drive
// held at the rail, so it reaches 1.0 there. Attack and release set the two
// resistors as time constants with the 4u7 cap. The bleed loads the charge path as
// in the circuit, so the peak falls as the release time approaches the attack.
class EnvelopeFollowerPlus {
public:

    struct Config
    {
        uint32_t sampleRate;
        float    gain;          // inverting stage gain, V/V: the sensitivity
        float    railVolts;     // highest the stage output reaches
        float    attackMs;      // charge resistor * 4u7
        float    releaseMs;     // bleed resistor * 4u7
    };

    static constexpr float kInputVolts   = 4.0f;    // at full scale: 8 Vpp into the ADC
    static constexpr float kMaxGain      = 1000.0f;
    static constexpr float kMinRailVolts = 1.0f;
    static constexpr float kMaxRailVolts = 30.0f;
    static constexpr float kMinReleaseMs = 0.01f;
    static constexpr float kMaxMs        = 10000.0f;

    // Starts from silence.
    void init(const Config& config);

    // Values are clamped to the limits above. setRailVolts, setAttackMs and
    // setReleaseMs cost two logf, so call them per block or on change.
    void setGain(float gain);
    void setRailVolts(float volts);
    void setAttackMs(float ms);
    void setReleaseMs(float ms);

    // Sets the envelope output, clamped to 0..1.
    void reset(float value = 0.0f);

    // Returns the envelope after this sample.
    float process(float input)
    {
        tick(input);
        return v_ * outScale_;
    }

    // Writes the envelope per sample. envelope may equal input.
    void process(const float* input, float* envelope, uint16_t frames)
    {
        for (uint16_t i = 0; i < frames; i++) {
            tick(input[i]);
            envelope[i] = v_ * outScale_;
        }
    }

    float value()     const { return v_ * outScale_; }
    float capVolts()  const { return v_; }
    float gain()      const { return gain_; }
    float railVolts() const { return rail_; }
    float attackMs()  const { return attackMs_; }
    float releaseMs() const { return releaseMs_; }

    // Diode: 1N4148 at 27 C.
    static constexpr float kIs  = 4.352e-9f;
    static constexpr float kNVt = 1.906f * 0.025865f;
    static constexpr float kRs  = 0.6458f;
    static constexpr float kCap = 4.7e-6f;

private:

    // NaN input gives a drive of 0, so it bleeds like silence.
    void tick(float input)
    {
        const float drive = clamp(-inputScale_ * input, 0.0f, rail_);
        const float vth   = v_ - bleed_ * v_;
        const float i     = currentScale_ * wrightOmega((drive - vth) * invNVt_ + offset_) - kIs;
        v_ = vth + i * rth_;
    }

    void update();

    float rate_         = 48000.0f;
    float gain_         = 1.0f;
    float inputScale_   = kInputVolts;
    float rail_         = 9.0f;
    float attackMs_     = 0.0f;
    float releaseMs_    = kMinReleaseMs;

    // Step constants, from update().
    float bleed_        = 0.0f;     // fraction of the cap volts the Thevenin source drops
    float rth_          = 0.0f;     // Thevenin resistance of cap and bleed
    float invNVt_       = 1.0f / kNVt;
    float offset_       = 0.0f;     // ln(Is R / N Vt) + Is R / N Vt
    float currentScale_ = 0.0f;     // N Vt / R
    float outScale_     = 1.0f;

    float v_            = 0.0f;     // cap volts
};

} // namespace dsp
