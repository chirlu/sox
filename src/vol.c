/*
 * (c) 20/03/2000 Fabien COELHO <fabien@coelho.net> for sox.
 *
 * Change volume of sound file, with basic linear amplitude formula.
 * Pretty redundant with -v general option.
 * Beware of saturations! clipping is checked and reported.
 * Cannot handle different number of channels.
 * Cannot handle rate change.
 */

#include "st.h"

#include <math.h>   /* exp(), sqrt() */
#include <limits.h> /* LONG_MAX */

/* type used for computations. 
 */
#ifndef VOL_FLOAT
#define VOL_FLOAT float
#define VOL_FLOAT_SCAN "%f"
#endif

/* constants
 */
#define ZERO	  ((VOL_FLOAT)(0.0e0))
#define LOG_10_20 ((VOL_FLOAT)(0.1151292546497022842009e0))
#define ONE	  ((VOL_FLOAT)(1.0e0))
#define TWENTY	  ((VOL_FLOAT)(20.0e0))

#define VOL_USAGE \
    "Usage: vol gain type" \
    " (default type=amplitude: 1.0 is constant, <0.0 change phase;" \
    " type=power 1.0 is constant; type=dB: 0.0 is constant, +6 doubles ampl.)"

typedef struct {
    VOL_FLOAT gain; /* amplitude gain. */
    int clipped;    /* number of clipped values to report. */
} * vol_t;

/*
 * Process options: gain (float) type (amplitude, power, dB)
 */
int st_vol_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
    vol_t vol = (vol_t) effp->priv; 
    
    vol->gain = ONE; /* default is no change */
    
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

    return ST_SUCCESS;
}

/*
 * Start processing
 */
int st_vol_start(effp)
eff_t effp;
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

    return ST_SUCCESS;
}

/* conversion. clipping could be smoother at high ends?
 * this could be a function on its own, with clip count and report
 * handled by eff_t and caller.
 */
static LONG clip(vol_t vol, const VOL_FLOAT v)
{
    if (v > LONG_MAX)
    {
	 vol->clipped++;
	 return LONG_MAX;
    }
    else if (v < -LONG_MAX)
    {
	vol->clipped++;
	return -LONG_MAX;
    }
    /* else */
    return (LONG) v;
}

#ifndef MIN
#define MIN(s1,s2) ((s1)<(s2)?(s1):(s2))
#endif

/*
 * Process data.
 */
int st_vol_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
LONG *isamp, *osamp;
{
    vol_t vol = (vol_t) effp->priv;
    register VOL_FLOAT gain = vol->gain;
    register LONG len;
    
    len = MIN(*osamp, *isamp);

    /* report back dealt with amount. */
    *isamp = len; *osamp = len;
    
    /* quite basic, with clipping */
    for (;len>0; len--)
	*obuf++ = clip(vol, gain * *ibuf++);

    return ST_SUCCESS;
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_vol_stop(effp)
eff_t effp;
{
    vol_t vol = (vol_t) effp->priv;
    if (vol->clipped) 
    {
	st_warn("VOL clipped %d values, amplitude gain=%f too high...", 
	     vol->clipped, vol->gain);
    }
    return ST_SUCCESS;
}
