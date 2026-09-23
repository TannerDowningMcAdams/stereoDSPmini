#pragma once
#include "audio.hpp"
#include "audio_buffer.hpp"
#include "relay.hpp"
#include "g0_spi.hpp"
#include "g0_bootloader.hpp"
#include "analog_dry.hpp"
#include "processor.hpp"
#include <cstdint>
#ifdef __cplusplus
extern "C" {
#endif
#include "stm32h7xx_hal.h"
#ifdef __cplusplus
}
#endif

class System {
public:

    System() = default;
    ~System() = default;

    void init();
    // Thread-mode work, called from the App_Run() loop.
    void poll();

    inline void audioRxHalfComplete()   { audio_.rxHalfComplete(); }
    inline void audioRxComplete()       { audio_.rxComplete(); }
    inline void audioTxHalfComplete()   { audio_.txHalfComplete(); }
    inline void audioTxComplete()       { audio_.txComplete(); }
    inline void audioErrorCallback(SAI_HandleTypeDef* hsai) { audio_.audioErrorHandler(hsai); }
    void spiTxRxComplete();
    void spiErrorCallback()             { g0Spi_.spiErrorHandler(); }
    void spiFrameEnd()                  { g0Spi_.onFrameEnd(); }

private:

    Audio audio_;
    G0Spi g0Spi_;
    G0Bootloader g0Bootloader_;
    // Covers a G0 that enters its bootloader at power-up: its app boot, debounce and reset.
    static constexpr uint32_t kG0ProbeWindowMs = 250u;
    // Covers the G0 app's boot to its first frame.
    static constexpr uint32_t kG0LinkWindowMs  = 100u;
    // Frames carrying the bootloader magic; the G0 acts on the first it receives.
    static constexpr uint32_t kG0RequestMs     = 20u;
    Relay relay_;
    AnalogDryThru analogDryThru_;
    Processor processor_;

    void updateG0();
    static ProcessorControls toProcessorControls(const G0Spi::Controls& controls);

} ;