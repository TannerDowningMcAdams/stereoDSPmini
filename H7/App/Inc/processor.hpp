#pragma once

#include "audio_buffer.hpp"
using dsp::AudioBuffer;

#include <cstdint>

struct ProcessorControls{

    float params[8];        // 0..1, meaning set by the engine manifest
    uint16_t discrete;      // field layout per engine_manifest.h
    float tempoHz;
    uint16_t tempoPhase;

};

class Processor {
public:

    Processor() = default;
    ~Processor() = default;

    //enum class AudioStatus { BUSY, READY, ERROR };

    void init(uint32_t sampleRate);

    void processAudioBlock(dsp::ConstAudioBuffer input, dsp::AudioBuffer output);
    void processLeftRight(dsp::ConstAudioBuffer input, dsp::AudioBuffer output);
    // Called from the SPI ISR. Refused while the previous set is unconsumed, so the
    // payload has one owner at a time; the caller simply tries again next tick.
    bool pushControls(const ProcessorControls &controls);
    
private:

    uint32_t sampleRate_;
    float samplePeriod_;

    void updateAlgorithmParams();
    // pendingControls_ belongs to pushControls() while the flag is clear, and to
    // processAudioBlock() while it is set.
    ProcessorControls activeControls_;
    ProcessorControls pendingControls_;
    volatile bool controlsReady_ = false;

} ;

