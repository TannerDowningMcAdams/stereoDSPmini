#include "audio.hpp"
#include "frame.hpp"
#include "main.h"
#include "stm32h7xx_hal_def.h"
#include "system.hpp"
#include <cstring>

extern System gSystem;

UNCACHED_RAM int32_t audioAdcDataDMA_[Audio::kCodecBufferSize];
UNCACHED_RAM int32_t audioDacDataDMA_[Audio::kCodecBufferSize];

int32_t *audioInPointer_ = &audioAdcDataDMA_[0];
int32_t *audioOutPointer_ = &audioDacDataDMA_[0];

void Audio::init(){

    inputBufferPointer_ = &inputBuffer_[0];
    outputBufferPointer_ = &outputBuffer_[0];

    resetCodec();

    HAL_StatusTypeDef txStatus = HAL_SAI_Transmit_DMA(txHandle_, (uint8_t *) audioDacDataDMA_, kBufferSize);
    HAL_StatusTypeDef rxStatus = HAL_SAI_Receive_DMA(rxHandle_, (uint8_t *) audioAdcDataDMA_, kBufferSize);

    if (txStatus != HAL_OK || rxStatus != HAL_OK) { audioInitErrorHandler(); }

    //else{ return Status::READY; }

}

void Audio::packUnpackAudioData(){

    for(int n = 0; n < kBufferSize - 1; n+=2) 
    {
        FloatFrame frame = outputBuffer_[n/2];
        int32_t leftSample = static_cast<int32_t>(frame.left * kFloatToInt24);
        int32_t rightSample = static_cast<int32_t>(frame.right * kFloatToInt24);
        audioDacDataCache_[n] = leftSample << 8;
        audioDacDataCache_[n+1] = rightSample << 8;
    }

    std::memcpy(const_cast<int32_t*>(audioOutPointer_), audioDacDataCache_, sizeof(audioDacDataCache_));

    std::memcpy(audioAdcDataCache_, const_cast<int32_t*>(audioInPointer_), sizeof(audioAdcDataCache_));

    for(int n = 0; n < kBufferSize; n++) 
    {
        //OPTION 1: Masking to 24 bits and sign extending if necessary
        // audioAdcDataCache[n]	&= 0xFFFFFF;

		// if (audioAdcDataCache[n] & 0x800000) 
        // {
		// 	audioAdcDataCache[n] |= ~0xFFFFFF;
		// }

        //OPTION 2: Left shift
        audioAdcDataCache_[n] = audioAdcDataCache_[n] >> 8;
    }

    for(int n = 0; n < kBufferSize - 1; n+=2) 
    {
        FloatFrame frame;
        frame.left = static_cast<float>((audioAdcDataCache_[n]) * kInt24ToFloat);
        frame.right = static_cast<float>((audioAdcDataCache_[n+1]) * kInt24ToFloat);
        inputBuffer_[n/2] = frame;
    }

    receiveReady_ = false;
    transmitReady_ = false;

    gSystem.onAudioReady();

}

void Audio::rxHalfComplete(){

    audioInPointer_ = &audioAdcDataDMA_[0];
    receiveReady_ = true;
    if(transmitReady_) { packUnpackAudioData(); }

}

void Audio::rxComplete(){

    audioInPointer_ = &audioAdcDataDMA_[kBufferSize];
    receiveReady_ = true;
    if(transmitReady_) { packUnpackAudioData(); }

}

void Audio::txHalfComplete(){

    audioOutPointer_ = &audioDacDataDMA_[0];
    transmitReady_ = true;
    if (receiveReady_) { packUnpackAudioData(); }

}

void Audio::txComplete(){

    audioOutPointer_ = &audioDacDataDMA_[kBufferSize];
    transmitReady_ = true;
    if (receiveReady_) { packUnpackAudioData(); }

}

AudioBuffer Audio::getInputBuffer(){

    return {inputBufferPointer_, kFrameBufferSize};

}

AudioBuffer Audio::getOutputBuffer(){

    return {outputBufferPointer_, kFrameBufferSize};

}

void Audio::resetCodec(){

    HAL_GPIO_WritePin(CODEC_NRST_GPIO_Port, CODEC_NRST_Pin, GPIO_PIN_RESET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(CODEC_NRST_GPIO_Port, CODEC_NRST_Pin, GPIO_PIN_SET);
    HAL_Delay(50);

}

