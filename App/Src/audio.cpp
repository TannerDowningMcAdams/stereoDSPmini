#include "audio.hpp"
#include "audio_buffer.hpp"
#include "main.h"
#include "status.hpp"
#include "stm32h7xx_hal_def.h"
#include "stm32h7xx_hal_sai.h"
#include "system.hpp"
#include "dsp.hpp"
#include <cstdint>
#include <algorithm>
#include <iterator>

extern System gSystem;

// DMA buffers are placed in an uncached SRAM carve-out to prevent cache coherency issues
UNCACHED_RAM int32_t Audio::audioAdcDataDMA_[Audio::kCodecBufferSize];
UNCACHED_RAM int32_t Audio::audioDacDataDMA_[Audio::kCodecBufferSize];

// ISR handoff. Seeded to 1 so the first half-transfer interrupt moves it to 0.
volatile uint32_t Audio::offset_ = 1u;

void Audio::init() 
{
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

    resetCodec();
    // One retry through the same abort-and-rearm path the runtime uses.
    const bool started = (startDMA() == HAL_OK) || (restart() == HAL_OK);
    status_ = started ? Status::OK : Status::ERROR;
}

void Audio::serviceBlock() 
{
    // One volatile read per block, so both rings are addressed consistently if
    // the master's interrupt lands mid-function.
    const uint32_t half = offset_;

    // DMA owns the opposite half for the duration of this call, so these two
    // never alias and the region is stable.
    const int32_t* __restrict src = audioAdcDataDMA_ + (half * kHalfWords);
    int32_t*       __restrict dst = audioDacDataDMA_ + (half * kHalfWords);

    inputBuffer_.fromInterleaved(src, kInt24ToFloat, kDmaWordShift);

    // Call to System to pass audio data to Processor
    gSystem.onAudioReady();

    outputBuffer_.toInterleaved(dst, kFloatToInt24, kDmaWordShift);
}

// A1 is the clock master and the only block that publishes, so these set the
// half for both rings: at half-transfer the DAC half is sent, the ADC filled.
void Audio::txHalfComplete()
{
    offset_ = 0u;
    serviceBlock();
}

void Audio::txComplete()
{
    offset_ = 1u;
    serviceBlock();
}

void Audio::resetCodec()
{
    // Pull NRST low, wait, then back high
    HAL_GPIO_WritePin(CODEC_NRST_GPIO_Port, CODEC_NRST_Pin, GPIO_PIN_RESET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(CODEC_NRST_GPIO_Port, CODEC_NRST_Pin, GPIO_PIN_SET);
    HAL_Delay(50);
}

HAL_StatusTypeDef Audio::startDMA()
{ 
    // B1 is the synchronous slave and is enabled first: SCK and FS only go live
    // on master A1's enable, so B1 must be listening to lock the first frame.
    HAL_StatusTypeDef rxStatus = HAL_SAI_Receive_DMA(rxHandle_, reinterpret_cast<uint8_t*>(audioAdcDataDMA_), kCodecBufferSize);
    if (rxStatus != HAL_OK) { return rxStatus; }

    // Clocks start here.
    offset_ = 1u;
    return HAL_SAI_Transmit_DMA(txHandle_, reinterpret_cast<uint8_t*>(audioDacDataDMA_), kCodecBufferSize);
}

HAL_StatusTypeDef Audio::restart()
{
    // HAL_SAI_Abort tolerates a stream the HAL already stopped, so both blocks are
    // always aborted and re-armed together, whichever one faulted.
    (void) HAL_SAI_Abort(txHandle_);
    (void) HAL_SAI_Abort(rxHandle_);
    return startDMA();
}

// ============================================================================
// Error handling
// ============================================================================

Audio::SaiId Audio::identify(const SAI_HandleTypeDef* hsai) const
{
    return (hsai == txHandle_) ? SaiId::DAC : SaiId::ADC;
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

    // A failed restart raises no callback, so re-flag it for the next pass.
    if (restart() == HAL_OK) { status_ = Status::OK; }
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
