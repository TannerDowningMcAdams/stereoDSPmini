#pragma once

void appInit(void);
void appPoll(void);
void onAdcReady(void);

typedef enum {

    TRUE_BYPASS,
    VCA_BYPASS

} BypassMode;

typedef enum {

    BYPASS,
    ENGAGED

} BypassState;
