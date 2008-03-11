/*
 * Effect: Contrast Enhancement        (c) 2008 robs@users.sourceforge.net
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, write to the Free Software Foundation,
 * Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301, USA.
 */

#include "sox_i.h"
#include <math.h>

typedef struct {double contrast;} priv_t;
assert_static(sizeof(priv_t) <= SOX_MAX_EFFECT_PRIVSIZE, PRIVSIZE_too_big);

static int create(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *)effp->priv;

  p->contrast = 75;
  do {NUMERIC_PARAMETER(contrast, 0, 100)} while (0);
  p->contrast /= 750;
  return argc?  sox_usage(effp) : SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, sox_size_t * isamp, sox_size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  sox_size_t len = *isamp = *osamp = min(*isamp, *osamp);

  while (len--) {
    double d = *ibuf++ * (-M_PI_2 / SOX_SAMPLE_MIN);
    *obuf++ = sin(d + p->contrast * sin(d * 4)) * SOX_SAMPLE_MAX;
  }
  return SOX_SUCCESS;
}

sox_effect_handler_t const * sox_contrast_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "contrast", "[enhancement]", SOX_EFF_MCHAN, create, 0, flow, 0, 0, 0
  };
  return &handler;
}
