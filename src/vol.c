/*
 * (c) 20/03/2000 Fabien COELHO <fabien@coelho.net> for sox.
 *
 * Change volume of sound file, with basic linear amplitude formula.
 * Pretty redundant with -v general option.
 * Beware of saturations! clipping is checked and reported.
 * Cannot handle different number of channels.
 * Cannot handle rate change.
 */

#include "st_i.h"

#include <math.h>   /* exp(), sqrt() */

/* type used for computations. 
 */
#ifndef VOL_FLOAT
#define VOL_FLOAT float
#define VOL_FLOAT_SCAN "%f"
#endif

/* constants
 */
#define ZERO      ((VOL_FLOAT)(0.0e0))
#define LOG_10_20 ((VOL_FLOAT)(0.1151292546497022842009e0))
#define ONE       ((VOL_FLOAT)(1.0e0))
#define TWENTY    ((VOL_FLOAT)(20.0e0))

#define VOL_USAGE \
    "Usage: vol gain [ type [ limitergain ] ]" \
    " (default type=amplitude: 1.0 is constant, <0.0 change phase;" \
    " type=power 1.0 is constant; type=dB: 0.0 is constant, +6 doubles ampl.)" \
    " The peak limiter has a gain much less than 1.0 (ie 0.05 or 0.02) which is only" \
    " used on peaks to prevent clipping. (default is no limiter)"

typedef struct {
    VOL_FLOAT gain; /* amplitude gain. */
    
    int uselimiter; /* boolean: are we using the limiter? */
    VOL_FLOAT limiterthreshhold;
    VOL_FLOAT limitergain; /* limiter gain. */
    int limited; /* number of limited values to report. */
    int totalprocessed;
    
    int clipped;    /* number of clipped values to report. */
} * vol_t;

/*
 * Process options: gain (float) type (amplitude, power, dB)
 */
int st_vol_getopts(eff_t effp, int n, char **argv) 
{
    vol_t vol = (vol_t) effp->priv; 
    vol->gain = ONE; /* default is no change */
    vol->uselimiter = 0; /* default is no limiter */
    
    if (n && (!sscanf(argv[0], VOL_FLOAT_SCAN, &vol->gain)))
    {
        st_fail(VOL_USAGE);
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
            if (vol->gain > ZERO)
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
        if ((fabs(vol->gain) < ONE) || !sscanf(argv[2], VOL_FLOAT_SCAN, &vol->limitergain) || !((vol->limitergain > ZERO) && (vol->limitergain < ONE)))
        {
                st_fail(VOL_USAGE);
                return ST_EOF;                  
        }
        
        vol->uselimiter = 1; /* ok, we'll use it */
        /* The following equation is derived so that there is no 
         * discontinuity in output amplitudes */
        /* and a ST_SAMPLE_MAX input always maps to a ST_SAMPLE_MAX output 
         * when the limiter is activated. */
        /* (NOTE: There **WILL** be a discontinuity in the slope 
         * of the output amplitudes when using the limiter.) */
        vol->limiterthreshhold = ST_SAMPLE_MAX * (ONE - vol->limitergain) / (fabs(vol->gain) - vol->limitergain);
    }
    

    return ST_SUCCESS;
}

/*
 * Start processing
 */
int st_vol_start(eff_t effp)
{
    vol_t vol = (vol_t) effp->priv;
    
    if (effp->outinfo.channels != effp->ininfo.channels)
    {
        st_warn("VOL cannot handle different channels (in=%d, out=%d)"
             " use avg or pan", effp->ininfo.channels, effp->outinfo.channels);
    }

    if (effp->outinfo.rate != effp->ininfo.rate)
    {
        st_fail("VOL cannot handle different rates (in=%ld, out=%ld)"
             " use resample or rate", effp->ininfo.rate, effp->outinfo.rate);
        return ST_EOF;
    }

    vol->clipped = 0;
    vol->limited = 0;
    vol->totalprocessed = 0;

    return ST_SUCCESS;
}

/* conversion. clipping could be smoother at high ends?
 * this could be a function on its own, with clip count and report
 * handled by eff_t and caller.
 */
static st_sample_t clip(vol_t vol, const VOL_FLOAT v)
{
    if (v > ST_SAMPLE_MAX)
    {
         vol->clipped++;
         return ST_SAMPLE_MAX;
    }
    else if (v < ST_SAMPLE_MIN)
    {
        vol->clipped++;
        return ST_SAMPLE_MIN;
    }
    /* else */
    return (st_sample_t) v;
}

#ifndef MIN
#define MIN(s1,s2) ((s1)<(s2)?(s1):(s2))
#endif

/*
 * Process data.
 */
int st_vol_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                st_size_t *isamp, st_size_t *osamp)
{
    vol_t vol = (vol_t) effp->priv;
    register VOL_FLOAT gain = vol->gain;
    register VOL_FLOAT limiterthreshhold = vol->limiterthreshhold;
    register VOL_FLOAT sample;
    register st_size_t len;
    
    len = MIN(*osamp, *isamp);

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
                {
                        sample = gain * sample;
                }

                *obuf++ = clip(vol, sample);
            }
    }
    else
    {
        /* quite basic, with clipping */
        for (;len>0; len--)
                *obuf++ = clip(vol, gain * *ibuf++);
    }
    return ST_SUCCESS;
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_vol_stop(eff_t effp)
{
    vol_t vol = (vol_t) effp->priv;
    if (vol->limited)
    {
        st_warn("VOL limited %d values (%d percent).", 
             vol->limited, (int) (vol->limited * 100.0 / vol->totalprocessed));
    }
    if (vol->clipped) 
    {
        st_warn("VOL clipped %d values, amplitude gain=%f too high...", 
             vol->clipped, vol->gain);
    }
    return ST_SUCCESS;
}
