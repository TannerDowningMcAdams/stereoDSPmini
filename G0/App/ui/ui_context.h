#pragma once

#include "spi_protocol.h"
#include <stdbool.h>

// The UI contexts of plan §3.3: BOOT, then BYPASS and RUN. RECALL, STORE and
// SETTINGS arrive with M4 and M5. Owns the live state the G0 sends until
// param_store (G3) takes it over.

void uiInit(void);

// 1 ms tick, after switchPoll() and any pot update.
void uiTick(void);

// Fills the payload of the next frame. False during BOOT, when no frame is sent and
// the H7 holds its last applied state.
bool uiFillFrame(G0ToH7Packet* tx);
