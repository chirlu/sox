/*
 * Sound Tools High-Pass effect file.
 *
 * (C) 2000 Chris Bagwell <cbagwell@sprynet.com>
 * See License file for further copyright information.
 *
 * Algorithm:  Recursive single pole high-pass filter
 *
 * Reference: The Scientist and Engineer's Guide to Digital Processing
 *
 *      output[N] = A0 * input[N] + A1 * input[N-1] + B1 * output[N-1]
 *
 *      X  = exp(-2.0 * pi * Fc)
 *      A0 = (1 + X) / 2
 *      A1 = -(1 + X) / 2
 *      B1 = X
 *      Fc = cutoff freq / sample rate
 *
 * Mimics an RC high-pass filter:
 *
 *        || C
 *    ----||--------->
 *        ||    |
 *              <
 *              > R
 *              <
 *              |
 *              V
 *
 */

#include <math.h>
#include "st_i.h"

/* Private data for Highpass effect */
typedef struct highpstuff {
        float   cutoff;
        double  A0, A1, B1;
        double  inm1, outm1;
} *highp_t;

/*
 * Process options
 */
int st_highp_getopts(eff_t effp, int n, char **argv) 
{
        highp_t highp = (highp_t) effp->priv;

        if ((n < 1) || !sscanf(argv[0], "%f", &highp->cutoff))
        {
                st_fail("Usage: highp cutoff");
                return (ST_EOF);
        }
        return (ST_SUCCESS);
}

/*
 * Prepare processing.
 */
int st_highp_start(eff_t effp)
{
        highp_t highp = (highp_t) effp->priv;
        if (highp->cutoff > effp->ininfo.rate/2)
        {
                st_fail("Highpass: cutoff must be < sample rate / 2 (Nyquest rate)\n");
                return (ST_EOF);
        }

        highp->B1 = exp((-2.0 * M_PI * (highp->cutoff / effp->ininfo.rate)));
        highp->A0 = (1 + highp->B1) / 2;
        highp->A1 = (-1 * (1 + highp->B1)) / 2;
        highp->inm1 = 0.0;
        highp->outm1 = 0.0;
        return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
int st_highp_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                  st_size_t *isamp, st_size_t *osamp)
{
        highp_t highp = (highp_t) effp->priv;
        int len, done;
        double d;
        st_sample_t l;

        len = ((*isamp > *osamp) ? *osamp : *isamp);

        for(done = 0; done < len; done++) {
                l = *ibuf++;
                d = highp->A0 * l + 
                    highp->A1 * highp->inm1 + 
                    highp->B1 * highp->outm1;
                if (d < ST_SAMPLE_MIN)
                    d = ST_SAMPLE_MIN;
                else if (d > ST_SAMPLE_MAX)
                    d = ST_SAMPLE_MAX;
                highp->inm1 = l;
                highp->outm1 = d;
                *obuf++ = d;
        }
        *isamp = len;
        *osamp = len;
        return (ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_highp_stop(eff_t effp)
{
        /* nothing to do */
    return (ST_SUCCESS);
}

