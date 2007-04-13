/*-------------------------------------------------------------------*
 *                         RANDOM.C                                  *
 *-------------------------------------------------------------------*
 * Signed 16 bits random generator.                                  *
 *-------------------------------------------------------------------*/

#include "typedef.h"
#include "acelp.h"
#include "basic_op.h"
#include "count.h"


Word16 Random(Word16 * seed)
{
    /* static Word16 seed = 21845; */

    *seed = extract_l(L_add(L_shr(L_mult(*seed, 31821), 1), 13849L));   move16();

    return (*seed);
}
