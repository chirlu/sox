/*
 * August 21, 1998
 * Copyright 1998 Fabrice Bellard.
 *
 * [Rewrote completly the code of Lance Norskog And Sundry
 * Contributors with a more efficient algorithm.]
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.  
 */

/*
 * Sound Tools rate change effect file.
 */

#include "st_i.h"

#ifndef USE_OLD_RATE

#include <math.h>
/*
 * Linear Interpolation.
 *
 * The use of fractional increment allows us to use no buffer. It
 * avoid the problems at the end of the buffer we had with the old
 * method which stored a possibly big buffer of size
 * lcm(in_rate,out_rate).
 *
 * Limited to 16 bit samples and sampling frequency <= 65535 Hz. If
 * the input & output frequencies are equal, a delay of one sample is
 * introduced.  Limited to processing 32-bit count worth of samples.
 *
 * 1 << FRAC_BITS evaluating to zero in several places.  Changed with
 * an (unsigned long) cast to make it safe.  MarkMLl 2/1/99
 */

#define FRAC_BITS 16

/* Private data */
typedef struct ratestuff {
        unsigned long opos_frac;  /* fractional position of the output stream in input stream unit */
        unsigned long opos;

        unsigned long opos_inc_frac;  /* fractional position increment in the output stream */
        unsigned long opos_inc; 

        unsigned long ipos;      /* position in the input stream (integer) */

        st_sample_t ilast; /* last sample in the input stream */
} *rate_t;

/*
 * Process options
 */
int st_rate_getopts(eff_t effp, int n, char **argv) 
{
	if (n)
	{
		st_fail("Rate effect takes no options.");
		return (ST_EOF);
	}
	return (ST_SUCCESS);
}

/*
 * Prepare processing.
 */
int st_rate_start(eff_t effp)
{
	rate_t rate = (rate_t) effp->priv;
        unsigned long incr;

	if (effp->ininfo.rate == effp->outinfo.rate)
	{
	    st_fail("Input and Output rates must be different to use rate effect");
	    return(ST_EOF);
	}

	if (effp->ininfo.rate >= 65535 || effp->outinfo.rate >= 65535)
	{
	    st_fail("rate effect can only handle rates <= 65535");
	    return (ST_EOF);
	}
	if (effp->ininfo.size == ST_SIZE_DWORD || 
	    effp->ininfo.size == ST_SIZE_DDWORD)
	{
	    st_warn("rate effect reduces data to 16 bits");
	}

        rate->opos_frac=0;
        rate->opos=0;

        /* increment */
        incr=(unsigned long)((double)effp->ininfo.rate / (double)effp->outinfo.rate * 
                   (double) ((unsigned long) 1 << FRAC_BITS));

        rate->opos_inc_frac = incr & (((unsigned long) 1 << FRAC_BITS)-1);
        rate->opos_inc = incr >> FRAC_BITS;
        
        rate->ipos=0;

	rate->ilast = 0;
	return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
int st_rate_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                 st_size_t *isamp, st_size_t *osamp)
{
	rate_t rate = (rate_t) effp->priv;
	st_sample_t *istart,*iend;
	st_sample_t *ostart,*oend;
	st_sample_t ilast,icur,out;
        unsigned long tmp;
        double t;

        ilast=rate->ilast;

        istart = ibuf;
        iend = ibuf + *isamp;
        
        ostart = obuf;
        oend = obuf + *osamp;

        while (obuf < oend) {

		/* Safety catch to make sure we have input samples.  */
                if (ibuf >= iend) goto the_end;

                /* read as many input samples so that ipos > opos */
        
                while (rate->ipos <= rate->opos) {
                        ilast = *ibuf++;
                        rate->ipos++;
			/* See if we finished the input buffer yet */
                        if (ibuf >= iend) goto the_end;
                }

                icur = *ibuf;
        
                /* interpolate */
                t=(double) rate->opos_frac / ((unsigned long) 1 << FRAC_BITS);
                out = (double) ilast * (1.0 - t) + (double) icur * t;

                /* output sample & increment position */
                
                *obuf++=(st_sample_t) out;
                
                tmp = rate->opos_frac + rate->opos_inc_frac;
                rate->opos = rate->opos + rate->opos_inc + (tmp >> FRAC_BITS);
                rate->opos_frac = tmp & (((unsigned long) 1 << FRAC_BITS)-1);
        }
the_end:
	*isamp = ibuf - istart;
	*osamp = obuf - ostart;
	rate->ilast = ilast;
	return (ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_rate_stop(eff_t effp)
{
	/* nothing to do */
    return (ST_SUCCESS);
}

#else /* USE_OLD_RATE */

/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools rate change effect file.
 */

#include <math.h>

/*
 * Least Common Multiple Linear Interpolation 
 *
 * Find least common multiple of the two sample rates.
 * Construct the signal at the LCM by interpolating successive
 * input samples as straight lines.  Pull output samples from
 * this line at output rate.
 *
 * Of course, actually calculate only the output samples.
 *
 * LCM must be 32 bits or less.  Two prime number sample rates
 * between 32768 and 65535 will yield a 32-bit LCM, so this is 
 * stretching it.
 */

/*
 * Algorithm:
 *  
 *  Generate a master sample clock from the LCM of the two rates.
 *  Interpolate linearly along it.  Count up input and output skips.
 *
 *  Input:   |inskip |       |       |       |       |
 *                                                                      
 *                                                                      
 *                                                                      
 *  LCM:     |   |   |   |   |   |   |   |   |   |   |
 *                                                                      
 *                                                                      
 *                                                                      
 *  Output:  |  outskip  |           |           | 
 *
 *                                                                      
 */


/* Private data for Lerp via LCM file */
typedef struct ratestuff {
	st_rate_t lcmrate;		/* least common multiple of rates */
	unsigned long inskip, outskip;	/* LCM increments for I & O rates */
	unsigned long total;
	unsigned long intot, outtot;	/* total samples in LCM basis */
	st_sample_t lastsamp;		/* history */
} *rate_t;

/*
 * Process options
 */
int st_rate_getopts(eff_t effp, int n, char **argv) 
{
	if (n)
	{
		st_fail("Rate effect takes no options.");
		return (ST_EOF);
	}
	return (ST_SUCCESS);
}

/*
 * Prepare processing.
 */
int st_rate_start(eff_t effp)
{
	rate_t rate = (rate_t) effp->priv;
	
	rate->lcmrate = st_lcm((st_sample_t)effp->ininfo.rate, (st_sample_t)effp->outinfo.rate);
	/* Cursory check for LCM overflow.  
	 * If both rate are below 65k, there should be no problem.
	 * 16 bits x 16 bits = 32 bits, which we can handle.
	 */
	rate->inskip = rate->lcmrate / effp->ininfo.rate;
	rate->outskip = rate->lcmrate / effp->outinfo.rate; 
	rate->total = rate->intot = rate->outtot = 0;
	rate->lastsamp = 0;
	return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
int st_rate_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                 st_size_t *isamp, st_size_t *osamp)
{
	rate_t rate = (rate_t) effp->priv;
	int len, done;
	st_sample_t *istart = ibuf;
	st_sample_t last;

	done = 0;
	if (rate->total == 0) {
		/* Emit first sample.  We know the fence posts meet. */
		*obuf = *ibuf++;
		rate->lastsamp = *obuf++ / 65536L;
		done = 1;
		rate->total = 1;
		/* advance to second output */
		rate->outtot += rate->outskip;
		/* advance input range to span next output */
		while ((rate->intot + rate->inskip) <= rate->outtot){
			last = *ibuf++ / 65536L;
			rate->intot += rate->inskip;
		}
	} 

	/* start normal flow-through operation */
	last = rate->lastsamp;
		
	/* number of output samples the input can feed */
	len = (*isamp * rate->inskip) / rate->outskip;
	if (len > *osamp)
		len = *osamp;
	for(; done < len; done++) {
		*obuf = last;
		*obuf += ((float)((*ibuf / 65536L)  - last)* ((float)rate->outtot -
				rate->intot))/rate->inskip;
		*obuf *= 65536L;
		obuf++;
		/* advance to next output */
		rate->outtot += rate->outskip;
		/* advance input range to span next output */
		while ((rate->intot + rate->inskip) <= rate->outtot){
			last = *ibuf++ / 65536L;
			rate->intot += rate->inskip;
			if (ibuf - istart == *isamp)
				goto out;
		}
		/* long samples with high LCM's overrun counters! */
		if (rate->outtot == rate->intot)
			rate->outtot = rate->intot = 0;
	}
out:
	*isamp = ibuf - istart;
	*osamp = len;
	rate->lastsamp = last;
	return (ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_rate_stop(eff_t effp)
{
	/* nothing to do */
    return (ST_SUCCESS);
}
#endif /* USE_OLD_RATE */
