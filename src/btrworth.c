/*

    Butterworth effect file for SoX
    Copyright (C) 1999 Jan Paul Schmidt <jps@fundament.org>

    This source code is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public License
    as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    This source code is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA

    Code based on the butterworth implementation in

    Sound Processing Kit - A C++ Class Library for Audio Signal Processing
    Copyright (C) 1995-1998 Kai Lassfolk

    as described in

    Computer music: synthesis, composition, and performance
    Charles Dodge, Thomas A. Jerse
    [2nd ed.]
    Page 214

 */

#include <math.h>

#include "st_i.h"
#include "btrworth.h"

int st_butterworth_start (eff_t effp)
{
  butterworth_t butterworth = (butterworth_t) effp->priv;

  butterworth->x [0] = 0.0;
  butterworth->x [1] = 0.0;
  butterworth->y [0] = 0.0;
  butterworth->y [1] = 0.0;
  return (ST_SUCCESS);
}

void st_butterworth_plot (eff_t effp)
{
  butterworth_t butterworth = (butterworth_t) effp->priv;

  if (effp->globalinfo.octave_plot_effect)
  {
    printf(
      "title('SoX effect: %s centre=%g width=%g (rate=%u)')\n"
      "xlabel('Frequency (Hz)')\n"
      "ylabel('Amplitude Response (dB)')\n"
      "Fs=%u;minF=10;maxF=Fs/2;\n"
      "axis([minF maxF -95 5])\n"
      "sweepF=logspace(log10(minF),log10(maxF),200);\n"
      "grid on\n"
      "[h,w]=freqz([%f %f %f],[1 %f %f],sweepF,Fs);\n"
      "semilogx(w,20*log10(h),'b')\n"
      "pause\n"
      , effp->name, butterworth->frequency, butterworth->bandwidth
      , effp->ininfo.rate, effp->ininfo.rate
      , butterworth->a[0]
      , butterworth->a[1]
      , butterworth->a[2]
      , butterworth->b[0]
      , butterworth->b[1]
      );
    exit(0);
  }
}

int st_butterworth_flow (eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                         st_size_t *isamp, st_size_t *osamp)
{
  butterworth_t butterworth = (butterworth_t) effp->priv;

  double in;
  double out;

  int len;
  int done;

  len = ((*isamp > *osamp) ? *osamp : *isamp);

  for (done = 0; done < len; done++) {
    in = *ibuf++;

    /*
     * Substituting butterworth->a [x] and butterworth->b [x] with
     * variables, which are set outside of the loop, did not increased
     * speed on my AMD Box. GCC seems to do a good job :o)
     */

    out =
      butterworth->a [0] * in +
      butterworth->a [1] * butterworth->x [0] +
      butterworth->a [2] * butterworth->x [1] -
      butterworth->b [0] * butterworth->y [0] -
      butterworth->b [1] * butterworth->y [1];

    butterworth->x [1] = butterworth->x [0];
    butterworth->x [0] = in;
    butterworth->y [1] = butterworth->y [0];
    butterworth->y [0] = out;

    ST_SAMPLE_CLIP(out);

    *obuf++ = out;
  }

  *isamp = len;
  *osamp = len;
  return (ST_SUCCESS);
}
