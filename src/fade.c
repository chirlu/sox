/* This code is based in skel.c
 * Written by Chris Bagwell (cbagwell@sprynet.com) - March 16, 1999 
 * Non-skel parts written by
 * Ari Moisio <armoi@sci.fi> Aug 29 2000.
 * 
 * Copyright 1999 Chris Bagwell And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Chris Bagwell And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/* Time resolution one millisecond */
#define TIMERES 	1000
/* Fade curves */
#define FADE_QUARTER 	'q' 	/* Quarter of sine wave, 0 to pi/2 */
#define FADE_HALF 	'h' 	/* Half of sine wave, pi/2 to 1.5 * pi 
				* scaled so that -1 means no output
				* and 1 means 0 db attenuation. */
#define FADE_LOG 	'l'	/* Logarithmic curve. Fades -100 db 
				* in given time. */
#define FADE_TRI 	't'	/* Linear slope. */
#define FADE_PAR	'p'	/* Inverted parabola. */

#include <math.h>
#include "st.h"

/* Private data for fade file */
typedef struct fadestuff 
{ /* These are measured as samples */
    ULONG in_start,  in_stop, out_start, out_stop, samplesdone;
    char in_fadetype, out_fadetype;
    int endpadwarned;
} *fade_t;

#define FADE_USAGE "Usage: fade [ type ] fade-in-length [ stop-time [ fade-out-length ] ]\nTimes in seconds.\nFade type one of q, h, t, l or p.\n"

/* prototypes */
static double fade_gain(ULONG index, ULONG range, char fadetype);

/*
 * Process options
 *
 * Don't do initialization now.
 * The 'info' fields are not yet filled in.
 */

int st_fade_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{

    fade_t fade = (fade_t) effp->priv;
    double t_double;
    char t_char;
    int t_argno;

    if (n < 1 || n > 4)
    { /* Wrong number of arguments. */
	st_fail(FADE_USAGE);
	return(ST_EOF);
    }
    
    /* because sample rate is unavailable at this point we convert time 
     * values to seconds * TIMERES and convert them to samples later. 
     */

    if (sscanf(argv[0], "%1[qhltp]", &t_char))
    {
	fade->in_fadetype = t_char;
	fade->out_fadetype = t_char;

	argv++;
	n--;
    }
    else
    {
	/* No type given. */
        fade->in_fadetype = 'l';
        fade->out_fadetype = 'l';
    }

    t_double = st_parsetime(argv[0]);
    if (t_double >= 0)
    { /* got something numerical */
	/* Don't support in_start. Its job is done by trim effect */
	fade->in_start = 0;
	fade->in_stop = t_double * TIMERES;
    }
    else
    { /* unnumeric value */
	st_fail(FADE_USAGE);
	return(ST_EOF);
    } /* endif numeric fade-in */

    fade->out_start = fade->out_stop = 0;

    for (t_argno = 1; t_argno < n && t_argno <= 3; t_argno++)	
    { /* See if there is fade-in/fade-out times/curves specified. */
	t_double = st_parsetime(argv[t_argno]);
	if (t_double >= 0)
	{
	    if (t_argno == 1)
	    {
		fade->out_stop = t_double * TIMERES;
		/* Zero fade-out too */
		fade->out_start = fade->out_stop;
	    }
	    else
	    {
                fade->out_start = fade->out_stop - t_double * TIMERES;
	    }
	}
	else
	{
	    st_fail(FADE_USAGE);
	    return(ST_EOF);
	}
    } /* End for(t_argno) */

    /* Sanity check for fade times vs total time */
    if (fade->in_stop > fade->out_start && fade->out_start != 0)
    { /* Fades too long */
	st_fail("Fade: End of fade-in should not happen before beginning of fade-out");
	return(ST_EOF);
    } /* endif fade time sanity */

    return(ST_SUCCESS);
} 

/*
 * Prepare processing.
 * Do all initializations.
 */
void st_fade_start(effp)
eff_t effp;
{
    fade_t fade = (fade_t) effp->priv;

    /* converting time values to samples */
    fade->in_start = (double)fade->in_start * effp->ininfo.rate / TIMERES;
    fade->in_stop =  (double)fade->in_stop * effp->ininfo.rate / TIMERES;
    fade->out_start =  (double)fade->out_start * effp->ininfo.rate / TIMERES;
    fade->out_stop = (double)fade->out_stop * effp->ininfo.rate / TIMERES;

    /* If lead-in is required it is handled as negative sample numbers */
    fade->samplesdone = (fade->in_start < 0 ? fade->in_start :0);

    fade->endpadwarned = 0;
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
void st_fade_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
int *isamp, *osamp;
{
    fade_t fade = (fade_t) effp->priv;
    /* len is total samples, chcnt counts channels */
    int len = 0, chcnt = 0, t_output = 0;
    LONG t_ibuf;

    len = ((*isamp > *osamp) ? *osamp : *isamp);

    *osamp = 0;
    *isamp = 0;

    for(; len; len--)
    {
	t_ibuf = (fade->samplesdone < 0 ? 0 : *ibuf);

	if ((fade->samplesdone >= fade->in_start) && 
	    (fade->out_stop == 0 || fade->samplesdone < fade->out_stop))
	{ /* something to generate output */
	    if (fade->samplesdone < fade->in_stop)
	    { /* fade-in phase, increase gain */
		*obuf = t_ibuf * 
		    fade_gain(fade->samplesdone - fade->in_start, 
			      fade->in_stop - fade->in_start, 
			      fade->in_fadetype);
	    } /* endif fade-in */

	    if (fade->samplesdone >= fade->in_stop && 
		(fade->out_start == 0 || fade->samplesdone < fade->out_start))
	    { /* steady gain phase */
		*obuf = t_ibuf;
	    } /* endif  steady phase */

	    if (fade->out_start != 0 && fade->samplesdone >= fade->out_start)
	    { /* fade-out phase, decrease gain */
		*obuf = t_ibuf * 
		    fade_gain(fade->out_stop - fade->samplesdone, 
			      fade->out_stop - fade->out_start, 
			      fade->out_fadetype);
	    } /* endif fade-out */

	    t_output = 1;
	}
	else
	{ /* No output generated */
	    t_output = 0;
	} /* endif something to output */

	if (fade->samplesdone >= 0 )	 
	{ /* Something to input  */
	    *isamp += 1;
	    ibuf++;
	} /* endif something accepted as input */

	if (t_output) 
	{ /* Output generated, update pointers and counters */
	    obuf++;
	    *osamp += 1;
	} /* endif t_output */

	/* Process next channel */
	chcnt++;
	if (chcnt >= effp->ininfo.channels) 
	{ /* all channels of this sample processed */
	    chcnt = 0;
	    fade->samplesdone += 1;
	} /* endif all channels */
    } /* endfor */
}

/*
 * Drain out remaining samples if the effect generates any.
 */
void st_fade_drain(effp, obuf, osamp)
eff_t effp;
LONG *obuf;
int *osamp;
{
    fade_t fade = (fade_t) effp->priv;
    int len, t_chan = 0;

    len = *osamp;
    *osamp = 0;

    if (fade->out_stop != 0 && fade->samplesdone < fade->out_stop && 
	!(fade->endpadwarned))
    { /* Warning about padding silence into end of sample */
	st_warn("Fade: warning: End time passed end-of-file. Padding with silence");
	fade->endpadwarned = 1;
    } /* endif endpadwarned */

    for (;len && (fade->out_stop != 0 && 
		  fade->samplesdone < fade->out_stop); len--)
    {
	*obuf = 0;
	obuf++;
	*osamp += 1;

	t_chan++;
	if (t_chan >= effp->ininfo.channels)
	{
	    fade->samplesdone += 1;
	    t_chan = 0;
	} /* endif channels */
    } /* endfor */
}

/*
 * Do anything required when you stop reading samples.  
 *	(free allocated memory, etc.)
 */
void st_fade_stop(effp)
eff_t effp;
{
	/* nothing to do */
}

/* Function returns gain value 0.0 - 1.0 according index / range ratio
* and -1.0 if  type is invalid 
* todo: to optimize performance calculate gain every now and then and interpolate */
static double fade_gain(index, range, type)
ULONG index, range;
char type;
{
    double retval = 0.0, findex = 0.0;

    findex = 1.0 * index / range;

    /* todo: are these really needed */
    findex = (findex < 0 ? 0.0 : findex);
    findex = (findex > 1.0 ? 1.0 : findex);

    switch (type)
    {
	case FADE_TRI : /* triangle  */
	    retval = findex;
	    break;

	case FADE_QUARTER : /*  quarter of sinewave */
	    retval = sin(findex * M_PI / 2);
	    break;

	case FADE_HALF : /* half of sinewave... eh cosine wave */
	    retval = (1 - cos(findex * M_PI )) / 2 ;
	    break;

	case FADE_LOG : /* logaritmic */
	    /* 5 means 100 db attenuation. */
	    /* todo: should this be adopted with bit depth 	  */
	    retval =  pow(0.1, (1 - findex) * 5);
	    break;

	case FADE_PAR : /* inverted parabola */
	    retval = (1 - (1 - findex)  * (1 - findex));
	    break;

	    /* todo: more fade curves? */
	default : /* Error indicating wrong fade curve */
	    retval = -1.0;
	    break;
    } /* endswitch */

    return retval;
}
