#pragma once

#include "engine.hpp"
#include "engine_arena.hpp"
#include "g0_spi.hpp"
#include "processor.hpp"
#include <cstdint>

// Owns the active engine and switches it (plan §4.4, H6). Thread mode only, so an
// activation may take any time while PendSV keeps the audio running.
//
// A switch parks the engine (its wet path fades out), swaps it, gives the new one
// either the G0's values or its defaults, resumes it, and then hands the same values
// to the link, which echoes them. The echo reports the new engine ready only once the
// link has them, so the G0 never adopts values the engine does not have.
class EngineHost {
public:

    struct Config
    {
        G0Spi*     link;
        Processor* processor;
        uint32_t   sampleRate;
    };

    EngineHost() = default;
    ~EngineHost() = default;

    // Before audio starts: activates registry index 0 with its defaults.
    void init(const Config& config);

    // Thread mode, from the main loop.
    void poll();

private:

    enum class State : uint8_t { Running, Parking, Publishing };

    Config       config_ {};
    EngineArenas arenas_;
    Engine*      active_      = nullptr;
    uint8_t      activeIndex_ = 0;
    uint8_t      targetIndex_ = 0;
    State        state_       = State::Running;
    bool         unknown_     = false;
    // The last defaultsSeq applied, or adopted by the G0 without a request.
    uint8_t      defaultsSeq_ = 0;

    // What the active engine was given, until the link takes it.
    uint16_t     appliedParam_[SPI_PARAM_COUNT] = {};
    uint16_t     appliedDiscrete_ = 0;

    void handleRequest(const G0Spi::Request& request);
    void swap(const G0Spi::Request* request);
    void takeDefaults();
    EngineControls appliedControls() const;
    void publish(uint16_t flags);
};
