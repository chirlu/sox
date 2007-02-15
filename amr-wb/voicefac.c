/*-------------------------------------------------------------------*
 *                         VOICEFAC.C                                *
 *-------------------------------------------------------------------*
 * Find the voicing factor (1=voice to -1=unvoiced).                 *
 *-------------------------------------------------------------------*/

#include "typedef.h"
#include "basic_op.h"
#include "math_op.h"
#include "count.h"

Word16 voice_factor(                       /* (o) Q15   : factor (-1=unvoiced to 1=voiced) */
     Word16 exc[],                         /* (i) Q_exc : pitch excitation                 */
     Word16 Q_exc,                         /* (i)       : exc format                       */
     Word16 gain_pit,                      /* (i) Q14   : gain of pitch                    */
     Word16 code[],                        /* (i) Q9    : Fixed codebook excitation        */
     Word16 gain_code,                     /* (i) Q0    : gain of code                     */
     Word16 L_subfr                        /* (i)       : subframe length                  */
)
{
    Word16 i, tmp, exp, ener1, exp1, ener2, exp2;
    Word32 L_tmp;

    ener1 = extract_h(Dot_product12(exc, exc, L_subfr, &exp1));
    exp1 = sub(exp1, add(Q_exc, Q_exc));
    L_tmp = L_mult(gain_pit, gain_pit);
    exp = norm_l(L_tmp);
    tmp = extract_h(L_shl(L_tmp, exp));
    ener1 = mult(ener1, tmp);
    exp1 = sub(sub(exp1, exp), 10);        /* 10 -> gain_pit Q14 to Q9 */

    ener2 = extract_h(Dot_product12(code, code, L_subfr, &exp2));

    exp = norm_s(gain_code);
    tmp = shl(gain_code, exp);
    tmp = mult(tmp, tmp);
    ener2 = mult(ener2, tmp);
    exp2 = sub(exp2, add(exp, exp));

    i = sub(exp1, exp2);

    test();
    if (i >= 0)
    {
        ener1 = shr(ener1, 1);
        ener2 = shr(ener2, add(i, 1));
    } else
    {
        ener1 = shr(ener1, sub(1, i));
        ener2 = shr(ener2, 1);
    }

    tmp = sub(ener1, ener2);
    ener1 = add(add(ener1, ener2), 1);

    test();
    if (tmp >= 0)
    {
        tmp = div_s(tmp, ener1);
    } else
    {
        tmp = negate(div_s(negate(tmp), ener1));
    }

    return (tmp);
}
