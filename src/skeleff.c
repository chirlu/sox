
/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools skeleton effect file.
 */

#include <math.h>
#include "st.h"

/* Private data for SKEL file */
typedef struct skelstuff {
	int	rest;			/* bytes remaining in current block */
} *skel_t;

/*
 * Process options
 *
 * Don't do initialization now.
 * The 'info' fields are not yet filled in.
 */
void skel_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
	if (n)
		fail("Copy effect takes no options.");
}

/*
 * Prepare processing.
 * Do all initializations.
 */
skel_start(effp)
eff_t effp;
{
	/* nothing to do */
	/* stuff data into delaying effects here */
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

void skel_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
int *isamp, *osamp;
{
	skel_t skel = (skel_t) effp->priv;
	int len, done;
	
	char c;
	unsigned char uc;
	short s;
	unsigned short us;
	LONG l;
	ULONG ul;
	float f;
	double d;

	len = ((*isamp > *osamp) ? *osamp : *isamp);
	for(done = 0; done < len; done++) {
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


