#pragma once

#include "analog_dry.hpp"
#include "audio_buffer.hpp"
#include "relay.hpp"
#include <cstdint>

// Owns the relays and the dry VCA. Boots unengaged: relays off, which is true bypass.
// Runs in the audio thread once per block, around the engine, and ramps every gain
// across the block so no change steps.
//
// True bypass mutes the output around relay switching. Trails bypass keeps the relays
// on, fades the engine input to zero and brings the dry path up, so tails ring out.
class BypassController {
public:

    struct Config
    {
        Relay::Config         relay;
        AnalogDryThru::Config dry;
        uint32_t              sampleRate;
        uint16_t              blockSize;    // at most kMaxBlockSize
    };

    static constexpr uint16_t kMaxBlockSize = 32;
    // For an engine without a blend param.
    static constexpr float    kNoBlend = -1.0f;

    BypassController() = default;
    ~BypassController() = default;

    void init(const Config& config);

    // Audio thread, when a new control set lands. blend: 0 = dry, 1 = wet, or kNoBlend.
    void setControls(uint16_t runFlags, float blend);

    // Before the engine: the input it should process (mono copy, input fade).
    dsp::ConstAudioBuffer beginBlock(dsp::ConstAudioBuffer input);
    // After the engine has written output: wet level, digital dry and mute. Moves the
    // VCA and the relays.
    void endBlock(dsp::AudioBuffer output);

private:

    // Linear ramp, advanced once per block and interpolated across it.
    struct Ramp
    {
        float value  = 0.0f;
        float target = 0.0f;
        float step   = 1.0f;

        // Returns the value at the start of the block.
        float advance()
        {
            const float start = value;
            if (value < target)      { value = (value + step < target) ? value + step : target; }
            else if (value > target) { value = (value - step > target) ? value - step : target; }
            return start;
        }
    };

    enum class RelayPhase : uint8_t { Off, Settling, On, Releasing, Draining };

    static constexpr float kFadeMs   = 20.0f;   // input fade, wet and dry levels, VCA
    static constexpr float kMuteMs   = 5.0f;    // output mute around relay switching
    static constexpr float kSettleMs = 10.0f;   // relay operate and bounce
    static constexpr float kDrainMs  = 3.0f;    // muted output still in the DMA and codec

    Relay         relay_;
    AnalogDryThru dry_;
    uint16_t      blockSize_ = 0;
    uint16_t      settleBlocks_ = 0;
    uint16_t      drainBlocks_  = 0;
    uint16_t      countdown_    = 0;

    bool engaged_   = false;
    bool trails_    = false;
    bool analogDry_ = false;
    bool stereoIn_  = true;
    float wetEngaged_ = 1.0f;
    float dryEngaged_ = 0.0f;

    RelayPhase phase_ = RelayPhase::Off;
    Ramp input_;
    Ramp wet_;
    Ramp digitalDry_;
    Ramp vca_;
    Ramp mute_;
    float vcaWritten_ = -1.0f;

    dsp::ConstAudioBuffer dryInput_ {};
    float engineLeft_  [kMaxBlockSize] = {};
    float engineRight_ [kMaxBlockSize] = {};

    void  updateTargets();
    void  stepRelays();
    uint16_t blocksFor(float ms, uint32_t sampleRate) const;
};
