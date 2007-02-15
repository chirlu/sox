/*------------------------------------------------------------------------*
 *                         P_MED_OL.C                                     *
 *------------------------------------------------------------------------*
 * Compute the open loop pitch lag.                                       *
 *------------------------------------------------------------------------*/

#include "typedef.h"
#include "basic_op.h"
#include "acelp.h"
#include "oper_32b.h"
#include "count.h"
#include "math_op.h"

#include "p_med_ol.tab"


Word16 Pitch_med_ol(                       /* output: open loop pitch lag                             */
     Word16 wsp[],                         /* input : signal used to compute the open loop pitch      */
                                           /*         wsp[-pit_max] to wsp[-1] should be known        */
     Word16 L_min,                         /* input : minimum pitch lag                               */
     Word16 L_max,                         /* input : maximum pitch lag                               */
     Word16 L_frame,                       /* input : length of frame to compute pitch                */
     Word16 L_0,                           /* input : old_ open-loop pitch                            */
     Word16 * gain,                        /* output: normalize correlation of hp_wsp for the Lag     */
     Word16 * hp_wsp_mem,                  /* i:o   : memory of the hypass filter for hp_wsp[] (lg=9) */
     Word16 * old_hp_wsp,                  /* i:o   : hypass wsp[]                                    */
     Word16 wght_flg                       /* input : is weighting function used                      */
)
{
    Word16 i, j, Tm;
    Word16 hi, lo;
    Word16 *ww, *we, *hp_wsp;
    Word16 exp_R0, exp_R1, exp_R2;
    Word32 max, R0, R1, R2;

    ww = &corrweight[198];
    move16();
    we = &corrweight[98 + L_max - L_0];
    move16();

    max = MIN_32;                          move32();
    Tm = 0;                                move16();
    for (i = L_max; i > L_min; i--)
    {
        /* Compute the correlation */

        R0 = 0;                            move32();
        for (j = 0; j < L_frame; j++)
            R0 = L_mac(R0, wsp[j], wsp[j - i]);

        /* Weighting of the correlation function.   */

        L_Extract(R0, &hi, &lo);
        R0 = Mpy_32_16(hi, lo, *ww);
        ww--;

        test();
        test();
        if ((L_0 > 0) && (wght_flg > 0))
        {
            /* Weight the neighbourhood of the old lag. */
            L_Extract(R0, &hi, &lo);
            R0 = Mpy_32_16(hi, lo, *we);
            we--;
            move16();
        }
        test();
        if (L_sub(R0, max) >= 0)
        {
            max = R0;
            move32();
            Tm = i;
            move16();
        }
    }

    /* Hypass the wsp[] vector */

    hp_wsp = old_hp_wsp + L_max;           move16();
    Hp_wsp(wsp, hp_wsp, L_frame, hp_wsp_mem);

    /* Compute normalize correlation at delay Tm */

    R0 = 0;                                move32();
    R1 = 1L;                               move32();
    R2 = 1L;                               move32();
    for (j = 0; j < L_frame; j++)
    {
        R0 = L_mac(R0, hp_wsp[j], hp_wsp[j - Tm]);
        R1 = L_mac(R1, hp_wsp[j - Tm], hp_wsp[j - Tm]);
        R2 = L_mac(R2, hp_wsp[j], hp_wsp[j]);
    }

    /* gain = R0/ sqrt(R1*R2) */

    exp_R0 = norm_l(R0);
    R0 = L_shl(R0, exp_R0);

    exp_R1 = norm_l(R1);
    R1 = L_shl(R1, exp_R1);

    exp_R2 = norm_l(R2);
    R2 = L_shl(R2, exp_R2);


    R1 = L_mult(round(R1), round(R2));

    i = norm_l(R1);
    R1 = L_shl(R1, i);

    exp_R1 = add(exp_R1, exp_R2);
    exp_R1 = add(exp_R1, i);
    exp_R1 = sub(62, exp_R1);

    Isqrt_n(&R1, &exp_R1);

    R0 = L_mult(round(R0), round(R1));
    exp_R0 = sub(31, exp_R0);
    exp_R0 = add(exp_R0, exp_R1);

    *gain = round(L_shl(R0, exp_R0));
    move16();

    /* Shitf hp_wsp[] for next frame */

    for (i = 0; i < L_max; i++)
    {
        old_hp_wsp[i] = old_hp_wsp[i + L_frame];
        move16();
    }

    return (Tm);
}

/*____________________________________________________________________
 |
 |
 |  FUNCTION NAME median5
 |
 |      Returns the median of the set {X[-2], X[-1],..., X[2]},
 |      whose elements are 16-bit integers.
 |
 |  INPUT
 |      X[-2:2]   16-bit integers.
 |
 |  RETURN VALUE
 |      The median of {X[-2], X[-1],..., X[2]}.
 |_____________________________________________________________________
 */

Word16 median5(Word16 x[])
{
    Word16 x1, x2, x3, x4, x5;
    Word16 tmp;

    x1 = x[-2];                            move16();
    x2 = x[-1];                            move16();
    x3 = x[0];                             move16();
    x4 = x[1];                             move16();
    x5 = x[2];                             move16();

    test();test();test();test();test();test();test();test();test();

    if (sub(x2, x1) < 0)
    {
        tmp = x1;
        x1 = x2;
        x2 = tmp;                          move16();move16();move16();
    }
    if (sub(x3, x1) < 0)
    {
        tmp = x1;
        x1 = x3;
        x3 = tmp;                          move16();move16();move16();
    }
    if (sub(x4, x1) < 0)
    {
        tmp = x1;
        x1 = x4;
        x4 = tmp;                          move16();move16();move16();
    }
    if (sub(x5, x1) < 0)
    {
        x5 = x1;                           move16();
    }
    if (sub(x3, x2) < 0)
    {
        tmp = x2;
        x2 = x3;
        x3 = tmp;                          move16();move16();move16();
    }
    if (sub(x4, x2) < 0)
    {
        tmp = x2;
        x2 = x4;
        x4 = tmp;                          move16();move16();move16();
    }
    if (sub(x5, x2) < 0)
    {
        x5 = x2;                           move16();
    }
    if (sub(x4, x3) < 0)
    {
        x3 = x4;                           move16();
    }
    if (sub(x5, x3) < 0)
    {
        x3 = x5;                           move16();
    }
    return (x3);
}

/*____________________________________________________________________
 |
 |
 |  FUNCTION NAME med_olag
 |
 |
 |_____________________________________________________________________
 */


Word16 Med_olag(                           /* output : median of  5 previous open-loop lags       */
     Word16 prev_ol_lag,                   /* input  : previous open-loop lag                     */
     Word16 old_ol_lag[5]
)
{
    Word16 i;

    /* Use median of 5 previous open-loop lags as old lag */

    for (i = 4; i > 0; i--)
    {
        old_ol_lag[i] = old_ol_lag[i - 1]; move16();
    }

    old_ol_lag[0] = prev_ol_lag;           move16();

    i = median5(&old_ol_lag[2]);

    return i;

}
