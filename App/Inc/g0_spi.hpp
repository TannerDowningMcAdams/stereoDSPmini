#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include "spi.h"
#include "stm32h7xx_hal.h"
#ifdef __cplusplus
}
#endif

#include <cstdint>

class G0Spi {
public:

    G0Spi() = default;
    ~G0Spi() = default;

    uiParams params_;

    enum class CommStatus { BUSY, READY, OK, ERROR };
    static constexpr uint16_t kSpiPacketWords = 16;

    void init(); 
    void txRxComplete();
    void spiErrorCallback();
    
private:

    SPI_HandleTypeDef *handle_ = &hspi2;

    CommStatus status_;

    // DMA buffers for SPI transmission and reception
    static volatile uint16_t spiRxDataDMA_[kSpiPacketWords];
    static volatile uint16_t spiTxDataDMA_[kSpiPacketWords];
    
    // Cached copy buffers excluding CRC word
    int16_t spiRxDataCache_[kSpiPacketWords - 1];
    int16_t spiTxDataCache_[kSpiPacketWords - 1];

    void packUnpackSpiData();
    void parseControlPacket();
    void spiErrorHandler();
} ;

typedef struct {

} g0Params;

typedef struct {

    float potentiometers[5];
    bool toggle_1;
    bool toggle_2;
    bool relay_L;
    bool relay_R;
    float beats_per_second;
    float clock_phase;

} uiParams;