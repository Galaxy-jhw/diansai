#include "pvinv_mppt_inccond.h"
#include "pvinv_utils.h"
#include <math.h>

void PVINV_MPPT_Init(PVINV_MPPT_t *mppt, float control_ts)
{
    if (mppt == 0)
    {
        return;
    }

    mppt->enabled = PVINV_MPPT_ENABLE_DEFAULT;
    mppt->tick_div = 0u;

    mppt->v_ref = PVINV_MPPT_VREF_INIT;
    mppt->v_prev = 0.0f;
    mppt->i_prev = 0.0f;
    mppt->p_now = 0.0f;
    mppt->p_prev = 0.0f;
    mppt->i_amp_cmd = 0.0f;

    PVINV_PI_Init(&mppt->v_loop_pi,
                  PVINV_VPV_PI_KP,
                  PVINV_VPV_PI_KI,
                  control_ts,
                  PVINV_IAMP_MIN,
                  PVINV_IAMP_MAX);
}

void PVINV_MPPT_Reset(PVINV_MPPT_t *mppt)
{
    if (mppt == 0)
    {
        return;
    }

    mppt->tick_div = 0u;
    mppt->v_ref = PVINV_MPPT_VREF_INIT;
    mppt->v_prev = 0.0f;
    mppt->i_prev = 0.0f;
    mppt->p_now = 0.0f;
    mppt->p_prev = 0.0f;
    mppt->i_amp_cmd = 0.0f;
    PVINV_PI_Reset(&mppt->v_loop_pi);
}

void PVINV_MPPT_SetEnable(PVINV_MPPT_t *mppt, uint32_t en)
{
    if (mppt == 0)
    {
        return;
    }

    mppt->enabled = en ? 1u : 0u;
}

float PVINV_MPPT_Update(PVINV_MPPT_t *mppt, float ud, float id)
{
    float dv;
    float di;
    float slope;
    float err_v;

    if (mppt == 0)
    {
        return 0.0f;
    }

    if (mppt->enabled == 0u)
    {
        return 0.0f;
    }

    mppt->p_now = ud * id;

    mppt->tick_div++;
    if (mppt->tick_div >= PVINV_MPPT_PERIOD_ISR_TICKS)
    {
        mppt->tick_div = 0u;

        dv = ud - mppt->v_prev;
        di = id - mppt->i_prev;

        /*
         * 电导增量法：
         * dP/dV = I + V*dI/dV。
         * slope = dI/dV + I/V。
         *
         * slope>0：MPP左侧，升高Vref；
         * slope<0：MPP右侧，降低Vref。
         */
        if (fabsf(dv) > PVINV_MPPT_DV_EPS)
        {
            slope = (di / dv) + (id / (ud + 0.001f));

            if (slope > PVINV_MPPT_SLOPE_DEADBAND)
            {
                mppt->v_ref += PVINV_MPPT_VREF_STEP;
            }
            else if (slope < -PVINV_MPPT_SLOPE_DEADBAND)
            {
                mppt->v_ref -= PVINV_MPPT_VREF_STEP;
            }
            else
            {
                /* near MPP */
            }
        }

        mppt->v_ref = PVINV_ClampFloat(mppt->v_ref,
                                        PVINV_MPPT_VREF_MIN,
                                        PVINV_MPPT_VREF_MAX);

        mppt->v_prev = ud;
        mppt->i_prev = id;
        mppt->p_prev = mppt->p_now;
    }

    /*
     * PV电压外环：
     * error = Ud - Vmppt_ref。
     * Ud高于目标时提高取电流幅值；
     * Ud低于目标时降低取电流幅值。
     */
    err_v = ud - mppt->v_ref;
    mppt->i_amp_cmd = PVINV_PI_Update(&mppt->v_loop_pi, err_v);
    mppt->i_amp_cmd = PVINV_ClampFloat(mppt->i_amp_cmd, PVINV_IAMP_MIN, PVINV_IAMP_MAX);

    return mppt->i_amp_cmd;
}

float PVINV_MPPT_GetVref(PVINV_MPPT_t *mppt)
{
    if (mppt == 0)
    {
        return 0.0f;
    }

    return mppt->v_ref;
}

float PVINV_MPPT_GetIampCmd(PVINV_MPPT_t *mppt)
{
    if (mppt == 0)
    {
        return 0.0f;
    }

    return mppt->i_amp_cmd;
}
