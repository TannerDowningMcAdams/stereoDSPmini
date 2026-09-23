#pragma once

#include "audio_buffer.hpp"
#include "bypass_controller.hpp"
using dsp::AudioBuffer;

#include <cstdint>

struct ProcessorControls{

    float params[8];        // 0..1, meaning set by the engine manifest
    uint16_t discrete;      // field layout per engine_manifest.h
    uint16_t runFlags;      // RUN_FLAG_*, for the bypass controller
    float tempoHz;
    uint16_t tempoPhase;

};

class Processor {
public:

    struct Config
    {
        uint32_t          sampleRate;
        BypassController* bypass;   // runs around the engine on every block
    };

    Processor() = default;
    ~Processor() = default;

    //enum class AudioStatus { BUSY, READY, ERROR };

    void init(const Config& config);

    void processAudioBlock(dsp::ConstAudioBuffer input, dsp::AudioBuffer output);
    void processLeftRight(dsp::ConstAudioBuffer input, dsp::AudioBuffer output);
    // Called from the SPI ISR. Refused while the previous set is unconsumed, so the
    // payload has one owner at a time; the caller simply tries again next tick.
    bool pushControls(const ProcessorControls &controls);
    
private:

    uint32_t sampleRate_;
    float samplePeriod_;
    BypassController* bypass_ = nullptr;

    void updateAlgorithmParams();
    // pendingControls_ belongs to pushControls() while the flag is clear, and to
    // processAudioBlock() while it is set.
    ProcessorControls activeControls_;
    ProcessorControls pendingControls_;
    volatile bool controlsReady_ = false;

} ;
