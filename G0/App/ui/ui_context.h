#pragma once

#include "spi_protocol.h"
#include <stdbool.h>

// The UI contexts of plan §3.3: BOOT, then BYPASS, RUN, and the RECALL, STORE and
// ENGINE menus. SETTINGS arrives with M5. The live state it sends is param_store's.

void uiInit(void);

// 1 ms tick, after switchPoll() and any pot update.
void uiTick(void);

// Fills the payload of the next frame. False during BOOT, when no frame is sent and
// the H7 holds its last applied state.
bool uiFillFrame(G0ToH7Packet* tx);
