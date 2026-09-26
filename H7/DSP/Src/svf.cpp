#include "svf.hpp"
#include "dsp_math.hpp"
#include <cmath>

namespace dsp {

void Svf::init(const Config& config)
{
    piOverRate_ = kPi / static_cast<float>(config.sampleRate);
    maxHz_      = kMaxRatio * static_cast<float>(config.sampleRate);
    mode_       = config.mode;
    set(config.frequency, config.q, config.gainDb);
    reset();
}

void Svf::setFrequency(float hz)
{
    setFrequencyOnly(hz);
    update();
}

void Svf::setQ(float q)
{
    q_ = clamp(q, kMinQ, kMaxQ);
    update();
}

void Svf::setGainDb(float db)
{
    setGainOnly(db);
    update();
}

void Svf::setMode(Mode mode)
{
    mode_ = mode;
    update();
}

void Svf::set(float hz, float q, float db)
{
    setFrequencyOnly(hz);
    q_ = clamp(q, kMinQ, kMaxQ);
    setGainOnly(db);
    update();
}

void Svf::reset()
{
    ic1eq_ = 0.0f;
    ic2eq_ = 0.0f;
}

void Svf::process(const float* input, float* output, uint16_t frames)
{
    for (uint16_t i = 0; i < frames; i++)
    {
        output[i] = process(input[i]);
    }
}

void Svf::setFrequencyOnly(float hz)
{
    hz_  = clamp(hz, 0.0f, maxHz_);
    tan_ = std::tan(hz_ * piOverRate_);
}

void Svf::setGainOnly(float db)
{
    db_      = clamp(db, -kMaxDb, kMaxDb);
    amp_     = std::pow(10.0f, db_ / 40.0f);
    sqrtAmp_ = std::sqrt(amp_);
}

// Mix coefficients from the Cytomic paper, with the input, bandpass and lowpass
// outputs as the three terms.
void Svf::update()
{
    float g = tan_;
    float k = 1.0f / q_;
    const float a = amp_;

    switch (mode_)
    {
    case Mode::Lowpass:   m0_ = 0.0f;  m1_ = 0.0f;        m2_ = 1.0f;         break;
    case Mode::Highpass:  m0_ = 1.0f;  m1_ = -k;          m2_ = -1.0f;        break;
    case Mode::Bandpass:  m0_ = 0.0f;  m1_ = k;           m2_ = 0.0f;         break;
    case Mode::Notch:     m0_ = 1.0f;  m1_ = -k;          m2_ = 0.0f;         break;
    case Mode::Peak:      m0_ = 1.0f;  m1_ = -k;          m2_ = -2.0f;        break;
    case Mode::Allpass:   m0_ = 1.0f;  m1_ = -2.0f * k;   m2_ = 0.0f;         break;
    case Mode::Bell:
        k   = 1.0f / (q_ * a);
        m0_ = 1.0f;  m1_ = k * (a * a - 1.0f);  m2_ = 0.0f;
        break;
    case Mode::LowShelf:
        g   = tan_ / sqrtAmp_;
        m0_ = 1.0f;  m1_ = k * (a - 1.0f);  m2_ = a * a - 1.0f;
        break;
    case Mode::HighShelf:
        g   = tan_ * sqrtAmp_;
        m0_ = a * a;  m1_ = k * (1.0f - a) * a;  m2_ = 1.0f - a * a;
        break;
    }

    a1_ = 1.0f / (1.0f + g * (g + k));
    a2_ = g * a1_;
    a3_ = g * a2_;
}

} // namespace dsp
