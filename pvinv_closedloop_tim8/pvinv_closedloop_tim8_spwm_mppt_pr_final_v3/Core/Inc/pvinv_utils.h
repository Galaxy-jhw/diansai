#ifndef __PVINV_UTILS_H__
#define __PVINV_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

static inline float PVINV_ClampFloat(float x, float min_v, float max_v)
{
    if (x > max_v)
    {
        return max_v;
    }

    if (x < min_v)
    {
        return min_v;
    }

    return x;
}

static inline float PVINV_Lpf1(float old_v, float new_v, float alpha)
{
    return old_v + alpha * (new_v - old_v);
}

#ifdef __cplusplus
}
#endif

#endif
