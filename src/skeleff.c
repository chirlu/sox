/*
 * skeleff - Skelton Effect.  Use as sample for new effects.
 *
 * Written by Chris Bagwell (cbagwell@sprynet.com) - March 16, 1999
 *
  * Copyright 1999 Chris Bagwell And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Chris Bagwell And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */


#include "st_i.h"

/* Private data for SKEL file */
typedef struct skelleffstuff {
	int  localdata;
} *skeleff_t;

/*
 * Process options
 *
 * Don't do initialization now.
 * The 'info' fields are not yet filled in.
 */
int st_skeleff_getopts(eff_t effp, int n, char **argv) 
{
    skeleff_t skeleff = (skeleff_t) effp->priv;

    if (n)
    {
	if (n != 1)
	{
	    st_fail("Usage: skeleff [option]");
	    return (ST_EOF);
	}
    }
    return (ST_SUCCESS);
}

/*
 * Prepare processing.
 * Do all initializations.
 */
int st_skeleff_start(eff_t effp)
{
    if (effp->outinfo.channels == 1)
    {
	st_fail("Can't run skeleff on mono data.");
	return (ST_EOF);
    }
    return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
int st_skeleff_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                    st_size_t *isamp, st_size_t *osamp)
{
    skeleff_t skeleff = (skeleff_t) effp->priv;
    int len, done;

    switch (effp->outinfo.channels)
    {
    case 2:
	/* Length to process will be buffer length / 2 since we
	 * work with two samples at a time.
	 */
	len = ((*isamp > *osamp) ? *osamp : *isamp) / 2;
	for(done = 0; done < len; done++)
	{
	    obuf[0] = ibuf[0];
	    obuf[1] = ibuf[1];
	    /* Advance buffer by 2 samples */
	    ibuf += 2;
	    obuf += 2;
	}
	
	*isamp = len * 2;
	*osamp = len * 2;
	
	break;
	
    }
    return (ST_SUCCESS);
}

/*
 * Drain out remaining samples if the effect generates any.
 */

int st_skeleff_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
	*osamp = 0;
	return (ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.  
 *	(free allocated memory, etc.)
 */
int st_skeleff_stop(eff_t effp)
{
	/* nothing to do */
    return (ST_SUCCESS);
}
