/*
    Equalizer filter effect file for SoX
    Copyright (C) 2006 Pascal Giard <evilynux@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
 
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Code based on the biquad filters described in
    
    Cookbook formulae for audio EQ biquad filter coefficients
    by Robert Bristow-Johnson <rbj@audioimagination.com>

    Theory:
     y[n] = (a0/b0)*x[n] + (b1/a0)*x[n-1] + (b2/a0)*x[n-2]
            - (a1/a0)*y[n-1] - (a2/a0)*y[n-2]

     Where:
       w0 = 2*M_PI*cfreq/srate
       A = 10^(gain/40)
       alpha = sin(w0)/( 2*Q )

     For a PeakingEQ filter:
       b0 =  1 + alpha*A
       b1 = -2*cos(w0)
       b2 =  1 - alpha*A
       a0 =  1 + alpha/A
       a1 = -2*cos(w0)
       a2 =  1 - alpha/A

     Reminder:
       Q = sqrt(2^n)/(2^n - 1) where n is bandwidth in octave
       n = log2(bw) where bw is bandwidth in Hz

     Transfer function is:
             (a0/b0) + (b1/a0)z^-1 + (b2/a0)z^-2
      H(z) = -----------------------------------
                1 + (a1/a0)z^-1 + (a2/a0)z^-2
 */

#include "biquad.h"

static int equalizer_getopts(eff_t effp, int n, char **argv) 
{
  biquad_t eq = (biquad_t) effp->priv;
  char dummy;

  if (n != 3 ||
      sscanf(argv[0], "%lf %c", &eq->fc   , &dummy) != 1 ||
      sscanf(argv[1], "%lf %c", &eq->width.q, &dummy) != 1 ||
      sscanf(argv[2], "%lf %c", &eq->gain , &dummy) != 1 ||
      eq->fc <= 0 ||
      eq->width.q <= 0) {
    st_fail(effp->h->usage);
    return ST_EOF;
  }
  return ST_SUCCESS;
}

static int equalizer_start(eff_t effp)
{
  biquad_t eq = (biquad_t) effp->priv;
  double w0;
  double amp;
  double alpha;

  /* Set the filter constants */
  w0 = 2*M_PI*eq->fc/effp->ininfo.rate;
  amp = pow( 10, eq->gain/40 );
  alpha = sin(w0)/( 2*eq->width.q );

  st_debug("cfreq: %fHz", eq->fc);
  st_debug("Q: %f", eq->width.q);
  st_debug("gain: %fdB", eq->gain);
  st_debug("w0: %f", w0);
  st_debug("amp: %f", amp);
  st_debug("alpha: %f", alpha);

  /* Initialisation */
  eq->b0 =  1 + alpha*amp;
  eq->b1 = -2*cos(w0);
  eq->b2 =  1 - alpha*amp;
  eq->a0 =  1 + alpha/amp;
  eq->a1 = -2*cos(w0);
  eq->a2 =  1 - alpha/amp;

  return st_biquad_start(effp, "Q");
}

const st_effect_t *st_equalizer_effect_fn(void)
{
  static st_effect_t driver = {
    "equalizer",
    "Usage: equalizer central-frequency Q gain",
    0,
    equalizer_getopts,
    equalizer_start,
    st_biquad_flow,
    st_effect_nothing_drain,
    st_effect_nothing,
    st_effect_nothing
  };
  return &driver;
}
