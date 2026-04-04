#pragma once

#include "frame.hpp"
#include "audio_buffer.hpp"
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

    void init(float sampleRate);

    void processAudioBlock(AudioBuffer input, AudioBuffer output);
    FloatFrame processLeftRight(FloatFrame frame);
    void pushControls(const ProcessorControls &controls);
    
private:

    uint16_t sampleRate_;
    float samplePeriod_;

    void updateAlgorithmParams();
    ProcessorControls activeControls_;
    ProcessorControls pendingControls_;
    volatile bool controlsReady_ = false;

} ;

