#include "fdn_reverb.hpp"
#include "lfo.hpp"
#include <cmath>

namespace dsp {

namespace {

using namespace fdn_layout;

// Where each channel's delay falls inside its slot of the step's range, as a
// fraction of the slot. Fixed values that avoid the slot edges, so no two channels
// of a step land on the same delay and no delay is zero.
constexpr float kDiffusionJitter[kDiffusionSteps][kChannels] = {
    { 0.62f, 0.18f, 0.83f, 0.41f, 0.27f, 0.74f, 0.55f, 0.12f },
    { 0.31f, 0.77f, 0.46f, 0.89f, 0.15f, 0.58f, 0.23f, 0.69f },
    { 0.84f, 0.37f, 0.13f, 0.66f, 0.49f, 0.21f, 0.92f, 0.35f },
    { 0.19f, 0.53f, 0.71f, 0.28f, 0.86f, 0.44f, 0.63f, 0.87f },
};
// Polarity flips after each step's shuffle, four channels each.
constexpr uint8_t kDiffusionFlips[kDiffusionSteps] = { 0x5Au, 0x33u, 0x96u, 0x6Cu };

// Modulation rates, spread so no two lines sweep in step for long.
constexpr float kModHz[kChannels] = { 0.11f, 0.17f, 0.23f, 0.29f, 0.37f, 0.43f, 0.53f, 0.61f };

// Default shimmer interval: an octave up.
constexpr float kShimmerSemitones = 12.0f;

constexpr float kGlideMs = 300.0f;
constexpr float kFadeMs  = 30.0f;

// The diffused input also reaches the output, as early reflections, while the
// loop's first pass is still on its way.
constexpr float kEarlyGain  = 0.5f;
// Brings a 2 s tail on white noise to about the input's level.
constexpr float kOutputGain = 0.5f;

// The mixes and damping hold the loop well inside this; the clamp makes that true
// whatever arrives.
constexpr float kLoopCeiling = 4.0f;

// log2(1000): -60 dB is a gain of 2^-kLog2Of1000.
constexpr float kLog2Of1000 = 9.96578428f;

// Normalised 8-point Hadamard transform, as a fast Walsh-Hadamard butterfly written
// out: 24 adds, and every output takes an equal share of every input.
void hadamard8(float* x)
{
    const float a0 = x[0] + x[1], a1 = x[0] - x[1], a2 = x[2] + x[3], a3 = x[2] - x[3];
    const float a4 = x[4] + x[5], a5 = x[4] - x[5], a6 = x[6] + x[7], a7 = x[6] - x[7];
    const float b0 = a0 + a2, b1 = a1 + a3, b2 = a0 - a2, b3 = a1 - a3;
    const float b4 = a4 + a6, b5 = a5 + a7, b6 = a4 - a6, b7 = a5 - a7;
    constexpr float kScale = 0.353553391f;     // 1 / sqrt(8)
    x[0] = kScale * (b0 + b4);
    x[1] = kScale * (b1 + b5);
    x[2] = kScale * (b2 + b6);
    x[3] = kScale * (b3 + b7);
    x[4] = kScale * (b0 - b4);
    x[5] = kScale * (b1 - b5);
    x[6] = kScale * (b2 - b6);
    x[7] = kScale * (b3 - b7);
}

}

void FdnReverb::init(const Config& config)
{
    sampleRate_  = static_cast<float>(config.sampleRate);
    msToSamples_ = sampleRate_ * 0.001f;
    glide_       = 1.0f / (kGlideMs * msToSamples_);
    fade_        = 1.0f / (kFadeMs * msToSamples_);

    // Binds a ring to the next `floats` of a block and clears them.
    auto take = [](float*& block, Ring& ring, uint32_t floats)
    {
        ring.data = block;
        ring.mask = floats - 1u;
        for (uint32_t i = 0u; i < floats; i++) { block[i] = 0.0f; }
        block += floats;
    };
    float* fast  = config.fast;
    float* large = config.large;

    large += kStaggerFloats;
    take(large, pre_[0], preDelayFloats());
    large += kStaggerFloats;
    take(large, pre_[1], preDelayFloats());
    preDelayMax_ = static_cast<float>(pre_[0].mask - 2u);

    for (uint32_t s = 0u; s < kDiffusionSteps; s++)
    {
        const float slot = kDiffusionMs[s] * msToSamples_ / static_cast<float>(kChannels);
        for (uint32_t c = 0u; c < kChannels; c++)
        {
            take(fast, diffusion_[s][c], diffusionFloats(s, c));
            const float delay = slot * (static_cast<float>(c) + kDiffusionJitter[s][c]);
            diffusionDelay_[s][c] = static_cast<uint32_t>(clamp(delay, 1.0f, static_cast<float>(diffusion_[s][c].mask)));
        }
    }

    const uint32_t window = static_cast<uint32_t>(kShimmerWindowMs * msToSamples_);
    for (uint32_t c = 0u; c < kChannels; c++)
    {
        large += kStaggerFloats;
        take(large, lines_[c], lineFloats(c));
        lineSamples_[c] = kLineMs[c] * msToSamples_;
        maxDelay_[c]    = static_cast<float>(lines_[c].mask - 3u);
        allpass_[c]     = 0.0f;
        damp_[c]        = 0.0f;
        modPhase_[c]    = static_cast<float>(c) / static_cast<float>(kChannels);
        modStep_[c]     = kModHz[c] / sampleRate_;
        if (c < kShimmerLines)
        {
            large += kStaggerFloats;
            shifters_[c].init({ large, shifterFloats(), window, kShimmerSemitones });
            large += shifterFloats();
        }
    }
    pos_ = 0u;

    decay_          = 2.0f;
    damping_        = 1.0f;
    sizeTarget_     = 1.0f;
    preDelayTarget_ = 0.0f;
    modTarget_      = 0.0f;
    shimmerAmount_  = 0.0f;
    shimmerTarget_  = 0.0f;
    inputTarget_    = 1.0f;
    freeze_         = false;
    loopDirty_      = true;
    snap_           = true;
}

void FdnReverb::setDecay(float seconds)
{
    seconds = clamp(seconds, kMinDecaySeconds, kMaxDecaySeconds);
    if (seconds == decay_) { return; }
    decay_     = seconds;
    loopDirty_ = true;
}

void FdnReverb::setDamping(float ratio)
{
    ratio = clamp(ratio, kMinDamping, 1.0f);
    if (ratio == damping_) { return; }
    damping_   = ratio;
    loopDirty_ = true;
}

void FdnReverb::setSize(float size)
{
    size = clamp(size, kMinSize, 1.0f);
    if (size == sizeTarget_) { return; }
    sizeTarget_ = size;
    loopDirty_  = true;
}

// Whole samples: a linear read at a fixed fraction would roll off the treble.
void FdnReverb::setPreDelayMs(float ms)
{
    preDelayTarget_ = clamp(std::round(clamp(ms, 0.0f, kMaxPreDelayMs) * msToSamples_), 0.0f, preDelayMax_);
}

void FdnReverb::setModulation(float depth)
{
    modTarget_ = clamp(depth, 0.0f, 1.0f) * fdn_layout::kMaxModMs * msToSamples_;
}

void FdnReverb::setShimmer(float amount)
{
    shimmerAmount_ = clamp(amount, 0.0f, 1.0f);
    shimmerTarget_ = freeze_ ? 0.0f : shimmerAmount_;
}

void FdnReverb::setShimmerSemitones(float semitones)
{
    for (PitchShifter& shifter : shifters_) { shifter.setSemitones(semitones); }
}

// Shimmer is withdrawn while frozen: it would keep raising the held chord.
void FdnReverb::setFreeze(bool freeze)
{
    if (freeze == freeze_) { return; }
    freeze_        = freeze;
    inputTarget_   = freeze ? 0.0f : 1.0f;
    shimmerTarget_ = freeze ? 0.0f : shimmerAmount_;
    loopDirty_     = true;
}

// Damping filter targets for each line: the gain at DC loses 60 dB per decay time,
// and the gain at Nyquist 60 dB per decay * damping, both over the line's length.
void FdnReverb::updateLoop()
{
    loopDirty_ = false;
    for (uint32_t c = 0u; c < kChannels; c++)
    {
        if (freeze_)
        {
            gainTarget_[c] = 1.0f;
            poleTarget_[c] = 0.0f;
            continue;
        }
        const float pass  = sizeTarget_ * lineSamples_[c] / sampleRate_;
        const float low   = std::exp2(-kLog2Of1000 * pass / decay_);
        const float high  = std::exp2(-kLog2Of1000 * pass / (decay_ * damping_));
        const float ratio = high / low;
        const float pole  = (1.0f - ratio) / (1.0f + ratio);
        gainTarget_[c] = low * (1.0f - pole);
        poleTarget_[c] = pole;
    }
}

float FdnReverb::lineDelay(uint32_t c) const
{
    const float delay = size_ * lineSamples_[c] + modDepth_ * Lfo::shapeAt(Lfo::Shape::Sine, modPhase_[c]);
    return clamp(delay, 1.0f, maxDelay_[c]);
}

void FdnReverb::process(const float* inLeft, const float* inRight, float* outLeft, float* outRight, uint16_t frames)
{
    if (frames == 0u) { return; }
    if (loopDirty_) { updateLoop(); }
    if (snap_)
    {
        size_     = sizeTarget_;
        preDelay_ = preDelayTarget_;
        shimmer_  = shimmerTarget_;
        input_    = inputTarget_;
        modDepth_ = modTarget_;
        for (uint32_t c = 0u; c < kChannels; c++)
        {
            gain_[c]  = gainTarget_[c];
            pole_[c]  = poleTarget_[c];
            delay_[c] = lineDelay(c);
        }
        snap_ = false;
    }

    // Each control moves to its value at the end of the block, and the sample loop
    // ramps to it from where the last block ended.
    const float n     = static_cast<float>(frames);
    const float inv   = 1.0f / n;
    const float glide = (glide_ * n < 1.0f) ? glide_ * n : 1.0f;
    const float fade  = (fade_ * n < 1.0f) ? fade_ * n : 1.0f;

    float preDelay = preDelay_;
    float input    = input_;
    float shimmer  = shimmer_;
    preDelay_ += (preDelayTarget_ - preDelay_) * glide;
    input_    += (inputTarget_ - input_) * fade;
    shimmer_  += (shimmerTarget_ - shimmer_) * fade;
    size_     += (sizeTarget_ - size_) * glide;
    modDepth_  = modTarget_;
    const float preDelayStep = (preDelay_ - preDelay) * inv;
    const float inputStep    = (input_ - input) * inv;
    const float shimmerStep  = (shimmer_ - shimmer) * inv;
    // Silent shifters are fed but not read.
    const bool  shimmerOn    = shimmer > 0.0f || shimmer_ > 0.0f;

    float delayEnd[kChannels];
    float delayStep[kChannels];
    float gainStep[kChannels];
    float poleStep[kChannels];
    for (uint32_t c = 0u; c < kChannels; c++)
    {
        modPhase_[c] += modStep_[c] * n;
        if (modPhase_[c] >= 1.0f) { modPhase_[c] -= 1.0f; }
        delayEnd[c]  = lineDelay(c);
        delayStep[c] = (delayEnd[c] - delay_[c]) * inv;
        gainStep[c]  = (gainTarget_[c] - gain_[c]) * inv;
        poleStep[c]  = (poleTarget_[c] - pole_[c]) * inv;
    }

    for (uint16_t i = 0; i < frames; i++)
    {
        const uint32_t pos = ++pos_;
        preDelay += preDelayStep;
        input    += inputStep;
        shimmer  += shimmerStep;

        // NaN stops here, before it can reach a line.
        const float l = inLeft[i];
        const float r = inRight[i];
        pre_[0].data[pos & pre_[0].mask] = (l == l) ? l : 0.0f;
        pre_[1].data[pos & pre_[1].mask] = (r == r) ? r : 0.0f;

        // Four copies of each side: half each keeps the energy of the pair.
        const uint32_t whole = static_cast<uint32_t>(preDelay);
        const float    frac  = preDelay - static_cast<float>(whole);
        const uint32_t tap   = pos - whole;
        const float*   pl    = pre_[0].data;
        const float*   pr    = pre_[1].data;
        const uint32_t pm    = pre_[0].mask;
        const float l0 = pl[tap & pm], l1 = pl[(tap - 1u) & pm];
        const float r0 = pr[tap & pm], r1 = pr[(tap - 1u) & pm];
        const float left  = 0.5f * input * (l0 + frac * (l1 - l0));
        const float right = 0.5f * input * (r0 + frac * (r1 - r0));
        float x[kChannels];
        for (uint32_t c = 0u; c < kChannels; c++) { x[c] = (c & 1u) ? right : left; }

        // Each diffusion step delays every channel by its own amount, shuffles and
        // flips them, and mixes them. Channel c takes delayed channel 3c + s: 3 is
        // odd, so that is a permutation of 8, and a different one on each step.
        for (uint32_t s = 0u; s < kDiffusionSteps; s++)
        {
            float delayed[kChannels];
            for (uint32_t c = 0u; c < kChannels; c++)
            {
                const Ring& ring = diffusion_[s][c];
                ring.data[pos & ring.mask] = x[c];
                delayed[c] = ring.data[(pos - diffusionDelay_[s][c]) & ring.mask];
            }
            for (uint32_t c = 0u; c < kChannels; c++)
            {
                const float v = delayed[(3u * c + s) & (kChannels - 1u)];
                x[c] = ((kDiffusionFlips[s] >> c) & 1u) ? -v : v;
            }
            hadamard8(x);
        }

        // Loop reads, as FracDelayLine::AllpassTap: the fraction is kept in
        // [0.5, 1.5), which keeps the pole well inside the unit circle.
        float y[kChannels];
        float sumLeft  = 0.0f;
        float sumRight = 0.0f;
        float sum      = 0.0f;
        for (uint32_t c = 0u; c < kChannels; c++)
        {
            delay_[c] += delayStep[c];
            const float    delay = delay_[c];
            const uint32_t base  = static_cast<uint32_t>(delay - 0.5f);
            const float    d     = delay - static_cast<float>(base);
            const float    a     = (1.0f - d) / (1.0f + d);
            const Ring&    line  = lines_[c];
            const uint32_t k     = pos - 1u - base;     // the newest sample is at pos - 1
            allpass_[c] = a * line.data[k & line.mask] + line.data[(k - 1u) & line.mask] - a * allpass_[c];
            y[c] = allpass_[c];
            sum += y[c];
            const float out = y[c] + kEarlyGain * x[c];
            if (c & 1u) { sumRight += out; } else { sumLeft += out; }
        }

        // Householder reflection: each line less a quarter of the sum.
        const float share = 0.25f * sum;
        for (uint32_t c = 0u; c < kChannels; c++)
        {
            float v = y[c] - share;
            if (c < kShimmerLines)
            {
                if (shimmerOn) { v += shimmer * (shifters_[c].process(v) - v); }
                else           { shifters_[c].feed(v); }
            }
            gain_[c] += gainStep[c];
            pole_[c] += poleStep[c];
            damp_[c] = pole_[c] * damp_[c] + gain_[c] * v;
            float w = x[c] + damp_[c];
            w = (w > kLoopCeiling) ? kLoopCeiling : ((w < -kLoopCeiling) ? -kLoopCeiling : w);
            lines_[c].data[pos & lines_[c].mask] = w;
        }

        outLeft[i]  = kOutputGain * sumLeft;
        outRight[i] = kOutputGain * sumRight;
    }

    // Land exactly on the block's end values, whatever rounding the steps gathered.
    for (uint32_t c = 0u; c < kChannels; c++)
    {
        delay_[c] = delayEnd[c];
        gain_[c]  = gainTarget_[c];
        pole_[c]  = poleTarget_[c];
    }
}

} // namespace dsp
