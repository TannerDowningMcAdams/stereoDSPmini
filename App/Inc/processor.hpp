#pragma once

#include "audio_buffer.hpp"
#include "ui_params.hpp"
using dsp::AudioBuffer;

#include <cstdint>

struct ProcessorControls{

    float potentiometers[5];
    uint8_t effectMode;
    float beatsPerSecond;
    uint16_t clockPhase;

};

class Processor {
public:

    Processor() = default;
    ~Processor() = default;

    //enum class AudioStatus { BUSY, READY, ERROR };

    void init(uint32_t sampleRate);

    void processAudioBlock(dsp::AudioBuffer input, dsp::AudioBuffer output);
    void processLeftRight(dsp::AudioBuffer input, dsp::AudioBuffer output);
    void pushControls(const ProcessorControls &controls);
    
private:

    uint32_t sampleRate_;
    float samplePeriod_;

    void updateAlgorithmParams();
    ProcessorControls activeControls_;
    ProcessorControls pendingControls_;
    volatile bool controlsReady_ = false;

} ;

