/*
 * swap - effect to swap ordering of channels in multi-channel audio.
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
typedef struct swapstuff {
    int		order[4];
} *swap_t;

/*
 * Process options
 *
 * Don't do initialization now.
 * The 'info' fields are not yet filled in.
 */
void swap_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
    swap_t swap = (swap_t) effp->priv;

    swap->order[0] = swap->order[1] = swap->order[2] = swap->order[3] = 0;
    if (n)
    {
	if (n != 4)
	{
	    fail("Usage: swap [1 2 3 4]");
	}
	else
	{
	    sscanf(argv[0],"%d",&swap->order[0]);
	    sscanf(argv[1],"%d",&swap->order[1]);
	    sscanf(argv[2],"%d",&swap->order[2]);
	    sscanf(argv[3],"%d",&swap->order[3]);
	}
    }
}

/*
 * Prepare processing.
 * Do all initializations.
 */
swap_start(effp)
eff_t effp;
{
    if (effp->outinfo.channels == 1)
	fail("Can't swap channels on mono data.");
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

void swap_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
int *isamp, *osamp;
{
    swap_t swap = (swap_t) effp->priv;
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
	    obuf[0] = ibuf[1];
	    obuf[1] = ibuf[0];
	    /* Advance buffer by 2 samples */
	    ibuf += 2;
	    obuf += 2;
	}
	
	*isamp = len * 2;
	*osamp = len * 2;
	
	break;
	
    case 4:
	/* If nothing set then default to the following order */
	if (!swap->order[0] && !swap->order[1] &&
	    !swap->order[2] && !swap->order[3])
	{
	    swap->order[0] = 1;
	    swap->order[1] = 0;
	    swap->order[2] = 3;
	    swap->order[3] = 2;
	}
	/* Length to process will be buffer length / 4 since we
	 * work with four samples at a time.
	 */
	len = ((*isamp > *osamp) ? *osamp : *isamp) / 4;
	for(done = 0; done < len; done++)
	{
	    obuf[0] = ibuf[swap->order[0]];
	    obuf[1] = ibuf[swap->order[1]];
	    obuf[2] = ibuf[swap->order[2]];
	    obuf[3] = ibuf[swap->order[3]];
	    /* Advance buffer by 2 samples */
	    ibuf += 4;
	    obuf += 4;
	}
	*isamp = len * 4;
	*osamp = len * 4;
	
	break;
    }
}

/*
 * Drain out remaining samples if the effect generates any.
 */

void swap_drain(effp, obuf, osamp)
LONG *obuf;
int *osamp;
{
	*osamp = 0;
}

/*
 * Do anything required when you stop reading samples.  
 *	(free allocated memory, etc.)
 */
void swap_stop(effp)
eff_t effp;
{
	/* nothing to do */
}
