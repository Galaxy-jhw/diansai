#ifndef __PVINV_MPPT_INCCOND_H__
#define __PVINV_MPPT_INCCOND_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "pvinv_project_config.h"
#include "pvinv_pi.h"
#include <stdint.h>

typedef struct
{
    uint32_t enabled;
    uint32_t tick_div;

    float v_ref;
    float v_prev;
    float i_prev;
    float p_now;
    float p_prev;

    float i_amp_cmd;

    PVINV_PI_t v_loop_pi;
} PVINV_MPPT_t;

void PVINV_MPPT_Init(PVINV_MPPT_t *mppt, float control_ts);
void PVINV_MPPT_Reset(PVINV_MPPT_t *mppt);
void PVINV_MPPT_SetEnable(PVINV_MPPT_t *mppt, uint32_t en);

float PVINV_MPPT_Update(PVINV_MPPT_t *mppt, float ud, float id);

float PVINV_MPPT_GetVref(PVINV_MPPT_t *mppt);
float PVINV_MPPT_GetIampCmd(PVINV_MPPT_t *mppt);

#ifdef __cplusplus
}
#endif

#endif
