/* Ari Moisio <armoi@sci.fi> Aug 29 2000, based on skeleton effect
 * Written by Chris Bagwell (cbagwell@sprynet.com) - March 16, 1999
 *
 * Copyright 1999 Chris Bagwell And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Chris Bagwell And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"

/* Fade curves */
#define FADE_QUARTER    'q'     /* Quarter of sine wave, 0 to pi/2 */
#define FADE_HALF       'h'     /* Half of sine wave, pi/2 to 1.5 * pi
                                * scaled so that -1 means no output
                                * and 1 means 0 db attenuation. */
#define FADE_LOG        'l'     /* Logarithmic curve. Fades -100 db
                                * in given time. */
#define FADE_TRI        't'     /* Linear slope. */
#define FADE_PAR        'p'     /* Inverted parabola. */

#include <string.h>

/* Private data for fade file */
typedef struct { /* These are measured as samples */
    size_t in_start, in_stop, out_start, out_stop, samplesdone;
    char *in_stop_str, *out_start_str, *out_stop_str;
    char in_fadetype, out_fadetype;
    char do_out;
    int endpadwarned;
} priv_t;

/* prototypes */
static double fade_gain(size_t index, size_t range, int fadetype);

/*
 * Process options
 *
 * Don't do initialization now.
 * The 'info' fields are not yet filled in.
 */

static int sox_fade_getopts(sox_effect_t * effp, int n, char **argv)
{

    priv_t * fade = (priv_t *) effp->priv;
    char t_char[2];
    int t_argno;

    if (n < 1 || n > 4)
         return lsx_usage(effp);

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

    fade->in_stop_str = lsx_malloc(strlen(argv[0])+1);
    strcpy(fade->in_stop_str,argv[0]);
    /* Do a dummy parse to see if it will fail */
    if (lsx_parsesamples(0., fade->in_stop_str, &fade->in_stop, 't') == NULL)
      return lsx_usage(effp);

    fade->out_start_str = fade->out_stop_str = 0;

    for (t_argno = 1; t_argno < n && t_argno < 3; t_argno++)
    {
        /* See if there is fade-in/fade-out times/curves specified. */
        if(t_argno == 1)
        {
            fade->out_stop_str = lsx_malloc(strlen(argv[t_argno])+1);
            strcpy(fade->out_stop_str,argv[t_argno]);

            /* Do a dummy parse to see if it will fail */
            if (lsx_parsesamples(0., fade->out_stop_str,
                                &fade->out_stop, 't') == NULL)
              return lsx_usage(effp);
        }
        else
        {
            fade->out_start_str = lsx_malloc(strlen(argv[t_argno])+1);
            strcpy(fade->out_start_str,argv[t_argno]);

            /* Do a dummy parse to see if it will fail */
            if (lsx_parsesamples(0., fade->out_start_str,
                                &fade->out_start, 't') == NULL)
              return lsx_usage(effp);
        }
    } /* End for(t_argno) */

    return(SOX_SUCCESS);
}

/*
 * Prepare processing.
 * Do all initializations.
 */
static int sox_fade_start(sox_effect_t * effp)
{
    priv_t * fade = (priv_t *) effp->priv;

    /* converting time values to samples */
    fade->in_start = 0;
    if (lsx_parsesamples(effp->in_signal.rate, fade->in_stop_str,
                        &fade->in_stop, 't') == NULL)
      return lsx_usage(effp);

    fade->do_out = 0;
    /* See if user specified a stop time */
    if (fade->out_stop_str)
    {
        fade->do_out = 1;
        if (lsx_parsesamples(effp->in_signal.rate, fade->out_stop_str,
                            &fade->out_stop, 't') == NULL)
          return lsx_usage(effp);

        if (!fade->out_stop) {
          fade->out_stop = effp->in_signal.length / effp->in_signal.channels;
          if (!fade->out_stop) {
            lsx_fail("cannot fade out: audio length is neither known nor given");
            return SOX_EOF;
          }
        }

        /* See if user wants to fade out. */
        if (fade->out_start_str)
        {
            if (lsx_parsesamples(effp->in_signal.rate, fade->out_start_str,
                        &fade->out_start, 't') == NULL)
              return lsx_usage(effp);
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
        lsx_fail("Fade: End of fade-in should not happen before beginning of fade-out");
        return(SOX_EOF);
    } /* endif fade time sanity */

    fade->samplesdone = fade->in_start;
    fade->endpadwarned = 0;

    lsx_debug("fade: in_start = %lu in_stop = %lu out_start = %lu out_stop = %lu", (unsigned long)fade->in_start, (unsigned long)fade->in_stop, (unsigned long)fade->out_start, (unsigned long)fade->out_stop);

    if (fade->in_start == fade->in_stop && fade->out_start == fade->out_stop)
      return SOX_EFF_NULL;

    return SOX_SUCCESS;
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
static int sox_fade_flow(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf,
                 size_t *isamp, size_t *osamp)
{
    priv_t * fade = (priv_t *) effp->priv;
    /* len is total samples, chcnt counts channels */
    int len = 0, t_output = 1, more_output = 1;
    sox_sample_t t_ibuf;
    size_t chcnt = 0;

    len = ((*isamp > *osamp) ? *osamp : *isamp);

    *osamp = 0;
    *isamp = 0;

    for(; len && more_output; len--)
    {
        t_ibuf = *ibuf;

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

        *isamp += 1;
        ibuf++;

        if (t_output)
        { /* Output generated, update pointers and counters */
            obuf++;
            *osamp += 1;
        } /* endif t_output */

        /* Process next channel */
        chcnt++;
        if (chcnt >= effp->in_signal.channels)
        { /* all channels of this sample processed */
            chcnt = 0;
            fade->samplesdone += 1;
        } /* endif all channels */
    } /* endfor */

    /* If not more samples will be returned, let application know
     * this.
     */
    if (fade->do_out && fade->samplesdone >= fade->out_stop)
        return SOX_EOF;
    else
        return SOX_SUCCESS;
}

/*
 * Drain out remaining samples if the effect generates any.
 */
static int sox_fade_drain(sox_effect_t * effp, sox_sample_t *obuf, size_t *osamp)
{
    priv_t * fade = (priv_t *) effp->priv;
    int len;
    size_t t_chan = 0;

    len = *osamp;
    *osamp = 0;

    if (fade->do_out && fade->samplesdone < fade->out_stop &&
        !(fade->endpadwarned))
    { /* Warning about padding silence into end of sample */
        lsx_warn("Fade: warning: End time passed end-of-file. Padding with silence");
        fade->endpadwarned = 1;
    } /* endif endpadwarned */

    for (;len && (fade->do_out &&
                  fade->samplesdone < fade->out_stop); len--)
    {
        *obuf = 0;
        obuf++;
        *osamp += 1;

        t_chan++;
        if (t_chan >= effp->in_signal.channels)
        {
            fade->samplesdone += 1;
            t_chan = 0;
        } /* endif channels */
    } /* endfor */

    if (fade->do_out && fade->samplesdone >= fade->out_stop)
        return SOX_EOF;
    else
        return SOX_SUCCESS;
}

/*
 * Do anything required when you stop reading samples.
 *      (free allocated memory, etc.)
 */
static int kill(sox_effect_t * effp)
{
    priv_t * fade = (priv_t *) effp->priv;

    free(fade->in_stop_str);
    free(fade->out_start_str);
    free(fade->out_stop_str);
    return (SOX_SUCCESS);
}

/* Function returns gain value 0.0 - 1.0 according index / range ratio
* and -1.0 if  type is invalid
* todo: to optimize performance calculate gain every now and then and interpolate */
static double fade_gain(size_t index, size_t range, int type)
{
    double retval = 0.0, findex = 0.0;

    /* TODO: does it really have to be contrained to [0.0, 1.0]? */
    findex = max(0.0, min(1.0, 1.0 * index / range));

    switch (type) {
    case FADE_TRI :             /* triangle */
      retval = findex;
      break;

    case FADE_QUARTER :         /* quarter of sinewave */
      retval = sin(findex * M_PI / 2);
      break;

    case FADE_HALF :          /* half of sinewave... eh cosine wave */
      retval = (1 - cos(findex * M_PI )) / 2 ;
      break;

    case FADE_LOG :             /* logarithmic */
      /* 5 means 100 db attenuation. */
      /* TODO: should this be adopted with bit depth */
      retval =  pow(0.1, (1 - findex) * 5);
      break;

    case FADE_PAR :             /* inverted parabola */
      retval = (1 - (1 - findex)  * (1 - findex));
      break;

    /* TODO: more fade curves? */
    default :                  /* Error indicating wrong fade curve */
      retval = -1.0;
      break;
    }

    return retval;
}

static sox_effect_handler_t sox_fade_effect = {
  "fade",
  "[ type ] fade-in-length [ stop-time [ fade-out-length ] ]\n"
  "       Time is in hh:mm:ss.frac format.\n"
  "       Fade type one of q, h, t, l or p.",
  SOX_EFF_MCHAN,
  sox_fade_getopts,
  sox_fade_start,
  sox_fade_flow,
  sox_fade_drain,
  NULL,
  kill, sizeof(priv_t)
};

const sox_effect_handler_t *sox_fade_effect_fn(void)
{
    return &sox_fade_effect;
}
