#include "g0_spi.hpp"
#include "engine_registry.hpp"
#include "protocol_version.h"
#include <cstdint>
#ifdef __cplusplus
extern "C" {
#endif
#include "main.h"
#ifdef __cplusplus
}
#endif

UNCACHED_RAM G0ToH7Packet G0Spi::rxPacketDMA_;
UNCACHED_RAM H7ToG0Packet G0Spi::txPacketDMA_;

// The DWT cycle counter stamps each CS edge; System::init() starts it.
void G0Spi::init(const Config& config)
{
    controls_ = defaultControls();

    // Not armed here: the first CS rising edge does it, on a packet boundary.
    config_ = config;
}

// The engine's own values come from the engine host through setApplied().
G0Spi::Controls G0Spi::defaultControls()
{
    Controls controls {};
    controls.engineId = ENGINE_ID_NONE;
    controls.runFlags = RUN_FLAG_STEREO_IN;
    return controls;
}

void G0Spi::restartFrame()
{
    EXTI->SWIER1 = config_.csPin;
}

bool G0Spi::request(Request& out) const
{
    // An ISR that lands during the copy moves the count, and the copy is retried.
    uint32_t count;
    do
    {
        count = validFrameCount_;
        __DMB();
        out = request_;
        __DMB();
    } while (count != validFrameCount_);
    return count != 0u;
}

void G0Spi::publishEngine(const EngineStatus& status)
{
    const uint8_t next = static_cast<uint8_t>(engineStatusIndex_ ^ 1u);
    engineStatus_[next] = status;
    __DMB();    // release: the buffer is complete before the ISRs can read it
    engineStatusIndex_ = next;
}

bool G0Spi::setApplied(const uint16_t* param, uint16_t discrete, uint8_t defaultsSeq)
{
    if (appliedReady_) { return false; }

    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { applied_.param[i] = param[i]; }
    applied_.discrete    = discrete;
    applied_.defaultsSeq = defaultsSeq;
    __DMB();    // release: payload visible before the flag that publishes it
    appliedReady_ = true;
    return true;
}

bool G0Spi::txRxComplete()
{
    // Parsed before onFrameEnd() re-arms into this buffer. Both run at priority 2,
    // so that cannot preempt this.
    if (rxPacketDMA_.messageId != SPI_MSG_ID_G0_TO_H7) { return false; }

    // Recorded whatever the version: a G0 on another protocol is one to reprogram.
    g0ProtocolVersion_ = rxPacketDMA_.version;
    g0FwVersion_       = rxPacketDMA_.g0FwVersion;
    g0Seen_            = true;
    if (rxPacketDMA_.version != PROTOCOL_VERSION) { return false; }

    parse();
    frameValid_      = true;
    lastValidMs_     = HAL_GetTick();
    validFrameCount_ = validFrameCount_ + 1u;
    return true;
}

void G0Spi::parse()
{
    const G0ToH7Packet& rx = rxPacketDMA_;
    frameSeq_              = rx.frameSeq;
    controls_.presetIndex  = rx.presetIndex;
    controls_.runFlags     = rx.runFlags;
    controls_.eventToggles = rx.eventToggles;
    controls_.tempoHz      = rx.tempoHz;
    controls_.tempoPhase   = rx.tempoPhase;
    controls_.ownerMask    = rx.ownerMask;
    controls_.frameSeq     = rx.frameSeq;
    engineQuery_           = rx.engineQuery;

    const bool pending = (rx.g0Flags & G0_FLAG_DEFAULTS_PENDING) != 0u;
    request_.engineId        = rx.engineId;
    request_.defaultsSeq     = rx.defaultsSeq;
    request_.defaultsPending = pending;
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { request_.param[i] = rx.param[i]; }
    request_.discrete        = rx.discrete;

    if (appliedReady_)
    {
        __DMB();    // acquire: flag read before payload read
        for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { controls_.param[i] = applied_.param[i]; }
        controls_.discrete = applied_.discrete;
        defaultsSeqEcho_   = applied_.defaultsSeq;
        __DMB();    // payload consumed before the channel reopens
        appliedReady_ = false;
    }

    const EngineStatus& status = engineStatus_[engineStatusIndex_];
    const bool ready = (status.flags & H7_FLAG_ENGINE_READY) != 0u;
    controls_.engineId = ready ? status.activeId : static_cast<uint16_t>(ENGINE_ID_NONE);

    // A new preset's values must not drive the engine it replaces (plan §4.1), and
    // values from before a defaults request must not replace the defaults.
    if (!ready || rx.engineId != status.activeId || pending) { return; }
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { controls_.param[i] = rx.param[i]; }
    controls_.discrete = rx.discrete;
}

bool G0Spi::linkUp() const
{
    return validFrameCount_ != 0u && (HAL_GetTick() - lastValidMs_) < kLinkTimeoutMs;
}

void G0Spi::buildTx()
{
    const EngineStatus& status = engineStatus_[engineStatusIndex_];
    H7ToG0Packet& tx = txPacketDMA_;
    tx.messageId       = SPI_MSG_ID_H7_TO_G0;
    tx.version         = PROTOCOL_VERSION;
    tx.bootloaderMagic = bootloaderRequest_ ? SPI_BOOTLOADER_MAGIC : 0u;
    tx.h7FwVersion     = H7_FW_VERSION;
    tx.frameSeqEcho    = frameSeq_;
    tx.h7Flags         = static_cast<uint16_t>(status.flags |
                                               (validFrameCount_ != 0u ? H7_FLAG_STATE_VALID : 0u));
    tx.activeEngine    = status.activeId;
    tx.engineDesc      = status.descriptor;
    tx.engineCount     = EngineRegistry::count();
    tx.activeIndex     = status.activeIndex;
    tx.engineQueryEcho = engineQuery_;
    tx.defaultsSeqEcho = defaultsSeqEcho_;
    tx.engineQueryId   = EngineRegistry::idAt(engineQuery_);
    tx.presetIndex     = controls_.presetIndex;
    tx.runFlags        = controls_.runFlags;
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { tx.param[i] = controls_.param[i]; }
    tx.discrete        = controls_.discrete;
    tx.tempoHz         = controls_.tempoHz;
    tx.ownerMask       = controls_.ownerMask;
    // The packet is in uncached RAM; the stores must land before the DMA is enabled.
    __DMB();
}

Status G0Spi::arm()
{
    return fromHAL(HAL_SPI_TransmitReceive_DMA(config_.spi,
                                               reinterpret_cast<uint8_t*>(&txPacketDMA_),
                                               reinterpret_cast<uint8_t*>(&rxPacketDMA_),
                                               kSpiPacketWords));
}

// Every HAL error path (CRC, overrun, mode fault, DMA) has already stopped the
// transfer and left the handle READY by the time this runs.
void G0Spi::spiErrorHandler()
{
    errors_     = errors_ | HAL_SPI_GetError(config_.spi);
    errorCount_ = errorCount_ + 1u;
}

bool G0Spi::onFrameEnd()
{
    const uint32_t edge = DWT->CYCCNT;

    // EXTI is live from MX_GPIO_Init, before init() runs. Nothing is armed until
    // config_ is set.
    if (config_.spi == nullptr) { return false; }
    frameCount_ = frameCount_ + 1u;
    const bool valid = frameValid_;
    if (valid)
    {
        frameEdgeCycles_ = edge;
        frameValid_      = false;
    }

    // Still busy when the G0 ends its frame means the transfer began mid-packet.
    // Abort is quick here: its suspend-wait only runs in master mode.
    if (HAL_SPI_GetState(config_.spi) != HAL_SPI_STATE_READY)
    {
        resyncCount_ = resyncCount_ + 1u;
        (void) HAL_SPI_Abort(config_.spi);
    }

    // Built after the abort, so no transfer is reading the buffer.
    buildTx();
    // A failed arm needs no retry logic: the next frame end tries again.
    status_ = arm();
    return valid;
}
