#pragma once

#include "engine.hpp"
#include "engine_ids.hpp"

// Copies the input to the output. The mode field has three options that change
// nothing, so the mode switch and small LEDs can be checked without an effect.
class PassthroughEngine final : public Engine {
public:

    static constexpr EngineInfo kInfo {
        engine_id::kPassthrough,
        3u,                                             // modeOptions
        AUX_ROLE_NONE,
        0u,                                             // auxField
        false,                                          // usesTempo
        { ENGINE_MODE_FIELD_WIDTH },
        { 0u },
        { 0x8000u, 0x8000u, 0x8000u, 0x8000u, 0x8000u, 0x8000u, 0x8000u, 0x8000u },
        kEngineParamNone,                               // fully wet
        0u,
        0u,
    };

    const EngineInfo& info() const override { return kInfo; }
    void activate(EngineArenas&, uint32_t) override {}
    void deactivate() override {}
    void setControls(const EngineControls&) override {}
    void process(dsp::ConstAudioBuffer in, dsp::AudioBuffer out, const BlockContext& ctx) override;
};
