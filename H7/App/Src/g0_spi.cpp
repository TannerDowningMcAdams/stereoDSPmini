#include "g0_spi.hpp"
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

void G0Spi::init(const Config& config)
{
    // The cycle counter timestamps each CS edge. The H7's DWT is locked until LAR is written.
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->LAR = 0xC5ACCE55u;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    controls_ = defaultControls();

    // Not armed here: the first CS rising edge does it, on a packet boundary.
    config_ = config;
}

// The manifest defaults of the only engine the H7 has.
G0Spi::Controls G0Spi::defaultControls()
{
    const EngineManifest& engine = kEngineManifest[ENGINE_PASSTHROUGH];
    Controls controls {};
    controls.engine = ENGINE_PASSTHROUGH;
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { controls.param[i] = engine.paramDefault[i]; }
    controls.discrete = engineDiscreteDefault(&engine);
    return controls;
}

void G0Spi::restartFrame()
{
    EXTI->SWIER1 = config_.csPin;
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
    frameSeq_              = rxPacketDMA_.frameSeq;
    controls_.engine       = rxPacketDMA_.engine;
    controls_.presetIndex  = rxPacketDMA_.presetIndex;
    controls_.runFlags     = rxPacketDMA_.runFlags;
    controls_.eventToggles = rxPacketDMA_.eventToggles;
    controls_.tempoHz      = rxPacketDMA_.tempoHz;
    controls_.tempoPhase   = rxPacketDMA_.tempoPhase;
    controls_.ownerMask    = rxPacketDMA_.ownerMask;

    // A new preset's values must not drive the engine it replaces (plan §4.1).
    if (rxPacketDMA_.engine != activeEngine_) { return; }
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { controls_.param[i] = rxPacketDMA_.param[i]; }
    controls_.discrete = rxPacketDMA_.discrete;
}

bool G0Spi::linkUp() const
{
    return validFrameCount_ != 0u && (HAL_GetTick() - lastValidMs_) < kLinkTimeoutMs;
}

void G0Spi::buildTx()
{
    H7ToG0Packet& tx = txPacketDMA_;
    tx.messageId       = SPI_MSG_ID_H7_TO_G0;
    tx.version         = PROTOCOL_VERSION;
    tx.bootloaderMagic = bootloaderRequest_ ? SPI_BOOTLOADER_MAGIC : 0u;
    tx.h7FwVersion     = H7_FW_VERSION;
    tx.frameSeqEcho    = frameSeq_;
    tx.h7Flags         = static_cast<uint16_t>(H7_FLAG_ENGINE_READY |
                                               (validFrameCount_ != 0u ? H7_FLAG_STATE_VALID : 0u));
    tx.activeEngine    = activeEngine_;
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

void G0Spi::onFrameEnd()
{
    const uint32_t edge = DWT->CYCCNT;

    // EXTI is live from MX_GPIO_Init, before init() runs. Nothing is armed until
    // config_ is set.
    if (config_.spi == nullptr) { return; }
    frameCount_ = frameCount_ + 1u;
    if (frameValid_)
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
}
