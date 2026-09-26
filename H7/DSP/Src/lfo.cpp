#include "lfo.hpp"
#include <cmath>

namespace dsp {

void Lfo::init(const Config& config)
{
    rate_  = static_cast<float>(config.sampleRate);
    shape_ = config.shape;
    phase_ = 0.0f;
    setFrequency(config.frequency);
}

void Lfo::setFrequency(float hz)
{
    hz_        = clamp(hz, 0.0f, kMaxRatio * rate_);
    increment_ = hz_ / rate_;
}

void Lfo::setPhase(float cycles)
{
    if (cycles != cycles) { cycles = 0.0f; }
    float wrapped = cycles - std::floor(cycles);
    // A tiny negative input rounds up to exactly 1.
    if (wrapped >= 1.0f) { wrapped = 0.0f; }
    phase_ = wrapped;
}

} // namespace dsp
