#pragma once
#include "dsp_math.hpp"
#include "frac_delay_line.hpp"
#include "pitch_shifter.hpp"
#include <cstdint>

namespace dsp {

// Buffer layout of FdnReverb, at namespace scope so its size is a constant the
// engine can declare before the class is complete.
namespace fdn_layout {

constexpr uint32_t kChannels       = 8u;
constexpr uint32_t kDiffusionSteps = 4u;
constexpr uint32_t kShimmerLines   = 4u;
// Buffers are sized for this rate; above it, delays are clamped to the buffers. The
// codec runs at 48077 Hz.
constexpr uint32_t kMaxSampleRate  = 48128u;

constexpr float kMaxPreDelayMs   = 150.0f;
constexpr float kMaxModMs        = 1.0f;
constexpr float kShimmerWindowMs = 50.0f;
// Each diffusion step spreads its channels across this range.
constexpr float kDiffusionMs[kDiffusionSteps] = { 3.0f, 6.0f, 12.0f, 24.0f };
// Loop lengths at full size: 60 ms * 2^(c/8). The ratios are irrational, so no two
// lines share a period and the modes of the loop spread evenly.
constexpr float kLineMs[kChannels] = { 60.0f, 65.43f, 71.35f, 77.81f, 84.85f, 92.53f, 100.91f, 110.04f };

constexpr uint32_t samplesFor(float ms)
{
    return static_cast<uint32_t>(ms * (static_cast<float>(kMaxSampleRate) / 1000.0f)) + 1u;
}

// Every buffer is a power of two with FracDelayLine's guard, so an index wraps with
// a mask and a read two samples past the longest delay stays inside.
constexpr uint32_t preDelayFloats() { return FracDelayLine::sizeFor(samplesFor(kMaxPreDelayMs)); }

// Channel c of a step reaches up to (c + 1) / kChannels of the step's range.
constexpr uint32_t diffusionFloats(uint32_t step, uint32_t channel)
{
    return FracDelayLine::sizeFor(samplesFor(kDiffusionMs[step] * static_cast<float>(channel + 1u) /
                                             static_cast<float>(kChannels)));
}

constexpr uint32_t lineFloats(uint32_t channel) { return FracDelayLine::sizeFor(samplesFor(kLineMs[channel] + kMaxModMs)); }

constexpr uint32_t shifterFloats() { return PitchShifter::sizeFor(samplesFor(kShimmerWindowMs)); }

// The rings in cached memory: two pre-delays, the loop lines and the shifters.
constexpr uint32_t kCachedRings = 2u + kChannels + kShimmerLines;
// Padding ahead of each cached ring. Every ring is written at the same index each
// sample, and every cached ring is a multiple of 4 KB, the H7 D-cache's way size,
// so back to back they would all write into one 4-way cache set and evict each
// other every sample. Nine 32-byte lines apart, the 14 write positions fall in 14
// different sets of the 128.
constexpr uint32_t kStaggerFloats = 9u * 32u / sizeof(float);

// Diffusion: 32 small rings touched twice per sample, for tightly coupled memory,
// where there is no cache to contend for.
constexpr uint32_t fastFloats()
{
    uint32_t floats = 0u;
    for (uint32_t s = 0u; s < kDiffusionSteps; s++)
    {
        for (uint32_t c = 0u; c < kChannels; c++) { floats += diffusionFloats(s, c); }
    }
    return floats;
}

constexpr uint32_t largeFloats()
{
    uint32_t floats = 2u * preDelayFloats() + kShimmerLines * shifterFloats() + kCachedRings * kStaggerFloats;
    for (uint32_t c = 0u; c < kChannels; c++) { floats += lineFloats(c); }
    return floats;
}

} // namespace fdn_layout

// Stereo reverb on an 8-line feedback delay network, after Geraint Luff's design
// ("Let's Write a Reverb", ADC 2021) with Jot's absorptive damping:
//
//   pre-delay -> diffuser -> loop of 8 modulated delay lines -> output
//
// - The diffuser is 4 steps of 8 short delays, each step followed by a shuffle,
//   polarity flips and a Hadamard mix, so one input sample leaves as 8^4 echoes
//   within 45 ms and the onset is dense from the start.
// - The loop mixes its 8 line outputs through a Householder reflection and feeds them
//   back through one-pole damping filters. Each filter's DC and Nyquist gains are
//   set from the line's own length, so every line loses the same dB per second and
//   the decay time holds at any size: setDecay() is the low-frequency T60 and
//   setDamping() the high-frequency T60 as a fraction of it.
// - Each line's read is swept by its own slow sine, which breaks up the metallic
//   ringing of a static loop and adds a slight chorus to the tail. The reads are
//   first-order allpass interpolators, flat at every fraction: a cubic read loses
//   treble on every pass, which darkens the tail well beyond the set damping and
//   drains a frozen one.
// - Shimmer crossfades part of the feedback on 4 lines through pitch shifters, so on
//   every pass round the loop the tail climbs by the interval. Being a crossfade, it
//   leaves the loop gain unchanged, and the loop stays stable at any setting.
// - Freeze makes the loop lossless and fades the input out of it.
//
// Left feeds the even lines and right the odd ones, and each output side sums the
// same lines, so the mixes decorrelate the two sides. Outputs wet only.
//
// Controls move once per block and ramp linearly across it; the sines are evaluated
// once per block. The sample loop indexes every buffer from one shared counter and
// is written out without helper calls: the H7 Debug build runs at -Og, which inlines
// nothing, and a call per delay read put the engine over the block budget.
//
// The caller owns the memory: kFastFloats floats for the diffuser, from the fast
// arena, and kLargeFloats for the rest, from the large arena.
class FdnReverb {
public:

    struct Config
    {
        uint32_t sampleRate;
        float*   fast;          // kFastFloats floats
        float*   large;         // kLargeFloats floats
    };

    static constexpr uint32_t kChannels     = fdn_layout::kChannels;
    static constexpr uint32_t kShimmerLines = fdn_layout::kShimmerLines;
    static constexpr uint32_t kFastFloats   = fdn_layout::fastFloats();
    static constexpr uint32_t kLargeFloats  = fdn_layout::largeFloats();

    static constexpr float kMinDecaySeconds = 0.1f;
    static constexpr float kMaxDecaySeconds = 60.0f;
    static constexpr float kMinDamping      = 0.02f;
    static constexpr float kMinSize         = 0.25f;
    static constexpr float kMaxPreDelayMs   = fdn_layout::kMaxPreDelayMs;

    // Clears the memory. Starts at a 2 s decay, no damping, full size, no pre-delay,
    // modulation or shimmer, and snaps to whatever is set before the first process().
    void init(const Config& config);

    // Every block, before process(). Values are clamped to the limits above, and
    // setting the value already set does nothing. Size and pre-delay glide, which
    // bends the pitch of the tail as a tape delay would; the rest ramp across a
    // block or fade over a few tens of ms.
    void setDecay(float seconds);
    void setDamping(float ratio);           // high-frequency T60 / low-frequency T60
    void setSize(float size);               // kMinSize..1: scales the loop lengths
    void setPreDelayMs(float ms);
    void setModulation(float depth);        // 0..1: up to 1 ms of sweep on each line
    void setShimmer(float amount);          // 0..1: share of the feedback pitch-shifted
    void setShimmerSemitones(float semitones);
    void setFreeze(bool freeze);

    // output may not alias input.
    void process(const float* inLeft, const float* inRight, float* outLeft, float* outRight, uint16_t frames);

    float decay()   const { return decay_; }
    float damping() const { return damping_; }
    float size()    const { return sizeTarget_; }
    bool  frozen()  const { return freeze_; }

private:

    // A power-of-two circular buffer, indexed by the shared counter.
    struct Ring
    {
        float*   data = nullptr;
        uint32_t mask = 0;
    };

    void  updateLoop();
    // Read delay of line c at the current size, depth and modulation phase, within
    // the line's buffer.
    float lineDelay(uint32_t c) const;

    Ring     pre_[2];
    Ring     diffusion_[fdn_layout::kDiffusionSteps][kChannels];
    uint32_t diffusionDelay_[fdn_layout::kDiffusionSteps][kChannels] = {};
    Ring     lines_[kChannels];
    PitchShifter shifters_[kShimmerLines];
    uint32_t pos_ = 0;                      // index of the newest sample, before the mask

    float lineSamples_[kChannels] = {};     // loop lengths at full size
    float maxDelay_[kChannels]    = {};     // longest read each line's buffer allows
    float delay_[kChannels]       = {};     // read delays, ramped per sample
    float allpass_[kChannels]     = {};     // allpass interpolator states
    float damp_[kChannels]        = {};     // damping filter states
    float modPhase_[kChannels]    = {};     // cycles
    float modStep_[kChannels]     = {};     // cycles per sample
    // Damping filter y = pole * y + gain * x, ramped across each block.
    float gainTarget_[kChannels]  = {};
    float poleTarget_[kChannels]  = {};
    float gain_[kChannels]        = {};
    float pole_[kChannels]        = {};

    float sampleRate_     = 48000.0f;
    float msToSamples_    = 48.0f;
    float preDelayMax_    = 0.0f;
    float glide_          = 0.0f;           // per-sample rate, size and pre-delay
    float fade_           = 0.0f;           // per-sample rate, input and shimmer
    float decay_          = 2.0f;
    float damping_        = 1.0f;
    float sizeTarget_     = 1.0f;
    float size_           = 1.0f;
    float preDelayTarget_ = 0.0f;           // samples
    float preDelay_       = 0.0f;
    float modTarget_      = 0.0f;           // samples
    float modDepth_       = 0.0f;
    float shimmerAmount_  = 0.0f;           // as set; withdrawn while frozen
    float shimmerTarget_  = 0.0f;
    float shimmer_        = 0.0f;
    float inputTarget_    = 1.0f;
    float input_          = 1.0f;
    bool  freeze_         = false;
    bool  loopDirty_      = true;
    bool  snap_           = true;
};

} // namespace dsp
