#include "pvinv_h563_ll.h"
TIM_TypeDef fake_tim8;
int main(void){ PVINV_LL_Init(); PVINV_LL_SetRawSamples(100,100,100,100,100); PVINV_LL_Start(); PVINV_LL_OnAdcDmaCompleteIrq(); return 0; }
