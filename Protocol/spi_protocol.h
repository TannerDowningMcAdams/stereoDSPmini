#ifndef SPI_PROTOCOL_H
#define SPI_PROTOCOL_H

#include <stdint.h>

#define SPI_PACKET_NUM_WORDS   16
#define PROTOCOL_CRC_POLY      0x11021

typedef struct __attribute__((packed))
{
    uint16_t messageId;
    uint16_t vcaValue; // 12 bit
    uint16_t flags;
    uint16_t pot[5]; // 12 bits each
    float beatsPerSecond; // 32 bits, uses 2 words
    uint16_t clockPhase;
    uint16_t reserved[4]; // Zero padding for future use
    uint16_t crc; // Do not read or write in software


} SpiControlPacket;

// Bit positions for flags word
#define MODE_SWITCH_MASK    (0b11) // Keep at LSB to avoid bit shifting
#define FLAG_RELAY_RIGHT    (1 << 2)
#define FLAG_RELAY_LEFT     (1 << 3)
#define FLAG_KILL_WET       (1 << 4)

#endif