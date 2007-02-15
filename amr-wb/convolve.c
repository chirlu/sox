/*------------------------------------------------------------------------*
 *                         CONVOLVE.C                                     *
 *------------------------------------------------------------------------*
 * Perform the convolution between two vectors x[] and h[] and            *
 * write the result in the vector y[].                                    *
 * All vectors are of length L.                                           *
 *------------------------------------------------------------------------*/

#include "typedef.h"
#include "basic_op.h"
#include "count.h"

void Convolve(
     Word16 x[],                           /* (i)        : input vector                           */
     Word16 h[],                           /* (i) Q15    : impulse response                       */
     Word16 y[],                           /* (o) 12 bits: output vector                          */
     Word16 L                              /* (i)        : vector size                            */
)
{
    Word16 i, n;
    Word32 L_sum;

    for (n = 0; n < L; n++)
    {
        L_sum = 0L;                        move32();
        for (i = 0; i <= n; i++)
            L_sum = L_mac(L_sum, x[i], h[n - i]);

        y[n] = round(L_sum);               move16();
    }

    return;
}
