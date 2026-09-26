#include "envelope_follower.hpp"
#include <cmath>

namespace dsp {

void EnvelopeFollower::init(const Config& config)
{
    msToSamples_ = static_cast<float>(config.sampleRate) * 0.001f;
    state_       = 0.0f;
    setGain(config.gain);
    setThreshold(config.threshold);
    setAttackMs(config.attackMs);
    setReleaseMs(config.releaseMs);
}

void EnvelopeFollower::setGain(float gain)
{
    gain_ = clamp(gain, 0.0f, kMaxGain);
}

void EnvelopeFollower::setThreshold(float threshold)
{
    threshold_ = clamp(threshold, 0.0f, kMaxThreshold);
    outScale_  = 1.0f / (1.0f - threshold_);
    // A higher drop lowers the most the cap can hold; the output stays within 0..1.
    const float top = 1.0f - threshold_;
    if (state_ > top) { state_ = top; }
}

void EnvelopeFollower::setAttackMs(float ms)
{
    attackMs_ = clamp(ms, 0.0f, kMaxMs);
    attack_   = decayFor(attackMs_);
}

void EnvelopeFollower::setReleaseMs(float ms)
{
    releaseMs_ = clamp(ms, 0.0f, kMaxMs);
    release_   = decayFor(releaseMs_);
}

void EnvelopeFollower::reset(float value)
{
    state_ = clamp(value, 0.0f, 1.0f) * (1.0f - threshold_);
}

float EnvelopeFollower::decayFor(float ms) const
{
    const float samples = ms * msToSamples_;
    return (samples > 0.0f) ? std::exp(-1.0f / samples) : 0.0f;
}

} // namespace dsp
