#include "pvinv_pr.h"
#include "pvinv_utils.h"
#include <math.h>

#define PVINV_PR_TWO_PI (6.28318530717958647692f)

void PVINV_PR_Init(PVINV_PR_t *pr,
                   float kp,
                   float kr,
                   float f0_hz,
                   float wc_hz,
                   float ts,
                   float out_min,
                   float out_max)
{
    float w0;
    float wc;
    float den;

    if (pr == 0)
    {
        return;
    }

    w0 = PVINV_PR_TWO_PI * f0_hz;
    wc = PVINV_PR_TWO_PI * wc_hz;

    pr->kp = kp;
    pr->kr = kr;
    pr->w0 = w0;
    pr->wc = wc;
    pr->ts = ts;
    pr->out_min = out_min;
    pr->out_max = out_max;

    /*
     * 准PR谐振项：
     * Gres(s)=Kr*(2*wc*s)/(s^2+2*wc*s+w0^2)
     * Tustin离散。
     */
    den = 4.0f + 4.0f * wc * ts + w0 * w0 * ts * ts;

    pr->b0 = (4.0f * kr * wc * ts) / den;
    pr->b1 = 0.0f;
    pr->b2 = -pr->b0;

    pr->a1 = (-8.0f + 2.0f * w0 * w0 * ts * ts) / den;
    pr->a2 = (4.0f - 4.0f * wc * ts + w0 * w0 * ts * ts) / den;

    PVINV_PR_Reset(pr);
}

void PVINV_PR_Reset(PVINV_PR_t *pr)
{
    if (pr == 0)
    {
        return;
    }

    pr->e1 = 0.0f;
    pr->e2 = 0.0f;
    pr->y1 = 0.0f;
    pr->y2 = 0.0f;
    pr->out = 0.0f;
}

float PVINV_PR_Update(PVINV_PR_t *pr, float err)
{
    float y_res;
    float out;

    if (pr == 0)
    {
        return 0.0f;
    }

    y_res = pr->b0 * err +
            pr->b1 * pr->e1 +
            pr->b2 * pr->e2 -
            pr->a1 * pr->y1 -
            pr->a2 * pr->y2;

    pr->e2 = pr->e1;
    pr->e1 = err;
    pr->y2 = pr->y1;
    pr->y1 = y_res;

    out = pr->kp * err + y_res;
    out = PVINV_ClampFloat(out, pr->out_min, pr->out_max);

    pr->out = out;
    return out;
}
