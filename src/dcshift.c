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

#include "st.h"

#include <math.h>   /* exp(), sqrt() */
#include <limits.h> /* LONG_MAX */

/* type used for computations. 
 */
#ifndef DCSHIFT_FLOAT
#define DCSHIFT_FLOAT float
#define DCSHIFT_FLOAT_SCAN "%f"
#endif

/* constants
 */
#define ZERO	  ((DCSHIFT_FLOAT)(0.0e0))
#define LOG_10_20 ((DCSHIFT_FLOAT)(0.1151292546497022842009e0))
#define ONE	  ((DCSHIFT_FLOAT)(1.0e0))
#define TWENTY	  ((DCSHIFT_FLOAT)(20.0e0))

#define DCSHIFT_USAGE "Usage: dcshift shift [ limitergain ]"
// The following is dieing on a solaris gcc 2.8.1 compiler!?!!
#if 0
#define DCSHIFT_USAGE2 " The peak limiter has a gain much less than 1.0 (ie 0.05 or 0.02) which is only"
#define DCSHIFT_USAGE3 " used on peaks to prevent clipping. (default is no limiter)"
#endif

typedef struct {
    DCSHIFT_FLOAT dcshift; /* DC shift. */
    
    int uselimiter; /* boolean: are we using the limiter? */
    DCSHIFT_FLOAT limiterthreshhold;
    DCSHIFT_FLOAT limitergain; /* limiter gain. */
    int limited; /* number of limited values to report. */
    int totalprocessed;
    
    int clipped;    /* number of clipped values to report. */
} * dcs_t;

/*
 * Process options: dcshift (float) type (amplitude, power, dB)
 */
int st_dcshift_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
    dcs_t dcs = (dcs_t) effp->priv; 
    dcs->dcshift = ONE; /* default is no change */
    dcs->uselimiter = 0; /* default is no limiter */
    
    if (n && (!sscanf(argv[0], DCSHIFT_FLOAT_SCAN, &dcs->dcshift)))
    {
	st_fail(DCSHIFT_USAGE);
	return ST_EOF;
    }

    if (n>1)
    {
    	if (!sscanf(argv[1], DCSHIFT_FLOAT_SCAN, &dcs->limitergain))
    	{
  		st_fail(DCSHIFT_USAGE);
		return ST_EOF;  		
    	}
    	
    	dcs->uselimiter = 1; /* ok, we'll use it */
    	/* The following equation is derived so that there is no discontinuity in output amplitudes */
    	/* and a LONG_MAX input always maps to a LONG_MAX output when the limiter is activated. */
    	/* (NOTE: There **WILL** be a discontinuity in the slope of the output amplitudes when using the limiter.) */
    	dcs->limiterthreshhold = LONG_MAX * (ONE - (fabs(dcs->dcshift) - dcs->limitergain));
    }
    
    return ST_SUCCESS;
}

/*
 * Start processing
 */
int st_dcshift_start(effp)
eff_t effp;
{
    dcs_t dcs = (dcs_t) effp->priv;
    
    if (effp->outinfo.channels != effp->ininfo.channels)
    {
	st_warn("DCSHIFT cannot handle different channels (in=%d, out=%d)"
	     " use avg or pan", effp->ininfo.channels, effp->outinfo.channels);
    }

    if (effp->outinfo.rate != effp->ininfo.rate)
    {
	st_fail("DCSHIFT cannot handle different rates (in=%ld, out=%ld)"
	     " use resample or rate", effp->ininfo.rate, effp->outinfo.rate);
	return ST_EOF;
    }

    dcs->clipped = 0;
    dcs->limited = 0;
    dcs->totalprocessed = 0;

    return ST_SUCCESS;
}

/* conversion. clipping could be smoother at high ends?
 * this could be a function on its own, with clip count and report
 * handled by eff_t and caller.
 */
static LONG clip(dcs_t dcs, const DCSHIFT_FLOAT v)
{
    if (v > LONG_MAX)
    {
	 dcs->clipped++;
	 return LONG_MAX;
    }
    else if (v < -LONG_MAX)
    {
	dcs->clipped++;
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
int st_dcshift_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
LONG *isamp, *osamp;
{
    dcs_t dcs = (dcs_t) effp->priv;
    register DCSHIFT_FLOAT dcshift = dcs->dcshift;
    register DCSHIFT_FLOAT limitergain = dcs->limitergain;
    register DCSHIFT_FLOAT limiterthreshhold = dcs->limiterthreshhold;
    register DCSHIFT_FLOAT sample;
    register LONG len;
    
    len = MIN(*osamp, *isamp);

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
	    		sample =  (sample - limiterthreshhold) * limitergain / (LONG_MAX - limiterthreshhold) + limiterthreshhold + dcshift;
	    		dcs->limited++;
	    	}
	    	else if (sample < -limiterthreshhold && dcshift < 0)
	    	{
	    		sample =  (sample + limiterthreshhold) * limitergain / (LONG_MAX - limiterthreshhold) - limiterthreshhold + dcshift;
	    		dcs->limited++;
	    	}
	    	else
	    	{
	    		sample = dcshift * LONG_MAX + sample;
	    	}

		*obuf++ = clip(dcs, sample);
	    }
    }
    else
    {
    	/* quite basic, with clipping */
    	for (;len>0; len--)
		*obuf++ = clip(dcs, dcshift * LONG_MAX + *ibuf++);
    }
    return ST_SUCCESS;
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_dcshift_stop(effp)
eff_t effp;
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
