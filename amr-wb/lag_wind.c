/*---------------------------------------------------------*
 *                         LAG_WIND.C                      *
 *---------------------------------------------------------*
 * Lag_window on autocorrelations.                         *
 *                                                         *
 * r[i] *= lag_wind[i]                                     *
 *                                                         *
 *  r[i] and lag_wind[i] are in special double precision.  *
 *  See "oper_32b.c" for the format                        *
 *---------------------------------------------------------*/

#include "typedef.h"
#include "acelp.h"
#include "basic_op.h"
#include "oper_32b.h"

#include "lag_wind.tab"


void Lag_window(
     Word16 r_h[],                         /* (i/o)   : Autocorrelations  (msb)          */
     Word16 r_l[]                          /* (i/o)   : Autocorrelations  (lsb)          */
)
{
    Word16 i;
    Word32 x;

    for (i = 1; i <= M; i++)
    {
        x = Mpy_32(r_h[i], r_l[i], lag_h[i - 1], lag_l[i - 1]);
        L_Extract(x, &r_h[i], &r_l[i]);
    }
    return;
}
