/*-------------------------------------------------------------------*
 *                         UTIL.C                                    *
 *-------------------------------------------------------------------*
 * Vector routines                                                   *
 *-------------------------------------------------------------------*/

#include "typedef.h"
#include "acelp.h"
#include "basic_op.h"
#include "count.h"


/*-------------------------------------------------------------------*
 * Function  Set zero()                                              *
 *           ~~~~~~~~~~                                              *
 * Set vector x[] to zero                                            *
 *-------------------------------------------------------------------*/

void Set_zero(
     Word16 x[],                           /* (o)    : vector to clear     */
     Word16 L                              /* (i)    : length of vector    */
)
{
    Word16 i;

    for (i = 0; i < L; i++)
    {
        x[i] = 0;                          move16();
    }

    return;
}


/*-------------------------------------------------------------------*
 * Function  Copy:                                                   *
 *           ~~~~~                                                   *
 * Copy vector x[] to y[]                                            *
 *-------------------------------------------------------------------*/

void Copy(
     Word16 x[],                           /* (i)   : input vector   */
     Word16 y[],                           /* (o)   : output vector  */
     Word16 L                              /* (i)   : vector length  */
)
{
    Word16 i;

    for (i = 0; i < L; i++)
    {
        y[i] = x[i];                       move16();
    }

    return;
}
