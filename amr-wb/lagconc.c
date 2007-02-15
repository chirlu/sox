/*---------------------------------------------------------*
 *                         LAGCONC.C                       *
 *---------------------------------------------------------*
 * Concealment of LTP lags during bad frames               *
 *---------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>

#include "typedef.h"
#include "basic_op.h"
#include "count.h"
#include "cnst.h"
#include "acelp.h"

#define L_LTPHIST 5
#define ONE_PER_3 10923
#define ONE_PER_LTPHIST 6554

void insertion_sort(Word16 array[], Word16 n);
void insert(Word16 array[], Word16 num, Word16 x);

void Init_Lagconc(Word16 lag_hist[])
{
    Word16 i;

    for (i = 0; i < L_LTPHIST; i++)
    {
        lag_hist[i] = 64;
    }
}

void lagconc(
     Word16 gain_hist[],                   /* (i) : Gain history     */
     Word16 lag_hist[],                    /* (i) : Subframe size    */
     Word16 * T0,
     Word16 * old_T0,
     Word16 * seed,
     Word16 unusable_frame
)
{
    Word16 maxLag, minLag, lastLag, lagDif, meanLag = 0;
    Word16 lag_hist2[L_LTPHIST] = {0};
    Word16 i, tmp, tmp2;
    Word16 minGain, lastGain, secLastGain;
    Word16 D, D2;

    /* Is lag index such that it can be aplied directly or does it has to be subtituted */

    lastGain = gain_hist[4];               move16();
    secLastGain = gain_hist[3];            move16();

    lastLag = lag_hist[0];                 move16();

    /***********SMALLEST history lag***********/
    minLag = lag_hist[0];                  move16();
    for (i = 1; i < L_LTPHIST; i++)
    {
        test();
        if (sub(lag_hist[i], minLag) < 0)
        {
            minLag = lag_hist[i];          move16();
        }
    }
    /*******BIGGEST history lag*******/
    maxLag = lag_hist[0];                  move16();
    for (i = 1; i < L_LTPHIST; i++)
    {
        test();
        if (sub(lag_hist[i], maxLag) > 0)
        {
            maxLag = lag_hist[i];          move16();
        }
    }
    /***********SMALLEST history gain***********/
    minGain = gain_hist[0];                move16();
    for (i = 1; i < L_LTPHIST; i++)
    {
        test();
        if (sub(gain_hist[i], minGain) < 0)
        {
            minGain = gain_hist[i];        move16();
        }
    }
    /***Difference between MAX and MIN lag**/
    lagDif = sub(maxLag, minLag);

    test();
    if (unusable_frame != 0)
    {
        /* LTP-lag for RX_SPEECH_LOST */
        /**********Recognition of the LTP-history*********/
        test();test();test();test();
        if ((sub(minGain, 8192) > 0) && (sub(lagDif, 10) < 0))
        {
            *T0 = *old_T0;                 move16();
        } else if (sub(lastGain, 8192) > 0 && sub(secLastGain, 8192) > 0)
        {
            *T0 = lag_hist[0];             move16();
        } else
        {
            /********SORT************/
            /* The sorting of the lag history */
            for (i = 0; i < L_LTPHIST; i++)
            {
                lag_hist2[i] = lag_hist[i];move16();
            }
            insertion_sort(lag_hist2, 5);

            /* Lag is weighted towards bigger lags */
            /* and random variation is added */
            lagDif = sub(lag_hist2[4], lag_hist2[2]);

            test();
            if (sub(lagDif, 40) > 0)
                lagDif = 40;               move16();

            D = Random(seed);              /* D={-1, ...,1} */
            /* D2={-lagDif/2..lagDif/2} */
            tmp = shr(lagDif, 1);
            D2 = mult(tmp, D);
            tmp = add(add(lag_hist2[2], lag_hist2[3]), lag_hist2[4]);
            *T0 = add(mult(tmp, ONE_PER_3), D2);        move16();
        }
        /* New lag is not allowed to be bigger or smaller than last lag values */
        test();
        if (sub(*T0, maxLag) > 0)
        {
            *T0 = maxLag;                  move16();
        }
        test();
        if (sub(*T0, minLag) < 0)
        {
            *T0 = minLag;                  move16();
        }
    } else
    {
        /* LTP-lag for RX_BAD_FRAME */

        /***********MEAN lag**************/
        meanLag = 0;                       move16();
        for (i = 0; i < L_LTPHIST; i++)
        {
            meanLag = add(meanLag, lag_hist[i]);
        }
        meanLag = mult(meanLag, ONE_PER_LTPHIST);

        tmp = sub(*T0, maxLag);
        tmp2 = sub(*T0, lastLag);

        test();test();test();
        test();test();test();test();
        test();test();test();test();
        test();test();test();
        test();
        if (sub(lagDif, 10) < 0 && (sub(*T0, sub(minLag, 5)) > 0) && (sub(tmp, 5) < 0))
        {
            *T0 = *T0;                     move16();
        } else if (sub(lastGain, 8192) > 0 && sub(secLastGain, 8192) > 0 && (add(tmp2, 10) > 0 && sub(tmp2, 10) < 0))
        {
            *T0 = *T0;                     move16();
        } else if (sub(minGain, 6554) < 0 && sub(lastGain, minGain) == 0 && (sub(*T0, minLag) > 0 && sub(*T0, maxLag) < 0))
        {
            *T0 = *T0;                     move16();
        } else if (sub(lagDif, 70) < 0 && sub(*T0, minLag) > 0 && sub(*T0, maxLag) < 0)
        {
            *T0 = *T0;                     move16();
        } else if (sub(*T0, meanLag) > 0 && sub(*T0, maxLag) < 0)
        {
            *T0 = *T0;                     move16();
        } else
        {
            test();test();
            test();test();
            if ((sub(minGain, 8192) > 0) & (sub(lagDif, 10) < 0))
            {
                *T0 = lag_hist[0];         move16();
            } else if (sub(lastGain, 8192) > 0 && sub(secLastGain, 8192) > 0)
            {
                *T0 = lag_hist[0];         move16();
            } else
            {
                /********SORT************/
                /* The sorting of the lag history */
                for (i = 0; i < L_LTPHIST; i++)
                {
                    lag_hist2[i] = lag_hist[i]; move16();
                }
                insertion_sort(lag_hist2, 5);

                /* Lag is weighted towards bigger lags */
                /* and random variation is added */
                lagDif = sub(lag_hist2[4], lag_hist2[2]);
                test();
                if (sub(lagDif, 40) > 0)
                    lagDif = 40;           move16();

                D = Random(seed);          /* D={-1,.., 1} */
                /* D2={-lagDif/2..lagDif/2} */
                tmp = shr(lagDif, 1);
                D2 = mult(tmp, D);
                tmp = add(add(lag_hist2[2], lag_hist2[3]), lag_hist2[4]);
                *T0 = add(mult(tmp, ONE_PER_3), D2);    move16();
            }
            /* New lag is not allowed to be bigger or smaller than last lag values */
            test();
            if (sub(*T0, maxLag) > 0)
            {
                *T0 = maxLag;              move16();
            }
            test();
            if (sub(*T0, minLag) < 0)
            {
                *T0 = minLag;              move16();
            }
        }
    }
}
void insertion_sort(Word16 array[], Word16 n)
{
    Word16 i;

    for (i = 0; i < n; i++)
    {
        insert(array, i, array[i]);
    }
}


void insert(Word16 array[], Word16 n, Word16 x)
{
    Word16 i;

    for (i = (Word16) (n - 1); i >= 0; i--)
    {
        test();
        if (sub(x, array[i]) < 0)
        {
            array[i + 1] = array[i];       move16();
        } else
            break;
    }
    array[i + 1] = x;                      move16();
}
