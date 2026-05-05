#include "audio.hpp"
#include "audio_buffer.hpp"
#include "frame.hpp"
#include "main.h"
#include "status.hpp"
#include "stm32h7xx_hal_def.h"
#include "stm32h7xx_hal_sai.h"
#include "system.hpp"
#include "dsp.hpp"
#include <cstdint>
#include <cstring>

extern System gSystem;

// DMA buffers are placed in an uncached SRAM carve-out to prevent cache coherency issues
UNCACHED_RAM volatile int32_t Audio::audioAdcDataDMA_[Audio::kCodecBufferSize];
UNCACHED_RAM volatile int32_t Audio::audioDacDataDMA_[Audio::kCodecBufferSize];

volatile int32_t* Audio::audioInPointer_  = &Audio::audioAdcDataDMA_[0];
volatile int32_t* Audio::audioOutPointer_ = &Audio::audioDacDataDMA_[0];

void Audio::init() 
{
    // Clear out audio buffers
    memset((void*)audioAdcDataDMA_, 0, sizeof(audioAdcDataDMA_));
    memset((void*)audioDacDataDMA_, 0, sizeof(audioDacDataDMA_));
    memset(leftInputBuffer_,   0.0f,  sizeof(leftInputBuffer_));
    memset(rightInputBuffer_,  0.0f,  sizeof(rightInputBuffer_));
    memset(leftOutputBuffer_,  0.0f,  sizeof(leftOutputBuffer_));
    memset(rightOutputBuffer_, 0.0f,  sizeof(rightOutputBuffer_));
    // Initialization and check
    resetCodec();
    HAL_StatusTypeDef dmaStatus = startDMA();
    if (dmaStatus != HAL_OK) { initErrorHandler(); }
    else { status_ = Status::OK; }
}

void Audio::packUnpackAudioData() 
{
    // SAFETY:
    // DMA is currently operating on the opposite half-buffer
    // This region is stable for the duration of this function
    // Therefore, it is safe to treat as non-volatile for performance

    const int32_t* src = const_cast<const int32_t*>(audioInPointer_);
    int32_t* dst = const_cast<int32_t*>(audioOutPointer_);

    inputBuffer_.fromInterleaved(src, kInt24ToFloat, 8);
    outputBuffer_.toInterleaved(dst, kFloatToInt24, 8);

    // Reset flags
    receiveReady_ = false;
    transmitReady_ = false;

    // Call to System to pass audio data to Processor
    gSystem.onAudioReady();
}

void Audio::rxHalfComplete()
{
    // Set access pointer to the first half of the DMA array
    audioInPointer_ = &audioAdcDataDMA_[0];
    receiveReady_ = true;
    if(transmitReady_) { packUnpackAudioData(); }
}

void Audio::rxComplete()
{
    // Set access pointer to the second half of the DMA array
    audioInPointer_ = &audioAdcDataDMA_[kCodecBufferSize/2];
    receiveReady_ = true;
    if(transmitReady_) { packUnpackAudioData(); }
}

void Audio::txHalfComplete()
{
    // Set access pointer to the first half of the DMA array
    audioOutPointer_ = &audioDacDataDMA_[0];
    transmitReady_ = true;
    if (receiveReady_) { packUnpackAudioData(); }
}

void Audio::txComplete()
{
    // Set access pointer to the second half of the DMA array
    audioOutPointer_ = &audioDacDataDMA_[kCodecBufferSize/2];
    transmitReady_ = true;
    if (receiveReady_) { packUnpackAudioData(); }
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
    HAL_StatusTypeDef txStatus = HAL_SAI_Transmit_DMA(txHandle_, (uint8_t *) audioDacDataDMA_, kCodecBufferSize);
    if (txStatus != HAL_OK) { return  txStatus; }
    else { return HAL_SAI_Receive_DMA(rxHandle_, (uint8_t *) audioAdcDataDMA_, kCodecBufferSize); }
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