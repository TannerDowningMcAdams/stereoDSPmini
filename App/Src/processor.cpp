#include "processor.hpp"
#include "audio_buffer.hpp"
#include <cstdint>
#include <cstring>

void Processor::init(uint32_t sampleRate)
{
    sampleRate_ = sampleRate;
    samplePeriod_ = 1.0f / sampleRate;

    // Initialize DSP, buffers, etc
}

void Processor::processAudioBlock(dsp::AudioBuffer input, dsp::AudioBuffer output)
{
    if(controlsReady_)
    {
        activeControls_ = pendingControls_;
        updateAlgorithmParams();
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

void Processor::pushControls(const ProcessorControls &controls)
{
    pendingControls_ = controls;
    controlsReady_ = true;
}