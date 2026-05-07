#include "pvinv_pi.h"
#include "pvinv_utils.h"

void PVINV_PI_Init(PVINV_PI_t *pi, float kp, float ki, float ts, float out_min, float out_max)
{
    if (pi == 0)
    {
        return;
    }

    pi->kp = kp;
    pi->ki = ki;
    pi->ts = ts;
    pi->out_min = out_min;
    pi->out_max = out_max;
    pi->integrator = 0.0f;
    pi->out = 0.0f;
}

void PVINV_PI_Reset(PVINV_PI_t *pi)
{
    if (pi == 0)
    {
        return;
    }

    pi->integrator = 0.0f;
    pi->out = 0.0f;
}

float PVINV_PI_Update(PVINV_PI_t *pi, float err)
{
    float p;
    float out_unsat;
    float out;

    if (pi == 0)
    {
        return 0.0f;
    }

    p = pi->kp * err;

    pi->integrator += pi->ki * pi->ts * err;
    pi->integrator = PVINV_ClampFloat(pi->integrator, pi->out_min, pi->out_max);

    out_unsat = p + pi->integrator;
    out = PVINV_ClampFloat(out_unsat, pi->out_min, pi->out_max);

    pi->out = out;
    return out;
}
