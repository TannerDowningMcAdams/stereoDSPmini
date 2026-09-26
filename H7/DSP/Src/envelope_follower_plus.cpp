#include "envelope_follower_plus.hpp"
#include <cmath>

namespace dsp {

// Current through the diode in series with r ohms, dV volts across both.
static float diodeCurrent(float dv, float r)
{
    const float isr = EnvelopeFollowerPlus::kIs * r / EnvelopeFollowerPlus::kNVt;
    const float x   = std::log(isr) + isr + dv / EnvelopeFollowerPlus::kNVt;
    return EnvelopeFollowerPlus::kNVt / r * wrightOmega(x) - EnvelopeFollowerPlus::kIs;
}

void EnvelopeFollowerPlus::init(const Config& config)
{
    rate_       = static_cast<float>(config.sampleRate);
    gain_       = clamp(config.gain, 0.0f, kMaxGain);
    inputScale_ = gain_ * kInputVolts;
    rail_       = clamp(config.railVolts, kMinRailVolts, kMaxRailVolts);
    attackMs_   = clamp(config.attackMs, 0.0f, kMaxMs);
    releaseMs_  = clamp(config.releaseMs, kMinReleaseMs, kMaxMs);
    v_          = 0.0f;
    update();
}

void EnvelopeFollowerPlus::setGain(float gain)
{
    gain_       = clamp(gain, 0.0f, kMaxGain);
    inputScale_ = gain_ * kInputVolts;
}

void EnvelopeFollowerPlus::setRailVolts(float volts)
{
    rail_ = clamp(volts, kMinRailVolts, kMaxRailVolts);
    update();
}

void EnvelopeFollowerPlus::setAttackMs(float ms)
{
    attackMs_ = clamp(ms, 0.0f, kMaxMs);
    update();
}

void EnvelopeFollowerPlus::setReleaseMs(float ms)
{
    releaseMs_ = clamp(ms, kMinReleaseMs, kMaxMs);
    update();
}

void EnvelopeFollowerPlus::reset(float value)
{
    v_ = clamp(value, 0.0f, 1.0f) / outScale_;
}

void EnvelopeFollowerPlus::update()
{
    const float ra = attackMs_ * 0.001f / kCap;
    const float rb = releaseMs_ * 0.001f / kCap;
    const float gc = kCap * rate_;      // backward Euler companion of the cap
    const float gb = 1.0f / rb;

    rth_   = 1.0f / (gc + gb);
    bleed_ = gb * rth_;

    const float r   = ra + kRs + rth_;
    const float isr = kIs * r / kNVt;
    currentScale_ = kNVt / r;
    offset_       = std::log(isr) + isr;

    // Settled at the rail, the cap draws nothing: the diode, ra and rb in series.
    outScale_ = 1.0f / (diodeCurrent(rail_, ra + kRs + rb) * rb);
}

} // namespace dsp
