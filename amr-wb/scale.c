/*-------------------------------------------------------------------*
 *                         SCALE.C                                   *
 *-------------------------------------------------------------------*
 * Scale signal to get maximum of dynamic.                           *
 *-------------------------------------------------------------------*/

#include "typedef.h"
#include "acelp.h"
#include "basic_op.h"
#include "count.h"


void Scale_sig(
     Word16 x[],                           /* (i/o) : signal to scale               */
     Word16 lg,                            /* (i)   : size of x[]                   */
     Word16 exp                            /* (i)   : exponent: x = roundL(x << exp) */
)
{
    Word16 i;
    Word32 L_tmp;

    for (i = 0; i < lg; i++)
    {
        L_tmp = L_deposit_h(x[i]);
        L_tmp = L_shl(L_tmp, exp);         /* saturation can occur here */
        x[i] = roundL(L_tmp);               move16();
    }

    return;
}
