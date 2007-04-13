/*-------------------------------------------------------------------*
 *                         DEEMPH.C                                  *
 *-------------------------------------------------------------------*
 * Deemphasis: filtering through 1/(1-mu z^-1)                       *
 *                                                                   *
 * Deemph2   --> signal is divided by 2.                             *
 * Deemph_32 --> for 32 bits signal.                                 *
 *-------------------------------------------------------------------*/

#include "typedef.h"
#include "acelp.h"
#include "basic_op.h"
#include "math_op.h"
#include "count.h"


void Deemph(
     Word16 x[],                           /* (i/o)   : input signal overwritten by the output */
     Word16 mu,                            /* (i) Q15 : deemphasis factor                      */
     Word16 L,                             /* (i)     : vector size                            */
     Word16 * mem                          /* (i/o)   : memory (y[-1])                         */
)
{
    Word16 i;
    Word32 L_tmp;

    L_tmp = L_deposit_h(x[0]);
    L_tmp = L_mac(L_tmp, *mem, mu);
    x[0] = round(L_tmp);                   move16();

    for (i = 1; i < L; i++)
    {
        L_tmp = L_deposit_h(x[i]);
        L_tmp = L_mac(L_tmp, x[i - 1], mu);
        x[i] = round(L_tmp);               move16();
    }

    *mem = x[L - 1];                       move16();

    return;
}


void Deemph2(
     Word16 x[],                           /* (i/o)   : input signal overwritten by the output */
     Word16 mu,                            /* (i) Q15 : deemphasis factor                      */
     Word16 L,                             /* (i)     : vector size                            */
     Word16 * mem                          /* (i/o)   : memory (y[-1])                         */
)
{
    Word16 i;
    Word32 L_tmp;

    /* saturation can occur in L_mac() */

    L_tmp = L_mult(x[0], 16384);
    L_tmp = L_mac(L_tmp, *mem, mu);
    x[0] = round(L_tmp);                   move16();

    for (i = 1; i < L; i++)
    {
        L_tmp = L_mult(x[i], 16384);
        L_tmp = L_mac(L_tmp, x[i - 1], mu);
        x[i] = round(L_tmp);               move16();
    }

    *mem = x[L - 1];                       move16();

    return;
}


void Deemph_32(
     Word16 x_hi[],                        /* (i)     : input signal (bit31..16) */
     Word16 x_lo[],                        /* (i)     : input signal (bit15..4)  */
     Word16 y[],                           /* (o)     : output signal (x16)      */
     Word16 mu,                            /* (i) Q15 : deemphasis factor        */
     Word16 L,                             /* (i)     : vector size              */
     Word16 * mem                          /* (i/o)   : memory (y[-1])           */
)
{
    Word16 i, fac;
    Word32 L_tmp;

    fac = shr(mu, 1);                      /* Q15 --> Q14 */

    /* L_tmp = hi<<16 + lo<<4 */

    L_tmp = L_deposit_h(x_hi[0]);
    L_tmp = L_mac(L_tmp, x_lo[0], 8);
    L_tmp = L_shl(L_tmp, 3);
    L_tmp = L_mac(L_tmp, *mem, fac);
    L_tmp = L_shl(L_tmp, 1);               /* saturation can occur here */
    y[0] = round(L_tmp);                   move16();

    for (i = 1; i < L; i++)
    {
        L_tmp = L_deposit_h(x_hi[i]);
        L_tmp = L_mac(L_tmp, x_lo[i], 8);
        L_tmp = L_shl(L_tmp, 3);
        L_tmp = L_mac(L_tmp, y[i - 1], fac);
        L_tmp = L_shl(L_tmp, 1);           /* saturation can occur here */
        y[i] = round(L_tmp);               move16();
    }

    *mem = y[L - 1];                       move16();

    return;
}
