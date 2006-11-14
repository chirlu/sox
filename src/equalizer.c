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

#include <math.h>
#include "st_i.h"

static st_effect_t st_equalizer_effect;

/* Filter parameters */
typedef struct filterparams {
  float rate;  // Sample rate
  float Q;     // Q-factor
  float cfreq; // Central frequency (Hz)
  float gain;  // Gain (dB)
  double x[3]; // In where x[2] <=> x[ n - 2 ]
  double y[3]; // Out
  double b[3]; // From this point, equation constants...
  double a[3];
} *equalizer_t;

int st_equalizer_getopts(eff_t effp, int n, char **argv) 
{
  equalizer_t eq = (equalizer_t) effp->priv;
  int i;

  if (n < 3)
    {
      st_fail("Usage: equalizer center-freq Q gain");
      return (ST_EOF);
    }

  i = 0;
  sscanf(argv[i++], "%f", &eq->cfreq);
  sscanf(argv[i++], "%f", &eq->Q);
  sscanf(argv[i++], "%f", &eq->gain);

  // TODO: Would be nice to validate the params..

  return (ST_SUCCESS);
}

// Set the filter constants
int st_equalizer_start(eff_t effp)
{
  equalizer_t eq = (equalizer_t) effp->priv;
  double w0;
  double amp;
  double alpha;

  // Sample rate
  eq->rate = effp->ininfo.rate;

  w0 = 2*M_PI*eq->cfreq/eq->rate;
  amp = pow( 10, eq->gain/40 );
  alpha = sin(w0)/( 2*eq->Q );

  st_report("Debug: cfreq: %fHz", eq->cfreq);
  st_report("Debug: Q: %f", eq->Q);
  st_report("Debug: gain: %fdB", eq->gain);
  st_report("Debug: rate: %f", eq->rate);
  st_report("Debug: w0: %f", w0);
  st_report("Debug: amp: %f", amp);
  st_report("Debug: alpha: %f", alpha);

  // Initialisation
  eq->b[0] =  1 + alpha*amp;
  eq->b[1] = -2*cos(w0);
  eq->b[2] =  1 - alpha*amp;
  eq->a[0] =  1 + alpha/amp;
  eq->a[1] = -2*cos(w0);
  eq->a[2] =  1 - alpha/amp;

  eq->x[0] = 0; // x[n]
  eq->x[1] = 0; // x[n-1]
  eq->x[2] = 0; // x[n-2]
  eq->y[0] = 0; // y[n]
  eq->y[1] = 0; // y[n-1]
  eq->y[2] = 0; // y[n-2]

  if (effp->globalinfo.octave_plot_effect)
  {
    printf(
      "title('SoX effect: %s gain=%g centre=%g Q=%g (rate=%u)')\n"
      "xlabel('Frequency (Hz)')\n"
      "ylabel('Amplitude Response (dB)')\n"
      "Fs=%u;minF=10;maxF=Fs/2;\n"
      "axis([minF maxF -25 25])\n"
      "sweepF=logspace(log10(minF),log10(maxF),200);\n"
      "grid on\n"
      "[h,w]=freqz([%f %f %f],[%f %f %f],sweepF,Fs);\n"
      "semilogx(w,20*log10(h),'b')\n"
      "pause\n"
      , effp->name, eq->gain, eq->cfreq, eq->Q
      , effp->ininfo.rate, effp->ininfo.rate
      , eq->b[0], eq->b[1], eq->b[2], eq->a[0], eq->a[1], eq->a[2]
      );
    exit(0);
  }

  return (ST_SUCCESS);
}

int st_equalizer_flow(eff_t effp, st_sample_t *ibuf,
                      st_sample_t *obuf, st_size_t *isamp,
                      st_size_t *osamp)
{
  equalizer_t eq = (equalizer_t) effp->priv;
  st_size_t len, done;
  double out;

  len = ((*isamp > *osamp) ? *osamp : *isamp);

  for(done = 0; done < len; done++) {
    eq->x[2] = eq->x[1];
    eq->x[1] = eq->x[0];
    eq->x[0] = *ibuf++;
      
    eq->y[2] = eq->y[1];
    eq->y[1] = eq->y[0];
    out = (
           (eq->b[0]/eq->a[0])*eq->x[0] +
           (eq->b[1]/eq->a[0])*eq->x[1] +
           (eq->b[2]/eq->a[0])*eq->x[2] -
           (eq->a[1]/eq->a[0])*eq->y[1] -
           (eq->a[2]/eq->a[0])*eq->y[2]
           );
    eq->y[0] = out;

    if (out < ST_SAMPLE_MIN) {
      out = ST_SAMPLE_MIN;
    }
    else if (out > ST_SAMPLE_MAX) {
      out = ST_SAMPLE_MAX;
    }

    *obuf++ = out;
  }

  *isamp = len;
  *osamp = len;

  return (ST_SUCCESS);
}

static st_effect_t st_equalizer_effect = {
  "equalizer",
  "Usage: equalizer central-frequency Q gain",
  0,
  st_equalizer_getopts, st_equalizer_start,
  st_equalizer_flow, st_effect_nothing_drain,
  st_effect_nothing
};

const st_effect_t *st_equalizer_effect_fn(void)
{
    return &st_equalizer_effect;
}
