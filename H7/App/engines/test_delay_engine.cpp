#include "test_delay_engine.hpp"

void TestDelayEngine::activate(EngineArenas& arenas, uint32_t sampleRate)
{
    delay_.init({ sampleRate, &arenas.large });
}

void TestDelayEngine::process(dsp::ConstAudioBuffer in, dsp::AudioBuffer out, const BlockContext& ctx)
{
    delay_.setControls(ctx.params, ctx.field[kFieldRepeat], ctx.tempoHz, ctx.engaged);
    delay_.process(in, out, ctx.beatIndex);
}
