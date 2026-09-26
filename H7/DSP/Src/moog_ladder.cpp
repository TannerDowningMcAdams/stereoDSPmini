#include "moog_ladder.hpp"
#include <cmath>

namespace dsp {

void MoogLadder::init(const Config& config)
{
    piOverRate_ = kPi / static_cast<float>(config.sampleRate);
    maxHz_      = kMaxRatio * static_cast<float>(config.sampleRate);
    resonance_  = clamp(config.resonance, 0.0f, kMaxResonance);
    k_          = 4.0f * resonance_;
    setDrive(config.drive);
    setFrequency(config.frequency);
    reset();
}

void MoogLadder::setFrequency(float hz)
{
    hz_ = clamp(hz, 0.0f, maxHz_);
    const float g = std::tan(hz_ * piOverRate_);
    gain_ = g / (1.0f + g);
    beta_ = 1.0f - gain_;
    updateLoop();
}

void MoogLadder::setResonance(float resonance)
{
    resonance_ = clamp(resonance, 0.0f, kMaxResonance);
    k_         = 4.0f * resonance_;
    updateLoop();
}

void MoogLadder::setDrive(float drive)
{
    drive_    = clamp(drive, kMinDrive, kMaxDrive);
    invDrive_ = 1.0f / drive_;
}

void MoogLadder::reset()
{
    for (float& state : s_)
    {
        state = 0.0f;
    }
}

void MoogLadder::updateLoop()
{
    const float g2 = gain_ * gain_;
    loopScale_ = 1.0f / (1.0f + k_ * g2 * g2);
}

} // namespace dsp
