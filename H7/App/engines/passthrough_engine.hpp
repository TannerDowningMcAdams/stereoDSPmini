#pragma once

#include "engine.hpp"
#include "engine_ids.hpp"

// Copies the input to the output. The mode field has three options that change
// nothing, so the mode switch and small LEDs can be checked without an effect. No
// blend param, so it runs fully wet.
class PassthroughEngine final : public Engine {
public:

    static constexpr EngineInfo kInfo {
        .id          = engine_id::kPassthrough,
        .modeOptions = 3u,
        .fields      = { { ENGINE_MODE_FIELD_WIDTH, 0u } },
    };

    const EngineInfo& info() const override { return kInfo; }
    void activate(EngineArenas&, uint32_t) override {}
    void process(dsp::ConstAudioBuffer in, dsp::AudioBuffer out, const BlockContext& ctx) override;
};
