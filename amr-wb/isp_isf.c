/*-------------------------------------------------------------------*
 *                         ISP_ISF.C                                 *
 *-------------------------------------------------------------------*
 *   Isp_isf   Transformation isp to isf                             *
 *   Isf_isp   Transformation isf to isp                             *
 *                                                                   *
 * The transformation from isp[i] to isf[i] and isf[i] to isp[i] are *
 * approximated by a look-up table and interpolation.                *
 *-------------------------------------------------------------------*/

#include "typedef.h"
#include "basic_op.h"
#include "count.h"

#include "isp_isf.tab"                     /* Look-up table for transformations */

void Isp_isf(
     Word16 isp[],                         /* (i) Q15 : isp[m] (range: -1<=val<1)                */
     Word16 isf[],                         /* (o) Q15 : isf[m] normalized (range: 0.0<=val<=0.5) */
     Word16 m                              /* (i)     : LPC order                                */
)
{
    Word16 i, ind;
    Word32 L_tmp;

    ind = 127;                             move16();  /* beging at end of table -1 */

    for (i = (Word16) (m - 1); i >= 0; i--)
    {
        test();
        if (sub(i, sub(m, 2)) >= 0)
        {                                  /* m-2 is a constant */
            ind = 127;                     move16();  /* beging at end of table -1 */
        }
        /* find value in table that is just greater than isp[i] */
        test();
        while (sub(table[ind], isp[i]) < 0)
            ind--;

        /* acos(isp[i])= ind*128 + ( ( isp[i]-table[ind] ) * slope[ind] )/2048 */

        L_tmp = L_mult(sub(isp[i], table[ind]), slope[ind]);
        isf[i] = round(L_shl(L_tmp, 4));   /* (isp[i]-table[ind])*slope[ind])>>11 */
        move16();
        isf[i] = add(isf[i], shl(ind, 7)); move16();
    }

    isf[m - 1] = shr(isf[m - 1], 1);       move16();

    return;
}


void Isf_isp(
     Word16 isf[],                         /* (i) Q15 : isf[m] normalized (range: 0.0<=val<=0.5) */
     Word16 isp[],                         /* (o) Q15 : isp[m] (range: -1<=val<1)                */
     Word16 m                              /* (i)     : LPC order                                */
)
{
    Word16 i, ind, offset;
    Word32 L_tmp;

    for (i = 0; i < m - 1; i++)
    {
        isp[i] = isf[i];                   move16();
    }
    isp[m - 1] = shl(isf[m - 1], 1);

    for (i = 0; i < m; i++)
    {
        ind = shr(isp[i], 7);              /* ind    = b7-b15 of isf[i] */
        offset = (Word16) (isp[i] & 0x007f);    logic16();  /* offset = b0-b6  of isf[i] */

        /* isp[i] = table[ind]+ ((table[ind+1]-table[ind])*offset) / 128 */

        L_tmp = L_mult(sub(table[ind + 1], table[ind]), offset);
        isp[i] = add(table[ind], extract_l(L_shr(L_tmp, 8)));   move16();
    }

    return;
}
