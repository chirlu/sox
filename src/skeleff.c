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


#include "st.h"

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
void skeleff_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
    skeleff_t skeleff = (skeleff_t) effp->priv;

    if (n)
    {
	if (n != 1)
	{
	    fail("Usage: skeleff [option]");
	}
    }
}

/*
 * Prepare processing.
 * Do all initializations.
 */
skeleff_start(effp)
eff_t effp;
{
    if (effp->outinfo.channels == 1)
	fail("Can't run skeleff on mono data.");
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

void skeleff_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
int *isamp, *osamp;
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
}

/*
 * Drain out remaining samples if the effect generates any.
 */

void skeleff_drain(effp, obuf, osamp)
LONG *obuf;
int *osamp;
{
	*osamp = 0;
}

/*
 * Do anything required when you stop reading samples.  
 *	(free allocated memory, etc.)
 */
void skeleff_stop(effp)
eff_t effp;
{
	/* nothing to do */
}
