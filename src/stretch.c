/*
 * (c) march/april 2000 Fabien COELHO <fabien@coelho.net> for sox.
 *
 * Basic time stretcher.
 * cross fade samples so as to go slower of faster.
 *
 * The automaton is based on 6 parameters:
 * - stretch factor f
 * - window size w
 * - input step i
 *   output step o=f*i
 * - steady state of window s, ss = s*w
 * - type of cross fading
 *
 * I decided of the default values of these parameters based
 * on some small non extensive tests. maybe better defaults
 * can be suggested.
 * 
 * It cannot handle different number of channels.
 * It cannot handle rate change.
 */
#include "st_i.h"

#include <stdlib.h> /* malloc and free */
#include <string.h> /* memcpy() */

#ifndef MIN
#define MIN(s1,s2) ((s1)<(s2)?(s1):(s2))
#endif

#ifndef STRETCH_FLOAT
#define STRETCH_FLOAT float
#define STRETCH_FLOAT_SCAN "%f"
#endif

#define STRETCH_USAGE \
    "Usage: stretch factor [window fade shift fading]\n" \
    "\t(expansion, frame in ms, lin/..., unit<1.0, unit<0.5)\n" \
    "\t(defaults: 1.0 20 lin ...)"

/* ok, it looks stupid to have such constant.
   this is because of the cast, if floats are switched to doubles.
 */
#define ZERO                            ((STRETCH_FLOAT)(0.0e0))
#define HALF                            ((STRETCH_FLOAT)(0.5e0))
#define ONE                             ((STRETCH_FLOAT)(1.0e0))
#define MONE                            ((STRETCH_FLOAT)(-1.0e0))
#define ONETHOUSANDS                    ((STRETCH_FLOAT)(0.001e0))

#define DEFAULT_SLOW_SHIFT_RATIO        ((STRETCH_FLOAT)(0.8e0))
#define DEFAULT_FAST_SHIFT_RATIO        ONE

#define DEFAULT_STRETCH_WINDOW          ((STRETCH_FLOAT)(20.0e0))  /* ms */

/* I'm planing to put some common fading stuff outside. 
   It's also used in pitch.c
 */
typedef enum { st_linear_fading } st_fading_t;

#define DEFAULT_FADING st_linear_fading

typedef enum { input_state, output_state } stretch_status_t;

typedef struct 
{
    /* options
     * Q: maybe shift could be allowed > 1.0 with factor < 1.0 ???
     */
    STRETCH_FLOAT factor;   /* strech factor. 1.0 means copy. */
    STRETCH_FLOAT window;   /* window in ms */
    st_fading_t fade;       /* type of fading */
    STRETCH_FLOAT shift;    /* shift ratio wrt window. <1.0 */
    STRETCH_FLOAT fading;   /* fading ratio wrt window. <0.5 */

    /* internal stuff 
     */
    stretch_status_t state; /* automaton status */
    int clipped;            /* number of clipped values. */

    int size;               /* buffer size */
    int index;              /* next available element */
    st_sample_t *ibuf;      /* input buffer */
    int ishift;             /* input shift */

    int oindex;             /* next evailable element */
    STRETCH_FLOAT * obuf;   /* output buffer */
    int oshift;             /* output shift */

    int fsize;              /* fading size */
    STRETCH_FLOAT * fbuf;   /* fading, 1.0 -> 0.0 */

} * stretch_t;

/*
static void debug(stretch_t s, char * where)
{
    fprintf(stderr, 
            "%s: (f=%.2f w=%.2f r=%.2f f=%.2f)"
            " st=%d s=%d ii=%d is=%d oi=%d os=%d fs=%d\n",
            where, s->factor, s->window, s->shift, s->fading,
            s->state, s->size, s->index, s->ishift,
            s->oindex, s->oshift, s->fsize);
}
*/

/* clip amplitudes and count number of clipped values.
 */
static st_sample_t clip(stretch_t stretch, STRETCH_FLOAT v)
{
    if (v < ST_SAMPLE_MIN)
    {
        stretch->clipped++;
        return ST_SAMPLE_MIN;
    }
    else if (v > ST_SAMPLE_MAX)
    {
        stretch->clipped++;
        return ST_SAMPLE_MAX;
    }
    else
    {
        return (st_sample_t) v;
    }
}

/*
 * Process options
 */
int st_stretch_getopts(eff_t effp, int n, char **argv) 
{
    stretch_t stretch = (stretch_t) effp->priv; 
    
    /* default options */
    stretch->factor = ONE; /* default is no change */
    stretch->window = DEFAULT_STRETCH_WINDOW;
    stretch->fade   = st_linear_fading;

    if (n>0 && !sscanf(argv[0], STRETCH_FLOAT_SCAN, &stretch->factor))
    {
        st_fail(STRETCH_USAGE "\n\terror while parsing factor");
        return ST_EOF;
    }

    if (n>1 && !sscanf(argv[1], STRETCH_FLOAT_SCAN, &stretch->window))
    {
        st_fail(STRETCH_USAGE "\n\terror while parsing window size");
        return ST_EOF;
    }

    if (n>2) 
    {
        switch (argv[2][0])
        {
        case 'l':
        case 'L':
            stretch->fade = st_linear_fading;
            break;
        default:
            st_fail(STRETCH_USAGE "\n\terror while parsing fade type");
            return ST_EOF;
        }
    }

    /* default shift depends whether we go slower or faster */
    stretch->shift = (stretch->factor <= ONE) ?
        DEFAULT_FAST_SHIFT_RATIO: DEFAULT_SLOW_SHIFT_RATIO;
 
    if (n>3 && !sscanf(argv[3], STRETCH_FLOAT_SCAN, &stretch->shift))
    {
        st_fail(STRETCH_USAGE "\n\terror while parsing shift ratio");
        return ST_EOF;
    }

    if (stretch->shift > ONE || stretch->shift <= ZERO)
    {
        st_fail(STRETCH_USAGE "\n\terror with shift ratio value");
        return ST_EOF;
    }

    /* default fading stuff... 
       it makes sense for factor >= 0.5
    */
    if (stretch->factor<ONE)
        stretch->fading = ONE - (stretch->factor*stretch->shift);
    else
        stretch->fading = ONE - stretch->shift;
    if (stretch->fading > HALF) stretch->fading = HALF;

    if (n>4 && !sscanf(argv[4], STRETCH_FLOAT_SCAN, &stretch->fading))
    {
        st_fail(STRETCH_USAGE "\n\terror while parsing fading ratio");
        return ST_EOF;
    }

    if (stretch->fading > HALF || stretch->fading < ZERO)
    {
        st_fail(STRETCH_USAGE "\n\terror with fading ratio value");
        return ST_EOF;
    }

    return ST_SUCCESS;
}

/*
 * Start processing
 */
int st_stretch_start(eff_t effp)
{
    stretch_t stretch = (stretch_t) effp->priv;
    register int i;

    /* not necessary. taken care by effect processing? */
    if (effp->outinfo.channels != effp->ininfo.channels)
    {
        st_fail("STRETCH cannot handle different channels (in=%d, out=%d)"
             " use avg or pan", effp->ininfo.channels, effp->outinfo.channels);
        return ST_EOF;
    }

    if (effp->outinfo.rate != effp->ininfo.rate)
    {
        st_fail("STRETCH cannot handle different rates (in=%ld, out=%ld)"
             " use resample or rate", effp->ininfo.rate, effp->outinfo.rate);
        return ST_EOF;
    }

    stretch->state = input_state;
    stretch->clipped = 0;

    stretch->size = (int)(effp->outinfo.rate * ONETHOUSANDS * stretch->window);
    /* start in the middle of an input to avoid initial fading... */
    stretch->index = stretch->size/2;
    stretch->ibuf  = (st_sample_t *) malloc(stretch->size * 
                                            sizeof(st_sample_t));

    /* the shift ratio deal with the longest of ishift/oshift
       hence ishift<=size and oshift<=size. should be asserted.
     */
    if (stretch->factor < ONE)
    {
        stretch->ishift = (int) (stretch->shift * stretch->size);
        stretch->oshift = (int) (stretch->factor * stretch->ishift);
    }
    else
    {
        stretch->oshift = (int) (stretch->shift * stretch->size);
        stretch->ishift = (int) (stretch->oshift / stretch->factor);
    }

    stretch->oindex = stretch->index; /* start as synchronized */
    stretch->obuf = (STRETCH_FLOAT *)
        malloc(stretch->size * sizeof(STRETCH_FLOAT));
    
    stretch->fsize = (int) (stretch->fading * stretch->size);

    stretch->fbuf = (STRETCH_FLOAT *)
        malloc(stretch->fsize * sizeof(STRETCH_FLOAT));
        
    if (!stretch->ibuf || !stretch->obuf || !stretch->fbuf) 
    {
        st_fail("some malloc failed");
        return ST_EOF;
    }

    /* initialize buffers
     */
    for (i=0; i<stretch->size; i++)
        stretch->ibuf[i] = 0;

    for (i=0; i<stretch->size; i++)
        stretch->obuf[i] = ZERO;

    if (stretch->fsize>1)
    {
        register STRETCH_FLOAT slope = ONE / (stretch->fsize - 1);
        
        stretch->fbuf[0] = ONE;
        for (i=1; i<stretch->fsize-1; i++)
            stretch->fbuf[i] = slope * (stretch->fsize-i-1);
        stretch->fbuf[stretch->fsize-1] = ZERO;
    } else if (stretch->fsize==1)
        stretch->fbuf[0] = ONE;

    /* debug(stretch, "start"); */

    return ST_SUCCESS;
}

/* accumulates input ibuf to output obuf with fading fbuf
 */
static void combine(stretch_t stretch)
{
    register int i, size, fsize;

    size = stretch->size;
    fsize = stretch->fsize;

    /* fade in */
    for (i=0; i<fsize; i++)
        stretch->obuf[i] += stretch->fbuf[fsize-i-1]*stretch->ibuf[i];

    /* steady state */
    for (; i<size-fsize; i++)
        stretch->obuf[i] += stretch->ibuf[i];

    /* fade out */
    for (; i<size; i++)
        stretch->obuf[i] += stretch->fbuf[i-size+fsize]*stretch->ibuf[i];
}

/*
 * Processes flow.
 */
int st_stretch_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                    st_size_t *isamp, st_size_t *osamp)
{
    stretch_t stretch = (stretch_t) effp->priv;
    st_size_t iindex, oindex;
    int i;

    iindex = 0;
    oindex = 0;

    while (iindex<*isamp && oindex<*osamp)
    {
        if (stretch->state == input_state)
        {
            st_size_t tocopy = MIN(*isamp-iindex, 
                                   stretch->size-stretch->index);

            memcpy(stretch->ibuf+stretch->index, 
                   ibuf+iindex, tocopy*sizeof(st_sample_t));

            iindex += tocopy;
            stretch->index += tocopy; 

            if (stretch->index == stretch->size)
            {
                /* compute */
                combine(stretch);

                /* shift input */
                for (i=0; i+stretch->ishift<stretch->size; i++)
                    stretch->ibuf[i] = stretch->ibuf[i+stretch->ishift];

                stretch->index -= stretch->ishift;

                /* switch to output state */
                stretch->state = output_state;
            }
        }

        if (stretch->state == output_state)
        {
            while (stretch->oindex<stretch->oshift && oindex<*osamp)
                obuf[oindex++] = 
                    clip(stretch, stretch->obuf[stretch->oindex++]);

            if (stretch->oindex >= stretch->oshift && oindex<*osamp)
            {
                stretch->oindex -= stretch->oshift;

                /* shift internal output buffer */
                for (i=0; i+stretch->oshift<stretch->size; i++)
                    stretch->obuf[i] = stretch->obuf[i+stretch->oshift];

                /* pad with 0 */
                for (; i<stretch->size; i++)
                    stretch->obuf[i] = ZERO;
                    
                stretch->state = input_state;
            }
        }
    }

    *isamp = iindex;
    *osamp = oindex;

    return ST_SUCCESS;
}


/*
 * Drain buffer at the end
 * maybe not correct ? end might be artificially faded?
 */
int st_stretch_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
    stretch_t stretch = (stretch_t) effp->priv;
    register int i;
    st_size_t oindex;

    oindex = 0;

    if (stretch->state == input_state)
    {
        for (i=stretch->index; i<stretch->size; i++)
            stretch->ibuf[i] = 0;

        combine(stretch);
        
        stretch->state = output_state;
    }

    if (stretch->state == output_state)
    {
        for (; oindex<*osamp && stretch->oindex<stretch->index;)
            obuf[oindex++] = clip(stretch, stretch->obuf[stretch->oindex++]);
    }
    
    *osamp = oindex;

    return ST_SUCCESS;
}


/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_stretch_stop(eff_t effp)
{
    stretch_t stretch = (stretch_t) effp->priv;

    free(stretch->ibuf);
    free(stretch->obuf);
    free(stretch->fbuf);

    if (stretch->clipped)
        st_warn("STRETCH clipped %d values...", stretch->clipped);

    return ST_SUCCESS;
}
