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

/* Biquad filter effects   (c) 2006-7 robs@users.sourceforge.net
 *
 * 2-pole filters designed by Robert Bristow-Johnson <rbj@audioimagination.com>
 *   see http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
 *
 * 1-pole filters based on code (c) 2000 Chris Bagwell <cbagwell@sprynet.com>
 *   Algorithms: Recursive single pole low/high pass filter
 *   Reference: The Scientist and Engineer's Guide to Digital Signal Processing
 *
 *   low-pass: output[N] = input[N] * A + output[N-1] * B
 *     X = exp(-2.0 * pi * Fc)
 *     A = 1 - X
 *     B = X
 *     Fc = cutoff freq / sample rate
 *   
 *     Mimics an RC low-pass filter:   
 *   
 *     ---/\/\/\/\----------->
 *                   |
 *                  --- C
 *                  ---
 *                   |
 *                   |
 *                   V
 *   
 *   high-pass: output[N] = A0 * input[N] + A1 * input[N-1] + B1 * output[N-1]
 *     X  = exp(-2.0 * pi * Fc)
 *     A0 = (1 + X) / 2
 *     A1 = -(1 + X) / 2
 *     B1 = X
 *     Fc = cutoff freq / sample rate
 *   
 *     Mimics an RC high-pass filter:
 *   
 *         || C
 *     ----||--------->
 *         ||    |
 *               <
 *               > R
 *               <
 *               |
 *               V
 */


#include "biquad.h"
#include <string.h>
#include <math.h>


static int hilo1_getopts(eff_t effp, int n, char **argv) {
  return st_biquad_getopts(effp, n, argv, 1, 1, 0, 1, 2, "",
      *effp->name == 'l'? filter_LPF_1 : filter_HPF_1);
}


static int hilo2_getopts(eff_t effp, int n, char **argv) {
  biquad_t p = (biquad_t) effp->priv;
  if (n != 0 && strcmp(argv[0], "-1") == 0)
    return hilo1_getopts(effp, n - 1, argv + 1);
  if (n != 0 && strcmp(argv[0], "-2") == 0)
    ++argv, --n;
  p->width = sqrt(0.5); /* Default to Butterworth */
  return st_biquad_getopts(effp, n, argv, 1, 2, 0, 1, 2, "qoh",
      *effp->name == 'l'? filter_LPF : filter_HPF);
}


static int bandpass_getopts(eff_t effp, int n, char **argv) {
  filter_t type = filter_BPF;
  if (n != 0 && strcmp(argv[0], "-c") == 0)
    ++argv, --n, type = filter_BPF_CSG;
  return st_biquad_getopts(effp, n, argv, 2, 2, 0, 1, 2, "hqob", type);
}


static int bandrej_getopts(eff_t effp, int n, char **argv) {
  return st_biquad_getopts(effp, n, argv, 2, 2, 0, 1, 2, "hqob", filter_notch);
}


static int allpass_getopts(eff_t effp, int n, char **argv) {
  filter_t type = filter_APF;
  int m;
  if (n != 0 && strcmp(argv[0], "-1") == 0)
    ++argv, --n, type = filter_AP1;
  else if (n != 0 && strcmp(argv[0], "-2") == 0)
    ++argv, --n, type = filter_AP2;
  m = 1 + (type == filter_APF);
  return st_biquad_getopts(effp, n, argv, m, m, 0, 1, 2, "hqo", type);
}


static int tone_getopts(eff_t effp, int n, char **argv) {
  biquad_t p = (biquad_t) effp->priv;
  p->width = 0.5;
  p->fc = *effp->name == 'b'? 100 : 3000;
  return st_biquad_getopts(effp, n, argv, 1, 3, 1, 2, 0, "shqo",
      *effp->name == 'b'?  filter_lowShelf: filter_highShelf);
}


static int equalizer_getopts(eff_t effp, int n, char **argv) {
  return st_biquad_getopts(effp, n, argv, 3, 3, 0, 1, 2, "qoh", filter_peakingEQ);
}


static int band_getopts(eff_t effp, int n, char **argv) {
  filter_t type = filter_BPF_SPK;
  if (n != 0 && strcmp(argv[0], "-n") == 0)
    ++argv, --n, type = filter_BPF_SPK_N;
  return st_biquad_getopts(effp, n, argv, 1, 2, 0, 1, 2, "hqo", type);
}


static int deemph_getopts(eff_t effp, int n, char **argv) {
  return st_biquad_getopts(effp, n, argv, 0, 0, 0, 1, 2, "", filter_deemph);
}


static int start(eff_t effp)
{
  biquad_t p = (biquad_t) effp->priv;
  double w0 = 2 * M_PI * p->fc / effp->ininfo.rate;
  double A  = exp(p->gain / 40 * log(10.));
  double alpha = 0;

  if (w0 > M_PI) {
    st_fail("frequency must be less than half the sample-rate (Nyquist rate)");
    return ST_EOF;
  }
  
  /* Set defaults: */
  p->b0 = p->b1 = p->b2 = p->a1 = p->a2 = 0;
  p->a0 = 1;

  if (p->width) switch (p->width_type) {
    case width_slope:
      alpha = sin(w0)/2 * sqrt((A + 1/A)*(1/p->width - 1) + 2);
      break;

    case width_Q:
      alpha = sin(w0)/(2*p->width);
      break;

    case width_bw_oct:
      alpha = sin(w0)*sinh(log(2.)/2 * p->width * w0/sin(w0));
      break;

    case width_bw_Hz:
      alpha = sin(w0)/(2*p->fc/p->width);
      break;

    case width_bw_old:
      alpha = tan(M_PI * p->width / effp->ininfo.rate);
      break;
  }
  switch (p->filter_type) {
    case filter_LPF: /* H(s) = 1 / (s^2 + s/Q + 1) */
      p->b0 =  (1 - cos(w0))/2;
      p->b1 =   1 - cos(w0);
      p->b2 =  (1 - cos(w0))/2;
      p->a0 =   1 + alpha;
      p->a1 =  -2*cos(w0);
      p->a2 =   1 - alpha;
      break;

    case filter_HPF: /* H(s) = s^2 / (s^2 + s/Q + 1) */
      p->b0 =  (1 + cos(w0))/2;
      p->b1 = -(1 + cos(w0));
      p->b2 =  (1 + cos(w0))/2;
      p->a0 =   1 + alpha;
      p->a1 =  -2*cos(w0);
      p->a2 =   1 - alpha;
      break;

    case filter_BPF_CSG: /* H(s) = s / (s^2 + s/Q + 1)  (constant skirt gain, peak gain = Q) */
      p->b0 =   sin(w0)/2;
      p->b1 =   0;
      p->b2 =  -sin(w0)/2;
      p->a0 =   1 + alpha;
      p->a1 =  -2*cos(w0);
      p->a2 =   1 - alpha;
      break;

    case filter_BPF: /* H(s) = (s/Q) / (s^2 + s/Q + 1)      (constant 0 dB peak gain) */
      p->b0 =   alpha;
      p->b1 =   0;
      p->b2 =  -alpha;
      p->a0 =   1 + alpha;
      p->a1 =  -2*cos(w0);
      p->a2 =   1 - alpha;
      break;

    case filter_notch: /* H(s) = (s^2 + 1) / (s^2 + s/Q + 1) */
      p->b0 =   1;
      p->b1 =  -2*cos(w0);
      p->b2 =   1;
      p->a0 =   1 + alpha;
      p->a1 =  -2*cos(w0);
      p->a2 =   1 - alpha;
      break;

    case filter_APF: /* H(s) = (s^2 - s/Q + 1) / (s^2 + s/Q + 1) */
      p->b0 =   1 - alpha;
      p->b1 =  -2*cos(w0);
      p->b2 =   1 + alpha;
      p->a0 =   1 + alpha;
      p->a1 =  -2*cos(w0);
      p->a2 =   1 - alpha;
      break;

    case filter_peakingEQ: /* H(s) = (s^2 + s*(A/Q) + 1) / (s^2 + s/(A*Q) + 1) */
      if (A == 1)
        return ST_EFF_NULL;
      p->b0 =   1 + alpha*A;
      p->b1 =  -2*cos(w0);
      p->b2 =   1 - alpha*A;
      p->a0 =   1 + alpha/A;
      p->a1 =  -2*cos(w0);
      p->a2 =   1 - alpha/A;
      break;

    case filter_lowShelf: /* H(s) = A * (s^2 + (sqrt(A)/Q)*s + A)/(A*s^2 + (sqrt(A)/Q)*s + 1) */
      if (A == 1)
        return ST_EFF_NULL;
      p->b0 =    A*( (A+1) - (A-1)*cos(w0) + 2*sqrt(A)*alpha );
      p->b1 =  2*A*( (A-1) - (A+1)*cos(w0)                   );
      p->b2 =    A*( (A+1) - (A-1)*cos(w0) - 2*sqrt(A)*alpha );
      p->a0 =        (A+1) + (A-1)*cos(w0) + 2*sqrt(A)*alpha;
      p->a1 =   -2*( (A-1) + (A+1)*cos(w0)                   );
      p->a2 =        (A+1) + (A-1)*cos(w0) - 2*sqrt(A)*alpha;
      break;

    case filter_highShelf: /* H(s) = A * (A*s^2 + (sqrt(A)/Q)*s + 1)/(s^2 + (sqrt(A)/Q)*s + A) */
      if (!A)
        return ST_EFF_NULL;
      p->b0 =    A*( (A+1) + (A-1)*cos(w0) + 2*sqrt(A)*alpha );
      p->b1 = -2*A*( (A-1) + (A+1)*cos(w0)                   );
      p->b2 =    A*( (A+1) + (A-1)*cos(w0) - 2*sqrt(A)*alpha );
      p->a0 =        (A+1) - (A-1)*cos(w0) + 2*sqrt(A)*alpha;
      p->a1 =    2*( (A-1) - (A+1)*cos(w0)                   );
      p->a2 =        (A+1) - (A-1)*cos(w0) - 2*sqrt(A)*alpha;
      break;

    case filter_LPF_1: /* single-pole */
      p->a1 = -exp(-w0);
      p->b0 = 1 + p->a1;
      break;

    case filter_HPF_1: /* single-pole */
      p->a1 = -exp(-w0);
      p->b0 = (1 - p->a1)/2;
      p->b1 = -p->b0;
      break;

    case filter_BPF_SPK: case filter_BPF_SPK_N: {
      double bw_Hz;
      if (!p->width)
        p->width = p->fc / 2;
      bw_Hz = p->width_type == width_Q?  p->fc / p->width :
        p->width_type == width_bw_Hz? p->width :
        p->fc * (pow(2., p->width) - 1) * pow(2., -0.5 * p->width); /* bw_oct */
      #include "band.h" /* Has different licence */
      break;
    }

    case filter_AP1:     /* Experimental 1-pole all-pass from Tom Erbe @ UCSD */
      p->b0 = exp(-w0);
      p->b1 = -1;
      p->a1 = -exp(-w0);
      break;

    case filter_AP2:     /* Experimental 2-pole all-pass from Tom Erbe @ UCSD */
      p->b0 = 1 - sin(w0);
      p->b1 = -2 * cos(w0);
      p->b2 = 1 + sin(w0);
      p->a0 = 1 + sin(w0);
      p->a1 = -2 * cos(w0);
      p->a2 = 1 - sin(w0);
      break;

    case filter_deemph: {
      #include "deemph.h" /* Has own documentation */
      break;
    }
  }
  return st_biquad_start(effp);
}


#define BIQUAD_EFFECT(name,group,usage,flags) \
st_effect_t const * st_##name##_effect_fn(void) { \
  static st_effect_t driver = { \
    #name, "Usage: " #name " " usage, flags, \
    group##_getopts, start, st_biquad_flow, 0, 0, 0, \
  }; \
  return &driver; \
}

BIQUAD_EFFECT(highp,     hilo1,    "cutoff-frequency", ST_EFF_DEPRECATED)
BIQUAD_EFFECT(lowp,      hilo1,    "cutoff-frequency", ST_EFF_DEPRECATED)
BIQUAD_EFFECT(highpass,  hilo2,    "[-1|-2] frequency [width[q|o|h]]", 0)
BIQUAD_EFFECT(lowpass,   hilo2,    "[-1|-2] frequency [width[q|o|h]]", 0)
BIQUAD_EFFECT(bandpass,  bandpass, "[-c] frequency width[h|q|o]", 0)
BIQUAD_EFFECT(bandreject,bandrej,  "frequency width[h|q|o]", 0)
BIQUAD_EFFECT(allpass,   allpass,  "frequency width[h|q|o]", 0)
BIQUAD_EFFECT(bass,      tone,     "gain [frequency [width[s|h|q|o]]]", 0)
BIQUAD_EFFECT(treble,    tone,     "gain [frequency [width[s|h|q|o]]]", 0)
BIQUAD_EFFECT(equalizer, equalizer,"frequency width[q|o|h] gain", 0)
BIQUAD_EFFECT(band,      band,     "[-n] center [width[h|q|o]]", 0)
BIQUAD_EFFECT(deemph,    deemph,   "takes no options", 0)
