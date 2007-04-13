/*-------------------------------------------------------------------*
 *                         SYN_FILT.C                                *
 *-------------------------------------------------------------------*
 * Do the synthesis filtering 1/A(z).                                *
 *-------------------------------------------------------------------*/

#include "typedef.h"
#include "acelp.h"
#include "basic_op.h"
#include "math_op.h"
#include "count.h"
#include "cnst.h"


void Syn_filt(
     Word16 a[],                           /* (i) Q12 : a[m+1] prediction coefficients           */
     Word16 m,                             /* (i)     : order of LP filter                       */
     Word16 x[],                           /* (i)     : input signal                             */
     Word16 y[],                           /* (o)     : output signal                            */
     Word16 lg,                            /* (i)     : size of filtering                        */
     Word16 mem[],                         /* (i/o)   : memory associated with this filtering.   */
     Word16 update                         /* (i)     : 0=no update, 1=update of memory.         */
)
{
    Word16 i, j, y_buf[L_SUBFR16k + M16k], a0, s;
    Word32 L_tmp;
    Word16 *yy;

    yy = &y_buf[0];                        move16();

    /* copy initial filter states into synthesis buffer */
    for (i = 0; i < m; i++)
    {
        *yy++ = mem[i];                    move16();
    }

    s = sub(norm_s(a[0]), 2);
    a0 = shr(a[0], 1);                     /* input / 2 */

    /* Do the filtering. */

    for (i = 0; i < lg; i++)
    {
        L_tmp = L_mult(x[i], a0);

        for (j = 1; j <= m; j++)
            L_tmp = L_msu(L_tmp, a[j], yy[i - j]);

        L_tmp = L_shl(L_tmp, add(3, s));

        y[i] = yy[i] = round(L_tmp);       move16();move16();
    }

    /* Update memory if required */
    test();
    if (update)
        for (i = 0; i < m; i++)
        {
            mem[i] = yy[lg - m + i];       move16();
        }

    return;
}


void Syn_filt_32(
     Word16 a[],                           /* (i) Q12 : a[m+1] prediction coefficients */
     Word16 m,                             /* (i)     : order of LP filter             */
     Word16 exc[],                         /* (i) Qnew: excitation (exc[i] >> Qnew)    */
     Word16 Qnew,                          /* (i)     : exc scaling = 0(min) to 8(max) */
     Word16 sig_hi[],                      /* (o) /16 : synthesis high                 */
     Word16 sig_lo[],                      /* (o) /16 : synthesis low                  */
     Word16 lg                             /* (i)     : size of filtering              */
)
{
    Word16 i, j, a0, s;
    Word32 L_tmp;

    s = sub(norm_s(a[0]), 2);

    a0 = shr(a[0], add(4, Qnew));          /* input / 16 and >>Qnew */

    /* Do the filtering. */

    for (i = 0; i < lg; i++)
    {
        L_tmp = 0;                         move32();
        for (j = 1; j <= m; j++)
            L_tmp = L_msu(L_tmp, sig_lo[i - j], a[j]);

        L_tmp = L_shr(L_tmp, 16 - 4);      /* -4 : sig_lo[i] << 4 */

        L_tmp = L_mac(L_tmp, exc[i], a0);

        for (j = 1; j <= m; j++)
            L_tmp = L_msu(L_tmp, sig_hi[i - j], a[j]);

        /* sig_hi = bit16 to bit31 of synthesis */
        L_tmp = L_shl(L_tmp, add(3, s));           /* ai in Q12 */
        sig_hi[i] = extract_h(L_tmp);      move16();

        /* sig_lo = bit4 to bit15 of synthesis */
        L_tmp = L_shr(L_tmp, 4);           /* 4 : sig_lo[i] >> 4 */
        sig_lo[i] = extract_l(L_msu(L_tmp, sig_hi[i], 2048));   move16();
    }

    return;
}
