#include "test_delay.hpp"
#include "engine_manifest.h"
#include <cmath>

// The CubeMX linker script has no NOLOAD section for AXI SRAM, so the lines are
// declared %nobits: loadable contents would put 385 KB of zeros into the image. The
// startup code does not clear AXI SRAM, which powers up with random data and ECC
// words (AN5342), so init() writes every word once.
#if defined(__arm__)
#define AXI_NOINIT __attribute__((section(".RAM_AXI0,\"aw\",%nobits@")))
#else
#define AXI_NOINIT
#endif

AXI_NOINIT float TestDelay::left_[TestDelay::kMaxFrames];
AXI_NOINIT float TestDelay::right_[TestDelay::kMaxFrames];
float TestDelay::clickTable_[TestDelay::kClickFrames];

// Repeat per mode field value: quarter, dotted eighth, eighth.
static constexpr float kSubdivision[ENGINE_MAX_MODE_OPTIONS] = { 1.0f, 0.75f, 0.5f, 0.5f };

// Glide time for a change of repeat time. The pitch bend while it glides is the
// audible sign that a new tempo arrived.
static constexpr float kGlideSeconds = 0.05f;

// Tanh-like limit on what re-enters the line: unity gain for small signals, and
// never beyond +-1, so no feedback setting or stray value can build up. NaN
// fails both comparisons and is dropped.
static float limitLoop(float x)
{
    if (!(x > -3.0f) || !(x < 3.0f))
    {
        return (x != x) ? 0.0f : ((x > 0.0f) ? 1.0f : -1.0f);
    }
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

void TestDelay::init(const Config& config)
{
    sampleRate_ = static_cast<float>(config.sampleRate);
    decay_ = std::exp(-1.0f / (kGlideSeconds * sampleRate_));

    // Endpoints excluded, so every sample is inside the window and none is zero.
    constexpr float kTwoPi = 6.28318530718f;
    for (uint16_t i = 0; i < kClickFrames; i++)
    {
        clickTable_[i] = 0.5f - 0.5f * std::cos(kTwoPi * (i + 1u) / (kClickFrames + 1u));
    }

    for (uint32_t i = 0; i < kMaxFrames; i++)
    {
        left_[i]  = 0.0f;
        right_[i] = 0.0f;
    }

    target_ = sampleRate_ / kDefaultHz;
    offset_ = 0.0f;
    reset();
}

void TestDelay::reset()
{
    write_    = 0u;
    written_  = 0u;
    clickPos_ = kClickFrames;
}

float TestDelay::clickSample(uint16_t index)
{
    return (index < kClickFrames) ? clickTable_[index] : 0.0f;
}

void TestDelay::setControls(const float* params, uint16_t discrete, float tempoHz, bool engaged)
{
    const EngineManifest& manifest = kEngineManifest[ENGINE_TEST_DELAY];
    const DiscreteField&  mode     = manifest.field[ENGINE_MODE_FIELD];
    const uint16_t option = (discrete >> mode.offset) & ((1u << mode.width) - 1u);

    const float hz = (tempoHz > 0.0f) ? tempoHz : kDefaultHz;
    float frames = sampleRate_ * kSubdivision[option] / hz;
    while (frames > kMaxFrames - 2u) { frames *= 0.5f; }
    // The delay heard now stays put; only the target moves.
    offset_ += target_ - frames;
    target_  = frames;

    feedback_   = params[TEST_DELAY_PARAM_FEEDBACK] * kMaxFeedback;
    clickLevel_ = params[TEST_DELAY_PARAM_CLICK] * kClickPeak;
    // Bypassed with trails, the repeats ring out but the metronome stops.
    clickOn_    = engaged && tempoHz > 0.0f;
}

// Linear interpolation between the two frames around the delay. Anything older than
// what has been written since reset() is silence.
float TestDelay::read(const float* line, float delay) const
{
    const uint32_t whole = static_cast<uint32_t>(delay);
    if (whole + 1u > written_) { return 0.0f; }
    const float frac = delay - static_cast<float>(whole);
    const uint32_t a = (write_ + kMaxFrames - whole) % kMaxFrames;
    const uint32_t b = (a + kMaxFrames - 1u) % kMaxFrames;
    return line[a] + frac * (line[b] - line[a]);
}

void TestDelay::process(dsp::ConstAudioBuffer input, dsp::AudioBuffer output, int32_t beatIndex)
{
    for (uint16_t i = 0; i < input.size(); i++)
    {
        if (static_cast<int32_t>(i) == beatIndex && clickOn_) { clickPos_ = 0u; }

        offset_ *= decay_;
        // Cut off before it reaches the denormal range.
        if (std::fabs(offset_) < 1e-4f) { offset_ = 0.0f; }
        const float delay = target_ + offset_;
        const float wetL = read(left_, delay);
        const float wetR = read(right_, delay);

        left_[write_]  = limitLoop(input.leftAt(i)  + feedback_ * wetL);
        right_[write_] = limitLoop(input.rightAt(i) + feedback_ * wetR);
        write_ = (write_ + 1u) % kMaxFrames;
        if (written_ < kMaxFrames) { written_++; }

        // The click goes to the output only, so its repeats do not blur the reference.
        float click = 0.0f;
        if (clickPos_ < kClickFrames)
        {
            click = clickLevel_ * clickTable_[clickPos_];
            clickPos_++;
        }
        output.setLeft(i,  wetL + click);
        output.setRight(i, wetR + click);
    }
}
