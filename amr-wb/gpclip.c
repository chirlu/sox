/*-----------------------------------------------------------------*
 *                         GPCLIP.C                                *
 *-----------------------------------------------------------------*
 * To avoid unstable synthesis on frame erasure, the gain need to  *
 * be limited (gain pitch < 1.0) when the following case occurs:   *
 *   - a resonance on LPC filter (lp_disp < 60 Hz)                 *
 *   - a good pitch prediction (lp_gp > 0.95)                      *
 *-----------------------------------------------------------------*/

#include "typedef.h"
#include "acelp.h"
#include "basic_op.h"
#include "count.h"

#define DIST_ISF_MAX    307                /* 120 Hz (6400Hz=16384) */
#define DIST_ISF_THRES  154                /* 60     (6400Hz=16384) */
#define GAIN_PIT_THRES  14746              /* 0.9 in Q14 */
#define GAIN_PIT_MIN    9830               /* 0.6 in Q14 */
#define M               16


void Init_gp_clip(
     Word16 mem[]                          /* (o) : memory of gain of pitch clipping algorithm */
)
{
    mem[0] = DIST_ISF_MAX;                 move16();
    mem[1] = GAIN_PIT_MIN;                 move16();
}


Word16 Gp_clip(
     Word16 mem[]                          /* (i/o) : memory of gain of pitch clipping algorithm */
)
{
    Word16 clip;

    clip = 0;                              move16();  /* move16 */
    test();
    if ((sub(mem[0], DIST_ISF_THRES) < 0) && (sub(mem[1], GAIN_PIT_THRES) > 0))
        clip = 1;                          move16();

    return (clip);
}


void Gp_clip_test_isf(
     Word16 isf[],                         /* (i)   : isf values (in frequency domain)           */
     Word16 mem[]                          /* (i/o) : memory of gain of pitch clipping algorithm */
)
{
    Word16 i, dist, dist_min;

    dist_min = sub(isf[1], isf[0]);

    for (i = 2; i < M - 1; i++)
    {
        dist = sub(isf[i], isf[i - 1]);
        test();
        if (sub(dist, dist_min) < 0)
        {
            dist_min = dist;               move16();
        }
    }

    dist = extract_h(L_mac(L_mult(26214, mem[0]), 6554, dist_min));

    test();
    if (sub(dist, DIST_ISF_MAX) > 0)
    {
        dist = DIST_ISF_MAX;               move16();
    }
    mem[0] = dist;                         move16();

    return;
}


void Gp_clip_test_gain_pit(
     Word16 gain_pit,                      /* (i) Q14 : gain of quantized pitch                    */
     Word16 mem[]                          /* (i/o)   : memory of gain of pitch clipping algorithm */
)
{
    Word16 gain;
    Word32 L_tmp;

    L_tmp = L_mult(29491, mem[1]);
    L_tmp = L_mac(L_tmp, 3277, gain_pit);
    gain = extract_h(L_tmp);

    test();
    if (sub(gain, GAIN_PIT_MIN) < 0)
    {
        gain = GAIN_PIT_MIN;               move16();
    }
    mem[1] = gain;                         move16();

    return;
}
