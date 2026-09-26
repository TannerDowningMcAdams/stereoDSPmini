#include "shimmer_reverb_engine.hpp"
#include <cmath>

namespace {

constexpr float kMinDecay = 0.3f;
constexpr float kMaxDecay = 20.0f;
// Tone 0 leaves the treble a 20th of the decay time; tone 1 decays the band evenly.
constexpr float kDarkest  = 0.05f;
constexpr float kMinLowCutHz = 20.0f;
constexpr float kMaxLowCutHz = 400.0f;
// Tail chorus outside Hall, where param 3 does something else.
constexpr float kModulation = 0.3f;

// Shimmer intervals in semitones, from param 6.
constexpr float   kIntervals[]   = { -12.0f, -5.0f, 5.0f, 7.0f, 12.0f, 19.0f, 24.0f };
constexpr uint8_t kIntervalCount = sizeof(kIntervals) / sizeof(kIntervals[0]);

// The detector reaches the rail at -18 dBFS, so the duck depth is reached at a
// moderate playing level. Attack and release set how fast the tail gets out of the
// way and how long it waits before swelling back.
constexpr dsp::EnvelopeFollower::Config kDetector {
    .sampleRate = 48000u,
    .gain       = 8.0f,
    .threshold  = 0.0f,
    .attackMs   = 5.0f,
    .releaseMs  = 250.0f,
};

// lo * (hi / lo)^p: a pot position to a value on a log scale.
float logScale(float p, float lo, float hi)
{
    return lo * std::exp2(p * std::log2(hi / lo));
}

}

void ShimmerReverbEngine::activate(EngineArenas& arenas, uint32_t sampleRate)
{
    reverb_.init({ sampleRate,
                   arenas.fast.allocate<float>(dsp::FdnReverb::kFastFloats),
                   arenas.large.allocate<float>(dsp::FdnReverb::kLargeFloats) });
    lowCutLeft_.init({ sampleRate, dsp::OnePole::Mode::Highpass, kMinLowCutHz });
    lowCutRight_.init({ sampleRate, dsp::OnePole::Mode::Highpass, kMinLowCutHz });
    dsp::EnvelopeFollower::Config detector = kDetector;
    detector.sampleRate = sampleRate;
    follower_.init(detector);
    duck_.snap(0.0f);
    duckTarget_ = 0.0f;
}

// Every block. The reverb's setters ignore a value they already hold, so a quiet
// block costs the few exp2f, log2f and tanf of the mappings and the low cut.
void ShimmerReverbEngine::applyControls(const BlockContext& ctx)
{
    const float* p    = ctx.params;
    const uint8_t mode = ctx.field[kFieldMode];

    reverb_.setDecay(logScale(p[kParamDecay], kMinDecay, kMaxDecay));
    reverb_.setDamping(logScale(p[kParamTone], kDarkest, 1.0f));
    reverb_.setSize(dsp::FdnReverb::kMinSize + (1.0f - dsp::FdnReverb::kMinSize) * p[kParamSize]);
    reverb_.setPreDelayMs(dsp::FdnReverb::kMaxPreDelayMs * p[kParamPreDelay]);

    uint8_t interval = static_cast<uint8_t>(p[kParamInterval] * kIntervalCount);
    if (interval >= kIntervalCount) { interval = kIntervalCount - 1u; }
    reverb_.setShimmerSemitones(kIntervals[interval]);

    const float character = p[kParamCharacter];
    reverb_.setModulation((mode == kModeHall) ? character : kModulation);
    reverb_.setShimmer((mode == kModeShimmer) ? character : 0.0f);
    duckTarget_ = (mode == kModeDuck) ? character : 0.0f;
    reverb_.setFreeze(ctx.field[kFieldFreeze] != 0u);

    const float lowCut = logScale(p[kParamLowCut], kMinLowCutHz, kMaxLowCutHz);
    lowCutLeft_.setFrequency(lowCut);
    lowCutRight_.setFrequency(lowCut);

    if (ctx.first) { duck_.snap(duckTarget_); }
}

void ShimmerReverbEngine::process(dsp::ConstAudioBuffer in, dsp::AudioBuffer out, const BlockContext& ctx)
{
    applyControls(ctx);

    for (uint16_t start = 0; start < in.size(); start = static_cast<uint16_t>(start + kChunk))
    {
        const uint16_t remaining = static_cast<uint16_t>(in.size() - start);
        const uint16_t frames    = (remaining < kChunk) ? remaining : kChunk;
        const float* inLeft   = in.left() + start;
        const float* inRight  = in.right() + start;
        float*       outLeft  = out.left() + start;
        float*       outRight = out.right() + start;

        // The detector hears the playing before the low cut, as the player does.
        for (uint16_t i = 0; i < frames; i++) { envelope_[i] = 0.5f * (inLeft[i] + inRight[i]); }
        follower_.process(envelope_, envelope_, frames);

        lowCutLeft_.process(inLeft, left_, frames);
        lowCutRight_.process(inRight, right_, frames);
        reverb_.process(left_, right_, outLeft, outRight, frames);

        duck_.setTarget(duckTarget_, frames);
        for (uint16_t i = 0; i < frames; i++)
        {
            const float gain = 1.0f - duck_.next() * envelope_[i];
            outLeft[i]  *= gain;
            outRight[i] *= gain;
        }
    }
}
