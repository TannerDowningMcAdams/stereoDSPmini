#include "system.hpp"

#ifdef __cplusplus
extern "C" {
#endif
#include "spi.h"
#include "sai.h"
#include "main.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_sai.h"
#include "stm32h7xx_hal_spi.h"
#ifdef __cplusplus
}
#endif

extern System gSystem;

#ifdef __cplusplus
extern "C" {
#endif

    void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai){
        
        gSystem.audioRxHalfComplete(); 
        
    }
    void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai){
    
        gSystem.audioRxComplete(); 
        
    }
    void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai){

        gSystem.audioTxHalfComplete(); 
        
    }
    void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai){

        gSystem.audioTxComplete(); 

    }
    void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai){

        gSystem.audioErrorCallback();

    }

    void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {

        gSystem.spiTxRxComplete();

    }

    void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi){

        gSystem.spiErrorCallback();

    }

#ifdef __cplusplus
}
#endif


