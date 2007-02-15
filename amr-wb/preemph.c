/*-------------------------------------------------------------------*
 *                         PREEMPH.C                                 *
 *-------------------------------------------------------------------*
 * Preemphasis: filtering through 1 - g z^-1                         *
 *                                                                   *
 * Preemph2 --> signal is multiplied by 2.                           *
 *-------------------------------------------------------------------*/

#include "typedef.h"
#include "basic_op.h"
#include "count.h"


void Preemph(
     Word16 x[],                           /* (i/o)   : input signal overwritten by the output */
     Word16 mu,                            /* (i) Q15 : preemphasis coefficient                */
     Word16 lg,                            /* (i)     : lenght of filtering                    */
     Word16 * mem                          /* (i/o)   : memory (x[-1])                         */
)
{
    Word16 i, temp;
    Word32 L_tmp;

    temp = x[lg - 1];                      move16();

    for (i = (Word16) (lg - 1); i > 0; i--)
    {
        L_tmp = L_deposit_h(x[i]);
        L_tmp = L_msu(L_tmp, x[i - 1], mu);
        x[i] = round(L_tmp);               move16();
    }

    L_tmp = L_deposit_h(x[0]);
    L_tmp = L_msu(L_tmp, *mem, mu);
    x[0] = round(L_tmp);                   move16();

    *mem = temp;                           move16();

    return;
}


void Preemph2(
     Word16 x[],                           /* (i/o)   : input signal overwritten by the output */
     Word16 mu,                            /* (i) Q15 : preemphasis coefficient                */
     Word16 lg,                            /* (i)     : lenght of filtering                    */
     Word16 * mem                          /* (i/o)   : memory (x[-1])                         */
)
{
    Word16 i, temp;
    Word32 L_tmp;

    temp = x[lg - 1];                      move16();

    for (i = (Word16) (lg - 1); i > 0; i--)
    {
        L_tmp = L_deposit_h(x[i]);
        L_tmp = L_msu(L_tmp, x[i - 1], mu);
        L_tmp = L_shl(L_tmp, 1);
        x[i] = round(L_tmp);               move16();
    }

    L_tmp = L_deposit_h(x[0]);
    L_tmp = L_msu(L_tmp, *mem, mu);
    L_tmp = L_shl(L_tmp, 1);
    x[0] = round(L_tmp);                   move16();

    *mem = temp;                           move16();

    return;
}
