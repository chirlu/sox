/*
 * (c) Fabien Coelho <fabien@coelho.net> 03/2000 for sox. see sox copyright.
 *
 * pitch shifting.
 *
 * I found a code on the Computer Music Journal web site
 * <http://mitpress.mit.edu/e-journals/Computer_Music_Journal/>
 * for pitch shifting the AD 1848 PC soundcards, with
 * a lot of (unclear) pointer and integer arithmetics, and 
 * combine effects (feedback, delay, mixing).
 *
 * I tried to understand the code, dropped the other effects,
 * translated the stuff in float so it's easier to understand, 
 * drop one of the lookup tables (I know that sin(pi/2-x) = cos(x)), 
 * and added interpolation and fade options of my own.
 * cross fading is always symetric.
 *
 * Basically, the algorithm performs a resampling at the desire rate
 * to achieve the shift (interpolation function) on small overlapping windows,
 * and successive windows are faded in/out one into the other to
 * rebuild the final signal.
 *
 * I'm quite disappointed. At first thought, I looked for an FT-based
 * algorithm, something like "switch the signal to frequencies, shift
 * frequencies, and come back to time", but it does not seem to work
 * that way... at least not so easily. Or maybe my attempt was buggy.
 *
 * Here is the result. It can certainly be improved. 
 * The result buzzes some time.
 * Lot of options available so than one can adjust the result.
 *
 * so as to lower the pitch, a larger window sounds better (30ms)?
 * so as to upper the pitch, a smaller window... (10ms)?
 *
 * Some speed-optimization could be added at code size expanse/expense?
 */

#include "st_i.h"

#include <stdlib.h> /* malloc(), free() */
#include <string.h> /* memcpy() */

#include <math.h>   /* cos(), pow() */

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

/* float type for the computations.
   should be common to all effects?
   I use such trick in vol, pan, speed, pitch and stretch...
 */
#ifndef PITCH_FLOAT
#define PITCH_FLOAT double
#define PITCH_FLOAT_SCAN "%lf"
#endif

#define PITCH_USAGE \
    "Usage: pitch shift width interpole fade" \
    " (in cents, in ms, cub/lin, cos/ham/lin/trap)" \
    " (defaults: 0 20 c c)"

/* cross fading options for transitions
 */
#define PITCH_FADE_COS 0 /* cosine */
#define PITCH_FADE_HAM 1 /* Hamming */
#define PITCH_FADE_LIN 2 /* linear */
#define PITCH_FADE_TRA 3 /* trapezoid */

#define PITCH_FADE_DEFAULT PITCH_FADE_COS

/* interpolation options
 */
#define PITCH_INTERPOLE_CUB 0 /* cubic */
#define PITCH_INTERPOLE_LIN 1 /* linear */

#define PITCH_INTERPOLE_DEFAULT PITCH_INTERPOLE_CUB

/* default window width
 */
#define PITCH_DEFAULT_WIDTH ((PITCH_FLOAT)(20.0e0)) /* 20 ms */

/* constants.
 */
#define OCTAVA             ((PITCH_FLOAT)(1200.0e0)) /* in cents */
#define THOUSAND           ((PITCH_FLOAT)(1000.0e0)) /* in units */
#define HUNDRED            ((PITCH_FLOAT)(100.0e0))
#define FOUR               ((PITCH_FLOAT)(4.0e0))
#define TWO                ((PITCH_FLOAT)(2.0e0))
#define ONE                ((PITCH_FLOAT)(1.0e0))
#define HALF               ((PITCH_FLOAT)(0.5e0))
#define QUARTER            ((PITCH_FLOAT)(0.25e0))
#define ONESIXTH           ((PITCH_FLOAT)(1.0e0/6.0e0))
#define ZERO               ((PITCH_FLOAT)(0.0e0))

/* linear factors for the Hamming window
   0<=i<=n: HAM_n(i) = HAM0 + HAM1*cos(i*PI/n)
 */
#define HAM1               ((PITCH_FLOAT)(0.46e0))
#define HAM0               ((PITCH_FLOAT)(0.54e0))

/* state of buffer management... */
typedef enum { pi_input, pi_compute, pi_output } pitch_state_t;

/* structure hold by the effect descriptor. */
typedef struct 
{
    /* OPTIONS
     */
    PITCH_FLOAT shift;   /* shift in cents, >0 to treble, <0 to bass */

    PITCH_FLOAT width;   /* sweep size in ms */

    int interopt;        /* interpole option */

    int fadeopt;         /* fade option */
    PITCH_FLOAT coef;    /* coefficient used by trapezoid */
    /* what about coef1/coef2 for hamming... */

    /* COMPUTATION
     */
    PITCH_FLOAT rate;    /* sweep rate, around 1.0 */

    unsigned int step;   /* size of half a sweep, rounded to integer... */
    PITCH_FLOAT * fade;  /* fading factors table lookup, ~ 1.0 -> ~ 0.0 */

    int overlap;         /* needed overlap */

    PITCH_FLOAT * tmp;   /* temporary buffer */
    PITCH_FLOAT * acc;   /* accumulation buffer */

    unsigned int iacc;   /* part of acc already output */

    st_size_t size;      /* size of buffer for processing chunks. */
    unsigned int index;  /* index of next empty input item. */
    st_sample_t *buf;    /* bufferize input */

    pitch_state_t state; /* buffer management status. */

    int clipped;         /* number of clipped values (i.e. overflows) */

} * pitch_t;

/* // debug functions

static char * fadeoptname(int opt)
{
    switch (opt)
    {
    case PITCH_FADE_COS: return "cosine";
    case PITCH_FADE_HAM: return "hamming";
    case PITCH_FADE_LIN: return "linear";
    case PITCH_FADE_TRA: return "trapezoid";
    default: return "UNEXPECTED";
    }
}

static void debug(pitch_t pitch, char * where)
{
  fprintf(stderr, 
  "%s: ind=%d sz=%ld step=%d o=%d rate=%f ia=%d st=%d fo=%s\n", 
  where, pitch->index, pitch->size, pitch->step, pitch->overlap, 
  pitch->rate, pitch->iacc, pitch->state, fadeoptname(pitch->fadeopt));
}
*/

/* compute f(x) as a linear interpolation...
 */
static PITCH_FLOAT lin(
  PITCH_FLOAT f0,  /* f(0)  */
  PITCH_FLOAT f1,  /* f(1)  */
  PITCH_FLOAT x)   /* 0.0 <= x < 1.0 */
{
    return f0 * (ONE - x) + f1 * x;
}

/* compute f(x) as a cubic function...
 */
static PITCH_FLOAT cub(
  PITCH_FLOAT fm1, /* f(-1) */
  PITCH_FLOAT f0,  /* f(0)  */
  PITCH_FLOAT f1,  /* f(1)  */
  PITCH_FLOAT f2,  /* f(2)  */
  PITCH_FLOAT x)   /* 0.0 <= x < 1.0 */
{
    /* a x^3 + b x^2 + c x + d */
    register PITCH_FLOAT a, b, c, d;

    d = f0;
    b = HALF * (f1+fm1) - f0;
    a = ONESIXTH * (f2-f1+fm1-f0-FOUR*b);
    c = f1 - a - b - d;
    
    return ((a * x + b) * x + c) * x + d;
}

/* interpolate a quarter (half a window)
 *
 * ibuf buffer of ilen length is swept at rate speed.
 * result put in output buffer obuf of size olen.
 */
static void interpolation(
  pitch_t pitch,
  st_sample_t *ibuf, int ilen, 
  PITCH_FLOAT * out, int olen,
  PITCH_FLOAT rate) /* signed */
{
    register int i, size;
    register PITCH_FLOAT index;

    size = pitch->step; /* size == olen? */

    if (rate>0) /* sweep forwards */
    {
        for (index=ZERO, i=0; i<olen; i++, index+=rate)
        {
            register int ifl = (int) index; /* FLOOR */
            register PITCH_FLOAT frac = index - ifl;

            if (pitch->interopt==PITCH_INTERPOLE_LIN)
                out[i] = lin((PITCH_FLOAT) ibuf[ifl], 
                             (PITCH_FLOAT) ibuf[ifl+1],
                             frac);
            else
                out[i] = cub((PITCH_FLOAT) ibuf[ifl-1], 
                             (PITCH_FLOAT) ibuf[ifl], 
                             (PITCH_FLOAT) ibuf[ifl+1], 
                             (PITCH_FLOAT) ibuf[ifl+2],
                             frac);
        }
    }
    else /* rate < 0, sweep backwards */
    {
        for (index=ilen-1, i=olen-1; i>=0; i--, index+=rate)
        {
            register int ifl = (int) index; /* FLOOR */
            register PITCH_FLOAT frac = index - ifl;

            if (pitch->interopt==PITCH_INTERPOLE_LIN)
                out[i] = lin((PITCH_FLOAT) ibuf[ifl], 
                             (PITCH_FLOAT) ibuf[ifl+1],
                             frac);
            else
                out[i] = cub((PITCH_FLOAT) ibuf[ifl-1], 
                             (PITCH_FLOAT) ibuf[ifl], 
                             (PITCH_FLOAT) ibuf[ifl+1], 
                             (PITCH_FLOAT) ibuf[ifl+2],
                             frac);
        }
    }
}

/* from input buffer to acc
 */
static void process_intput_buffer(pitch_t pitch)
{
    register int i, len = pitch->step;

    /* forwards sweep */
    interpolation(pitch, 
                  pitch->buf+pitch->overlap, pitch->step+pitch->overlap, 
                  pitch->tmp, pitch->step,
                  pitch->rate);
    
    for (i=0; i<len; i++)
        pitch->acc[i] = pitch->fade[i]*pitch->tmp[i];

    /* backwards sweep */
    interpolation(pitch,
                  pitch->buf, pitch->step+pitch->overlap,
                  pitch->tmp, pitch->step,
                  -pitch->rate);
    
    for (i=0; i<len; i++)
        pitch->acc[i] += pitch->fade[pitch->step-i-1]*pitch->tmp[i];
}

static st_sample_t clip(pitch_t pitch, PITCH_FLOAT v)
{
    if (v < ST_SAMPLE_MIN)
    {
        pitch->clipped++;
        return ST_SAMPLE_MIN;
    }
    else if (v > ST_SAMPLE_MAX)
    {
        pitch->clipped++;
        return ST_SAMPLE_MAX;
    }
    else
        return (st_sample_t) v;
}

/*
 * Process options
 */
int st_pitch_getopts(eff_t effp, int n, char **argv) 
{
    pitch_t pitch = (pitch_t) effp->priv; 
    
    /* get pitch shift */
    pitch->shift = ZERO; /* default is no change */

    if (n && !sscanf(argv[0], PITCH_FLOAT_SCAN, &pitch->shift))
    {
        st_fail(PITCH_USAGE);
        return ST_EOF;
    }

    /* sweep size in ms */
    pitch->width = PITCH_DEFAULT_WIDTH;
    if (n>1 && !sscanf(argv[1], PITCH_FLOAT_SCAN, &pitch->width))
    {
        st_fail(PITCH_USAGE);
        return ST_EOF;
    }

    /* interpole option */
    pitch->interopt = PITCH_INTERPOLE_DEFAULT;
    if (n>2)
    {
        switch(argv[2][0])
        {
        case 'l':
        case 'L':
            pitch->interopt = PITCH_INTERPOLE_LIN;
            break;
        case 'c':
        case 'C':
            pitch->interopt = PITCH_INTERPOLE_CUB;
            break;
        default:
            st_fail(PITCH_USAGE);
            return ST_EOF;
        }
    }

    /* fade option */
    pitch->fadeopt = PITCH_FADE_DEFAULT; /* default */
    if (n>3) 
    {
        switch (argv[3][0]) /* what a parser;-) */
        {
        case 'l':
        case 'L':
            pitch->fadeopt = PITCH_FADE_LIN;
            break;
        case 't':
        case 'T':
            pitch->fadeopt = PITCH_FADE_TRA;
            break;
        case 'h':
        case 'H':
            pitch->fadeopt = PITCH_FADE_HAM;
            break;
        case 'c':
        case 'C':
            pitch->fadeopt = PITCH_FADE_COS;
            break;
        default:
            st_fail(PITCH_USAGE);
            return ST_EOF;
        }
    }
    
    pitch->coef = QUARTER;
    if (n>4 && (!sscanf(argv[4], PITCH_FLOAT_SCAN, &pitch->coef) ||
                pitch->coef<ZERO || pitch->coef>HALF))
    {
        st_fail(PITCH_USAGE);
        return ST_EOF;
    }

    pitch->clipped = 0;

    return ST_SUCCESS;
}

/*
 * Start processing
 */
int st_pitch_start(eff_t effp)
{
    pitch_t pitch = (pitch_t) effp->priv;
    register int sample_rate = effp->outinfo.rate;
    unsigned int i;

    /* check constraints. sox does already take care of that I guess?
     */
    if (effp->outinfo.rate != effp->ininfo.rate)
    {
        st_fail("PITCH cannot handle different rates (in=%ld, out=%ld)"
             " use resample or rate", effp->ininfo.rate, effp->outinfo.rate);
        return ST_EOF;
    }
 
    if (effp->outinfo.channels != effp->ininfo.channels)
    {
        st_fail("PITCH cannot handle different channels (in=%ld, out=%ld)"
             " use avg or pan", effp->ininfo.channels, effp->outinfo.channels);
        return ST_EOF;
    }

    /* computer inner stuff... */

    /* Should I trust pow?
     * BTW, the twelve's root of two is 1.0594630943592952645618252,
     * if we consider an equal temperament.
     */
    pitch->rate = pow(TWO, pitch->shift/OCTAVA);

    /* size is half of the actual target window size, because of symetry.
     */
    pitch->step = ((pitch->width*(HALF/THOUSAND))*sample_rate);

    /* make size odd? do we care? */
    /* if (!(size & 1)) size++; */

    /* security for safe cubic interpolation */
    if (pitch->rate > ONE)
        pitch->overlap = (int) ((pitch->rate-ONE)*pitch->step) + 2;
    else
        pitch->overlap = 2; 

    pitch->size = pitch->step + 2*pitch->overlap;

    pitch->fade = (PITCH_FLOAT *) malloc(pitch->step*sizeof(PITCH_FLOAT));
    pitch->tmp  = (PITCH_FLOAT *) malloc(pitch->step*sizeof(PITCH_FLOAT));
    pitch->acc  = (PITCH_FLOAT *) malloc(pitch->step*sizeof(PITCH_FLOAT));
    pitch->buf  = (st_sample_t *) malloc(pitch->size*sizeof(st_sample_t));

    if (!pitch->fade || !pitch->tmp || !pitch->acc || !pitch->buf)
    {
        st_fail("malloc failed in st_pitch_start");
        return ST_EOF;
    }

    pitch->index = pitch->overlap;

    /* default initial signal */
    for (i=0; i<pitch->size; i++)
        pitch->buf[i] = 0;

    if (pitch->fadeopt == PITCH_FADE_HAM)
    {
        /* does it make sense to have such an option? */
        register PITCH_FLOAT pi_step = M_PI / (pitch->step-1);
        
        for (i=0; i<pitch->step; i++)
            pitch->fade[i] = (PITCH_FLOAT) (HAM0 + HAM1*cos(pi_step*i));
    }
    else if (pitch->fadeopt == PITCH_FADE_COS)
    {
        register PITCH_FLOAT pi_2_step = M_PI_2 / (pitch->step-1);

        pitch->fade[0] = ONE; /* cos(0) == 1.0 */
        for (i=1; i<pitch->step-1; i++)
            pitch->fade[i]  = (PITCH_FLOAT) cos(pi_2_step*i);
        pitch->fade[pitch->step-1] = ZERO; /* cos(PI/2) == 0.0 */
    }
    else if (pitch->fadeopt == PITCH_FADE_LIN)
    {
        register PITCH_FLOAT stepth = ONE / (pitch->step-1);

        pitch->fade[0] = ONE;
        for (i=1; i<pitch->step-1; i++)
            pitch->fade[i] = (pitch->step-i-1) * stepth;
        pitch->fade[pitch->step-1] = ZERO;
    }
    else if (pitch->fadeopt == PITCH_FADE_TRA)
    {
        /* 0 <= coef <= 0.5 */
        register unsigned int plat = (int) (pitch->step*pitch->coef);
        register PITCH_FLOAT slope = ONE / (pitch->step - 2*plat);

        for (i=0; i<plat; i++)
            pitch->fade[i] = ONE;

        for (; i<pitch->step-plat; i++)
            pitch->fade[i] = slope * (pitch->step-plat-i-1);

        for (; i<pitch->step; i++)
            pitch->fade[i] = ZERO;
    }
    else
    {
        st_fail("unexpected PITCH_FADE parameter %d", pitch->fadeopt);
        return ST_EOF;
    }

    pitch->clipped = 0;

    return ST_SUCCESS;
}

/* Processes input.
 */
int st_pitch_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                st_size_t *isamp, st_size_t *osamp)
{
    pitch_t pitch = (pitch_t) effp->priv;
    int i, size;
    st_size_t len, iindex, oindex;

    size = pitch->size;
    /* size to process */
    len = MIN(*isamp, *osamp);
    iindex = 0;
    oindex = 0;

    /* warning:
       because of the asynchroneous nature of buffering,
       the output index can reach the buffer limits before full consumption.
       I put the input index just in case. 
       If the code is correct, eithier len or iindex is redundant.
    */
    while (len>0 && iindex<*isamp && oindex<*osamp)
    {
        if (pitch->state == pi_input)
        {
            register int tocopy = MIN(pitch->size-pitch->index, len);

            memcpy(pitch->buf+pitch->index, ibuf+iindex, tocopy*sizeof(st_sample_t));

            len -= tocopy;
            pitch->index += tocopy;
            iindex += tocopy;

            if (pitch->index==pitch->size)
                pitch->state = pi_compute;
        }

        if (pitch->state == pi_compute)
        {
            process_intput_buffer(pitch);
            pitch->state = pi_output;
            pitch->iacc = 0;
        }

        if (pitch->state == pi_output)
        {
            int toout = MIN(*osamp-oindex, pitch->step-pitch->iacc);

            for (i=0; i<toout; i++)
                obuf[oindex++] = clip(pitch, pitch->acc[pitch->iacc++]);

            if (pitch->iacc == pitch->step)
            {
                pitch->state = pi_input;

                /* shift input buffer. memmove? */
                for (i=0; i<2*pitch->overlap; i++)
                    pitch->buf[i] = pitch->buf[i+pitch->step];
                
                pitch->index = 2*pitch->overlap;
            }
        }
    }

    /* report consumption. */
    *isamp = iindex;
    *osamp = oindex;

    return ST_SUCCESS;
}

/* at the end...
 */
int st_pitch_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
    pitch_t pitch = (pitch_t) effp->priv;
    st_size_t i;

    if (pitch->state == pi_input)
    {
        /* complete input buffer content with 0. */
        for (i=pitch->index; i<pitch->size; i++)
            pitch->buf[i] = 0;

        pitch->state = pi_compute;
    }

    if (pitch->state == pi_compute)
    {
        process_intput_buffer(pitch);
        pitch->state = pi_output;
        pitch->iacc = 0;
    }

    /* (pitch->state == pi_output) */
    for (i=0; i<*osamp && i<pitch->index-pitch->overlap;)
        obuf[i++] = clip(pitch, pitch->acc[pitch->iacc++]);

    /* report... */
    *osamp = i;

    return ST_SUCCESS;
}
    
/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_pitch_stop(eff_t effp)
{
    pitch_t pitch = (pitch_t) effp->priv;

    free(pitch->fade);
    free(pitch->tmp);
    free(pitch->acc);
    free(pitch->buf);

    if (pitch->clipped)
        st_warn("PITCH clipped %d values... adjust volume with -v option maybe?", 
             pitch->clipped);

    return ST_SUCCESS;
}
