
/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 *
 * Channel duplication code by Graeme W. Gill - 93/5/18
 */

/*
 * Sound Tools stereo/quad -> mono mixdown effect file.
 * and mono/stereo -> stereo/quad channel duplication.
 *
 * What's in a center channel?
 */

#include "st.h"

/* Private data for SKEL file */
typedef struct avgstuff {
	int	mix;			/* How are we mixing it? */
} *avg_t;

#define MIX_CENTER	0
#define MIX_LEFT	1
#define MIX_RIGHT	2

/*
 * Process options
 */
int st_avg_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
	avg_t avg = (avg_t) effp->priv;

	/* NOTE :- should probably have a pan option for both */
	/* 2 -> 1 and 1 -> 2 etc. conversion, rather than just */
	/* left and right. If 4 channels is to be fully supported, */
	/* front and back pan is also needed. (GWG) */
	/* (should  at least have MIX_FRONT and MIX_BACK) */
	avg->mix = MIX_CENTER;
	if (n) {
		if(!strcmp(argv[0], "-l"))
			avg->mix = MIX_LEFT;
		else if (!strcmp(argv[0], "-r"))
			avg->mix = MIX_RIGHT;
		else
		{
			st_fail("Usage: avg [ -l | -r ]");
			return (ST_EOF);
		}
	}
	return (ST_SUCCESS);
}

/*
 * Start processing
 */

int st_avg_start(effp)
eff_t effp;
{
        if ((effp->ininfo.channels == effp->outinfo.channels) ||
	    effp->outinfo.channels == -1)
	{
	    st_fail("Output must have different number of channels to use avg effect");
	    return(ST_EOF);
	}

	switch (effp->outinfo.channels) 
	{
	case 1: switch (effp->ininfo.channels) 
		{
		case 2: 
		case 4:
			return (ST_SUCCESS);
		default:
			break;
		}
		break;
	case 2: switch (effp->ininfo.channels) 
		{
		case 1:
		case 4:
			return (ST_SUCCESS);
		default:
			break;
		}
		break;
	case 4: switch (effp->ininfo.channels) 
		{
		case 1:
		case 2:
			return (ST_SUCCESS);
		default:
			break;
		}
	default:
		break;
	}	
	st_fail("Can't average %d channels into %d channels",
		effp->ininfo.channels, effp->outinfo.channels);
	return (ST_EOF);
}

/*
 * Process either isamp or osamp samples, whichever is smaller.
 */

int st_avg_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
LONG *isamp, *osamp;
{
	avg_t avg = (avg_t) effp->priv;
	int len, done;
	
	switch (effp->outinfo.channels) {
		case 1: switch (effp->ininfo.channels) {
			case 2:
				/* average 2 channels into 1 */
				len = ((*isamp/2 > *osamp) ? *osamp : *isamp/2);
				switch(avg->mix) {
				    case MIX_CENTER:
					for(done = 0; done < len; done++) {
						*obuf++ = ibuf[0]/2 + ibuf[1]/2;
						ibuf += 2;
					}
					break;
				    case MIX_LEFT:
					for(done = 0; done < len; done++) {
						*obuf++ = ibuf[0];
						ibuf += 2;
					}
					break;
				    case MIX_RIGHT:
					for(done = 0; done < len; done++) {
						*obuf++ = ibuf[1];
						ibuf += 2;
					}
					break;
				}
				*isamp = len * 2;
				*osamp = len;
				break;
			case 4:
				/* average 4 channels into 1 */
				len = ((*isamp/4 > *osamp) ? *osamp : *isamp/4);
				switch(avg->mix) {
				    case MIX_CENTER:
					for(done = 0; done < len; done++) {
						*obuf++ = ibuf[0]/4 + ibuf[1]/4 +
							ibuf[2]/4 + ibuf[3]/4;
						ibuf += 4;
					}
					break;
				    case MIX_LEFT:
					for(done = 0; done < len; done++) {
						*obuf++ = ibuf[0]/2 + ibuf[2]/2;
						ibuf += 4;
					}
					break;
				    case MIX_RIGHT:
					for(done = 0; done < len; done++) {
						*obuf++ = ibuf[1]/2 + ibuf[3]/2;
						ibuf += 4;
					}
					break;
				}
				*isamp = len * 4;
				*osamp = len;
				break;
			}
			break;
		case 2: switch (effp->ininfo.channels) {
			case 1:
				/* duplicate 1 channel into 2 */
				len = ((*isamp > *osamp/2) ? *osamp/2 : *isamp);
				switch(avg->mix) {
				    case MIX_CENTER:
					for(done = 0; done < len; done++) {
						obuf[0] = obuf[1] = ibuf[0];
						ibuf += 1;
						obuf += 2;
					}
					break;
				    case MIX_LEFT:
					for(done = 0; done < len; done++) {
						obuf[0] = ibuf[0];
						obuf[1] = 0;
						ibuf += 1;
						obuf += 2;
					}
					break;
				    case MIX_RIGHT:
					for(done = 0; done < len; done++) {
						obuf[0] = 0;
						obuf[1] = ibuf[0];
						ibuf += 1;
						obuf += 2;
					}
					break;
				}
				*isamp = len;
				*osamp = len * 2;
				break;
			/*
			 * After careful inspection of CSOUND source code,
			 * I'm mildly sure the order is:
			 * 	front-left, front-right, rear-left, rear-right
			 */
			case 4:
				/* average 4 channels into 2 */
				len = ((*isamp/4 > *osamp/2) ? *osamp/2 : *isamp/4);
				for(done = 0; done < len; done++) {
					obuf[0] = ibuf[0]/2 + ibuf[2]/2;
					obuf[1] = ibuf[1]/2 + ibuf[3]/2;
					ibuf += 4;
					obuf += 2;
				}
				*isamp = len * 4;
				*osamp = len * 2;
				break;
			}
			break;
		case 4: switch (effp->ininfo.channels) {
			case 1:
				/* duplicate 1 channel into 4 */
				len = ((*isamp > *osamp/4) ? *osamp/4 : *isamp);
				switch(avg->mix) {
				    case MIX_CENTER:
					for(done = 0; done < len; done++) {
						obuf[0] = obuf[1] = 
						obuf[2] = obuf[3] = ibuf[0];
						ibuf += 1;
						obuf += 4;
					}
					break;
				    case MIX_LEFT:
					for(done = 0; done < len; done++) {
						obuf[0] = obuf[2] = ibuf[0];
						obuf[1] = obuf[3] = 0;
						ibuf += 1;
						obuf += 4;
					}
					break;
				    case MIX_RIGHT:
					for(done = 0; done < len; done++) {
						obuf[0] = obuf[2] = 0;
						obuf[1] = obuf[3] = ibuf[0];
						ibuf += 1;
						obuf += 4;
					}
					break;
				}
				*isamp = len;
				*osamp = len * 4;
				break;
			case 2:
				/* duplicate 2 channels into 4 */
				len = ((*isamp/2 > *osamp/4) ? *osamp/4 : *isamp/2);
				for(done = 0; done < len; done++) {
					obuf[0] = obuf[2] = ibuf[0];
					obuf[1] = obuf[3] = ibuf[1];
					ibuf += 2;
					obuf += 4;
				}
				*isamp = len * 2;
				*osamp = len * 4;
				break;
			}
			break;
	}	/* end switch out channels */
	return (ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 *
 * Should have statistics on right, left, and output amplitudes.
 */
int st_avg_stop(effp)
eff_t effp;
{
	return (ST_SUCCESS); /* nothing to do */
}

