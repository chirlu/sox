/*
 * Sound Tools Vibro effect file.
 *
 * Modeled on world-famous Fender(TM) Amp Vibro knobs.
 * 
 * Algorithm: generate a sine wave ranging from
 * 0 + depth to 1.0, where signal goes from -1.0 to 1.0.
 * Multiply signal with sine wave.  I think.
 *
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 *
 * April 28, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *
 *  Rearranged some functions so that they are declared before they are
 *  used.  Clears up some compiler warnings.  Because this functions passed
 *  foats, it helped out some dump compilers pass stuff on the stack
 *  correctly.
 *
 */


#include <math.h>
#include <stdlib.h>
#include "st_i.h"

/* Private data for Vibro effect */
typedef struct vibrostuff {
	float 		speed;
	float 		depth;
	short		*sinetab;		/* sine wave to apply */
	int		mult;			/* multiplier */
	unsigned	length;			/* length of table */
	int		counter;		/* current counter */
} *vibro_t;

/*
 * Process options
 */
int st_vibro_getopts(eff_t effp, int n, char **argv) 
{
	vibro_t vibro = (vibro_t) effp->priv;

	vibro->depth = 0.5;
	if ((n == 0) || !sscanf(argv[0], "%f", &vibro->speed) ||
		((n == 2) && !sscanf(argv[1], "%f", &vibro->depth)))
	{
		st_fail("Usage: vibro speed [ depth ]");
		return (ST_EOF);
	}
	if ((vibro->speed <= 0.001) || (vibro->speed > 30.0) || 
			(vibro->depth < 0.0) || (vibro->depth > 1.0))
	{
		st_fail("Vibro: speed must be < 30.0, 0.0 < depth < 1.0");
		return (ST_EOF);
	}
	return (ST_SUCCESS);
}

/* This was very painful.  We need a sine library. */
/* SJB: this is somewhat different than st_sine()  */
/* FIXME: move to misc.c */
static void sine(short *buf, int len, float depth)
{
	int i;
	int scale = depth * 128;
	int base = (1.0 - depth) * 128;
	double val;

	for (i = 0; i < len; i++) {
		val = sin((float)i/(float)len * 2.0 * M_PI);
		buf[i] = (val + 1.0) * scale + base * 2;
	}
}

/*
 * Prepare processing.
 */
int st_vibro_start(eff_t effp)
{
	vibro_t vibro = (vibro_t) effp->priv;

	vibro->length = effp->ininfo.rate / vibro->speed;
	if (! (vibro->sinetab = (short*) malloc(vibro->length * sizeof(short))))
	{
		st_fail("Vibro: Cannot malloc %d bytes",
			vibro->length * sizeof(short));
		return (ST_EOF);
	}

	sine(vibro->sinetab, vibro->length, vibro->depth);
	vibro->counter = 0;
	return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

int st_vibro_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                  st_size_t *isamp, st_size_t *osamp)
{
	vibro_t vibro = (vibro_t) effp->priv;
	register int counter, tablen;
	int len, done;
	short *sinetab;
	st_sample_t l;

	len = ((*isamp > *osamp) ? *osamp : *isamp);

	sinetab = vibro->sinetab;
	counter = vibro->counter;
	tablen = vibro->length;
	for(done = 0; done < len; done++) {
		l = *ibuf++;
		/* 24x8 gives 32-bit result */
		*obuf++ = ((l / 256) * sinetab[counter++ % tablen]);
	}
	vibro->counter = counter;
	/* processed all samples */
	return (ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_vibro_stop(eff_t effp)
{
	/* nothing to do */
    return (ST_SUCCESS);
}

