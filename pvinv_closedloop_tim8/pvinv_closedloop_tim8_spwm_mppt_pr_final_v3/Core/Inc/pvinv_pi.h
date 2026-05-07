#ifndef __PVINV_PI_H__
#define __PVINV_PI_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    float kp;
    float ki;
    float ts;
    float out_min;
    float out_max;
    float integrator;
    float out;
} PVINV_PI_t;

void PVINV_PI_Init(PVINV_PI_t *pi, float kp, float ki, float ts, float out_min, float out_max);
void PVINV_PI_Reset(PVINV_PI_t *pi);
float PVINV_PI_Update(PVINV_PI_t *pi, float err);

#ifdef __cplusplus
}
#endif

#endif
