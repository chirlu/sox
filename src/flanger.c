/*
 * August 24, 1998
 * Copyright (C) 1998 Juergen Mueller And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Juergen Mueller And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * 	Flanger effect.
 * 
 * Flow diagram scheme:
 *
 *                                                 * gain-in  ___
 * ibuff -----+--------------------------------------------->|   |
 *            |      _______                                 |   |
 *            |     |       |                      * decay   |   |
 *            +---->| delay |------------------------------->| + |
 *                  |_______|                                |   |
 *                     /|\                                   |   |
 *                      |                                    |___|
 *                      |                                      | 
 *              +---------------+      +------------------+    | * gain-out
 *              | Delay control |<-----| modulation speed |    |
 *              +---------------+      +------------------+    +----->obuff
 *
 *
 * The delay is controled by a sine or triangle modulation.
 *
 * Usage: 
 *   flanger gain-in gain-out delay decay speed [ -s | -t ]
 *
 * Where:
 *   gain-in, decay :  0.0 ... 1.0      volume
 *   gain-out :  0.0 ...      volume
 *   delay :  0.0 ... 5.0 msec
 *   speed :  0.1 ... 2.0 Hz       modulation
 *   -s : modulation by sine (default)
 *   -t : modulation by triangle
 *
 * Note:
 *   when decay is close to 1.0, the samples may begin clipping or the output
 *   can saturate! 
 *
 * Hint:
 *   1 / out-gain > gain-in * ( 1 + decay )
 *
*/

/*
 * Sound Tools flanger effect file.
 */

#include <stdlib.h> /* Harmless, and prototypes atof() etc. --dgc */
#include <math.h>
#include <string.h>
#include "st.h"

#define MOD_SINE	0
#define MOD_TRIANGLE	1

/* Private data for SKEL file */
typedef struct flangerstuff {
	int	modulation;
	int	counter;			
	int	phase;
	double	*flangerbuf;
	float	in_gain, out_gain;
	float	delay, decay;
	float	speed;
	long	length;
	int	*lookup_tab;
	long	maxsamples, fade_out;
} *flanger_t;

/* Private data for SKEL file */

LONG flanger_clip24(l)
LONG l;
{
	if (l >= ((LONG)1 << 24))
		return ((LONG)1 << 24) - 1;
	else if (l <= -((LONG)1 << 24))
		return -((LONG)1 << 24) + 1;
	else
		return l;
}

/* This was very painful.  We need a sine library. */

void flanger_sine(buf, len, depth)
int *buf;
long len;
long depth;
{
	long i;
	double val;

	for (i = 0; i < len; i++) {
		val = sin((double)i/(double)len * 2.0 * M_PI);
		buf[i] = (int) ((1.0 + val) * depth / 2.0);
	}
}

void flanger_triangle(buf, len, depth)
int *buf;
long len;
long depth;
{
	long i;
	double val;

	for (i = 0; i < len / 2; i++) {
		val = i * 2.0 / len;
		buf[i] = (int) (val * depth);
	}
	for (i = len / 2; i < len ; i++) {
		val = (len - i) * 2.0 / len;
		buf[i] = (int) (val * depth);
	}
}

/*
 * Process options
 */
void flanger_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
	flanger_t flanger = (flanger_t) effp->priv;

	if (!((n == 5) || (n == 6)))
	    fail("Usage: flanger gain-in gain-out delay decay speed [ -s | -t ]");

	sscanf(argv[0], "%f", &flanger->in_gain);
	sscanf(argv[1], "%f", &flanger->out_gain);
	sscanf(argv[2], "%f", &flanger->delay);
	sscanf(argv[3], "%f", &flanger->decay);
	sscanf(argv[4], "%f", &flanger->speed);
	flanger->modulation = MOD_SINE;
	if ( n == 6 ) {
		if ( !strcmp(argv[5], "-s"))
			flanger->modulation = MOD_SINE;
		else if ( ! strcmp(argv[5], "-t"))
			flanger->modulation = MOD_TRIANGLE;
		else
	    		fail("Usage: flanger gain-in gain-out delay decay speed [ -s | -t ]");
	}
}

/*
 * Prepare for processing.
 */
void flanger_start(effp)
eff_t effp;
{
	flanger_t flanger = (flanger_t) effp->priv;
	int i;

	flanger->maxsamples = flanger->delay * effp->ininfo.rate / 1000.0;

	if ( flanger->in_gain < 0.0 )
	    fail("flanger: gain-in must be positive!\n");
	if ( flanger->in_gain > 1.0 )
	    fail("flanger: gain-in must be less than 1.0!\n");
	if ( flanger->out_gain < 0.0 )
	    fail("flanger: gain-out must be positive!\n");
	if ( flanger->delay < 0.0 )
	    fail("flanger: delay must be positive!\n");
	if ( flanger->delay > 5.0 )
	    fail("flanger: delay must be less than 5.0 msec!\n");
	if ( flanger->speed < 0.1 )
	    fail("flanger: speed must be more than 0.1 Hz!\n");
	if ( flanger->speed > 2.0 )
	    fail("flanger: speed must be less than 2.0 Hz!\n");
	if ( flanger->decay < 0.0 )
	    fail("flanger: decay must be positive!\n" );
	if ( flanger->decay > 1.0 )
	    fail("flanger: decay must be less that 1.0!\n" );
	/* Be nice and check the hint with warning, if... */
	if ( flanger->in_gain * ( 1.0 + flanger->decay ) > 1.0 / flanger->out_gain )
		warn("flanger: warning >>> gain-out can cause saturation or clipping of output <<<");

	flanger->length = effp->ininfo.rate / flanger->speed;

	if (! (flanger->flangerbuf = 
		(double *) malloc(sizeof (double) * flanger->maxsamples)))
		fail("flanger: Cannot malloc %d bytes!\n", 
			sizeof(double) * flanger->maxsamples);
	for ( i = 0; i < flanger->maxsamples; i++ )
		flanger->flangerbuf[i] = 0.0;
	if (! (flanger->lookup_tab = 
		(int *) malloc(sizeof (int) * flanger->length)))
		fail("flanger: Cannot malloc %d bytes!\n", 
			sizeof(int) * flanger->length);

	if ( flanger->modulation == MOD_SINE )
		flanger_sine(flanger->lookup_tab, flanger->length, 
			flanger->maxsamples - 1);
	else
		flanger_triangle(flanger->lookup_tab, flanger->length, 
			flanger->maxsamples - 1);
	flanger->counter = 0;
	flanger->phase = 0;
	flanger->fade_out = flanger->maxsamples;
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

void flanger_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
int *isamp, *osamp;
{
	flanger_t flanger = (flanger_t) effp->priv;
	int len, done;
	
	double d_in, d_out;
	LONG out;

	len = ((*isamp > *osamp) ? *osamp : *isamp);
	for(done = 0; done < len; done++) {
		/* Store delays as 24-bit signed longs */
		d_in = (double) *ibuf++ / 256;
		/* Compute output first */
		d_out = d_in * flanger->in_gain;
		d_out += flanger->flangerbuf[(flanger->maxsamples + 
	flanger->counter - flanger->lookup_tab[flanger->phase]) % 
	flanger->maxsamples] * flanger->decay;
		/* Adjust the output volume and size to 24 bit */
		d_out = d_out * flanger->out_gain;
		out = flanger_clip24((LONG) d_out);
		*obuf++ = out * 256;
		/* Mix decay of delay and input */
		flanger->flangerbuf[flanger->counter] = d_in;
		flanger->counter = 
			( flanger->counter + 1 ) % flanger->maxsamples;
		flanger->phase  = ( flanger->phase + 1 ) % flanger->length;
	}
	/* processed all samples */
}

/*
 * Drain out reverb lines. 
 */
void flanger_drain(effp, obuf, osamp)
eff_t effp;
LONG *obuf;
int *osamp;
{
	flanger_t flanger = (flanger_t) effp->priv;
	int done;
	
	double d_in, d_out;
	LONG out;

	done = 0;
	while ( ( done < *osamp ) && ( done < flanger->fade_out ) ) {
		d_in = 0;
		d_out = 0;
		/* Compute output first */
		d_out += flanger->flangerbuf[(flanger->maxsamples + 
	flanger->counter - flanger->lookup_tab[flanger->phase]) % 
	flanger->maxsamples] * flanger->decay;
		/* Adjust the output volume and size to 24 bit */
		d_out = d_out * flanger->out_gain;
		out = flanger_clip24((LONG) d_out);
		*obuf++ = out * 256;
		/* Mix decay of delay and input */
		flanger->flangerbuf[flanger->counter] = d_in;
		flanger->counter = 
			( flanger->counter + 1 ) % flanger->maxsamples;
		flanger->phase  = ( flanger->phase + 1 ) % flanger->length;
		done++;
		flanger->fade_out--;
	}
	/* samples playd, it remains */
	*osamp = done;
}

/*
 * Clean up flanger effect.
 */
void flanger_stop(effp)
eff_t effp;
{
	flanger_t flanger = (flanger_t) effp->priv;

	free((char *) flanger->flangerbuf);
	flanger->flangerbuf = (double *) -1;   /* guaranteed core dump */
	free((char *) flanger->lookup_tab);
	flanger->lookup_tab = (int *) -1;   /* guaranteed core dump */
}

