/* libSoX dithering noise effect file.      July 5, 1991
 *
 * Copyright (c) 1999-8 Chris Bagwell & SoX contributors
 * Copyright 1991 Lance Norskog And Sundry Contributors
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 *
 * TODO: does triangular noise, could do local shaping.
 */

#include "sox_i.h"

typedef struct {double amount;} priv_t;

static int getopts(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *)effp->priv;
  p->amount = M_SQRT2;   /* Default to half a bit. */
  do {NUMERIC_PARAMETER(amount, 1, 10)} while (0);
  return argc?  lsx_usage(effp) : SOX_SUCCESS;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  if (effp->out_signal.precision > 16)
    return SOX_EFF_NULL;   /* Dithering not needed at >= 24 bits */
  p->amount *= 1 << (16 - effp->out_signal.precision);
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t len = *isamp = *osamp = min(*isamp, *osamp);

  while (len--) {   /* 16 signed bits of triangular noise: */
    int tri16 = ((rand() % 32768) + (rand() % 32768)) - 32767;
    double l = *ibuf++ + tri16 * p->amount;
    *obuf++ = SOX_ROUND_CLIP_COUNT(l, effp->clips);
  }
  return SOX_SUCCESS;
}

sox_effect_handler_t const * sox_dither_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "dither", "[amount]", SOX_EFF_MCHAN | SOX_EFF_PREC,
    getopts, start, flow, 0, 0, 0, sizeof(priv_t)
  };
  return &handler;
}
