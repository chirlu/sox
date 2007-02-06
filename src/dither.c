/*
 * Sound Tools dithering noise effect file.
 *
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 *
 * TODO: does triangular noise, could do local shaping
 *
 */

#include <stdlib.h>
#include <math.h>
#include "st_i.h"

typedef struct dither {
  double amount;
} * dither_t;

assert_static(sizeof(struct dither) <= ST_MAX_EFFECT_PRIVSIZE,
              /* else */ dither_PRIVSIZE_too_big);

static int getopts(eff_t effp, int n, char * * argv)
{
  dither_t dither = (dither_t) effp->priv;

  if (n > 1) {
    st_fail(effp->h->usage);
    return ST_EOF;
  }
  
  dither->amount = sqrt(2); /* M_SQRT2 missing in some places */   /* Default to half a bit. */
  if (n == 1) {
    double amount;
    char dummy;
    int scanned = sscanf(*argv, "%lf %c", &amount, &dummy);
    if (scanned == 1 && amount > 0)
      dither->amount *= amount;
    else {
      st_fail(effp->h->usage);
      return ST_EOF;
    }
  }

  return ST_SUCCESS;
}

static int start(eff_t effp)
{
  dither_t dither = (dither_t) effp->priv;

  if (effp->outinfo.encoding == ST_ENCODING_ULAW ||
      effp->outinfo.encoding == ST_ENCODING_ALAW) {
    dither->amount *= 16;
    return ST_SUCCESS;
  } else if (effp->outinfo.size == ST_SIZE_BYTE) {
    dither->amount *= 256;
    return ST_SUCCESS;
  } else if (effp->outinfo.size == ST_SIZE_16BIT)
    return ST_SUCCESS;

  return ST_EFF_NULL;   /* Dithering not needed at >= 24 bits */
}

static int flow(eff_t effp, const st_sample_t * ibuf,
    st_sample_t * obuf, st_size_t * isamp, st_size_t * osamp)
{
  dither_t dither = (dither_t)effp->priv;
  st_size_t len = min(*isamp, *osamp);

  *isamp = *osamp = len;
  while (len--) {             /* 16 signed bits of triangular noise */
    int tri16 = ((rand() % 32768) + (rand() % 32768)) - 32767;
    double l = *ibuf++ + tri16 * dither->amount;
    *obuf++ = ST_ROUND_CLIP_COUNT(l, effp->clips);
  }
  return ST_SUCCESS;
}

st_effect_t const * st_dither_effect_fn(void)
{
  static st_effect_t driver = {
    "dither", "Usage: dither [amount]", ST_EFF_MCHAN,
    getopts, start, flow, 0, 0, 0
  };
  return &driver;
}

st_effect_t const * st_mask_effect_fn(void)
{
  static st_effect_t driver = {
    "mask", "Usage: mask [amount]", ST_EFF_MCHAN | ST_EFF_DEPRECATED,
    getopts, start, flow, 0, 0, 0
  };
  return &driver;
}
