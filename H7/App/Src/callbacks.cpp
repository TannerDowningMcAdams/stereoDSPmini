#include "system.hpp"

#ifdef __cplusplus
extern "C" {
#endif
#include "spi.h"
#include "sai.h"
#include "main.h"
#include "stm32h7xx_hal.h"
#ifdef __cplusplus
}
#endif

extern System gSystem;

#ifdef __cplusplus
extern "C" {
#endif

    void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai)
    {    
        gSystem.audioRxHalfComplete();    
    }
    void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
    {
        gSystem.audioRxComplete();    
    }
    void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
    {
        gSystem.audioTxHalfComplete();    
    }
    void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
    {
        gSystem.audioTxComplete(); 
    }
    void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
    {
        gSystem.audioErrorCallback(hsai);
    }

    void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
    {
        gSystem.spiTxRxComplete();
    }

    void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
    {
        gSystem.spiErrorCallback();
    }

    // The G0 raises CS at the end of every packet.
    void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
    {
        if (GPIO_Pin == G0_EXTI_Pin) { gSystem.spiFrameEnd(); }
    }

#ifdef __cplusplus
}
#endif


