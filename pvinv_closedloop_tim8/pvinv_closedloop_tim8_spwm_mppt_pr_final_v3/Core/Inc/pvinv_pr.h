#ifndef __PVINV_PR_H__
#define __PVINV_PR_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    float kp;
    float kr;
    float w0;
    float wc;
    float ts;
    float out_min;
    float out_max;

    float b0;
    float b1;
    float b2;
    float a1;
    float a2;

    float e1;
    float e2;
    float y1;
    float y2;

    float out;
} PVINV_PR_t;

void PVINV_PR_Init(PVINV_PR_t *pr,
                   float kp,
                   float kr,
                   float f0_hz,
                   float wc_hz,
                   float ts,
                   float out_min,
                   float out_max);

void PVINV_PR_Reset(PVINV_PR_t *pr);
float PVINV_PR_Update(PVINV_PR_t *pr, float err);

#ifdef __cplusplus
}
#endif

#endif
