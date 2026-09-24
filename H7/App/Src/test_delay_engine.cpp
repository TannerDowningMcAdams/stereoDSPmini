#include "test_delay_engine.hpp"

static constexpr uint32_t kDescriptor = TestDelayEngine::kInfo.descriptor();

void TestDelayEngine::activate(EngineArenas& arenas, uint32_t sampleRate)
{
    float* left  = arenas.large.allocate<float>(TestDelay::kMaxFrames);
    float* right = arenas.large.allocate<float>(TestDelay::kMaxFrames);
    delay_.init({ sampleRate, left, right });
    dirty_ = true;
}

void TestDelayEngine::setControls(const EngineControls& controls)
{
    controls_ = controls;
    dirty_    = true;
}

void TestDelayEngine::process(dsp::ConstAudioBuffer in, dsp::AudioBuffer out, const BlockContext& ctx)
{
    if (dirty_ || ctx.tempoHz != tempoHz_ || ctx.engaged != engaged_)
    {
        tempoHz_ = ctx.tempoHz;
        engaged_ = ctx.engaged;
        dirty_   = false;
        const uint8_t repeat = engineFieldGet(kDescriptor,controls_.discrete, ENGINE_MODE_FIELD);
        delay_.setControls(controls_.params, repeat, tempoHz_, engaged_);
    }
    delay_.process(in, out, ctx.beatIndex);
}
