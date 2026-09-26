#pragma once
#include "dsp_math.hpp"
#include <cstdint>

namespace dsp {

// Peak envelope follower for one channel, modelled on the Q-Tron detector: an
// inverting gain stage on a single supply, a diode, and a capacitor charged through
// one resistor and bled to ground through another.
//
// Levels are in units of the gain stage's output swing: 1.0 is the rail. Per sample:
//   - The stage drives -gain * input, held to 0..1 by the rails. Only the negative
//     half of the input gets through, as in the circuit.
//   - The cap charges toward drive - threshold, the diode's forward drop, with the
//     attack time constant, while the drive is above it.
//   - Otherwise it bleeds toward ground with the release time constant, and cannot
//     fall below drive - threshold because the diode would conduct.
// Each step is the exact exponential solution for a drive held over the sample.
//
// The charge and bleed paths are steered apart, as in the two-diode AR circuit, so
// attack and release are independent. In the Q-Tron the 330k bleed also loads the
// charge path, which pulls the peak down by R_attack / R_release: 0.03% with its
// values, but a large fraction once the two times approach each other.
//
// The output is the cap level scaled by 1 / (1 - threshold), so a drive held at the
// rail settles at 1.0.
class EnvelopeFollower {
public:

    struct Config
    {
        uint32_t sampleRate;
        float    gain;          // input full scale to the rail: the sensitivity
        float    threshold;     // diode forward drop over the output swing
        float    attackMs;      // charge time constant
        float    releaseMs;     // bleed time constant
    };

    // Q-Tron detector values. The threshold and the gain in rail units depend on the
    // supply and on the input level at digital full scale:
    //   threshold = 0.6 V / swing,  gain = kQTronMaxGain * sensitivity * fullScaleV / swing.
    static constexpr float kQTronMaxGain   = 66.7f;     // 220k / 3k3
    static constexpr float kQTronAttackMs  = 0.47f;     // 100R * 4u7
    static constexpr float kQTronReleaseMs = 1551.0f;   // 330k * 4u7

    static constexpr float kMaxGain      = 1000.0f;
    static constexpr float kMaxThreshold = 0.9f;
    static constexpr float kMaxMs        = 10000.0f;

    // Starts from silence.
    void init(const Config& config);

    // Values are clamped to the limits above. setAttackMs and setReleaseMs cost an
    // expf each, so call them per block or on change.
    void setGain(float gain);
    void setThreshold(float threshold);
    void setAttackMs(float ms);
    void setReleaseMs(float ms);

    // Sets the envelope output, clamped to 0..1.
    void reset(float value = 0.0f);

    // Returns the envelope after this sample.
    float process(float input)
    {
        tick(input);
        return state_ * outScale_;
    }

    // Writes the envelope per sample. envelope may equal input.
    void process(const float* input, float* envelope, uint16_t frames)
    {
        for (uint16_t i = 0; i < frames; i++) {
            tick(input[i]);
            envelope[i] = state_ * outScale_;
        }
    }

    float value()     const { return state_ * outScale_; }
    float gain()      const { return gain_; }
    float threshold() const { return threshold_; }
    float attackMs()  const { return attackMs_; }
    float releaseMs() const { return releaseMs_; }

private:

    // NaN input gives a drive of 0, so it bleeds like silence.
    void tick(float input)
    {
        const float drive  = clamp(-gain_ * input, 0.0f, 1.0f);
        const float target = drive - threshold_;
        if (target > state_) {
            state_ = target + attack_ * (state_ - target);
        } else {
            const float bled = state_ * release_;
            state_ = (bled > target) ? bled : target;
        }
    }

    // The per-sample decay factor for a time constant, 0 for an instant one.
    float decayFor(float ms) const;

    float msToSamples_ = 48.0f;
    float gain_        = 1.0f;
    float threshold_   = 0.0f;
    float outScale_    = 1.0f;
    float attackMs_    = 0.0f;
    float releaseMs_   = 0.0f;
    float attack_      = 0.0f;
    float release_     = 0.0f;
    float state_       = 0.0f;  // cap level, 0..1 - threshold
};

} // namespace dsp
