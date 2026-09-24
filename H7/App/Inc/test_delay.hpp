#pragma once

#include "audio_buffer.hpp"
#include <cstdint>

// ENGINE_TEST_DELAY: a tempo-synced stereo delay with a click on each beat, for the
// M3 bench check. The repeats show that tempoHz arrived; the click, placed by the
// TempoFollower, shows that the phase matches the G0's LED. Outputs wet only: the
// bypass controller mixes the dry path by the mix param.
class TestDelay {
public:

    struct Config
    {
        uint32_t sampleRate;
    };

    // One second of stereo float in AXI SRAM. A repeat longer than that is halved
    // until it fits, so it stays on the beat grid.
    static constexpr uint32_t kMaxFrames   = 48128;
    // Hann-windowed click, 0.5 ms wide, so it carries no step and little energy
    // above a few kHz.
    static constexpr uint16_t kClickFrames = 24;
    static constexpr float    kClickPeak   = 0.5f;
    static constexpr float    kMaxFeedback = 0.9f;
    // Repeat time used until a tempo is set: 120 BPM.
    static constexpr float    kDefaultHz   = 2.0f;

    TestDelay() = default;
    ~TestDelay() = default;

    void init(const Config& config);

    // Forgets the buffer contents, for when the engine becomes active. O(1): samples
    // not written since are read as silence.
    void reset();

    // Audio thread, when a control set lands. params are 0..1.
    void setControls(const float* params, uint16_t discrete, float tempoHz, bool engaged);

    // beatIndex is the sample at which a beat falls in this block, or -1.
    void process(dsp::ConstAudioBuffer input, dsp::AudioBuffer output, int32_t beatIndex);

    // For the host tests.
    float delayFrames() const { return target_ + offset_; }
    static float clickSample(uint16_t index);

private:

    float    sampleRate_ = 48000.0f;
    uint32_t write_      = 0;
    uint32_t written_    = 0;       // since reset(), saturating at kMaxFrames
    // The current delay is target_ + offset_, and offset_ decays to zero. Gliding
    // the delay itself would stall frames short of the target once each step fell
    // below half an ulp of a value near 20000.
    float    target_     = 0.0f;
    float    offset_     = 0.0f;
    float    decay_      = 0.0f;    // per sample
    float    feedback_   = 0.0f;
    float    clickLevel_ = 0.0f;
    bool     clickOn_    = false;
    uint16_t clickPos_   = kClickFrames;    // kClickFrames = idle

    static float left_[kMaxFrames];
    static float right_[kMaxFrames];
    static float clickTable_[kClickFrames];

    float read(const float* line, float delay) const;
};
