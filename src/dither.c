/*
 * libSoX dithering noise effect file.
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
#include "sox_i.h"

typedef struct dither {
  double amount;
} * dither_t;

assert_static(sizeof(struct dither) <= SOX_MAX_EFFECT_PRIVSIZE,
              /* else */ dither_PRIVSIZE_too_big);

static int getopts(sox_effect_t effp, int n, char * * argv)
{
  dither_t dither = (dither_t) effp->priv;

  if (n > 1) {
    sox_fail(effp->handler.usage);
    return SOX_EOF;
  }
  
  dither->amount = sqrt(2.); /* M_SQRT2 missing in some places */   /* Default to half a bit. */
  if (n == 1) {
    double amount;
    char dummy;
    int scanned = sscanf(*argv, "%lf %c", &amount, &dummy);
    if (scanned == 1 && amount > 0)
      dither->amount *= amount;
    else {
      sox_fail(effp->handler.usage);
      return SOX_EOF;
    }
  }

  return SOX_SUCCESS;
}

static int start(sox_effect_t effp)
{
  dither_t dither = (dither_t) effp->priv;

  if (effp->outinfo.encoding == SOX_ENCODING_ULAW ||
      effp->outinfo.encoding == SOX_ENCODING_ALAW) {
    dither->amount *= 16;
    return SOX_SUCCESS;
  } else if (effp->outinfo.size == SOX_SIZE_BYTE) {
    dither->amount *= 256;
    return SOX_SUCCESS;
  } else if (effp->outinfo.size == SOX_SIZE_16BIT)
    return SOX_SUCCESS;

  return SOX_EFF_NULL;   /* Dithering not needed at >= 24 bits */
}

static int flow(sox_effect_t effp, const sox_ssample_t * ibuf,
    sox_ssample_t * obuf, sox_size_t * isamp, sox_size_t * osamp)
{
  dither_t dither = (dither_t)effp->priv;
  sox_size_t len = min(*isamp, *osamp);

  *isamp = *osamp = len;
  while (len--) {             /* 16 signed bits of triangular noise */
    int tri16 = ((rand() % 32768) + (rand() % 32768)) - 32767;
    double l = *ibuf++ + tri16 * dither->amount;
    *obuf++ = SOX_ROUND_CLIP_COUNT(l, effp->clips);
  }
  return SOX_SUCCESS;
}

sox_effect_handler_t const * sox_dither_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "dither", "Usage: dither [amount]", SOX_EFF_MCHAN,
    getopts, start, flow, 0, 0, 0
  };
  return &handler;
}

sox_effect_handler_t const * sox_mask_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "mask", "Usage: mask [amount]", SOX_EFF_MCHAN | SOX_EFF_DEPRECATED,
    getopts, start, flow, 0, 0, 0
  };
  return &handler;
}
