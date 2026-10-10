#pragma once

#include "spi_protocol.h"
#include <stdbool.h>

// The UI contexts: BOOT, then BYPASS, RUN, the RECALL, STORE and ENGINE menus, and
// SETTINGS, which only a cold boot with ON held enters. The live state it sends is
// param_store's.

void uiInit(void);

// 1 ms tick, after switchPoll() and any pot update.
void uiTick(void);

// Fills the payload of the next frame. False during BOOT, when no frame is sent and
// the H7 holds its last applied state.
bool uiFillFrame(UiToH7Packet* tx);
