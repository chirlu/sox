/*-----------------------------------------------------------------------*
 *                         ISFEXTRP.C                                    *
 *-----------------------------------------------------------------------*
 *  Conversion of 16th-order 12.8kHz ISF vector                          *
 *  into 20th-order 16kHz ISF vector                                     *
 *-----------------------------------------------------------------------*/

#include "typedef.h"
#include "basic_op.h"
#include "oper_32b.h"
#include "cnst.h"
#include "acelp.h"
#include "count.h"

#define INV_LENGTH 2731                    /* 1/12 */

void Isf_Extrapolation(Word16 HfIsf[])
{
    Word16 IsfDiff[M - 2];
    Word32 IsfCorr[3];
    Word32 L_tmp;
    Word16 coeff, mean, tmp, tmp2, tmp3;
    Word16 exp, exp2, hi, lo;
    Word16 i, MaxCorr;

    HfIsf[M16k - 1] = HfIsf[M - 1];        move16();

    /* Difference vector */
    for (i = 1; i < (M - 1); i++)
    {
        IsfDiff[i - 1] = sub(HfIsf[i], HfIsf[i - 1]);   move16();
    }
    L_tmp = 0;                             move32();

    /* Mean of difference vector */
    for (i = 3; i < (M - 1); i++)
        L_tmp = L_mac(L_tmp, IsfDiff[i - 1], INV_LENGTH);
    mean = round(L_tmp);

    IsfCorr[0] = 0;                        move32();
    IsfCorr[1] = 0;                        move32();
    IsfCorr[2] = 0;                        move32();

    tmp = 0;                               move16();
    for (i = 0; i < (M - 2); i++)
    {
        test();
        if (sub(IsfDiff[i], tmp) > 0)
        {
            tmp = IsfDiff[i];              move16();
        }
    }
    exp = norm_s(tmp);
    for (i = 0; i < (M - 2); i++)
    {
        IsfDiff[i] = shl(IsfDiff[i], exp); move16();
    }
    mean = shl(mean, exp);
    for (i = 7; i < (M - 2); i++)
    {
        tmp2 = sub(IsfDiff[i], mean);
        tmp3 = sub(IsfDiff[i - 2], mean);
        L_tmp = L_mult(tmp2, tmp3);
        L_Extract(L_tmp, &hi, &lo);
        L_tmp = Mpy_32(hi, lo, hi, lo);
        IsfCorr[0] = L_add(IsfCorr[0], L_tmp);  move32();
    }
    for (i = 7; i < (M - 2); i++)
    {
        tmp2 = sub(IsfDiff[i], mean);
        tmp3 = sub(IsfDiff[i - 3], mean);
        L_tmp = L_mult(tmp2, tmp3);
        L_Extract(L_tmp, &hi, &lo);
        L_tmp = Mpy_32(hi, lo, hi, lo);
        IsfCorr[1] = L_add(IsfCorr[1], L_tmp);  move32();
    }
    for (i = 7; i < (M - 2); i++)
    {
        tmp2 = sub(IsfDiff[i], mean);
        tmp3 = sub(IsfDiff[i - 4], mean);
        L_tmp = L_mult(tmp2, tmp3);
        L_Extract(L_tmp, &hi, &lo);
        L_tmp = Mpy_32(hi, lo, hi, lo);
        IsfCorr[2] = L_add(IsfCorr[2], L_tmp);  move32();
    }
    test();
    if (L_sub(IsfCorr[0], IsfCorr[1]) > 0)
    {
        MaxCorr = 0;                       move16();
    } else
    {
        MaxCorr = 1;                       move16();
    }

    test();
    if (L_sub(IsfCorr[2], IsfCorr[MaxCorr]) > 0)
        MaxCorr = 2;                       move16();

    MaxCorr = add(MaxCorr, 1);             /* Maximum correlation of difference vector */

    for (i = M - 1; i < (M16k - 1); i++)
    {
        tmp = sub(HfIsf[i - 1 - MaxCorr], HfIsf[i - 2 - MaxCorr]);
        HfIsf[i] = add(HfIsf[i - 1], tmp); move16();
    }

    /* tmp=7965+(HfIsf[2]-HfIsf[3]-HfIsf[4])/6; */
    tmp = add(HfIsf[4], HfIsf[3]);
    tmp = sub(HfIsf[2], tmp);
    tmp = mult(tmp, 5461);
    tmp = add(tmp, 20390);

    test();
    if (sub(tmp, 19456) > 0)
    {                                      /* Maximum value of ISF should be at most 7600 Hz */
        tmp = 19456;                       move16();
    }
    tmp = sub(tmp, HfIsf[M - 2]);
    tmp2 = sub(HfIsf[M16k - 2], HfIsf[M - 2]);

    exp2 = norm_s(tmp2);
    exp = norm_s(tmp);
    exp = sub(exp, 1);
    tmp = shl(tmp, exp);
    tmp2 = shl(tmp2, exp2);
    coeff = div_s(tmp, tmp2);              /* Coefficient for stretching the ISF vector */
    exp = sub(exp2, exp);

    for (i = M - 1; i < (M16k - 1); i++)
    {
        tmp = mult(sub(HfIsf[i], HfIsf[i - 1]), coeff);
        IsfDiff[i - (M - 1)] = shl(tmp, exp);   move16();
    }

    for (i = M; i < (M16k - 1); i++)
    {
        /* The difference between ISF(n) and ISF(n-2) should be at least 500 Hz */
        tmp = sub(add(IsfDiff[i - (M - 1)], IsfDiff[i - M]), 1280);
        test();
        if (tmp < 0)
        {
            test();
            if (sub(IsfDiff[i - (M - 1)], IsfDiff[i - M]) > 0)
            {
                IsfDiff[i - M] = sub(1280, IsfDiff[i - (M - 1)]);       move16();
            } else
            {
                IsfDiff[i - (M - 1)] = sub(1280, IsfDiff[i - M]);       move16();
            }
        }
    }

    for (i = M - 1; i < (M16k - 1); i++)
    {
        HfIsf[i] = add(HfIsf[i - 1], IsfDiff[i - (M - 1)]);     move16();
    }

    for (i = 0; i < (M16k - 1); i++)
    {
        move16();
        HfIsf[i] = mult(HfIsf[i], 26214);  /* Scale the ISF vector correctly for 16000 kHz */
    }

    Isf_isp(HfIsf, HfIsf, M16k);

    return;
}
