/*
 * (c) march/april 2000 Fabien COELHO <fabien@coelho.net> for sox.
 *
 * Basic time stretcher.
 * cross fade samples so as to go slower or faster.
 *
 * The filter is based on 6 parameters:
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static st_effect_t st_stretch_effect;

#define DEFAULT_SLOW_SHIFT_RATIO        0.8
#define DEFAULT_FAST_SHIFT_RATIO        1.0

#define DEFAULT_STRETCH_WINDOW          20.0  /* ms */

/* I'm planing to put some common fading stuff outside. 
   It's also used in pitch.c
 */
typedef enum { st_linear_fading } st_fading_t;

#define DEFAULT_FADING st_linear_fading

typedef enum { input_state, output_state } stretch_status_t;

typedef struct 
{
  /* options
   * FIXME: maybe shift could be allowed > 1.0 with factor < 1.0 ???
   */
  double factor;   /* strech factor. 1.0 means copy. */
  double window;   /* window in ms */
  st_fading_t fade;       /* type of fading */
  double shift;    /* shift ratio wrt window. <1.0 */
  double fading;   /* fading ratio wrt window. <0.5 */

  /* internal stuff */
  stretch_status_t state; /* automaton status */

  st_size_t size;         /* buffer size */
  st_size_t index;        /* next available element */
  st_sample_t *ibuf;      /* input buffer */
  st_size_t ishift;       /* input shift */
  
  st_size_t oindex;       /* next evailable element */
  double * obuf;   /* output buffer */
  st_size_t oshift;       /* output shift */
  
  st_size_t fsize;        /* fading size */
  double * fbuf;   /* fading, 1.0 -> 0.0 */
  
} *stretch_t;

/*
 * Process options
 */
static int st_stretch_getopts(eff_t effp, int n, char **argv) 
{
  char usage[1024];
  stretch_t stretch = (stretch_t) effp->priv; 
    
  /* default options */
  stretch->factor = 1.0; /* default is no change */
  stretch->window = DEFAULT_STRETCH_WINDOW;
  stretch->fade = st_linear_fading;

  if (n > 0 && !sscanf(argv[0], "%lf", &stretch->factor)) {
    sprintf(usage, "%s\n\terror while parsing factor", st_stretch_effect.usage);
    st_fail(usage);
    return ST_EOF;
  }

  if (n > 1 && !sscanf(argv[1], "%lf", &stretch->window)) {
    sprintf(usage, "%s\n\terror while parsing window size", st_stretch_effect.usage);
    st_fail(usage);
    return ST_EOF;
  }

  if (n > 2) {
    switch (argv[2][0]) {
    case 'l':
    case 'L':
      stretch->fade = st_linear_fading;
      break;
    default:
      sprintf (usage, "%s\n\terror while parsing fade type", st_stretch_effect.usage);
      st_fail(usage);
      return ST_EOF;
    }
  }

  /* default shift depends whether we go slower or faster */
  stretch->shift = (stretch->factor <= 1.0) ?
    DEFAULT_FAST_SHIFT_RATIO: DEFAULT_SLOW_SHIFT_RATIO;
 
  if (n > 3 && !sscanf(argv[3], "%lf", &stretch->shift)) {
    sprintf (usage, "%s\n\terror while parsing shift ratio", st_stretch_effect.usage);
    st_fail(usage);
    return ST_EOF;
  }

  if (stretch->shift > 1.0 || stretch->shift <= 0.0) {
    sprintf(usage, "%s\n\terror with shift ratio value", st_stretch_effect.usage);
    st_fail(usage);
    return ST_EOF;
  }

  /* default fading stuff... 
     it makes sense for factor >= 0.5 */
  if (stretch->factor < 1.0)
    stretch->fading = 1.0 - (stretch->factor * stretch->shift);
  else
    stretch->fading = 1.0 - stretch->shift;
  if (stretch->fading > 0.5)
    stretch->fading = 0.5;
  
  if (n > 4 && !sscanf(argv[4], "%lf", &stretch->fading)) {
    sprintf(usage, "%s\n\terror while parsing fading ratio", st_stretch_effect.usage);
    st_fail(usage);
    return ST_EOF;
  }

  if (stretch->fading > 0.5 || stretch->fading < 0.0) {
    sprintf(usage, "%s\n\terror with fading ratio value", st_stretch_effect.usage);
    st_fail(usage);
    return ST_EOF;
  }
  
  return ST_SUCCESS;
}

/*
 * Start processing
 */
static int st_stretch_start(eff_t effp)
{
  stretch_t stretch = (stretch_t)effp->priv;
  st_size_t i;

  if (stretch->factor == 1)
    return ST_EFF_NULL;

  /* FIXME: not necessary. taken care by effect processing? */
  if (effp->outinfo.channels != effp->ininfo.channels) {
    st_fail("stretch cannot handle different channels (in=%d, out=%d)"
            " use avg or pan", effp->ininfo.channels, effp->outinfo.channels);
    return ST_EOF;
  }

  if (effp->outinfo.rate != effp->ininfo.rate) {
    st_fail("stretch cannot handle different rates (in=%ld, out=%ld)"
            " use resample or rate", effp->ininfo.rate, effp->outinfo.rate);
    return ST_EOF;
  }

  stretch->state = input_state;

  stretch->size = (int)(effp->outinfo.rate * 0.001 * stretch->window);
  /* start in the middle of an input to avoid initial fading... */
  stretch->index = stretch->size / 2;
  stretch->ibuf = (st_sample_t *)xmalloc(stretch->size * sizeof(st_sample_t));

  /* the shift ratio deal with the longest of ishift/oshift
     hence ishift<=size and oshift<=size. */
  if (stretch->factor < 1.0) {
    stretch->ishift = stretch->shift * stretch->size;
    stretch->oshift = stretch->factor * stretch->ishift;
  } else {
    stretch->oshift = stretch->shift * stretch->size;
    stretch->ishift = stretch->oshift / stretch->factor;
  }
  assert(stretch->ishift <= stretch->size);
  assert(stretch->oshift <= stretch->size);

  stretch->oindex = stretch->index; /* start as synchronized */
  stretch->obuf = (double *)xmalloc(stretch->size * sizeof(double));
  stretch->fsize = (int)(stretch->fading * stretch->size);
  stretch->fbuf = (double *)xmalloc(stretch->fsize * sizeof(double));
        
  /* initialize buffers */
  for (i = 0; i<stretch->size; i++)
    stretch->ibuf[i] = 0;

  for (i = 0; i<stretch->size; i++)
    stretch->obuf[i] = 0.0;

  if (stretch->fsize>1) {
    double slope = 1.0 / (stretch->fsize - 1);
    stretch->fbuf[0] = 1.0;
    for (i = 1; i < stretch->fsize - 1; i++)
      stretch->fbuf[i] = slope * (stretch->fsize - i - 1);
    stretch->fbuf[stretch->fsize - 1] = 0.0;
  } else if (stretch->fsize == 1)
    stretch->fbuf[0] = 1.0;

  st_debug("start: (f=%.2f w=%.2f r=%.2f f=%.2f)"
           " st=%d s=%d ii=%d is=%d oi=%d os=%d fs=%d\n",
           stretch->factor, stretch->window, stretch->shift, stretch->fading,
           stretch->state, stretch->size, stretch->index, stretch->ishift,
           stretch->oindex, stretch->oshift, stretch->fsize);

  return ST_SUCCESS;
}

/* accumulates input ibuf to output obuf with fading fbuf */
static void combine(stretch_t stretch)
{
  int i, size, fsize;

  size = stretch->size;
  fsize = stretch->fsize;

  /* fade in */
  for (i = 0; i < fsize; i++)
    stretch->obuf[i] += stretch->fbuf[fsize - i - 1] * stretch->ibuf[i];

  /* steady state */
  for (; i < size - fsize; i++)
    stretch->obuf[i] += stretch->ibuf[i];

  /* fade out */
  for (; i<size; i++)
    stretch->obuf[i] += stretch->fbuf[i - size + fsize] * stretch->ibuf[i];
}

/*
 * Processes flow.
 */
static int st_stretch_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                    st_size_t *isamp, st_size_t *osamp)
{
  stretch_t stretch = (stretch_t) effp->priv;
  st_size_t iindex = 0, oindex = 0;
  st_size_t i;

  while (iindex<*isamp && oindex<*osamp) {
    if (stretch->state == input_state) {
      st_size_t tocopy = min(*isamp-iindex, 
                             stretch->size-stretch->index);

      memcpy(stretch->ibuf + stretch->index, ibuf + iindex, tocopy * sizeof(st_sample_t));
      
      iindex += tocopy;
      stretch->index += tocopy;

      if (stretch->index == stretch->size) {
        /* compute */
        combine(stretch);

        /* shift input */
        for (i = 0; i + stretch->ishift < stretch->size; i++)
          stretch->ibuf[i] = stretch->ibuf[i+stretch->ishift];

        stretch->index -= stretch->ishift;
        
        /* switch to output state */
        stretch->state = output_state;
      }
    }

    if (stretch->state == output_state) {
      while (stretch->oindex < stretch->oshift && oindex < *osamp) {
        float f;
        f = stretch->obuf[stretch->oindex++];
        ST_SAMPLE_CLIP_COUNT(f, effp->clips);
        obuf[oindex++] = f;
      }

      if (stretch->oindex >= stretch->oshift && oindex<*osamp) {
        stretch->oindex -= stretch->oshift;

        /* shift internal output buffer */
        for (i = 0; i + stretch->oshift < stretch->size; i++)
          stretch->obuf[i] = stretch->obuf[i + stretch->oshift];

        /* pad with 0 */
        for (; i < stretch->size; i++)
          stretch->obuf[i] = 0.0;
                    
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
static int st_stretch_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
  stretch_t stretch = (stretch_t) effp->priv;
  st_size_t i;
  st_size_t oindex = 0;
  
  if (stretch->state == input_state) {
    for (i=stretch->index; i<stretch->size; i++)
      stretch->ibuf[i] = 0;
    
    combine(stretch);
    
    stretch->state = output_state;
  }
  
  while (oindex<*osamp && stretch->oindex<stretch->index) {
    float f = stretch->obuf[stretch->oindex++];
    ST_SAMPLE_CLIP_COUNT(f, effp->clips);
    obuf[oindex++] = f;
  }
    
  *osamp = oindex;

  if (stretch->oindex == stretch->index)
    return ST_EOF;
  else
    return ST_SUCCESS;
}


/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
static int st_stretch_stop(eff_t effp)
{
  stretch_t stretch = (stretch_t) effp->priv;

  free(stretch->ibuf);
  free(stretch->obuf);
  free(stretch->fbuf);

  return ST_SUCCESS;
}

static st_effect_t st_stretch_effect = {
  "stretch",
  "Usage: stretch factor [window fade shift fading]\n"
  "       (expansion, frame in ms, lin/..., unit<1.0, unit<0.5)\n"
  "       (defaults: 1.0 20 lin ...)",
  0,
  st_stretch_getopts,
  st_stretch_start,
  st_stretch_flow,
  st_stretch_drain,
  st_stretch_stop,
  st_effect_nothing
};

const st_effect_t *st_stretch_effect_fn(void)
{
  return &st_stretch_effect;
}
