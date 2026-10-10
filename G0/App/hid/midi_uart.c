#include "midi_uart.h"
#include "main.h"
#include "usart.h"

// 164 ms at the full MIDI rate: a page change in the preset log stalls the thread
// for up to two page erases. A ring the DMA laps loses bytes undetected.
#define RING_SIZE 512u

// Written by the DMA behind the thread's back.
static volatile uint8_t ring[RING_SIZE];
static uint16_t readIndex;

void midiUartInit(void)
{
    // Not HAL_UART_Receive_DMA(): its error interrupt aborts the DMA on any framing
    // error, as a cable plugged in mid-stream causes. Here a bad byte is dropped
    // (DDRE clear) and reception carries on, with no error handling to write.
    readIndex = 0u;
    (void) HAL_DMA_Start(huart2.hdmarx, (uint32_t) &USART2->RDR, (uint32_t) ring, RING_SIZE);
    SET_BIT(USART2->CR3, USART_CR3_DMAR);
}

static uint16_t writeIndex(void)
{
    // The counter reloads at 0, so reading it there is RING_SIZE or 0.
    return (uint16_t) ((RING_SIZE - __HAL_DMA_GET_COUNTER(huart2.hdmarx)) % RING_SIZE);
}

uint16_t midiUartAvailable(void)
{
    return (uint16_t) ((writeIndex() + RING_SIZE - readIndex) % RING_SIZE);
}

uint8_t midiUartRead(void)
{
    const uint8_t byte = ring[readIndex];
    readIndex = (uint16_t) ((readIndex + 1u) % RING_SIZE);
    return byte;
}
