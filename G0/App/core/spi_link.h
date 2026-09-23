#pragma once

#include "spi_protocol.h"

// The G0 end of the 1 ms SPI link to the H7 (plan §5). The G0 is master and drives CS
// in software; the H7 treats each CS rising edge as the end of a frame. There is no
// init: CS idles high from MX_GPIO_Init, and the first CC1 compare starts the link.

// TIM1 CC1 ISR: take the frame the thread committed, if any, and start the transfer.
void spiLinkStartFrame(void);

// Thread mode, once per tick. Takes the latest received frame, refreshes the IWDG
// while frames are completing, and enters the bootloader on the H7's request.
void spiLinkPoll(void);

// Thread mode. The buffer for the next frame, or NULL while the previous commit has
// not been sent yet. The link fills the header on commit.
G0ToH7Packet* spiLinkBeginTx(void);
void          spiLinkCommitTx(void);

// Thread mode. The H7's last valid frame, or NULL if none arrived within the link
// timeout.
const H7ToG0Packet* spiLinkEcho(void);
