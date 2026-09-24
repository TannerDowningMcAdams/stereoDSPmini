#include "processor.hpp"
#include "audio_buffer.hpp"
#include "engine_manifest.h"
#include <cstdint>
#include <cstring>
#include "cmsis_compiler.h"

void Processor::init(const Config& config)
{
    sampleRate_ = config.sampleRate;
    samplePeriod_ = 1.0f / config.sampleRate;
    bypass_ = config.bypass;

    tempo_.init({ config.sampleRate, config.cyclesPerSecond });
    testDelay_.init({ config.sampleRate });
    engine_ = ENGINE_PASSTHROUGH;
}

void Processor::processAudioBlock(dsp::ConstAudioBuffer input, dsp::AudioBuffer output, uint32_t blockCycles)
{
    if (controlsReady_)
    {
        __DMB();    // acquire: flag read before payload read
        activeControls_ = pendingControls_;
        updateAlgorithmParams(blockCycles);
        __DMB();    // payload consumed before the channel reopens
        controlsReady_ = false;
    }

    // Runs whatever the engine, so a tempo engine starts on the beat.
    const int32_t beat = tempo_.advance(input.size());

    // Block Processing here, on the input the bypass controller hands over.
    const dsp::ConstAudioBuffer engineInput = bypass_->beginBlock(input);
    if (engine_ == ENGINE_TEST_DELAY) { testDelay_.process(engineInput, output, beat); }
    else                              { processLeftRight(engineInput, output); }
    bypass_->endBlock(output);
}

// Per-Sample processing for non-block processing
void Processor::processLeftRight(dsp::ConstAudioBuffer input, dsp::AudioBuffer output)
{

    for(uint16_t i = 0; i < input.size(); i++)
    {
        float left = input.leftAt(i);
        float right = input.rightAt(i);

        output.setLeft(i, left);
        output.setRight(i,  right);
    }

}

void Processor::updateAlgorithmParams(uint32_t blockCycles)
{
    // map and assign algorithm parameters
    // e.g. filter_cutoff = 20000.0f * activeControls_.params[0];

    const ProcessorControls& c = activeControls_;
    if (c.engine != engine_ && c.engine == ENGINE_TEST_DELAY) { testDelay_.reset(); }
    engine_ = (c.engine < ENGINE_COUNT) ? c.engine : static_cast<uint8_t>(ENGINE_PASSTHROUGH);

    tempo_.update(c.tempoHz, c.tempoPhase, c.tempoEdgeCycles, c.frameSeq, blockCycles);
    if (engine_ == ENGINE_TEST_DELAY)
    {
        testDelay_.setControls(c.params, c.discrete, c.tempoHz, (c.runFlags & RUN_FLAG_ENGAGED) != 0u);
    }

    const uint8_t blendParam = kEngineManifest[engine_].blendParam;
    const float blend = (blendParam == ENGINE_PARAM_NONE) ? BypassController::kNoBlend
                                                          : c.params[blendParam];
    bypass_->setControls(c.runFlags, blend);
}

bool Processor::pushControls(const ProcessorControls &controls)
{
    // Unconsumed: the DSP may be mid-copy, so writing now would tear the set.
    if (controlsReady_) { return false; }

    pendingControls_ = controls;
    __DMB();    // release: payload visible before the flag that publishes it
    controlsReady_ = true;
    return true;
}
