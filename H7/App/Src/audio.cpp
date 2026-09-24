#include "audio.hpp"
#include "audio_buffer.hpp"
#include "main.h"
#include "status.hpp"
#include "dsp.hpp"
#include <cstdint>
#include <algorithm>
#include <iterator>

// DMA buffers are placed in an uncached SRAM carve-out to prevent cache coherency issues
UNCACHED_RAM int32_t Audio::audioAdcDataDMA_[Audio::kCodecBufferSize];
UNCACHED_RAM int32_t Audio::audioDacDataDMA_[Audio::kCodecBufferSize];

// ISR handoff. Seeded to 1 so the first half-transfer interrupt moves it to 0.
volatile uint32_t Audio::offset_ = 1u;

void Audio::init(const Config& config)
{
    config_ = config;

    // memset takes a byte value, so a nonzero float fill would be silently
    // wrong; std::fill keeps all six buffers consistent.
    std::fill(std::begin(audioAdcDataDMA_),   std::end(audioAdcDataDMA_),   0);
    std::fill(std::begin(audioDacDataDMA_),   std::end(audioDacDataDMA_),   0);
    std::fill(std::begin(leftInputBuffer_),   std::end(leftInputBuffer_),   0.0f);
    std::fill(std::begin(rightInputBuffer_),  std::end(rightInputBuffer_),  0.0f);
    std::fill(std::begin(leftOutputBuffer_),  std::end(leftOutputBuffer_),  0.0f);
    std::fill(std::begin(rightOutputBuffer_), std::end(rightOutputBuffer_), 0.0f);

    clearSaiErrors();
    errorCount_     = 0u;
    restartPending_ = 0u;
    blockCount_     = 0u;
    consumedCount_  = 0u;
    blockOverruns_  = 0u;

    resetCodec();
    // One retry through the same abort-and-rearm path the runtime uses.
    const bool started = (startDMA() == Status::OK) || (restart() == Status::OK);
    status_ = started ? Status::OK : Status::ERROR;
}

void Audio::serviceBlock()
{
    // Snapshot count and half as a pair: an interrupt between two separate reads
    // would pair a stale count with a new half, and process that half twice.
    uint32_t count;
    uint32_t half;
    uint32_t cycles;
    do
    {
        count  = blockCount_;
        half   = offset_;
        cycles = blockCycles_;
    } while (count != blockCount_);

    if (count == consumedCount_) { return; }
    consumedCount_ = count;

    // DMA owns the opposite half until the next interrupt, so these two never
    // alias. Running past that interrupt is what signalBlock() counts.
    const int32_t* __restrict src = audioAdcDataDMA_ + (half * kHalfWords);
    int32_t*       __restrict dst = audioDacDataDMA_ + (half * kHalfWords);

    if (!dspEnabled_)
    {
        std::fill(dst, dst + kHalfWords, 0);
        return;
    }

    inputBuffer_.fromInterleaved(src, kInt24ToFloat, kDmaPadBits);

    if (processor_ != nullptr) { processor_->processAudioBlock(inputBuffer_, outputBuffer_, cycles); }

    outputBuffer_.toInterleaved(dst, kFloatToInt24);
}

// The adc block is the only one that publishes, so these set the half for both rings.
void Audio::rxHalfComplete()
{
    offset_ = 0u;
    signalBlock();
}

void Audio::rxComplete()
{
    offset_ = 1u;
    signalBlock();
}

// offset_ and blockCycles_ are written before blockCount_, so a reader that sees
// the new count also sees the new half and stamp.
void Audio::signalBlock()
{
    blockCycles_ = DWT->CYCCNT;
    const uint32_t n = blockCount_ + 1u;
    if ((n - consumedCount_) > 1u) { blockOverruns_ = blockOverruns_ + 1u; }
    blockCount_ = n;
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}

void Audio::resetCodec()
{
    // Pull NRST low, wait, then back high
    HAL_GPIO_WritePin(config_.codecReset.port, config_.codecReset.mask, GPIO_PIN_RESET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(config_.codecReset.port, config_.codecReset.mask, GPIO_PIN_SET);
    HAL_Delay(50);
}

Status Audio::startDMA()
{
    // The adc block is the synchronous slave and is enabled first: SCK and FS only
    // go live on the master's enable, so the slave must already be listening.
    const Status rxStatus = fromHAL(HAL_SAI_Receive_DMA(config_.adc, reinterpret_cast<uint8_t*>(audioAdcDataDMA_), kCodecBufferSize));
    if (rxStatus != Status::OK) { return rxStatus; }

    // Clocks start here.
    offset_ = 1u;
    return fromHAL(HAL_SAI_Transmit_DMA(config_.dac, reinterpret_cast<uint8_t*>(audioDacDataDMA_), kCodecBufferSize));
}

Status Audio::restart()
{
    // HAL_SAI_Abort tolerates a stream the HAL already stopped, so both blocks are
    // always aborted and re-armed together, whichever one faulted.
    (void) HAL_SAI_Abort(config_.dac);
    (void) HAL_SAI_Abort(config_.adc);

    // Both streams are stopped, so this is safe: drop any block pending from
    // before the fault, and silence the DAC rather than replay stale output.
    consumedCount_ = blockCount_;
    std::fill(std::begin(audioDacDataDMA_), std::end(audioDacDataDMA_), 0);
    return startDMA();
}

// ============================================================================
// Error handling
// ============================================================================

Audio::SaiId Audio::identify(const SAI_HandleTypeDef* hsai) const
{
    return (hsai == config_.dac) ? SaiId::DAC : SaiId::ADC;
}

void Audio::audioErrorHandler(SAI_HandleTypeDef* hsai)
{
    const uint8_t  idx  = static_cast<uint8_t>(identify(hsai));
    const uint32_t bits = HAL_SAI_GetError(hsai);

    // Reached at priority 0 (DMA) and 1 (SAI1), so the read-modify-writes must
    // not interleave. Explicit RMW: compound assignment on volatile is deprecated.
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    saiErrors_[idx]      = saiErrors_[idx] | bits;
    saiErrorCounts_[idx] = saiErrorCounts_[idx] + 1u;
    errorCount_          = errorCount_ + 1u;
    __set_PRIMASK(primask);

    // HAL only ever ORs into this; clearing it makes the next callback report
    // what is new. State is left alone, since the DMA may still be running.
    hsai->ErrorCode = HAL_SAI_ERROR_NONE;

    // No restart here: the HAL calls it needs poll SysTick, which cannot preempt
    // a priority-0 DMA IRQ. serviceErrors() does it from thread mode.
    if ((bits & ~kNonFatalErrors) != 0u) { restartPending_ = 1u; }
}

void Audio::serviceErrors()
{
    if (restartPending_ == 0u) { return; }
    restartPending_ = 0u;

    // Thread mode runs only while no block is live in PendSV, so shutting the gate
    // here is enough.
    dspEnabled_ = false;
    __DMB();
    const Status status = restart();
    __DMB();
    dspEnabled_ = true;

    // A failed restart raises no callback, so re-flag it for the next pass.
    if (status == Status::OK) { status_ = Status::OK; }
    else
    {
        status_ = Status::ERROR;
        restartPending_ = 1u;
    }
}

void Audio::clearSaiErrors()
{
    for (uint8_t i = 0u; i < static_cast<uint8_t>(SaiId::COUNT); ++i)
    {
        saiErrors_[i]      = 0u;
        saiErrorCounts_[i] = 0u;
    }
}
