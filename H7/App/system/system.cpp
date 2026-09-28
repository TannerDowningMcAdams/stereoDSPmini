#include "system.hpp"
#include "protocol_version.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "main.h"
#include "sai.h"
#include "spi.h"
#include "dac.h"
#include "opamp.h"
#include "usart.h"
#ifdef __cplusplus
}
#endif
#if STEREODSPMINI_G0_IMAGE
#include "g0_image.hpp"
#endif

void System::init()
{
    // Board wiring: every peripheral handle and pin the drivers use, in one place.
    // Locals, since GPIOx is a cast from an integer and cannot be constexpr.
    const Relay::Config relayConfig {
        { RELAY_L_GPIO_Port, RELAY_L_Pin },
        { RELAY_R_GPIO_Port, RELAY_R_Pin },
    };
    const AnalogDryThru::Config dryConfig   { &hdac1, DAC_CHANNEL_1, &hopamp1 };
    const BypassController::Config bypassConfig { relayConfig, dryConfig,
                                                  Audio::kSampleRate, Audio::kBlockSize };
    const UiSpi::Config         spiConfig   { &hspi2, G0_EXTI_Pin };
    const Audio::Config         audioConfig { &hsai_BlockA1, &hsai_BlockB1,
                                              { CODEC_NRST_GPIO_Port, CODEC_NRST_Pin } };
    static_assert(Audio::kBlockSize <= BypassController::kMaxBlockSize, "bypass scratch too small");

    // The cycle counter stamps CS edges and audio blocks for the tempo phase. The
    // H7's DWT is locked until LAR is written.
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->LAR = 0xC5ACCE55u;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    // Unengaged from here: relays off, VCA at -100 dB.
    bypass_.init(bypassConfig);
    // Pushed before SPI starts: pushControls() allows one producer, which is the SPI
    // ISR from here on. The defaults carry no engine params; the engine host supplies them.
    processor_.init({ Audio::kSampleRate, SystemCoreClock, &bypass_ });
    processor_.pushControls(toProcessorControls(UiSpi::defaultControls(), 0u));
    uiSpi_.init(spiConfig);
    engineHost_.init({ &uiSpi_, &processor_, Audio::kSampleRate });
#if STEREODSPMINI_G0_IMAGE
    uiState_ = updateG0();
#else
    uiState_ = waitForUi(kUiLinkWindowMs, 0u) ? UiState::UpToDate : UiState::Absent;
#endif
    // Blocks run in PendSV, below every interrupt, so the UI link's CS timestamp and
    // re-arm are never held up by DSP.
    HAL_NVIC_SetPriority(PendSV_IRQn, 15, 0);
    // Processor is ready before the first block can be published.
    audio_.setProcessor(&processor_);
    audio_.init(audioConfig);
}

// An unconfirmed UI MCU is not trusted, so its frames never engage the pedal.
bool System::uiConfirmed() const
{
    return uiState_ == UiState::UpToDate || uiState_ == UiState::Programmed;
}

// True once a valid frame newer than framesBefore reports the expected UI firmware version.
bool System::waitForUi(uint32_t windowMs, uint32_t framesBefore)
{
    const uint32_t start = HAL_GetTick();
    do
    {
        if (uiSpi_.validFrameCount() > framesBefore && uiSpi_.uiFwVersion() == UI_FW_VERSION)
        {
            return true;
        }
    } while ((HAL_GetTick() - start) < windowMs);
    return false;
}

// True once no CS edge has arrived for kUiSilentMs.
bool System::waitForUiSilent(uint32_t windowMs)
{
    const uint32_t start = HAL_GetTick();
    uint32_t lastCount = uiSpi_.frameCount();
    uint32_t lastEdge  = start;
    while ((HAL_GetTick() - start) < windowMs)
    {
        const uint32_t now   = HAL_GetTick();
        const uint32_t count = uiSpi_.frameCount();
        if (count != lastCount)
        {
            lastCount = count;
            lastEdge  = now;
        }
        else if ((now - lastEdge) >= kUiSilentMs)
        {
            return true;
        }
    }
    return false;
}

void System::poll()
{
    audio_.serviceErrors();
    engineHost_.poll();
}

#if STEREODSPMINI_G0_IMAGE
// Programs the G0 with the embedded image when its app reports another version, or when
// it is already in its bootloader (AUX held at power-up, or blank flash). Runs before
// audio starts, since programming blocks for seconds.
System::UiState System::updateG0()
{
    if (g0Bootloader_.init({ &huart3 }) != Status::OK) { return UiState::Failed; }

    const uint32_t start = HAL_GetTick();
    while (!uiSpi_.uiSeen() && (HAL_GetTick() - start) < kUiLinkWindowMs) {}

    const bool appRunning = uiSpi_.uiSeen();
    if (appRunning)
    {
        if (uiSpi_.uiProtocolVersion() == PROTOCOL_VERSION && uiSpi_.uiFwVersion() == UI_FW_VERSION)
        {
            return UiState::UpToDate;
        }
        uiSpi_.setBootloaderRequest(true);
        (void) waitForUiSilent(kG0ResetWindowMs);
        uiSpi_.setBootloaderRequest(false);
        // The transfer armed at the G0's last frame still holds the magic. Left in
        // place, the new image would receive it first and return to the bootloader.
        uiSpi_.restartFrame();
    }

    // No app and no bootloader: nothing on the FFC, or MIDI claimed the bootloader first.
    if (!g0Bootloader_.probe(kG0ProbeWindowMs)) { return appRunning ? UiState::Failed : UiState::Absent; }

    const uint32_t framesBefore = uiSpi_.validFrameCount();
    Status status = g0Bootloader_.program(g0_image_start, g0ImageSize());
    if (status != Status::OK && g0Bootloader_.resync())
    {
        status = g0Bootloader_.program(g0_image_start, g0ImageSize());
    }
    if (status != Status::OK) { return UiState::Failed; }

    // Go only proves the bootloader jumped; the new app must also start and report in.
    return waitForUi(kG0ConfirmWindowMs, framesBefore) ? UiState::Programmed : UiState::Failed;
}
#endif

// SPI DMA callback, once per UI frame (1 ms).
void System::spiTxRxComplete()
{
    // Only a valid frame drives anything. Until the UI MCU's first one, the defaults
    // set in init() stand.
    if (!uiSpi_.txRxComplete()) { return; }
    // A UI MCU that reports in after the boot check, e.g. one released from a debugger.
    // Failed stays latched.
    if (uiState_ == UiState::Absent && uiSpi_.uiFwVersion() == UI_FW_VERSION)
    {
        uiState_ = UiState::UpToDate;
    }
}

// EXTI on the UI MCU's CS rising edge. Controls go to the processor here rather than on
// SPI completion, so they carry the edge their tempo phase refers to.
void System::spiFrameEnd()
{
    if (!uiSpi_.onFrameEnd() || !uiConfirmed()) { return; }

    // A refused set is dropped: the next frame, 1 ms later, is fresher.
    (void) processor_.pushControls(toProcessorControls(uiSpi_.controls(), uiSpi_.frameEdgeCycles()));
}

ProcessorControls System::toProcessorControls(const UiSpi::Controls& controls, uint32_t edgeCycles)
{
    ProcessorControls out;
    out.engineId = controls.engineId;
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++)
    {
        out.params[i] = controls.param[i] * (1.0f / 65535.0f);
    }
    out.discrete   = controls.discrete;
    out.runFlags   = controls.runFlags;
    out.tempoHz    = controls.tempoHz;
    out.tempoPhase = controls.tempoPhase;
    out.tempoEdgeCycles = edgeCycles;
    out.frameSeq   = controls.frameSeq;
    return out;
}
