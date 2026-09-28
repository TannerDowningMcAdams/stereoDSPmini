#pragma once
#include "audio.hpp"
#include "audio_buffer.hpp"
#include "bypass_controller.hpp"
#include "ui_spi.hpp"
#include "g0_bootloader.hpp"
#include "engine_host.hpp"
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
    // Thread-mode work, called from the App_Run() loop: SAI recovery and engine switching.
    void poll();
    // PendSV, pended by the audio DMA callbacks: one audio block.
    void audioPendSV()                  { audio_.serviceBlock(); }

    inline void audioRxHalfComplete()   { audio_.rxHalfComplete(); }
    inline void audioRxComplete()       { audio_.rxComplete(); }
    inline void audioTxHalfComplete()   { audio_.txHalfComplete(); }
    inline void audioTxComplete()       { audio_.txComplete(); }
    inline void audioErrorCallback(SAI_HandleTypeDef* hsai) { audio_.audioErrorHandler(hsai); }
    void spiTxRxComplete();
    void spiErrorCallback()             { uiSpi_.spiErrorHandler(); }
    void spiFrameEnd();

private:

    Audio audio_;
    UiSpi uiSpi_;
    G0Bootloader g0Bootloader_;
    // Covers a G0 that enters its bootloader at power-up: its app boot, debounce and reset.
    static constexpr uint32_t kG0ProbeWindowMs = 250u;
    // Covers the UI MCU's boot to its first frame.
    static constexpr uint32_t kUiLinkWindowMs  = 100u;
    // The bootloader request stays up until the G0's frames stop for kUiSilentMs,
    // which means it has reset. kG0ResetWindowMs bounds a G0 that ignores the request.
    static constexpr uint32_t kUiSilentMs      = 5u;
    static constexpr uint32_t kG0ResetWindowMs = 100u;
    // Covers the new G0 app's boot after Go to its first valid frame.
    static constexpr uint32_t kG0ConfirmWindowMs = 500u;

    // Boot-time UI MCU check. The relays leave true bypass only on UpToDate or Programmed.
    enum class UiState : uint8_t { Pending, UpToDate, Programmed, Absent, Failed };
    // Read by the SPI ISR: UI controls reach the audio path only once the UI MCU is confirmed.
    volatile UiState uiState_ = UiState::Pending;

    BypassController bypass_;
    Processor processor_;
    EngineHost engineHost_;

    UiState updateG0();
    bool uiConfirmed() const;
    bool waitForUi(uint32_t windowMs, uint32_t framesBefore);
    bool waitForUiSilent(uint32_t windowMs);
    static ProcessorControls toProcessorControls(const UiSpi::Controls& controls, uint32_t edgeCycles);

} ;