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

    offset_ = 1u;

    // Initialization and check
    resetCodec();
    HAL_StatusTypeDef dmaStatus = startDMA();
    if (dmaStatus != HAL_OK) { initErrorHandler(); }
    else { status_ = Status::OK; }
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

// A1 is the clock master and the only block still interrupting, so these publish
// the half for both rings: at half-transfer the DAC half is sent, the ADC filled.
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

void Audio::maskBlockInterrupts(DMA_HandleTypeDef* hdma)
{
    if (hdma == nullptr) { return; }

    // Legal with the stream running, and leaves TEIE and DMEIE armed so
    // SAI_DMAError() still reaches audioErrorHandler().
    __HAL_DMA_DISABLE_IT(hdma, DMA_IT_HT | DMA_IT_TC);
}

HAL_StatusTypeDef Audio::startDMA()
{ 
    // B1 is the synchronous slave and is enabled first: SCK and FS only go live
    // on master A1's enable, so B1 must be listening to lock the first frame.
    HAL_StatusTypeDef rxStatus = HAL_SAI_Receive_DMA(rxHandle_, (uint8_t *) audioAdcDataDMA_, kCodecBufferSize);
    if (rxStatus != HAL_OK) { return rxStatus; }

    // The slave crosses the same midpoint on the same frame as the master, so
    // its block interrupts add only a race. Masked after HAL enables them.
    maskBlockInterrupts(rxHandle_->hdmarx);

    // Clocks start here.
    offset_ = 1u;
    return HAL_SAI_Transmit_DMA(txHandle_, (uint8_t *) audioDacDataDMA_, kCodecBufferSize);
}
void Audio::audioErrorHandler()
{
    uint32_t txError = HAL_SAI_GetError(txHandle_);
    uint32_t rxError = HAL_SAI_GetError(rxHandle_);
    errorCount_++;
    status_ = Status::ERROR;

    // Bitwise OR error codes to address tx/rx errors together
    uint32_t combinedError = txError | rxError;

    recoverFromError(combinedError);

    HAL_StatusTypeDef retryResult = startDMA();

    if (retryResult == HAL_OK) { status_ = Status::OK; }
    else { status_ = Status::ERROR; }
}

void Audio::initErrorHandler()
{
    uint32_t txError = HAL_SAI_GetError(txHandle_);
    uint32_t rxError = HAL_SAI_GetError(rxHandle_);
    errorCount_++;
    status_ = Status::ERROR;

    // Bitwise OR error codes to address tx/rx errors together
    uint32_t combinedError = txError | rxError;   

    // HAL_BUSY or HAL_TIMEOUT
    if (combinedError == HAL_SAI_ERROR_NONE)
    {
        HAL_SAI_Abort(txHandle_);
        HAL_SAI_Abort(rxHandle_);
    }
    else
    {
        recoverFromError(combinedError);
    }

    // Single retry after recovery
    HAL_StatusTypeDef retryResult = startDMA();

    if (retryResult == HAL_OK)
    {
        status_ = Status::OK;
        errorCount_--;  // successful recovery
    }
    else { status_ = Status::ERROR; }
}

void Audio::recoverFromError(uint32_t saiError)
{
    if (saiError & HAL_SAI_ERROR_DMA)
    {
        // DMA stream fault - full peripheral and DMA reinit
        HAL_SAI_MspDeInit(txHandle_);
        HAL_SAI_MspDeInit(rxHandle_);
        //HAL_DMA_Init();
        //HAL_DMA_Start();
        MX_SAI1_Init();
    }
    else if (saiError & HAL_SAI_ERROR_WCKCFG)
    {
        // Clock not present - brief delay then reinit peripheral
        HAL_Delay(10);
        HAL_SAI_MspDeInit(txHandle_);
        HAL_SAI_MspDeInit(rxHandle_);
        HAL_SAI_Init(txHandle_);
        HAL_SAI_Init(rxHandle_);
    }
    else if (saiError & (HAL_SAI_ERROR_AFSDET | HAL_SAI_ERROR_LFSDET))
    {
        // Frame sync lost - abort both and re-arm
        HAL_SAI_Abort(txHandle_);
        HAL_SAI_Abort(rxHandle_);
    }
    else if (saiError & (HAL_SAI_ERROR_OVR | HAL_SAI_ERROR_UDR))
    {
        // FIFO error - abort both and re-arm
        HAL_SAI_Abort(txHandle_);
        HAL_SAI_Abort(rxHandle_);
    }
    else if (saiError & HAL_SAI_ERROR_TIMEOUT) 
    {
        // Timeout - full peripheral reinit
        HAL_SAI_MspDeInit(txHandle_);
        HAL_SAI_MspDeInit(rxHandle_);
        MX_SAI1_Init();
    }
    else
    {
        // HAL_BUSY or unknown - abort and re-arm
        HAL_SAI_Abort(txHandle_);
        HAL_SAI_Abort(rxHandle_);
    }
}