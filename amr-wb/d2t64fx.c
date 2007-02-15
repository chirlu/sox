/*-------------------------------------------------------------------*
 *                         D2T64FX.C                                 *
 *-------------------------------------------------------------------*
 * 12 bits algebraic codebook decoder.                               *
 * 2 tracks x 32 positions per track = 64 samples.                   *
 *                                                                   *
 * 12 bits --> 2 pulses in a frame of 64 samples.                    *
 *                                                                   *
 * All pulses can have two (2) possible amplitudes: +1 or -1.        *
 * Each pulse can have 32 possible positions.                        *
 *                                                                   *
 * See dec2t64.c for more details of the algebraic code.             *
 *-------------------------------------------------------------------*/

#include "typedef.h"
#include "basic_op.h"
#include "count.h"
#include "cnst.h"

#define L_CODE    64                       /* codevector length  */
#define NB_TRACK  2                        /* number of track    */
#define NB_POS    32                       /* number of position */


void DEC_ACELP_2t64_fx(
     Word16 index,                         /* (i) :    12 bits index                                  */
     Word16 code[]                         /* (o) :Q9  algebraic (fixed) codebook excitation          */
)
{
    Word16 i, i0, i1;

    for (i = 0; i < L_CODE; i++)
    {
        code[i] = 0;                       move16();
    }

    /* decode the positions and signs of pulses and build the codeword */

    i0 = (Word16) (shr(index, 5) & 0x003E);logic16();
    i1 = (Word16) (add(shl((Word16) (index & 0x001F), 1), 1));  logic16();
    test();logic16();
    if ((shr(index, 6) & NB_POS) == 0)
        code[i0] = 512;
    else
        code[i0] = -512;

    test();logic16();
    if ((index & NB_POS) == 0)
        code[i1] = 512;
    else
        code[i1] = -512;
    return;
}
