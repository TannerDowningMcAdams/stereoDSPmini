#include <stdbool.h>

void appTick(void);
void onAdcReady(void);
bool adcDataReady(void);
void packSpiData(void);
void initiateSpiDma(void);

typedef enum {

    TRUE_BYPASS,
    VCA_BYPASS

} BypassMode;

typedef enum {

    BYPASS,
    ENGAGED

} BypassState;

