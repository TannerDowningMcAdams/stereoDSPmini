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
    const G0Spi::Config         spiConfig   { &hspi2, G0_EXTI_Pin };
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
    // Defaults are pushed before SPI starts: pushControls() assumes one producer,
    // and after this the SPI ISR is the only one.
    processor_.init({ Audio::kSampleRate, SystemCoreClock, &bypass_ });
    processor_.pushControls(toProcessorControls(G0Spi::defaultControls(), ENGINE_PASSTHROUGH, 0u));
    g0Spi_.init(spiConfig);
#if STEREODSPMINI_G0_IMAGE
    g0State_ = updateG0();
#else
    g0State_ = waitForG0(kG0LinkWindowMs, 0u) ? G0State::UpToDate : G0State::Absent;
#endif
    // Processor is ready before the first block can be published.
    audio_.setProcessor(&processor_);
    audio_.init(audioConfig);
}

// Without a confirmed G0 there is no UI to trust, so its frames never engage the pedal.
bool System::g0Confirmed() const
{
    return g0State_ == G0State::UpToDate || g0State_ == G0State::Programmed;
}

// True once a valid frame newer than framesBefore reports the expected G0 image version.
bool System::waitForG0(uint32_t windowMs, uint32_t framesBefore)
{
    const uint32_t start = HAL_GetTick();
    do
    {
        if (g0Spi_.validFrameCount() > framesBefore && g0Spi_.g0FwVersion() == G0_FW_VERSION)
        {
            return true;
        }
    } while ((HAL_GetTick() - start) < windowMs);
    return false;
}

// True once no CS edge has arrived for kG0SilentMs.
bool System::waitForG0Silent(uint32_t windowMs)
{
    const uint32_t start = HAL_GetTick();
    uint32_t lastCount = g0Spi_.frameCount();
    uint32_t lastEdge  = start;
    while ((HAL_GetTick() - start) < windowMs)
    {
        const uint32_t now   = HAL_GetTick();
        const uint32_t count = g0Spi_.frameCount();
        if (count != lastCount)
        {
            lastCount = count;
            lastEdge  = now;
        }
        else if ((now - lastEdge) >= kG0SilentMs)
        {
            return true;
        }
    }
    return false;
}

void System::poll()
{
    audio_.serviceErrors();
    audio_.serviceBlock();
}

#if STEREODSPMINI_G0_IMAGE
// Programs the G0 with the embedded image when its app reports another version, or when
// it is already in its bootloader (AUX held at power-up, or blank flash). Runs before
// audio starts, since programming blocks for seconds.
System::G0State System::updateG0()
{
    if (g0Bootloader_.init({ &huart3 }) != Status::OK) { return G0State::Failed; }

    const uint32_t start = HAL_GetTick();
    while (!g0Spi_.g0Seen() && (HAL_GetTick() - start) < kG0LinkWindowMs) {}

    const bool appRunning = g0Spi_.g0Seen();
    if (appRunning)
    {
        if (g0Spi_.g0ProtocolVersion() == PROTOCOL_VERSION && g0Spi_.g0FwVersion() == G0_FW_VERSION)
        {
            return G0State::UpToDate;
        }
        g0Spi_.setBootloaderRequest(true);
        (void) waitForG0Silent(kG0ResetWindowMs);
        g0Spi_.setBootloaderRequest(false);
        // The transfer armed at the G0's last frame still holds the magic. Left in
        // place, the new image would receive it first and return to the bootloader.
        g0Spi_.restartFrame();
    }

    // No app and no bootloader: nothing on the FFC, or MIDI claimed the bootloader first.
    if (!g0Bootloader_.probe(kG0ProbeWindowMs)) { return appRunning ? G0State::Failed : G0State::Absent; }

    const uint32_t framesBefore = g0Spi_.validFrameCount();
    Status status = g0Bootloader_.program(g0_image_start, g0ImageSize());
    if (status != Status::OK && g0Bootloader_.resync())
    {
        status = g0Bootloader_.program(g0_image_start, g0ImageSize());
    }
    if (status != Status::OK) { return G0State::Failed; }

    // Go only proves the bootloader jumped; the new app must also start and report in.
    return waitForG0(kG0ConfirmWindowMs, framesBefore) ? G0State::Programmed : G0State::Failed;
}
#endif

// SPI DMA callback, once per G0 frame (1 ms); see callbacks.cpp for origin
void System::spiTxRxComplete()
{
    // Only a valid frame drives anything. Until the G0's first one, the defaults
    // set in init() stand.
    if (!g0Spi_.txRxComplete()) { return; }
    // A G0 that reports in after the boot check, e.g. one released from a debugger.
    // Failed stays latched.
    if (g0State_ == G0State::Absent && g0Spi_.g0FwVersion() == G0_FW_VERSION)
    {
        g0State_ = G0State::UpToDate;
    }
}

// EXTI on the G0's CS rising edge. Controls go to the processor here rather than on
// SPI completion, so they carry the edge their tempo phase refers to.
void System::spiFrameEnd()
{
    if (!g0Spi_.onFrameEnd() || !g0Confirmed()) { return; }

    // A refused set is dropped, not retried: the next frame 1 ms later is fresher.
    (void) processor_.pushControls(toProcessorControls(g0Spi_.controls(), g0Spi_.activeEngine(),
                                                       g0Spi_.frameEdgeCycles()));
}

ProcessorControls System::toProcessorControls(const G0Spi::Controls& controls, uint8_t engine,
                                              uint32_t edgeCycles)
{
    ProcessorControls out;
    out.engine = engine;
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
