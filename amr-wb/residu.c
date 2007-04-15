/*-----------------------------------------------------------------------*
 *                         RESIDU.C                                      *
 *-----------------------------------------------------------------------*
 * Compute the LPC residual by filtering the input speech through A(z)   *
 *-----------------------------------------------------------------------*/

#include "typedef.h"
#include "acelp.h"
#include "basic_op.h"
#include "count.h"


void Residu(
     Word16 a[],                           /* (i) Q12 : prediction coefficients                     */
     Word16 m,                             /* (i)     : order of LP filter                          */
     Word16 x[],                           /* (i)     : speech (values x[-m..-1] are needed         */
     Word16 y[],                           /* (o) x2  : residual signal                             */
     Word16 lg                             /* (i)     : size of filtering                           */
)
{
    Word16 i, j;
    Word32 s;

    for (i = 0; i < lg; i++)
    {
        s = L_mult(x[i], a[0]);

        for (j = 1; j <= m; j++)
            s = L_mac(s, a[j], x[i - j]);

        s = L_shl(s, 3 + 1);               /* saturation can occur here */
        y[i] = roundL(s);                   move16();
    }

    return;
}
