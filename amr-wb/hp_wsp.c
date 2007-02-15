/*-----------------------------------------------------------------------*
 *                         HP_WSP.C                                      *
 *-----------------------------------------------------------------------*
 *                                                                       *
 * 3nd order high pass filter with cut off frequency at 180 Hz           *
 *                                                                       *
 * Algorithm:                                                            *
 *                                                                       *
 *  y[i] = b[0]*x[i] + b[1]*x[i-1] + b[2]*x[i-2] + b[3]*x[i-3]           *
 *                   + a[1]*y[i-1] + a[2]*y[i-2] + a[3]*y[i-3];          *
 *                                                                       *
 * float a_coef[HP_ORDER]= {                                             *
 *    -2.64436711600664f,                                                *
 *    2.35087386625360f,                                                 *
 *   -0.70001156927424f};                                                *
 *                                                                       *
 * float b_coef[HP_ORDER+1]= {                                           *
 *     -0.83787057505665f,                                               *
 *    2.50975570071058f,                                                 *
 *   -2.50975570071058f,                                                 *
 *    0.83787057505665f};                                                *
 *                                                                       *
 *-----------------------------------------------------------------------*/

#include "typedef.h"
#include "basic_op.h"
#include "oper_32b.h"
#include "acelp.h"
#include "count.h"


/* filter coefficients in Q12 */

static Word16 a[4] = {8192, 21663, -19258, 5734};
static Word16 b[4] = {-3432, +10280, -10280, +3432};


/* Initialization of static values */

void Init_Hp_wsp(Word16 mem[])
{
    Set_zero(mem, 9);

    return;
}

void scale_mem_Hp_wsp(Word16 mem[], Word16 exp)
{
    Word16 i;
    Word32 L_tmp;

    for (i = 0; i < 6; i += 2)
    {
        L_tmp = L_Comp(mem[i], mem[i + 1]);/* y_hi, y_lo */
        L_tmp = L_shl(L_tmp, exp);
        L_Extract(L_tmp, &mem[i], &mem[i + 1]);
    }

    for (i = 6; i < 9; i++)
    {
        L_tmp = L_deposit_h(mem[i]);       /* x[i] */
        L_tmp = L_shl(L_tmp, exp);
        mem[i] = round(L_tmp);             move16();
    }

    return;
}


void Hp_wsp(
     Word16 wsp[],                         /* i   : wsp[]  signal       */
     Word16 hp_wsp[],                      /* o   : hypass wsp[]        */
     Word16 lg,                            /* i   : lenght of signal    */
     Word16 mem[]                          /* i/o : filter memory [9]   */
)
{
    Word16 i;
    Word16 x0, x1, x2, x3;
    Word16 y3_hi, y3_lo, y2_hi, y2_lo, y1_hi, y1_lo;
    Word32 L_tmp;

    y3_hi = mem[0];                        move16();
    y3_lo = mem[1];                        move16();
    y2_hi = mem[2];                        move16();
    y2_lo = mem[3];                        move16();
    y1_hi = mem[4];                        move16();
    y1_lo = mem[5];                        move16();
    x0 = mem[6];                           move16();
    x1 = mem[7];                           move16();
    x2 = mem[8];                           move16();

    for (i = 0; i < lg; i++)
    {
        x3 = x2;                           move16();
        x2 = x1;                           move16();
        x1 = x0;                           move16();
        x0 = wsp[i];                       move16();

        /* y[i] = b[0]*x[i] + b[1]*x[i-1] + b140[2]*x[i-2] + b[3]*x[i-3]  */
        /* + a[1]*y[i-1] + a[2] * y[i-2]  + a[3]*y[i-3]  */

        move32();
        L_tmp = 16384L;                    /* rounding to maximise precision */
        L_tmp = L_mac(L_tmp, y1_lo, a[1]);
        L_tmp = L_mac(L_tmp, y2_lo, a[2]);
        L_tmp = L_mac(L_tmp, y3_lo, a[3]);
        L_tmp = L_shr(L_tmp, 15);
        L_tmp = L_mac(L_tmp, y1_hi, a[1]);
        L_tmp = L_mac(L_tmp, y2_hi, a[2]);
        L_tmp = L_mac(L_tmp, y3_hi, a[3]);
        L_tmp = L_mac(L_tmp, x0, b[0]);
        L_tmp = L_mac(L_tmp, x1, b[1]);
        L_tmp = L_mac(L_tmp, x2, b[2]);
        L_tmp = L_mac(L_tmp, x3, b[3]);

        L_tmp = L_shl(L_tmp, 2);           /* coeff Q12 --> Q15 */

        y3_hi = y2_hi;                     move16();
        y3_lo = y2_lo;                     move16();
        y2_hi = y1_hi;                     move16();
        y2_lo = y1_lo;                     move16();
        L_Extract(L_tmp, &y1_hi, &y1_lo);

        L_tmp = L_shl(L_tmp, 1);           /* coeff Q14 --> Q15 */
        hp_wsp[i] = round(L_tmp);          move16();
    }

    mem[0] = y3_hi;                        move16();
    mem[1] = y3_lo;                        move16();
    mem[2] = y2_hi;                        move16();
    mem[3] = y2_lo;                        move16();
    mem[4] = y1_hi;                        move16();
    mem[5] = y1_lo;                        move16();
    mem[6] = x0;                           move16();
    mem[7] = x1;                           move16();
    mem[8] = x2;                           move16();

    return;
}
