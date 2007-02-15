/*-----------------------------------------------------------------------*
 *                         PH_DISP.C                                     *
 *-----------------------------------------------------------------------*
 * post-processing to enhance noise in low bit rate.                     *
 *-----------------------------------------------------------------------*/

#include "typedef.h"
#include "basic_op.h"
#include "cnst.h"
#include "acelp.h"
#include "count.h"

#define pitch_0_9  14746                   /* 0.9 in Q14 */
#define pitch_0_6  9830                    /* 0.6 in Q14 */


#define L_SUBFR 64

/* impulse response with phase dispersion */

/* 2.0 - 6.4 kHz phase dispersion */
static Word16 ph_imp_low[L_SUBFR] =
{
    20182, 9693, 3270, -3437, 2864, -5240, 1589, -1357,
    600, 3893, -1497, -698, 1203, -5249, 1199, 5371,
    -1488, -705, -2887, 1976, 898, 721, -3876, 4227,
    -5112, 6400, -1032, -4725, 4093, -4352, 3205, 2130,
    -1996, -1835, 2648, -1786, -406, 573, 2484, -3608,
    3139, -1363, -2566, 3808, -639, -2051, -541, 2376,
    3932, -6262, 1432, -3601, 4889, 370, 567, -1163,
    -2854, 1914, 39, -2418, 3454, 2975, -4021, 3431
};

/* 3.2 - 6.4 kHz phase dispersion */
static Word16 ph_imp_mid[L_SUBFR] =
{
    24098, 10460, -5263, -763, 2048, -927, 1753, -3323,
    2212, 652, -2146, 2487, -3539, 4109, -2107, -374,
    -626, 4270, -5485, 2235, 1858, -2769, 744, 1140,
    -763, -1615, 4060, -4574, 2982, -1163, 731, -1098,
    803, 167, -714, 606, -560, 639, 43, -1766,
    3228, -2782, 665, 763, 233, -2002, 1291, 1871,
    -3470, 1032, 2710, -4040, 3624, -4214, 5292, -4270,
    1563, 108, -580, 1642, -2458, 957, 544, 2540
};



void Init_Phase_dispersion(
     Word16 disp_mem[]                     /* (i/o): static memory (size = 8) */
)
{
    Set_zero(disp_mem, 8);

    return;
}


void Phase_dispersion(
     Word16 gain_code,                     /* (i) Q0  : gain of code             */
     Word16 gain_pit,                      /* (i) Q14 : gain of pitch            */
     Word16 code[],                        /* (i/o)   : code vector              */
     Word16 mode,                          /* (i)     : level, 0=hi, 1=lo, 2=off */
     Word16 disp_mem[]                     /* (i/o)   : static memory (size = 8) */
)
{
    Word16 i, j, state;
    Word16 *prev_gain_pit, *prev_gain_code, *prev_state;
    Word16 code2[2 * L_SUBFR];

    prev_state = disp_mem;
    prev_gain_code = disp_mem + 1;
    prev_gain_pit = disp_mem + 2;

    Set_zero(code2, 2 * L_SUBFR);

    test();test();move16();
    if (sub(gain_pit, pitch_0_6) < 0)
        state = 0;
    else if (sub(gain_pit, pitch_0_9) < 0)
        state = 1;
    else
        state = 2;

    for (i = 5; i > 0; i--)
    {
        prev_gain_pit[i] = prev_gain_pit[i - 1];        move16();
    }
    prev_gain_pit[0] = gain_pit;           move16();

    if (sub(sub(gain_code, *prev_gain_code), shl(*prev_gain_code, 1)) > 0)
    {
        /* onset */
        test();
        if (sub(state, 2) < 0)
            state = add(state, 1);
    } else
    {
        j = 0;
        for (i = 0; i < 6; i++)
        {
            test();
            if (sub(prev_gain_pit[i], pitch_0_6) < 0)
                j = add(j, 1);
        }
        test();
        if (sub(j, 2) > 0)
        {
            state = 0;                     move16();
        }
        if (sub(sub(state, *prev_state), 1) > 0)
            state = sub(state, 1);
    }

    *prev_gain_code = gain_code;           move16();
    *prev_state = state;                   move16();

    /* circular convolution */

    state = add(state, mode);              /* level of dispersion */

    if (state == 0)
    {
        for (i = 0; i < L_SUBFR; i++)
        {
            test();
            if (code[i] != 0)
            {
                for (j = 0; j < L_SUBFR; j++)
                {
                    move16();
                    code2[i + j] = add(code2[i + j], mult_r(code[i], ph_imp_low[j]));
                }
            }
        }
    } else if (sub(state, 1) == 0)
    {
        for (i = 0; i < L_SUBFR; i++)
        {
            test();
            if (code[i] != 0)
            {
                for (j = 0; j < L_SUBFR; j++)
                {
                    move16();
                    code2[i + j] = add(code2[i + j], mult_r(code[i], ph_imp_mid[j]));
                }
            }
        }
    }
    if (sub(state, 2) < 0)
    {
        for (i = 0; i < L_SUBFR; i++)
        {
            move16();
            code[i] = add(code2[i], code2[i + L_SUBFR]);
        }
    }
    return;
}
