#pragma once

#include "audio_buffer.hpp"
#include "frame.hpp"
#include "frame_buffer.hpp"
#include "ui_params.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

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

    void init(uint16_t sampleRate);

    void processAudioBlock(dsp::AudioBuffer input, dsp::AudioBuffer output);
    void processLeftRight(dsp::AudioBuffer input, dsp::AudioBuffer output);
    void pushControls(const ProcessorControls &controls);
    
private:

    uint16_t sampleRate_;
    float samplePeriod_;

    void updateAlgorithmParams();
    ProcessorControls activeControls_;
    ProcessorControls pendingControls_;
    volatile bool controlsReady_ = false;

} ;

