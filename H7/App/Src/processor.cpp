#include "processor.hpp"
#include "audio_buffer.hpp"
#include "spi_protocol.h"
#include <cstdint>
#include "cmsis_compiler.h"

static float blendOf(const Engine& engine, const EngineControls& controls)
{
    const uint8_t blendParam = engine.info().blendParam;
    return (blendParam == kEngineParamNone) ? BypassController::kNoBlend : controls.params[blendParam];
}

void Processor::init(const Config& config)
{
    bypass_ = config.bypass;
    tempo_.init({ config.sampleRate, config.cyclesPerSecond });
}

void Processor::install(Engine* engine, const EngineControls& controls)
{
    engine_ = engine;
    engine_->setControls(controls);
    blend_ = blendOf(*engine_, controls);
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

    // Runs whatever the engine, so a tempo engine starts on the beat.
    const int32_t beat = tempo_.advance(input.size());

    // Block Processing here, on the input the bypass controller hands over.
    const dsp::ConstAudioBuffer engineInput = bypass_->beginBlock(input);
    if (!parked_ && engine_ != nullptr)
    {
        const BlockContext ctx { beat, activeControls_.tempoHz, (activeControls_.runFlags & RUN_FLAG_ENGAGED) != 0u };
        engine_->process(engineInput, output, ctx);
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

// While parked_ is set, thread mode owns engine_ and PendSV does not call it.
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
    applyEngineControls(0u);
    updateBypass();
}

void Processor::updateAlgorithmParams(uint32_t blockCycles)
{
    const ProcessorControls& c = activeControls_;
    tempo_.update(c.tempoHz, c.tempoPhase, c.tempoEdgeCycles, c.frameSeq, blockCycles);

    // The first set only takes the baseline, so a restarted H7 sees no stale events.
    const uint16_t edges = togglesSeen_ ? static_cast<uint16_t>(c.eventToggles ^ lastToggles_) : 0u;
    lastToggles_ = c.eventToggles;
    togglesSeen_ = true;

    applyEngineControls(edges);
    updateBypass();
}

// A set reaches the engine only when it was applied for that engine, so a new
// preset's values never drive the engine it replaces (plan §4.1).
void Processor::applyEngineControls(uint16_t edges)
{
    const ProcessorControls& c = activeControls_;
    if (parked_ || engine_ == nullptr || c.engineId != engine_->id()) { return; }

    EngineControls controls;
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { controls.params[i] = c.params[i]; }
    controls.discrete   = c.discrete;
    controls.eventEdges = edges;
    engine_->setControls(controls);
    blend_ = blendOf(*engine_, controls);
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
