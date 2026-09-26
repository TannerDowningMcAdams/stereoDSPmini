#pragma once

#include "engine.hpp"
#include "engine_ids.hpp"
#include "envelope_follower.hpp"
#include "fdn_reverb.hpp"
#include "one_pole.hpp"
#include "param_ramp.hpp"

// A stereo hall reverb on dsp::FdnReverb, with three characters on the mode switch
// and freeze on AUX. Param 3 is the character's own control:
//   Hall:    depth of the chorus in the tail.
//   Shimmer: share of the tail pitch-shifted on each pass round the loop, by the
//            interval on param 6, so the tail climbs octave on octave.
//   Duck:    how far the wet level drops under the playing. The Q-Tron detector
//            follows the input; the tail keeps building underneath and swells up
//            in the gaps.
// Freeze holds the tail: the loop turns lossless, the input fades out of it, and
// shimmer is withdrawn so the held chord keeps its pitch. With trails bypass, a
// frozen chord keeps sounding under the dry signal until freeze is released.
class ShimmerReverbEngine final : public Engine {
public:

    enum Field : uint8_t { kFieldMode = 0, kFieldFreeze };
    enum Mode  : uint8_t { kModeHall = 0, kModeShimmer, kModeDuck, kModeCount };
    // Knobs first: on the mini board params 0-4 are knobs and 5-7 are MIDI only.
    enum Param : uint8_t
    {
        kParamDecay = 0,    // 0.3 s .. 20 s
        kParamMix,
        kParamTone,         // dark .. flat decay across the band
        kParamCharacter,    // per mode, above
        kParamSize,
        kParamPreDelay,     // 0 .. 150 ms
        kParamInterval,     // shimmer interval, in steps of kIntervals
        kParamLowCut,       // 20 .. 400 Hz, into the reverb
    };

    static constexpr uint32_t kFastBytes  = arenaBytes(dsp::FdnReverb::kFastFloats * sizeof(float));
    static constexpr uint32_t kLargeBytes = arenaBytes(dsp::FdnReverb::kLargeFloats * sizeof(float));

    static constexpr EngineInfo kInfo {
        .id           = engine_id::kShimmerReverb,
        .modeOptions  = kModeCount,
        .auxRole      = AUX_ROLE_TOGGLE,
        .auxField     = kFieldFreeze,
        .fields       = { { ENGINE_MODE_FIELD_WIDTH, kModeHall }, { 1u, 0u } },
        // 2.4 s decay, a third wet, mid tone and character, 70% size, 14 ms
        // pre-delay, an octave up, 60 Hz low cut.
        .paramDefault = paramDefaults(0x8000u, 0x5800u, 0x8000u, 0x8000u, 0xB000u, 0x1800u, 0xA400u, 0x6000u),
        .blendParam   = kParamMix,
        .fastBytes    = kFastBytes,
        .largeBytes   = kLargeBytes,
    };

    const EngineInfo& info() const override { return kInfo; }
    void activate(EngineArenas& arenas, uint32_t sampleRate) override;
    void process(dsp::ConstAudioBuffer in, dsp::AudioBuffer out, const BlockContext& ctx) override;

private:

    // The block is taken in chunks of at most this many frames, which bounds the
    // scratch buffers.
    static constexpr uint16_t kChunk = 32;

    void applyControls(const BlockContext& ctx);

    dsp::FdnReverb        reverb_;
    dsp::OnePole          lowCutLeft_;
    dsp::OnePole          lowCutRight_;
    dsp::EnvelopeFollower follower_;
    dsp::ParamRamp        duck_;
    float                 duckTarget_ = 0.0f;

    float left_[kChunk]     = {};
    float right_[kChunk]    = {};
    float envelope_[kChunk] = {};
};
