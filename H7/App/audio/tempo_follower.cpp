#include "tempo_follower.hpp"
#include <cmath>

void TempoFollower::init(const Config& config)
{
    sampleRate_      = static_cast<float>(config.sampleRate);
    cyclesPerSecond_ = static_cast<float>(config.cyclesPerSecond);
    running_ = false;
    tempoHz_ = 0.0f;
    phase_   = 0.0f;
}

static float wrap01(float phase)
{
    return phase - std::floor(phase);
}

void TempoFollower::update(float tempoHz, uint16_t phase, uint32_t edgeCycles, uint16_t frameSeq,
                           uint32_t blockCycles)
{
    if (!(tempoHz > 0.0f))
    {
        running_ = false;
        tempoHz_ = 0.0f;
        return;
    }

    const bool starting = !running_;
    if (!starting && frameSeq == lastSeq_) { return; }
    lastSeq_   = frameSeq;
    tempoHz_   = tempoHz;
    increment_ = tempoHz / sampleRate_;

    // The edge usually precedes the block by under a frame. The signed difference
    // stays exact across the counter's wrap.
    const float edgeToBlock = static_cast<float>(static_cast<int32_t>(blockCycles - edgeCycles)) / cyclesPerSecond_;
    const float target = static_cast<float>(phase) * (1.0f / 65536.0f) + tempoHz * edgeToBlock;
    float error = target - phase_;
    error -= std::round(error);

    if (starting)
    {
        running_ = true;
        samplesSinceBeat_ = kSamplesSinceBeatMax;
        phase_ = wrap01(target);
    }
    else if (std::fabs(error) > kSnapSeconds * tempoHz)
    {
        phase_ = wrap01(phase_ + error);
    }
    else
    {
        phase_ = wrap01(phase_ + kGain * error);
    }
}

int32_t TempoFollower::advance(uint16_t frames)
{
    if (!running_ || frames == 0u) { return -1; }

    int32_t beat = -1;
    const float end = phase_ + increment_ * frames;
    if (end >= 1.0f)
    {
        int32_t index = static_cast<int32_t>(std::ceil((1.0f - phase_) / increment_));
        if (index < 0)                                { index = 0; }
        if (index >= static_cast<int32_t>(frames))    { index = frames - 1; }

        const uint32_t holdOff = static_cast<uint32_t>(0.25f / increment_);   // a quarter beat
        if (samplesSinceBeat_ + static_cast<uint32_t>(index) >= holdOff)
        {
            beat = index;
            samplesSinceBeat_ = frames - static_cast<uint32_t>(index);
        }
    }
    if (beat < 0 && samplesSinceBeat_ < kSamplesSinceBeatMax) { samplesSinceBeat_ += frames; }
    phase_ = wrap01(end);
    return beat;
}
