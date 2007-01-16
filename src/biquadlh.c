/*
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

/* Sound Tools Effects: 2-pole variable Q & 1-pole low/high-pass filters.
 *
 * This implementation (c) 2007 robs@users.sourceforge.net
 *
 * 2-pole with default Q gives a Butterworth response.
 *
 * 2-pole filter design by Robert Bristow-Johnson <rbj@audioimagination.com>
 * see http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
 *
 * 1-pole low-pass based on lowp.c:
 * (c) 2000 Chris Bagwell <cbagwell@sprynet.com>
 *
 * Algorithm:  Recursive single pole lowpass filter
 *
 * Reference: The Scientist and Engineer's Guide to Digital Signal Processing
 *
 *      output[N] = input[N] * A + output[N-1] * B
 *
 *      X = exp(-2.0 * pi * Fc)
 *      A = 1 - X
 *      B = X
 *      Fc = cutoff freq / sample rate
 *
 * Mimics an RC low-pass filter:   
 *
 *     ---/\/\/\/\----------->
 *                    |
 *                   --- C
 *                   ---
 *                    |
 *                    |
 *                    V
 *
 * 1-pole high-pass based on highp.c:
 * (c) 2000 Chris Bagwell <cbagwell@sprynet.com>
 *
 * Algorithm:  Recursive single pole high-pass filter
 *
 * Reference: The Scientist and Engineer's Guide to Digital Processing
 *
 *      output[N] = A0 * input[N] + A1 * input[N-1] + B1 * output[N-1]
 *
 *      X  = exp(-2.0 * pi * Fc)
 *      A0 = (1 + X) / 2
 *      A1 = -(1 + X) / 2
 *      B1 = X
 *      Fc = cutoff freq / sample rate
 *
 * Mimics an RC high-pass filter:
 *
 *        || C
 *    ----||--------->
 *        ||    |
 *              <
 *              > R
 *              <
 *              |
 *              V
 */

#include "biquad.h"
#include <string.h>
#undef st_fail
#define st_fail st_message_filename=effp->name,st_fail

static int getopts(eff_t effp, int n, char **argv) 
{
  biquad_t p = (biquad_t) effp->priv;
  char dummy;
  char order;

  if (n != 0 && sscanf(argv[0], "-%1[12] %c", &order, &dummy) == 1)
    ++argv, --n;
  else order = strlen(effp->name) <= 5 /* lowp|highp */ ? '1' : '2';
  order -= '0';

  p->width.q = order == 2? sqrt(0.5) : 0;
  if (n < 1 || n > order || sscanf(argv[0], "%lf %c", &p->fc, &dummy) != 1 ||
      p->fc <= 0 || (n == 2 && (sscanf(argv[1], "%lf %c", &p->width.q, &dummy)
          != 1 || p->width.q <= 0))) {
    st_fail(effp->h->usage);
    return ST_EOF;
  }
  return ST_SUCCESS;
}

static int start(eff_t effp)
{
  biquad_t p = (biquad_t) effp->priv;
  double w0 = 2 * M_PI * p->fc / effp->ininfo.rate;

  if (w0 > M_PI) {
    st_fail("cut-off frequency must be less than half the sample-rate (Nyquist rate)");
    return ST_EOF;
  }
  if (p->width.q) {           /* 2-pole */
    double alpha = sin(w0)/(2*p->width.q);

    if (*effp->name == 'l') { /* lowpass */
      p->b0 =  (1 - cos(w0))/2;
      p->b1 =   1 - cos(w0);
      p->b2 =  (1 - cos(w0))/2;
    } else {
      p->b0 =  (1 + cos(w0))/2;
      p->b1 = -(1 + cos(w0));
      p->b2 =  (1 + cos(w0))/2;
    }
    p->a0 =   1 + alpha;
    p->a1 =  -2*cos(w0);
    p->a2 =   1 - alpha;
  } else {                    /* 1-pole */
    p->a0 = 1;
    p->a1 = -exp(-w0);
    if (*effp->name == 'l') { /* lowpass */
      p->b0 = 1 + p->a1;
      p->b1 = 0;
    } else {
      p->b0 = (1 - p->a1)/2;
      p->b1 = -p->b0;
    }
    p->a2 = p->b2 = 0;
  }
  return st_biquad_start(effp, "Q");
}

st_effect_t const * st_lowp_effect_fn(void)
{
  static st_effect_t driver = {
    "lowp", "Usage: lowp cutoff", 0,
    getopts, start, st_biquad_flow,
    st_effect_nothing_drain, st_effect_nothing, st_effect_nothing
  };
  return &driver;
}

st_effect_t const * st_highp_effect_fn(void)
{
  static st_effect_t driver = {
    "highp", "Usage: highp cutoff", 0,
    getopts, start, st_biquad_flow,
    st_effect_nothing_drain, st_effect_nothing, st_effect_nothing
  };
  return &driver;
}

st_effect_t const * st_lowpass_effect_fn(void)
{
  static st_effect_t driver = {
    "lowpass", "Usage: lowpass [-1|-2] frequency [Q]", 0,
    getopts, start, st_biquad_flow,
    st_effect_nothing_drain, st_effect_nothing, st_effect_nothing
  };
  return &driver;
}

st_effect_t const * st_highpass_effect_fn(void)
{
  static st_effect_t driver = {
    "highpass", "Usage: highpass [-1|-2] frequency [Q]", 0,
    getopts, start, st_biquad_flow,
    st_effect_nothing_drain, st_effect_nothing, st_effect_nothing
  };
  return &driver;
}
