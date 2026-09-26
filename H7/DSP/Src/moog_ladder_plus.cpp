#include "moog_ladder_plus.hpp"
#include <cmath>

namespace dsp {

void MoogLadderPlus::init(const Config& config)
{
    piOverRate_ = kPi / static_cast<float>(config.sampleRate);
    maxHz_      = kMaxRatio * static_cast<float>(config.sampleRate);
    resonance_  = clamp(config.resonance, 0.0f, kMaxResonance);
    k_          = 4.0f * resonance_;
    setDrive(config.drive);
    setFrequency(config.frequency);
    reset();
}

void MoogLadderPlus::setFrequency(float hz)
{
    hz_ = clamp(hz, 0.0f, maxHz_);
    const float g = std::tan(hz_ * piOverRate_);
    gain_    = g / (1.0f + g);
    twoGain_ = 2.0f * gain_;
    updateLoop();
}

void MoogLadderPlus::setResonance(float resonance)
{
    resonance_ = clamp(resonance, 0.0f, kMaxResonance);
    k_         = 4.0f * resonance_;
    updateLoop();
}

void MoogLadderPlus::setDrive(float drive)
{
    drive_    = clamp(drive, kMinDrive, kMaxDrive);
    invDrive_ = 1.0f / drive_;
}

void MoogLadderPlus::reset()
{
    for (int i = 0; i < 4; i++)
    {
        v_[i] = 0.0f;
        t_[i] = 0.0f;
        x_[i] = 0.0f;
        y_[i] = 0.0f;
    }
}

// Small-signal, each stage is G (1 + z^-1) / (1 - b z^-1) with b = 1 - 2G, so the
// ladder output is Y = N / B * X for ladder input X, with N = G^4 (1 + z^-1)^4 and
// B = (1 - b z^-1)^4 = sum of C(4, m) (-b)^m z^-m. Then
//   Y[n] = G^4 X[n] + sum over m = 1..4 of (G^4 C(4, m) X[n-m] - C(4, m) (-b)^m Y[n-m])
// and with X[n] = u = drive * input - k Y[n], u solves to the weights below.
void MoogLadderPlus::updateLoop()
{
    static constexpr float kBinomial[4] = {4.0f, 6.0f, 4.0f, 1.0f};
    const float g2 = gain_ * gain_;
    const float g4 = g2 * g2;
    const float negB = twoGain_ - 1.0f;
    float power = 1.0f;
    for (int m = 0; m < 4; m++)
    {
        power   *= negB;
        fbX_[m]  = k_ * kBinomial[m] * g4;
        fbY_[m]  = k_ * kBinomial[m] * power;
    }
    loopScale_ = 1.0f / (1.0f + k_ * g4);
}

} // namespace dsp
