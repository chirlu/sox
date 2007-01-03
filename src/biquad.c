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

/* Biquad filter common functions   (c) 2006 robs@users.sourceforge.net */

#include "biquad.h"



int st_biquad_start(eff_t effp, char const * width_name)
{
  biquad_t p = (biquad_t) effp->priv;

  /* Simplify: */
  p->b2 = p->b2/p->a0;
  p->b1 = p->b1/p->a0;
  p->b0 = p->b0/p->a0;
  p->a2 = p->a2/p->a0;
  p->a1 = p->a1/p->a0;

  if (p->dcNormalise) /* Normalise to AV = 0dB at DC: */
  {
    double normalise = (1 + p->a1 + p->a2) / (p->b0 + p->b1 + p->b2);
    p->b0 = normalise * p->b0;
    p->b1 = normalise * p->b1;
    p->b2 = normalise * p->b2;
  }

  if (effp->globalinfo->octave_plot_effect)
  {
    printf(
      "title('SoX effect: %s gain=%g centre=%g %s=%g (rate=%u)')\n"
      "xlabel('Frequency (Hz)')\n"
      "ylabel('Amplitude Response (dB)')\n"
      "Fs=%u;minF=10;maxF=Fs/2;\n"
      "axis([minF maxF -25 25])\n"
      "sweepF=logspace(log10(minF),log10(maxF),200);\n"
      "grid on\n"
      "[h,w]=freqz([%f %f %f],[1 %f %f],sweepF,Fs);\n"
      "semilogx(w,20*log10(h),'b')\n"
      "pause\n"
      , effp->name, p->gain, p->fc, width_name, p->width.q
      , effp->ininfo.rate, effp->ininfo.rate
      , p->b0, p->b1, p->b2, p->a1, p->a2
      );
    return ST_EOF;
  }

  p->o2 = p->o1 = p->i2 = p-> i1 = 0;
  return ST_SUCCESS;
}


  
int st_biquad_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                        st_size_t *isamp, st_size_t *osamp)
{
  biquad_t p = (biquad_t) effp->priv;
  st_size_t len = (*isamp > *osamp)? *osamp : *isamp;
  *isamp = *osamp = len;

  while (len--)
  {
    double o0 = *ibuf*p->b0 +p->i1*p->b1 +p->i2*p->b2 -p->o1*p->a1 -p->o2*p->a2;
    p->i2 = p->i1, p->i1 = *ibuf++;
    p->o2 = p->o1, p->o1 = o0;
    *obuf++ = ST_ROUND_CLIP_COUNT(o0, effp->clippedCount);
  }
  return ST_SUCCESS;
}
