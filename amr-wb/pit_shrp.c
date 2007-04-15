/*-----------------------------------------------------------------------*
 *                         PIT_SHRP.C                                    *
 *-----------------------------------------------------------------------*
 * Performs Pitch sharpening routine                                     *
 *-----------------------------------------------------------------------*/

#include "typedef.h"
#include "acelp.h"
#include "basic_op.h"
#include "count.h"

void Pit_shrp(
     Word16 * x,                           /* in/out: impulse response (or algebraic code) */
     Word16 pit_lag,                       /* input : pitch lag                            */
     Word16 sharp,                         /* input : pitch sharpening factor (Q15)        */
     Word16 L_subfr                        /* input : subframe size                        */
)
{
    Word16 i;
    Word32 L_tmp;

    for (i = pit_lag; i < L_subfr; i++)
    {
        L_tmp = L_deposit_h(x[i]);
        L_tmp = L_mac(L_tmp, x[i - pit_lag], sharp);
        x[i] = roundL(L_tmp);
        move16();
    }

    return;
}
