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

    // Initialize DSP, buffers, etc
}

void Processor::processAudioBlock(dsp::ConstAudioBuffer input, dsp::AudioBuffer output)
{
    if (controlsReady_)
    {
        __DMB();    // acquire: flag read before payload read
        activeControls_ = pendingControls_;
        updateAlgorithmParams();
        __DMB();    // payload consumed before the channel reopens
        controlsReady_ = false;
    }

    // Block Processing here, on the input the bypass controller hands over.
    const dsp::ConstAudioBuffer engineInput = bypass_->beginBlock(input);
    processLeftRight(engineInput, output);
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

void Processor::updateAlgorithmParams()
{
    // map and assign algorithm parameters
    // e.g. filter_cutoff = 20000.0f * activeControls_.params[0];

    // The only engine until the engine host (H5) selects among them.
    const uint8_t blendParam = kEngineManifest[ENGINE_PASSTHROUGH].blendParam;
    const float blend = (blendParam == ENGINE_PARAM_NONE) ? BypassController::kNoBlend
                                                          : activeControls_.params[blendParam];
    bypass_->setControls(activeControls_.runFlags, blend);
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