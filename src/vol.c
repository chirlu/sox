/*
 * (c) 20/03/2000 Fabien COELHO <fabien@coelho.net> for sox.
 *
 * Change volume of sound file, with basic linear amplitude formula.
 * Beware of saturations! clipping is checked and reported.
 * Cannot handle different number of channels.
 * Cannot handle rate change.
 */

#include "st_i.h"

#include <math.h>   /* exp(), sqrt() */

static st_effect_t st_vol_effect;

#define LOG_10_20 ((double)(0.1151292546497022842009e0))

typedef struct {
    double gain; /* amplitude gain. */
    
    int uselimiter; /* boolean: are we using the limiter? */
    double limiterthreshhold;
    double limitergain; /* limiter gain. */
    int limited; /* number of limited values to report. */
    int totalprocessed;
} * vol_t;

/*
 * Process options: gain (float) type (amplitude, power, dB)
 */
static int st_vol_getopts(eff_t effp, int n, char **argv) 
{
    vol_t vol = (vol_t) effp->priv; 
    vol->gain = 1.0; /* default is no change */
    vol->uselimiter = 0; /* default is no limiter */
    
    if (n && (!sscanf(argv[0], "%lf", &vol->gain)))
    {
        st_fail(st_vol_effect.usage);
        return ST_EOF;
    }

    /* adjust gain depending on type (what a great parser;-) */
    if (n>1) 
    {
        switch (argv[1][0]) 
        {
        case 'd': /* decibels to amplitude */
        case 'D':
            vol->gain = exp(vol->gain*LOG_10_20);
            break;
        case 'p':
        case 'P': /* power to amplitude, keep phase change */
            if (vol->gain > 0.0)
                vol->gain = sqrt(vol->gain);
            else
                vol->gain = -sqrt(-vol->gain);
            break;
        case 'a': /* amplitude */
        case 'A':
        default:
            break;
        }
    }
    

    if (n>2)
    {
        if ((fabs(vol->gain) < 1.0) || !sscanf(argv[2], "%lf", &vol->limitergain) || !((vol->limitergain > 0.0) && (vol->limitergain < 1.0)))
        {
                st_fail(st_vol_effect.usage);
                return ST_EOF;                  
        }
        
        vol->uselimiter = 1; /* ok, we'll use it */
        /* The following equation is derived so that there is no 
         * discontinuity in output amplitudes */
        /* and a ST_SAMPLE_MAX input always maps to a ST_SAMPLE_MAX output 
         * when the limiter is activated. */
        /* (NOTE: There **WILL** be a discontinuity in the slope 
         * of the output amplitudes when using the limiter.) */
        vol->limiterthreshhold = ST_SAMPLE_MAX * (1.0 - vol->limitergain) / (fabs(vol->gain) - vol->limitergain);
    }
    

    return ST_SUCCESS;
}

/*
 * Start processing
 */
static int st_vol_start(eff_t effp)
{
    vol_t vol = (vol_t) effp->priv;
    
    if (vol->gain == 1)
      return ST_EFF_NULL;

    if (effp->outinfo.channels != effp->ininfo.channels) {
        st_fail("vol cannot handle different channels (in %d, out %d)"
             " use avg or pan", effp->ininfo.channels, effp->outinfo.channels);
        return ST_EOF;
    }

    if (effp->outinfo.rate != effp->ininfo.rate) {
        st_fail("vol cannot handle different rates (in %ld, out %ld)"
             " use resample", effp->ininfo.rate, effp->outinfo.rate);
        return ST_EOF;
    }

    vol->limited = 0;
    vol->totalprocessed = 0;

    return ST_SUCCESS;
}

/*
 * Process data.
 */
static int st_vol_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                st_size_t *isamp, st_size_t *osamp)
{
    vol_t vol = (vol_t) effp->priv;
    register double gain = vol->gain;
    register double limiterthreshhold = vol->limiterthreshhold;
    register double sample;
    register st_size_t len;
    
    len = min(*osamp, *isamp);

    /* report back dealt with amount. */
    *isamp = len; *osamp = len;
    
    if (vol->uselimiter)
    {
        vol->totalprocessed += len;
        
        for (;len>0; len--)
            {
                sample = *ibuf++;
                
                if (sample > limiterthreshhold)
                {
                        sample =  (ST_SAMPLE_MAX - vol->limitergain * (ST_SAMPLE_MAX - sample));
                        vol->limited++;
                }
                else if (sample < -limiterthreshhold)
                {
                        sample = -(ST_SAMPLE_MAX - vol->limitergain * (ST_SAMPLE_MAX + sample));
                        /* FIXME: MIN is (-MAX)-1 so need to make sure we
                         * don't go over that.  Probably could do this
                         * check inside the above equation but I didn't
                         * think it thru.
                         */
                        if (sample < ST_SAMPLE_MIN)
                            sample = ST_SAMPLE_MIN;
                        vol->limited++;
                } else
                        sample = gain * sample;

                ST_SAMPLE_CLIP_COUNT(sample, effp->clips);
               *obuf++ = sample;
            }
    }
    else
    {
        /* quite basic, with clipping */
        for (;len>0; len--)
        {
                sample = gain * *ibuf++;
                ST_SAMPLE_CLIP_COUNT(sample, effp->clips);
                *obuf++ = sample;
        }
    }
    return ST_SUCCESS;
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
static int st_vol_stop(eff_t effp)
{
    vol_t vol = (vol_t) effp->priv;
    if (vol->limited)
    {
        st_warn("limited %d values (%d percent).", 
             vol->limited, (int) (vol->limited * 100.0 / vol->totalprocessed));
    }
    return ST_SUCCESS;
}

static st_effect_t st_vol_effect = {
  "vol",
  "Usage: vol gain [ type [ limitergain ] ]"
  "       (default type=amplitude: 1.0 is constant, <0.0 change phase;\n"
  "       type=power 1.0 is constant; type=dB: 0.0 is constant, +6 doubles ampl.)\n"
  "       The peak limiter has a gain much less than 1.0 (ie 0.05 or 0.02) which is only\n"
  "       used on peaks to prevent clipping. (default is no limiter)",
  ST_EFF_MCHAN,
  st_vol_getopts,
  st_vol_start,
  st_vol_flow,
  st_effect_nothing_drain,
  st_vol_stop,
  st_effect_nothing
};

const st_effect_t *st_vol_effect_fn(void)
{
    return &st_vol_effect;
}
