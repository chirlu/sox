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

#include "st_i.h"

#include <math.h>   /* exp(), sqrt() */

static st_effect_t st_dcshift_effect;

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
static int st_dcshift_getopts(eff_t effp, int n, char **argv)
{
    dcs_t dcs = (dcs_t) effp->priv;
    dcs->dcshift = 1.0; /* default is no change */
    dcs->uselimiter = 0; /* default is no limiter */

    if (n < 1)
    {
        st_fail(st_dcshift_effect.usage);
        return ST_EOF;
    }

    if (n && (!sscanf(argv[0], "%lf", &dcs->dcshift)))
    {
        st_fail(st_dcshift_effect.usage);
        return ST_EOF;
    }

    if (n>1)
    {
        if (!sscanf(argv[1], "%lf", &dcs->limitergain))
        {
                st_fail(st_dcshift_effect.usage);
                return ST_EOF;
        }

        dcs->uselimiter = 1; /* ok, we'll use it */
        /* The following equation is derived so that there is no 
         * discontinuity in output amplitudes */
        /* and a ST_SAMPLE_MAX input always maps to a ST_SAMPLE_MAX output 
         * when the limiter is activated. */
        /* (NOTE: There **WILL** be a discontinuity in the slope of the 
         * output amplitudes when using the limiter.) */
        dcs->limiterthreshhold = ST_SAMPLE_MAX * (1.0 - (fabs(dcs->dcshift) - dcs->limitergain));
    }

    return ST_SUCCESS;
}

/*
 * Start processing
 */
static int st_dcshift_start(eff_t effp)
{
    dcs_t dcs = (dcs_t) effp->priv;

    if (dcs->dcshift == 0)
      return ST_EFF_NULL;

    if (effp->outinfo.channels != effp->ininfo.channels) {
        st_fail("DCSHIFT cannot handle different channels (in=%d, out=%d)"
             " use avg or pan", effp->ininfo.channels, effp->outinfo.channels);
        return ST_EOF;
    }

    if (effp->outinfo.rate != effp->ininfo.rate) {
        st_fail("DCSHIFT cannot handle different rates (in=%ld, out=%ld)"
             " use resample or rate", effp->ininfo.rate, effp->outinfo.rate);
        return ST_EOF;
    }

    dcs->clipped = 0;
    dcs->limited = 0;
    dcs->totalprocessed = 0;

    return ST_SUCCESS;
}

/*
 * Process data.
 */
static int st_dcshift_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                    st_size_t *isamp, st_size_t *osamp)
{
    dcs_t dcs = (dcs_t) effp->priv;
    double dcshift = dcs->dcshift;
    double limitergain = dcs->limitergain;
    double limiterthreshhold = dcs->limiterthreshhold;
    double sample;
    st_size_t len;

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
                        sample =  (sample - limiterthreshhold) * limitergain / (ST_SAMPLE_MAX - limiterthreshhold) + limiterthreshhold + dcshift;
                        dcs->limited++;
                }
                else if (sample < -limiterthreshhold && dcshift < 0)
                {
                        /* Note this should really be ST_SAMPLE_MIN but
                         * the clip() below will take care of the overflow.
                         */
                        sample =  (sample + limiterthreshhold) * limitergain / (ST_SAMPLE_MAX - limiterthreshhold) - limiterthreshhold + dcshift;
                        dcs->limited++;
                }
                else
                {
                        /* Note this should consider ST_SAMPLE_MIN but
                         * the clip() below will take care of the overflow.
                         */
                        sample = dcshift * ST_SAMPLE_MAX + sample;
                }

                ST_SAMPLE_CLIP_COUNT(sample, dcs->clipped);
                *obuf++ = sample;
            }
    }
    else
    {
        /* quite basic, with clipping */
        for (;len>0; len--)
        {
                double f;

                f = dcshift * ST_SAMPLE_MAX + *ibuf++;
                ST_SAMPLE_CLIP_COUNT(f, dcs->clipped);
                *obuf++ = f;
        }
    }
    return ST_SUCCESS;
}

/*
 * Do anything required when you stop reading samples.
 * Don't close input file!
 */
static int st_dcshift_stop(eff_t effp)
{
    dcs_t dcs = (dcs_t) effp->priv;

    if (dcs->limited)
    {
        st_warn("DCSHIFT limited %d values (%d percent).",
             dcs->limited, (int) (dcs->limited * 100.0 / dcs->totalprocessed));
    }
    if (dcs->clipped)
    {
        if (dcs->dcshift > 0)
        {
             st_warn("DCSHIFT clipped %d values, dcshift=%f too high...",
                  dcs->clipped, dcs->dcshift);
        }
        else
        {
             st_warn("DCSHIFT clipped %d values, dcshift=%f too low...",
                  dcs->clipped, dcs->dcshift);
        }
    }
    return ST_SUCCESS;
}

static st_effect_t st_dcshift_effect = {
   "dcshift",
   "Usage: dcshift shift [ limitergain ]\n"
   "       The peak limiter has a gain much less than 1.0 (ie 0.05 or 0.02) which is only\n"
   "       used on peaks to prevent clipping. (default is no limiter)",
   ST_EFF_MCHAN,
   st_dcshift_getopts,
   st_dcshift_start,
   st_dcshift_flow,
   st_effect_nothing_drain,
   st_dcshift_stop,
  st_effect_nothing
};

const st_effect_t *st_dcshift_effect_fn(void)
{
    return &st_dcshift_effect;
}
