#pragma once

#include <stdint.h>

void bridgeInit(void);
void bridgePoll(void);

// Called from the USB interrupt via usbd_cdc_if.c.
void bridgeOnUsbReceive(uint8_t* data, uint32_t length);
void bridgeSetLineCoding(const uint8_t* coding);
void bridgeGetLineCoding(uint8_t* coding);
