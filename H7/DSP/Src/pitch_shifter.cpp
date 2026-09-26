#include "pitch_shifter.hpp"
#include <cmath>

namespace dsp {

void PitchShifter::init(const Config& config)
{
    // The largest power of two in the buffer, so the index wraps with a mask.
    uint32_t size = 1u;
    while (size <= config.size / 2u) { size *= 2u; }
    buffer_ = config.buffer;
    mask_   = size - 1u;
    for (uint32_t i = 0; i < size; i++) { buffer_[i] = 0.0f; }
    pos_ = 0u;

    // The cubic read reaches two samples past its whole delay.
    const float reach = static_cast<float>(size) - 3.0f - static_cast<float>(kMinDelay);
    window_ = clamp(static_cast<float>(config.window), 1.0f, reach);
    phase_  = 0.0f;
    // Forces the ratio and step to be computed.
    semitones_ = config.semitones + 1.0f;
    setSemitones(config.semitones);
}

void PitchShifter::setSemitones(float semitones)
{
    semitones = clamp(semitones, -kMaxSemitones, kMaxSemitones);
    if (semitones == semitones_) { return; }
    semitones_ = semitones;
    ratio_     = std::exp2(semitones * (1.0f / 12.0f));
    // A falling phase shortens the delay, which raises the pitch.
    step_      = (1.0f - ratio_) / window_;
}

} // namespace dsp
