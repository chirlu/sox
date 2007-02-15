/*-----------------------------------------------------------------------*
 *                         HP400.C                                       *
 *-----------------------------------------------------------------------*
 * Interpolation of the LP parameters in 4 subframes.                    *
 *-----------------------------------------------------------------------*/

#include "typedef.h"
#include "basic_op.h"
#include "cnst.h"
#include "acelp.h"
#include "count.h"

#define MP1 (M+1)


void Int_isp(
     Word16 isp_old[],                     /* input : isps from past frame              */
     Word16 isp_new[],                     /* input : isps from present frame           */
     Word16 frac[],                        /* input : fraction for 3 first subfr (Q15)  */
     Word16 Az[]                           /* output: LP coefficients in 4 subframes    */
)
{
    Word16 i, k, fac_old, fac_new;
    Word16 isp[M];
    Word32 L_tmp;

    for (k = 0; k < 3; k++)
    {
        fac_new = frac[k];                 move16();
        fac_old = add(sub(32767, fac_new), 1);  /* 1.0 - fac_new */

        for (i = 0; i < M; i++)
        {
            L_tmp = L_mult(isp_old[i], fac_old);
            L_tmp = L_mac(L_tmp, isp_new[i], fac_new);
            isp[i] = round(L_tmp);         move16();
        }
        Isp_Az(isp, Az, M, 0);
        Az += MP1;
    }

    /* 4th subframe: isp_new (frac=1.0) */

    Isp_Az(isp_new, Az, M, 0);

    return;
}
