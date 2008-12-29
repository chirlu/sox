/* Effect: lowfir filter     Copyright (c) 2008 robs@users.sourceforge.net
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* This is W.I.P. hence marked SOX_EFF_DEPRECATED for now.
 * Need to add other filter types (and rename file), and add a decent
 * user interface.  When this is done, it should be a more user-friendly
 * & more capable version of the `filter' effect.
 */

#include "sox_i.h"
#include "dft_filter.h"
#include <string.h>

typedef struct {
  dft_filter_priv_t base;
  double Fp, Fc, Fn, att, phase;
  int allow_aliasing, num_taps, k;
} priv_t;

static int create(sox_effect_t * effp, int argc, char **argv)
{
  priv_t * p = (priv_t *)effp->priv;
  dft_filter_priv_t * b = &p->base;
  dft_filter_filter_t * f = &b->filter;
  double * h;
  int i;
  b->filter_ptr = &b->filter;
  --argc, ++argv;
  do {                    /* break-able block */
    NUMERIC_PARAMETER(Fp, 0, 100);
    NUMERIC_PARAMETER(Fc, 0, 100);
    NUMERIC_PARAMETER(Fn, 0, 100);
    NUMERIC_PARAMETER(allow_aliasing, 0, 1);
    NUMERIC_PARAMETER(att, 80, 200);
    NUMERIC_PARAMETER(num_taps, 0, 65536);
    NUMERIC_PARAMETER(k, 0, 512);
    NUMERIC_PARAMETER(phase, 0, 100);
  } while (0);

  if (p->phase != 0 && p->phase != 50 && p->phase != 100)
    p->att *= 34./33; /* negate degradation with intermediate phase */
  h = lsx_design_lpf(
      p->Fp, p->Fc, p->Fn, p->allow_aliasing, p->att, &p->num_taps, p->k);
  f->num_taps = p->num_taps;

  if (p->phase != 50)
    lsx_fir_to_phase(&h, &f->num_taps, &f->post_peak, p->phase);
  else f->post_peak = f->num_taps / 2;
  lsx_debug("%i %i %g%%",
      f->num_taps, f->post_peak, 100 - 100. * f->post_peak / (f->num_taps - 1));

  f->dft_length = lsx_set_dft_length(f->num_taps);
  f->coefs = lsx_calloc(f->dft_length, sizeof(*f->coefs));
  for (i = 0; i < f->num_taps; ++i)
    f->coefs[(i + f->dft_length - f->num_taps + 1) & (f->dft_length - 1)] = h[i] / f->dft_length * 2;
  free(h);
  lsx_safe_rdft(f->dft_length, 1, f->coefs);
  return argc? lsx_usage(effp) : SOX_SUCCESS;
}

sox_effect_handler_t const * sox_lowfir_effect_fn(void)
{
  static sox_effect_handler_t handler;
  handler = *sox_dft_filter_effect_fn();
  handler.name = "lowfir";
  handler.usage = "[options]";
  handler.flags |= SOX_EFF_DEPRECATED;
  handler.getopts = create;
  handler.priv_size = sizeof(priv_t);
  return &handler;
}
