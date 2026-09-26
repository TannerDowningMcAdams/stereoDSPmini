#pragma once

#include "engine.hpp"
#include "engine_ids.hpp"
#include "test_delay.hpp"

// TestDelay behind the engine interface, and the template for a new engine (plan §4.4,
// "Writing an engine"). The mode field picks the repeat; the lines come from the large
// arena.
class TestDelayEngine final : public Engine {
public:

    enum Field : uint8_t { kFieldRepeat = 0 };      // the mode field

    static constexpr EngineInfo kInfo {
        .id           = engine_id::kTestDelay,
        .modeOptions  = TestDelay::kRepeatOptions,
        .auxRole      = AUX_ROLE_TAP,
        .usesTempo    = true,
        .fields       = { { ENGINE_MODE_FIELD_WIDTH, 0u } },
        .paramDefault = paramDefaults(0x6000u),      // feedback; mix and click at half
        .blendParam   = TestDelay::kParamMix,
        .largeBytes   = TestDelay::kLargeBytes,
    };

    const EngineInfo& info() const override { return kInfo; }
    void activate(EngineArenas& arenas, uint32_t sampleRate) override;
    void process(dsp::ConstAudioBuffer in, dsp::AudioBuffer out, const BlockContext& ctx) override;

private:

    TestDelay delay_;
};
