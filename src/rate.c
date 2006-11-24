/*
 * August 21, 1998
 * Copyright 1998 Fabrice Bellard.
 *
 * [Rewrote completely the code of Lance Norskog And Sundry
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

static st_effect_t st_rate_effect = {
  "rate",
  "Usage: Rate effect takes no options",
  ST_EFF_RATE,
  st_rate_getopts,
  st_rate_start,
  st_rate_flow,
  st_effect_nothing_drain,
  st_effect_nothing
};

const st_effect_t *st_rate_effect_fn(void)
{
    return &st_rate_effect;
}
