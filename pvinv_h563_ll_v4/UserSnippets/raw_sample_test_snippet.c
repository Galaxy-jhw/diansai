/* 如果你暂时没有配置 ADC DMA，可以先用这段函数测试控制算法数据通路。
 * 真实上板时必须用真实 ADC raw 替代这些数值。
 */
#include "pvinv_h563_ll.h"

void PVINV_TestFeedRawSamples(void)
{
    uint16_t ud_raw   = 1500;  /* 示例：约 30V，取决于你的 PVINV_UD_SCALE */
    uint16_t id_raw   = 2100;
    uint16_t uref_raw = 2048;
    uint16_t ifb_raw  = 2048;
    uint16_t uo_raw   = 2048;

    PVINV_LL_SetRawSamples(ud_raw, id_raw, uref_raw, ifb_raw, uo_raw);
}
