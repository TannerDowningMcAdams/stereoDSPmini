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

struct processorControls{

    float potentiometers[5];
    uint8_t effect_mode;
    float beats_per_second;
    uint16_t clock_phase;

};

class Processor {
public:

    Processor() = default;
    ~Processor() = default;

    //enum class AudioStatus { BUSY, READY, ERROR };

    void init();

    void processAudioBlock(AudioBuffer input, AudioBuffer output);
    FloatFrame processLeftRight(FloatFrame frame);
    void pushControls(const processorControls &controls);
    
private:

    void updateAlgorithmParams();
    processorControls activeControls_;
    processorControls pendingControls_;
    volatile bool controlsReady_ = false;

} ;

