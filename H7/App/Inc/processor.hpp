#pragma once

#include "audio_buffer.hpp"
#include "bypass_controller.hpp"
#include "engine.hpp"
#include "tempo_follower.hpp"
using dsp::AudioBuffer;

#include <cstdint>

struct ProcessorControls{

    uint16_t engineId;      // the engine params and discrete belong to; ENGINE_ID_NONE = none
    float params[8];        // 0..1, meaning set by the engine
    uint16_t discrete;      // fields per the engine's EngineInfo
    uint16_t eventToggles;  // each G0 event flips its bit
    uint16_t runFlags;      // RUN_FLAG_*, for the bypass controller
    float tempoHz;
    uint16_t tempoPhase;
    uint32_t tempoEdgeCycles;   // cycle count at the CS edge tempoPhase refers to
    uint16_t frameSeq;

};

// Runs every audio block in PendSV: tempo follower, bypass controller and the active
// engine. The engine host swaps the engine from thread mode through park(), install()
// and resume().
class Processor {
public:

    struct Config
    {
        uint32_t          sampleRate;
        uint32_t          cyclesPerSecond;  // rate of the cycle counter behind the timestamps
        BypassController* bypass;   // runs around the engine on every block
    };

    Processor() = default;
    ~Processor() = default;

    void init(const Config& config);

    // PendSV. blockCycles: cycle count when the block's DMA half was published.
    void processAudioBlock(dsp::ConstAudioBuffer input, dsp::AudioBuffer output, uint32_t blockCycles);
    // Called from the SPI ISR. Refused while the previous set is unconsumed, so the
    // payload has one owner at a time; the caller simply tries again next tick.
    bool pushControls(const ProcessorControls &controls);

    // Thread mode. park() fades the engine out; once parked() is true, PendSV no longer
    // calls it, and install() may replace it and give it its first controls. resume()
    // fades the installed engine in. Before audio starts, install() needs no park.
    void park()         { parkRequest_ = true; }
    bool parked() const { return parked_; }
    void install(Engine* engine, const EngineControls& controls);
    void resume();

private:

    BypassController* bypass_ = nullptr;
    TempoFollower tempo_;
    // Written by thread mode only while parked_ is set, or before audio starts.
    Engine* engine_ = nullptr;
    // The blend param in the controls the engine last took, or BypassController::kNoBlend.
    float   blend_  = BypassController::kNoBlend;

    // parkRequest_ has one writer, thread mode; parked_ has one writer, PendSV.
    volatile bool parkRequest_ = false;
    volatile bool parked_      = false;

    uint16_t lastToggles_ = 0;
    bool     togglesSeen_ = false;

    void updateAlgorithmParams(uint32_t blockCycles);
    void updateBypass();
    void applyEngineControls(uint16_t edges);
    void followPark();
    // pendingControls_ belongs to pushControls() while the flag is clear, and to
    // processAudioBlock() while it is set.
    ProcessorControls activeControls_;
    ProcessorControls pendingControls_;
    volatile bool controlsReady_ = false;

} ;
