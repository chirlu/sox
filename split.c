
/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 *  "split" effect by Lauren Weinstein (lauren@vortex.com); 2/94
 *  Splits 1 channel file to 2 channels (stereo), or 4 channels (quad);
 *  or splits a 2 channel file to 4 channels.
 */

#include <math.h>
#include "st_i.h"

/* Private data for split */
typedef struct splitstuff {
	int	rest;		/* bytes remaining in current block */
} *split_t;

/*
 * Process options
 */
int st_split_getopts(eff_t effp, int n, char **argv) 
{
	if (n)
	{
		st_fail("Split effect takes no options.");
		return (ST_EOF);
	}
	return (ST_SUCCESS);
}

/*
 * Prepare processing.
 */
int st_split_start(eff_t effp)
{
	switch (effp->ininfo.channels) {
		case 1:   /* 1 channel must split to 2 or 4 */
			switch(effp->outinfo.channels) {
				case 2:
				case 4:
					return (ST_SUCCESS);
			}
			break;
		case 2:	  /* 2 channels must split to 4 */
			switch(effp->outinfo.channels) {
				case 4:
					return (ST_SUCCESS);
			}
			break;
	}
	st_fail("Can't split %d channels into %d channels",
		effp->ininfo.channels, effp->outinfo.channels);
	return (ST_EOF);
}

/*
 * Process signed long samples from ibuf to obuf,
 * isamp or osamp samples, whichever is smaller,
 * while splitting into appropriate channels.
 */
int st_split_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                  st_size_t *isamp, st_size_t *osamp)
{
	int len, done;

 	switch(effp->ininfo.channels) {
		case 1:  /* 1 input channel */
			switch(effp->outinfo.channels) {
				case 2:  /* split to 2 channels */
					len = ((*isamp > *osamp/2) 
					      ? *osamp/2 : *isamp);
					for(done = 0; done < len; done++) {
						obuf[0] = obuf[1] = *ibuf++;
						obuf += 2;
					}			
					*isamp = len;
					*osamp = len * 2;
					break;		
				case 4:  /* split to 4 channels */
					len = ((*isamp > *osamp/4) 
					      ? *osamp/4 : *isamp);
					for(done = 0; done < len; done++) {
						obuf[0] = obuf[1] = obuf[2]
					 	  = obuf[3] = *ibuf++;
						obuf += 4;
					}
					*isamp = len;
					*osamp = len * 4;
					break;
			}
			break;
		case 2:  /* 2 input channels; split to 4 channels  */
			 /* We're using the same channel ordering  */
			 /* as in "avg.c"--sure hope it's correct! */
			len = ((*isamp/2 > *osamp/4) 
			      ? *osamp/4 : *isamp/2);
			for(done = 0; done < len; done++) {
				obuf[0] = obuf[2] = ibuf[0];
				obuf[1] = obuf[3] = ibuf[1];
				ibuf += 2;
				obuf += 4;
			}
			*isamp = len;
			*osamp = len * 2;
			break;
	}
	return (ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_split_stop(eff_t effp)
{
	/* nothing to do */
    return (ST_SUCCESS);
}
