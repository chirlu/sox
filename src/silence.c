/*
 * silence - effect to detect periods of silence in audio data and
 *    use this to clip data before and after.
 *
 * Written by Chris Bagwell (cbagwell@sprynet.com) - January 10, 1999
 *
  * Copyright 1999 Chris Bagwell And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Chris Bagwell And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */


#include "st.h"

/* Private data for SKEL file */
typedef struct silencestuff {
        int     threshold;
        int     threshold_length;
        int     threshold_count;
        int     begin;
        int     begin_skip;
        int     begin_count;
        int     end;
        int     end_skip;
        int     end_count;
} *silenc_t;

/*
 * Process options
 *
 * Don't do initialization now.
 * The 'info' fields are not yet filled in.
 */
void silence_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
	if (n)
		fail("Silence effect takes no options.");
}

/*
 * Prepare processing.
 * Do all initializations.
 */
silence_start(effp)
eff_t effp;
{
    silence_t silence = (silence_t) effp->priv;

    silence.threshold = 5;
    silence.thres_length = 1000;
    silence.thres_count = 0;

    silence.begin = silence.end = 0;
    silence.begin_skip = silence.end_skip = 0;
    silence.begin_count = silence.end_count = 0;
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

void silence_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
int *isamp, *osamp;
{
	silence_t silence = (silence_t) effp->priv;
	int len, done;
	
	len = ((*isamp > *osamp) ? *osamp : *isamp);
	for(done = 0; done < len; done++) {
	    if (silence.threshold_count >= silence.threshold_length) { 
	        
	        
		if no more samples
			break
		get a sample
		l = sample converted to signed long
		*buf++ = l;
	}
	*isamp = 
	*osamp = 
}

/*
 * Drain out remaining samples if the effect generates any.
 */

void skel_drain(effp, obuf, osamp)
LONG *obuf;
int *osamp;
{
	*osamp = 0;
}

/*
 * Do anything required when you stop reading samples.  
 *	(free allocated memory, etc.)
 */
void skel_stop(effp)
eff_t effp;
{
	/* nothing to do */
}


