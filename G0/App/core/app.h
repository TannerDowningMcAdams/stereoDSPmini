#pragma once

void appInit(void);
void appPoll(void);

typedef enum {

    TRUE_BYPASS,
    VCA_BYPASS

} BypassMode;

typedef enum {

    BYPASS,
    ENGAGED

} BypassState;
