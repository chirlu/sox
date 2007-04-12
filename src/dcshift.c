/*
 * dcshift.c
 * (c) 2000.04.15 Chris Ausbrooks <weed@bucket.pp.ualr.edu>
 *
 * based on vol.c which is
 * (c) 20/03/2000 Fabien COELHO <fabien@coelho.net> for sox.
 *
 * DC shift a sound file, with basic linear amplitude formula.
 * Beware of saturations! clipping is checked and reported.
 * Cannot handle different number of channels.
 * Cannot handle rate change.
 */

#include "sox_i.h"

#include <math.h>   /* exp(), sqrt() */

static sox_effect_t sox_dcshift_effect;

typedef struct {
    double dcshift; /* DC shift. */
    int uselimiter; /* boolean: are we using the limiter? */
    double limiterthreshhold;
    double limitergain; /* limiter gain. */
    int limited; /* number of limited values to report. */
    int totalprocessed;
    int clipped;    /* number of clipped values to report. */
} * dcs_t;

/*
 * Process options: dcshift (double) type (amplitude, power, dB)
 */
static int sox_dcshift_getopts(eff_t effp, int n, char **argv)
{
    dcs_t dcs = (dcs_t) effp->priv;
    dcs->dcshift = 1.0; /* default is no change */
    dcs->uselimiter = 0; /* default is no limiter */

    if (n < 1)
    {
        sox_fail(sox_dcshift_effect.usage);
        return SOX_EOF;
    }

    if (n && (!sscanf(argv[0], "%lf", &dcs->dcshift)))
    {
        sox_fail(sox_dcshift_effect.usage);
        return SOX_EOF;
    }

    if (n>1)
    {
        if (!sscanf(argv[1], "%lf", &dcs->limitergain))
        {
                sox_fail(sox_dcshift_effect.usage);
                return SOX_EOF;
        }

        dcs->uselimiter = 1; /* ok, we'll use it */
        /* The following equation is derived so that there is no 
         * discontinuity in output amplitudes */
        /* and a SOX_SAMPLE_MAX input always maps to a SOX_SAMPLE_MAX output 
         * when the limiter is activated. */
        /* (NOTE: There **WILL** be a discontinuity in the slope of the 
         * output amplitudes when using the limiter.) */
        dcs->limiterthreshhold = SOX_SAMPLE_MAX * (1.0 - (fabs(dcs->dcshift) - dcs->limitergain));
    }

    return SOX_SUCCESS;
}

/*
 * Start processing
 */
static int sox_dcshift_start(eff_t effp)
{
    dcs_t dcs = (dcs_t) effp->priv;

    if (dcs->dcshift == 0)
      return SOX_EFF_NULL;

    if (effp->outinfo.channels != effp->ininfo.channels) {
        sox_fail("DCSHIFT cannot handle different channels (in=%d, out=%d)"
             " use avg or pan", effp->ininfo.channels, effp->outinfo.channels);
        return SOX_EOF;
    }

    if (effp->outinfo.rate != effp->ininfo.rate) {
        sox_fail("DCSHIFT cannot handle different rates (in=%ld, out=%ld)"
             " use resample or rate", effp->ininfo.rate, effp->outinfo.rate);
        return SOX_EOF;
    }

    dcs->clipped = 0;
    dcs->limited = 0;
    dcs->totalprocessed = 0;

    return SOX_SUCCESS;
}

/*
 * Process data.
 */
static int sox_dcshift_flow(eff_t effp, const sox_ssample_t *ibuf, sox_ssample_t *obuf, 
                    sox_size_t *isamp, sox_size_t *osamp)
{
    dcs_t dcs = (dcs_t) effp->priv;
    double dcshift = dcs->dcshift;
    double limitergain = dcs->limitergain;
    double limiterthreshhold = dcs->limiterthreshhold;
    double sample;
    sox_size_t len;

    len = min(*osamp, *isamp);

    /* report back dealt with amount. */
    *isamp = len; *osamp = len;

    if (dcs->uselimiter)
    {
        dcs->totalprocessed += len;

        for (;len>0; len--)
            {
                sample = *ibuf++;

                if (sample > limiterthreshhold && dcshift > 0)
                {
                        sample =  (sample - limiterthreshhold) * limitergain / (SOX_SAMPLE_MAX - limiterthreshhold) + limiterthreshhold + dcshift;
                        dcs->limited++;
                }
                else if (sample < -limiterthreshhold && dcshift < 0)
                {
                        /* Note this should really be SOX_SAMPLE_MIN but
                         * the clip() below will take care of the overflow.
                         */
                        sample =  (sample + limiterthreshhold) * limitergain / (SOX_SAMPLE_MAX - limiterthreshhold) - limiterthreshhold + dcshift;
                        dcs->limited++;
                }
                else
                {
                        /* Note this should consider SOX_SAMPLE_MIN but
                         * the clip() below will take care of the overflow.
                         */
                        sample = dcshift * SOX_SAMPLE_MAX + sample;
                }

                SOX_SAMPLE_CLIP_COUNT(sample, dcs->clipped);
                *obuf++ = sample;
            }
    }
    else
    {
        /* quite basic, with clipping */
        for (;len>0; len--)
        {
                double f;

                f = dcshift * SOX_SAMPLE_MAX + *ibuf++;
                SOX_SAMPLE_CLIP_COUNT(f, dcs->clipped);
                *obuf++ = f;
        }
    }
    return SOX_SUCCESS;
}

/*
 * Do anything required when you stop reading samples.
 * Don't close input file!
 */
static int sox_dcshift_stop(eff_t effp)
{
    dcs_t dcs = (dcs_t) effp->priv;

    if (dcs->limited)
    {
        sox_warn("DCSHIFT limited %d values (%d percent).",
             dcs->limited, (int) (dcs->limited * 100.0 / dcs->totalprocessed));
    }
    if (dcs->clipped)
    {
        if (dcs->dcshift > 0)
        {
             sox_warn("DCSHIFT clipped %d values, dcshift=%f too high...",
                  dcs->clipped, dcs->dcshift);
        }
        else
        {
             sox_warn("DCSHIFT clipped %d values, dcshift=%f too low...",
                  dcs->clipped, dcs->dcshift);
        }
    }
    return SOX_SUCCESS;
}

static sox_effect_t sox_dcshift_effect = {
   "dcshift",
   "Usage: dcshift shift [ limitergain ]\n"
   "       The peak limiter has a gain much less than 1.0 (ie 0.05 or 0.02) which is only\n"
   "       used on peaks to prevent clipping. (default is no limiter)",
   SOX_EFF_MCHAN,
   sox_dcshift_getopts,
   sox_dcshift_start,
   sox_dcshift_flow,
   sox_effect_nothing_drain,
   sox_dcshift_stop,
  sox_effect_nothing
};

const sox_effect_t *sox_dcshift_effect_fn(void)
{
    return &sox_dcshift_effect;
}
