#pragma once
#include "ui_params.hpp"
#ifdef __cplusplus
extern "C" {
#endif
#include "spi.h"
#include "stm32h7xx_hal.h"
#include "spi_protocol.h"
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
    static constexpr uint16_t kSpiPacketWords = SPI_PACKET_NUM_WORDS;

    void init(); 
    void txRxComplete() { packUnpackSpiData(); }
    void spiErrorHandler();
    
private:

    SPI_HandleTypeDef *handle_ = &hspi2;

    CommStatus status_;

    static constexpr float int12ToFloat = 1.0f / 4095.0f;

    // DMA buffers for SPI transmission and reception
    static volatile SpiControlPacket rxPacketDMA_;
    static volatile SpiControlPacket txPacketDMA_;

    void packUnpackSpiData();
    void parseControlPacket();
    void initErrorHandler();
    void recoverFromError();
} ;



