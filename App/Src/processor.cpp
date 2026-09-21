#include "processor.hpp"
#include "audio_buffer.hpp"
#include <cstdint>
#include <cstring>
#include "cmsis_compiler.h"

void Processor::init(uint32_t sampleRate)
{
    sampleRate_ = sampleRate;
    samplePeriod_ = 1.0f / sampleRate;

    // Initialize DSP, buffers, etc
}

void Processor::processAudioBlock(dsp::AudioBuffer input, dsp::AudioBuffer output)
{
    if (controlsReady_)
    {
        __DMB();    // acquire: flag read before payload read
        activeControls_ = pendingControls_;
        updateAlgorithmParams();
        __DMB();    // payload consumed before the channel reopens
        controlsReady_ = false;
    }


    // Block Processing here

    processLeftRight(input, output);
    
}

// Per-Sample processing for non-block processing
void Processor::processLeftRight(dsp::AudioBuffer input, dsp::AudioBuffer output)
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
    // e.g. filter_cutoff = 20000.0f * activeControls_.potentiometers[0];
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