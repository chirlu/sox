/*
 * Sound Tools masking noise effect file.
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

typedef struct mask {
  double amount;
} * mask_t;

assert_static(sizeof(struct mask) <= ST_MAX_EFFECT_PRIVSIZE,
              /* else */ mask_PRIVSIZE_too_big);

static int st_mask_getopts(eff_t effp, int n, char * * argv)
{
  mask_t mask = (mask_t) effp->priv;

  if (n > 1) {
    st_fail(effp->h->usage);
    return ST_EOF;
  }
  
  mask->amount = sqrt(2); /* M_SQRT2 missing in some places */   /* Default to half a bit. */
  if (n == 1) {
    double amount;
    char dummy;
    int scanned = sscanf(*argv, "%lf %c", &amount, &dummy);
    if (scanned == 1 && amount > 0)
      mask->amount *= amount;
    else {
      st_fail(effp->h->usage);
      return ST_EOF;
    }
  }

  return ST_SUCCESS;
}

static int st_mask_start(eff_t effp)
{
  mask_t mask = (mask_t) effp->priv;

  if (effp->outinfo.encoding == ST_ENCODING_ULAW ||
      effp->outinfo.encoding == ST_ENCODING_ALAW) {
    mask->amount *= 16;
    return ST_SUCCESS;
  } else if (effp->outinfo.size == ST_SIZE_BYTE) {
    mask->amount *= 256;
    return ST_SUCCESS;
  } else if (effp->outinfo.size == ST_SIZE_16BIT)
    return ST_SUCCESS;
  else if (effp->outinfo.size == ST_SIZE_24BIT) {
    mask->amount /= 256;
    return ST_SUCCESS;
  } else if (effp->outinfo.size == ST_SIZE_64BIT) {
    mask->amount /= 16384;
    return ST_SUCCESS;
  }

  st_fail("Invalid size %d", effp->outinfo.size);
  return ST_EOF;
}

/* FIXME: Scale noise more sensibly for sizes >= 24 bits */
static int st_mask_flow(eff_t effp, const st_sample_t * ibuf,
    st_sample_t * obuf, st_size_t * isamp, st_size_t * osamp)
{
  mask_t mask = (mask_t)effp->priv;
  st_size_t len = min(*isamp, *osamp);

  *isamp = *osamp = len;
  while (len--) {             /* 16 signed bits of triangular noise */
    int tri16 = ((rand() % 32768L) + (rand() % 32768L)) - 32767;
    double l = *ibuf++ + tri16 * mask->amount;
    *obuf++ = ST_ROUND_CLIP_COUNT(l, effp->clips);
  }
  return ST_SUCCESS;
}

static st_effect_t st_mask_effect = {
  "mask",
  "Usage: mask [amount]",
  ST_EFF_MCHAN,
  st_mask_getopts,
  st_effect_nothing,
  st_mask_flow,
  st_effect_nothing_drain,
  st_effect_nothing,
  st_effect_nothing
};

st_effect_t const * st_mask_effect_fn(void)
{
  return &st_mask_effect;
}

static st_effect_t st_dither_effect = {
  "dither",
  "Usage: dither [amount]",
  ST_EFF_MCHAN,
  st_mask_getopts,
  st_mask_start,
  st_mask_flow,
  st_effect_nothing_drain,
  st_effect_nothing,
  st_effect_nothing
};

st_effect_t const * st_dither_effect_fn(void)
{
  return &st_dither_effect;
}
