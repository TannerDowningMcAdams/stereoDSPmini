#pragma once

#include <stdint.h>

// MIDI in on USART2, received by circular DMA into a ring that the thread drains.
// The DMA keeps receiving through a flash erase, which stalls the CPU and every
// interrupt for 20-40 ms. The only HAL-facing part of MIDI.

void midiUartInit(void);

// Thread mode. Bytes received and not yet read.
uint16_t midiUartAvailable(void);

// Thread mode. The oldest unread byte; call only while midiUartAvailable() is nonzero.
uint8_t midiUartRead(void);
