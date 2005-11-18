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

/* Fade curves */
#define FADE_QUARTER    'q'     /* Quarter of sine wave, 0 to pi/2 */
#define FADE_HALF       'h'     /* Half of sine wave, pi/2 to 1.5 * pi
                                * scaled so that -1 means no output
                                * and 1 means 0 db attenuation. */
#define FADE_LOG        'l'     /* Logarithmic curve. Fades -100 db
                                * in given time. */
#define FADE_TRI        't'     /* Linear slope. */
#define FADE_PAR        'p'     /* Inverted parabola. */

#include <math.h>
#include <string.h>
#include "st_i.h"

/* Private data for fade file */
typedef struct fadestuff
{ /* These are measured as samples */
    st_size_t in_start,  in_stop, out_start, out_stop, samplesdone;
    char *in_stop_str, *out_start_str, *out_stop_str;
    char in_fadetype, out_fadetype;
    char do_out;
    int endpadwarned;
} *fade_t;

#define FADE_USAGE "Usage: fade [ type ] fade-in-length [ stop-time [ fade-out-length ] ]\nTime is in hh:mm:ss.frac format.\nFade type one of q, h, t, l or p.\n"

/* prototypes */
static double fade_gain(st_size_t index, st_size_t range, char fadetype);

/*
 * Process options
 *
 * Don't do initialization now.
 * The 'info' fields are not yet filled in.
 */

int st_fade_getopts(eff_t effp, int n, char **argv)
{

    fade_t fade = (fade_t) effp->priv;
    char t_char[2];
    int t_argno;

    if (n < 1 || n > 4)
    { /* Wrong number of arguments. */
        st_fail(FADE_USAGE);
        return(ST_EOF);
    }

    /* because sample rate is unavailable at this point we store the
     * string off for later computations.
     */

    if (sscanf(argv[0], "%1[qhltp]", t_char))
    {
        fade->in_fadetype = *t_char;
        fade->out_fadetype = *t_char;

        argv++;
        n--;
    }
    else
    {
        /* No type given. */
        fade->in_fadetype = 'l';
        fade->out_fadetype = 'l';
    }

    fade->in_stop_str = (char *)malloc(strlen(argv[0])+1);
    if (!fade->in_stop_str)
    {
        st_fail("Could not allocate memory");
        return (ST_EOF);
    }
    strcpy(fade->in_stop_str,argv[0]);
    /* Do a dummy parse to see if it will fail */
    if (st_parsesamples(0, fade->in_stop_str, &fade->in_stop, 't') !=
            ST_SUCCESS)
    {
        st_fail(FADE_USAGE);
        return(ST_EOF);
    }

    fade->out_start_str = fade->out_stop_str = 0;

    for (t_argno = 1; t_argno < n && t_argno < 3; t_argno++)
    {
        /* See if there is fade-in/fade-out times/curves specified. */
        if(t_argno == 1)
        {
            fade->out_stop_str = (char *)malloc(strlen(argv[t_argno])+1);
            if (!fade->out_stop_str)
            {
                st_fail("Could not allocate memory");
                return (ST_EOF);
            }
             strcpy(fade->out_stop_str,argv[t_argno]);

             /* Do a dummy parse to see if it will fail */
             if (st_parsesamples(0, fade->out_stop_str, 
                         &fade->out_stop, 't') != ST_SUCCESS)
             {
                 st_fail(FADE_USAGE);
                 return(ST_EOF);
             }
        }
        else
        {
            fade->out_start_str = (char *)malloc(strlen(argv[t_argno])+1);
            if (!fade->out_start_str)
            {
                st_fail("Could not allocate memory");
                return (ST_EOF);
            }
             strcpy(fade->out_start_str,argv[t_argno]);

             /* Do a dummy parse to see if it will fail */
             if (st_parsesamples(0, fade->out_start_str, 
                         &fade->out_start, 't') != ST_SUCCESS)
             {
                 st_fail(FADE_USAGE);
                 return(ST_EOF);
             }
        }
    } /* End for(t_argno) */

    return(ST_SUCCESS);
}

/*
 * Prepare processing.
 * Do all initializations.
 */
int st_fade_start(eff_t effp)
{
    fade_t fade = (fade_t) effp->priv;

    /* converting time values to samples */
    fade->in_start = 0;
    if (st_parsesamples(effp->ininfo.rate, fade->in_stop_str,
                        &fade->in_stop, 't') != ST_SUCCESS)
    {
        st_fail(FADE_USAGE);
        return(ST_EOF);
    }

    fade->do_out = 0;
    /* See if user specified a stop time */
    if (fade->out_stop_str)
    {
        fade->do_out = 1;
        if (st_parsesamples(effp->ininfo.rate, fade->out_stop_str,
                            &fade->out_stop, 't') != ST_SUCCESS)
        {
            st_fail(FADE_USAGE);
            return(ST_EOF);
        }

        /* See if user wants to fade out. */
        if (fade->out_start_str)
        {
            if (st_parsesamples(effp->ininfo.rate, fade->out_start_str,
                        &fade->out_start, 't') != ST_SUCCESS)
            {
                st_fail(FADE_USAGE);
                return(ST_EOF);
            }
            /* Fade time is relative to stop time. */
            fade->out_start = fade->out_stop - fade->out_start;

        }
        else
            /* If user doesn't specify fade out length then
             * use same length as input side.  This is stored
             * in in_stop.
             */
            fade->out_start = fade->out_stop - fade->in_stop;
    }
    else
        /* If not specified then user wants to process all 
         * of file.  Use a value of zero to indicate this.
         */
        fade->out_stop = 0;

    /* Sanity check for fade times vs total time */
    if (fade->in_stop > fade->out_start && fade->out_start != 0)
    { /* Fades too long */
        st_fail("Fade: End of fade-in should not happen before beginning of fade-out");
        return(ST_EOF);
    } /* endif fade time sanity */

    /* If lead-in is required it is handled as negative sample numbers */
    fade->samplesdone = (fade->in_start < 0 ? fade->in_start :0);

    fade->endpadwarned = 0;

    /* fprintf(stderr, "fade: in_start = %d in_stop = %d out_start = %d out_stop = %d\n", fade->in_start, fade->in_stop, fade->out_start, fade->out_stop); */

    return(ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
int st_fade_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                 st_size_t *isamp, st_size_t *osamp)
{
    fade_t fade = (fade_t) effp->priv;
    /* len is total samples, chcnt counts channels */
    int len = 0, chcnt = 0, t_output = 1, more_output = 1;
    st_sample_t t_ibuf;

    len = ((*isamp > *osamp) ? *osamp : *isamp);

    *osamp = 0;
    *isamp = 0;

    for(; len && more_output; len--)
    {
        t_ibuf = (fade->samplesdone < 0 ? 0 : *ibuf);

        if ((fade->samplesdone >= fade->in_start) &&
            (!fade->do_out || fade->samplesdone < fade->out_stop))
        { /* something to generate output */

            if (fade->samplesdone < fade->in_stop)
            { /* fade-in phase, increase gain */
                *obuf = t_ibuf *
                    fade_gain(fade->samplesdone - fade->in_start,
                              fade->in_stop - fade->in_start,
                              fade->in_fadetype);
            } /* endif fade-in */
            else if (!fade->do_out || fade->samplesdone < fade->out_start)
            { /* steady gain phase */
                *obuf = t_ibuf;
            } /* endif  steady phase */
            else
            { /* fade-out phase, decrease gain */
                *obuf = t_ibuf *
                    fade_gain(fade->out_stop - fade->samplesdone,
                              fade->out_stop - fade->out_start,
                              fade->out_fadetype);
            } /* endif fade-out */

            if (!(!fade->do_out || fade->samplesdone < fade->out_stop))
                more_output = 0;

            t_output = 1;
        }
        else
        { /* No output generated */
            t_output = 0;
        } /* endif something to output */

        /* samplesdone < 0 means we are inventing samples right now
         * and so not consuming (happens when in_start < 0).
         */
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

    /* If not more samples will be returned, let application know
     * this.
     */
    if (fade->do_out && fade->samplesdone >= fade->out_stop)
        return ST_EOF;
    else
        return ST_SUCCESS;
}

/*
 * Drain out remaining samples if the effect generates any.
 */
int st_fade_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
    fade_t fade = (fade_t) effp->priv;
    int len, t_chan = 0;

    len = *osamp;
    *osamp = 0;

    if (fade->do_out && fade->samplesdone < fade->out_stop &&
        !(fade->endpadwarned))
    { /* Warning about padding silence into end of sample */
        st_warn("Fade: warning: End time passed end-of-file. Padding with silence");
        fade->endpadwarned = 1;
    } /* endif endpadwarned */

    for (;len && (fade->do_out &&
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

    if (fade->do_out && fade->samplesdone >= fade->out_stop)
        return ST_EOF;
    else
        return ST_SUCCESS;
}

/*
 * Do anything required when you stop reading samples.
 *      (free allocated memory, etc.)
 */
int st_fade_stop(eff_t effp)
{
    fade_t fade = (fade_t) effp->priv;

    if (fade->in_stop_str)
        free(fade->in_stop_str);
    if (fade->out_start_str)
        free(fade->out_start_str);
    if (fade->out_stop_str)
        free(fade->out_stop_str);
    return (ST_SUCCESS);
}

/* Function returns gain value 0.0 - 1.0 according index / range ratio
* and -1.0 if  type is invalid
* todo: to optimize performance calculate gain every now and then and interpolate */
static double fade_gain(st_size_t index, st_size_t range, char type)
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
            /* todo: should this be adopted with bit depth        */
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
