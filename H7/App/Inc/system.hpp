#pragma once
#include "audio.hpp"
#include "audio_buffer.hpp"
#include "bypass_controller.hpp"
#include "g0_spi.hpp"
#include "g0_bootloader.hpp"
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
    // The bootloader request stays up until the G0's frames stop for kG0SilentMs,
    // which means it has reset. kG0ResetWindowMs bounds a G0 that ignores the request.
    static constexpr uint32_t kG0SilentMs      = 5u;
    static constexpr uint32_t kG0ResetWindowMs = 100u;
    // Covers the new G0 app's boot after Go to its first valid frame.
    static constexpr uint32_t kG0ConfirmWindowMs = 500u;

    // Boot-time G0 check. The relays leave true bypass only on UpToDate or Programmed.
    enum class G0State : uint8_t { Pending, UpToDate, Programmed, Absent, Failed };
    // Read by the SPI ISR: G0 controls reach the audio path only once it is confirmed.
    volatile G0State g0State_ = G0State::Pending;

    BypassController bypass_;
    Processor processor_;

    G0State updateG0();
    bool g0Confirmed() const;
    bool waitForG0(uint32_t windowMs, uint32_t framesBefore);
    bool waitForG0Silent(uint32_t windowMs);
    static ProcessorControls toProcessorControls(const G0Spi::Controls& controls);

} ;