#pragma once

#include "engine.hpp"
#include "engine_ids.hpp"
#include "test_delay.hpp"

// TestDelay behind the engine interface. The mode field picks the repeat; the lines
// come from the large arena.
class TestDelayEngine final : public Engine {
public:

    static constexpr EngineInfo kInfo {
        engine_id::kTestDelay,
        TestDelay::kRepeatOptions,                      // modeOptions
        AUX_ROLE_TAP,
        0u,                                             // auxField
        true,                                           // usesTempo
        { ENGINE_MODE_FIELD_WIDTH },
        { 0u },
        { 0x6000u, 0x8000u, 0x8000u, 0x8000u, 0x8000u, 0x8000u, 0x8000u, 0x8000u },
        TestDelay::kParamMix,
        0u,
        2u * arenaBytes(TestDelay::kMaxFrames * sizeof(float)),
    };

    const EngineInfo& info() const override { return kInfo; }
    void activate(EngineArenas& arenas, uint32_t sampleRate) override;
    void deactivate() override {}
    void setControls(const EngineControls& controls) override;
    void process(dsp::ConstAudioBuffer in, dsp::AudioBuffer out, const BlockContext& ctx) override;

private:

    TestDelay      delay_;
    EngineControls controls_ {};
    // TestDelay takes tempo and engaged with the params, so a change to any of them
    // re-applies the set.
    bool           dirty_   = true;
    float          tempoHz_ = 0.0f;
    bool           engaged_ = false;
};
