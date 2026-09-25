#include "processor.hpp"
#include "audio_buffer.hpp"
#include "engine_controls.hpp"
#include "spi_protocol.h"
#include <cstdint>
#include "cmsis_compiler.h"

void Processor::init(const Config& config)
{
    bypass_ = config.bypass;
    tempo_.init({ config.sampleRate, config.cyclesPerSecond });
}

void Processor::install(Engine* engine, const uint16_t* param, uint16_t discrete)
{
    float params[SPI_PARAM_COUNT];
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { params[i] = param[i] * (1.0f / 65535.0f); }
    engine_ = engine;
    beginControls(engine_->info(), params, discrete, ctx_);
    updateBlend();
}

void Processor::resume()
{
    __DMB();    // release: the installed engine is complete before PendSV runs it
    parkRequest_ = false;
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

    followPark();

    // Read before advance(), which moves the phase to the next block. Runs whatever
    // the engine, so a tempo engine starts on the beat.
    const float   phase = tempo_.running() ? tempo_.phase() : 0.0f;
    const int32_t beat  = tempo_.advance(input.size());

    // Block Processing here, on the input the bypass controller hands over.
    const dsp::ConstAudioBuffer engineInput = bypass_->beginBlock(input);
    if (!parked_ && engine_ != nullptr)
    {
        ctx_.beatIndex = beat;
        ctx_.beatPhase = phase;
        ctx_.tempoHz   = activeControls_.tempoHz;
        ctx_.engaged   = (activeControls_.runFlags & RUN_FLAG_ENGAGED) != 0u;
        engine_->process(engineInput, output, ctx_);
        finishBlock(ctx_);
    }
    else
    {
        for (uint16_t i = 0; i < output.size(); i++)
        {
            output.setLeft(i, 0.0f);
            output.setRight(i, 0.0f);
        }
    }
    bypass_->endBlock(output);
}

// While parked_ is set, thread mode owns engine_ and ctx_, and PendSV does not call it.
void Processor::followPark()
{
    if (parkRequest_)
    {
        bypass_->setEngineParked(true);
        // The wet fade ends with a block, so the engine stops from the next one.
        if (!parked_ && bypass_->wetSilent()) { parked_ = true; }
        return;
    }
    if (!parked_) { return; }

    __DMB();    // acquire: pairs with resume()
    parked_ = false;
    bypass_->setEngineParked(false);
    applyEngineControls();
    updateBypass();
}

void Processor::updateAlgorithmParams(uint32_t blockCycles)
{
    const ProcessorControls& c = activeControls_;
    tempo_.update(c.tempoHz, c.tempoPhase, c.tempoEdgeCycles, c.frameSeq, blockCycles);
    applyEngineControls();
    updateBypass();
}

// A set reaches the engine only when it was applied for that engine, so a new
// preset's values never drive the engine it replaces (plan §4.1).
void Processor::applyEngineControls()
{
    const ProcessorControls& c = activeControls_;
    if (parked_ || engine_ == nullptr || c.engineId != engine_->id()) { return; }

    applyControls(engine_->info(), c.params, c.discrete, ctx_);
    updateBlend();
}

void Processor::updateBlend()
{
    const uint8_t blendParam = engine_->info().blendParam;
    blend_ = (blendParam == kEngineParamNone) ? BypassController::kNoBlend : ctx_.params[blendParam];
}

void Processor::updateBypass()
{
    bypass_->setControls(activeControls_.runFlags, blend_);
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
