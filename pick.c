
/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 *  "pick" effect by Lauren Weinstein (lauren@vortex.com); 2/94
 *  Creates a 1 channel file by selecting a single channel from
 *  a 2 or 4 channel file.  Does not currently allow creating a 2 channel
 *  file by selecting 2 channels from a 4 channel file.
 */

#include "st.h"

/* Private data for SKEL file */
typedef struct pickstuff {
	int	chan;	 /* selected channel */
} *pick_t;

/* channel names are offset by 1 from actual channel byte array offsets */
#define CHAN_1	0	
#define CHAN_2	1
#define CHAN_3	2
#define CHAN_4	3

/*
 * Process options
 */
void
pick_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
	pick_t pick = (pick_t) effp->priv;

	if (n == 1 && argv[0][0] == '-') {  /* must specify channel to pick */
		switch (argv[0][1]) {
			case 'l':
				pick->chan = CHAN_1;
				return;
			case 'r':
				pick->chan = CHAN_2;
				return;
			case '1':
				pick->chan = CHAN_1;
				return;
			case '2':
				pick->chan = CHAN_2;
				return;
			case '3':
				pick->chan = CHAN_3;
				return;
			case '4':
				pick->chan = CHAN_4;
				return;
		}
	}
	pick->chan = -1;  /* invalid option */
}


/*
 * Start processing.  Final option checking is done here since
 * error/usage messages will vary based on the number of input/output
 * channels selected, and that info is not available in pick_getopts()
 * above.
 */
void
pick_start(effp)
eff_t effp;
{
	pick_t pick = (pick_t) effp->priv;

	if (effp->outinfo.channels != 1)  /* must be one output channel */
	   fail("Can't pick with other than 1 output channel."); 
	if (effp->ininfo.channels != 2 && effp->ininfo.channels != 4)
	        fail("Can't pick with other than 2 or 4 input channels.");
        if (effp->ininfo.channels == 2) {  /* check for valid option */
	   if (pick->chan == -1 || pick->chan == CHAN_3 || pick->chan == CHAN_4)
   	      fail("Must specify channel to pick: '-l', '-r', '-1', or '-2'.");
	}
	else  /* must be 4 channels; check for valid option */
	   if (pick->chan == -1)
	      fail("Must specify channel to pick: '-1', '-2', '-3', or '-4'.");
}

/*
 * Process signed long samples from ibuf to obuf,
 * isamp or osamp samples, whichever is smaller,
 * while picking appropriate channels.
 */

void pick_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
LONG *isamp, *osamp;
{
	pick_t pick = (pick_t) effp->priv;
	int len, done;
	
	switch (effp->ininfo.channels) {
		case 2:
			len = ((*isamp/2 > *osamp) ? *osamp : *isamp/2);
			for(done = 0; done < len; done++) {
				*obuf++ = ibuf[pick->chan];
				ibuf += 2;
			}
			*isamp = len * 2;
			*osamp = len;
			break;
		case 4:
			len = ((*isamp/4 > *osamp) ? *osamp : *isamp/4);
			for(done = 0; done < len; done++) {
				*obuf++ = ibuf[pick->chan];
				ibuf += 4;
			}
			*isamp = len * 4;
			*osamp = len;
			break;
	}
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
void pick_stop(effp)
eff_t effp;
{
	/* nothing to do */
}

