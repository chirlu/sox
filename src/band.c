/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools Bandpass effect file.
 *
 * Algorithm:  2nd order recursive filter.
 * Formula stolen from MUSIC56K, a toolkit of 56000 assembler stuff.
 * Quote:
 *   This is a 2nd order recursive band pass filter of the form.                
 *   y(n)= a * x(n) - b * y(n-1) - c * y(n-2)   
 *   where :    
 *        x(n) = "IN"           
 *        "OUT" = y(n)          
 *        c = EXP(-2*pi*cBW/S_RATE)             
 *        b = -4*c/(1+c)*COS(2*pi*cCF/S_RATE)   
 *   if cSCL=2 (i.e. noise input)               
 *        a = SQT(((1+c)*(1+c)-b*b)*(1-c)/(1+c))                
 *   else       
 *        a = SQT(1-b*b/(4*c))*(1-c)            
 *   endif      
 *   note :     cCF is the center frequency in Hertz            
 *        cBW is the band width in Hertz        
 *        cSCL is a scale factor, use 1 for pitched sounds      
 *   use 2 for noise.           
 *
 *
 * July 1, 1999 - Jan Paul Schmidt <jps@fundament.org>
 *
 *   This looks like the resonator band pass in SPKit. It's a
 *   second order all-pole (IIR) band-pass filter described
 *   at the pages 186 - 189 in
 *     Dodge, Charles & Jerse, Thomas A. 1985: 
 *       Computer Music -- Synthesis, Composition and Performance.
 *       New York: Schirmer Books.  
 *   Reference from the SPKit manual.
 */

#include <math.h>
#include <string.h>
#include "st_i.h"

/* Private data for Bandpass effect */
typedef struct bandstuff {
	float	center;
	float	width;
	double	A, B, C;
	double	out1, out2;
	short	noise;
	/* 50 bytes of data, 52 bytes long for allocation purposes. */
} *band_t;

/*
 * Process options
 */
int st_band_getopts(eff_t effp, int n, char **argv) 
{
	band_t band = (band_t) effp->priv;

	band->noise = 0;
	if (n > 0 && !strcmp(argv[0], "-n")) {
		band->noise = 1;
		n--;
		argv++;
	}
	if ((n < 1) || !sscanf(argv[0], "%f", &band->center))
	{
		st_fail("Usage: band [ -n ] center [ width ]");
		return (ST_EOF);
	}
	band->width = band->center / 2;
	if ((n >= 2) && !sscanf(argv[1], "%f", &band->width))
	{
		st_fail("Usage: band [ -n ] center [ width ]");
		return (ST_EOF);
	}
	return (ST_SUCCESS);
}

/*
 * Prepare processing.
 */
int st_band_start(eff_t effp)
{
	band_t band = (band_t) effp->priv;
	if (band->center > effp->ininfo.rate/2)
	{
		st_fail("Band: center must be < minimum data rate/2\n");
		return (ST_EOF);
	}

	band->C = exp(-2*M_PI*band->width/effp->ininfo.rate);
	band->B = -4*band->C/(1+band->C)*
		cos(2*M_PI*band->center/effp->ininfo.rate);
	if (band->noise)
		band->A = sqrt(((1+band->C)*(1+band->C)-band->B *
			band->B)*(1-band->C)/(1+band->C));
	else
		band->A = sqrt(1-band->B*band->B/(4*band->C))*(1-band->C);
	band->out1 = band->out2 = 0.0;
	return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

int st_band_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
	         st_size_t *isamp, st_size_t *osamp)
{
	band_t band = (band_t) effp->priv;
	int len, done;
	double d;
	st_sample_t l;

	len = ((*isamp > *osamp) ? *osamp : *isamp);

	/* yeah yeah yeah registers & integer arithmetic yeah yeah yeah */
	for(done = 0; done < len; done++) {
		l = *ibuf++;
		d = (band->A * l - band->B * band->out1) - band->C * band->out2;
		band->out2 = band->out1;
		band->out1 = d;
		*obuf++ = d;
	}
	*isamp = len;
	*osamp = len;
	return(ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_band_stop(eff_t effp)
{
	return (ST_SUCCESS);	/* nothing to do */
}

