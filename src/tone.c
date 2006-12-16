/*
 * Sound Tools Tone Control Effects: bass & treble
 *
 * The bass and treble effects approximate to the venerable
 * Baxandall circuit used in the analogue world.
 *
 * Filter design by Robert Bristow-Johnson  <rbj@audioimagination.com>
 *
 * This implementation (c) 2006 robs@users.sourceforge.net
 *
 * See LICENSE file for further copyright information.
 */

#include "biquad.h"

#include <string.h>

static int getopts(eff_t effp, int n, char **argv, double fc, int dcNormalise)
{
  bool isFcSet = false;
  double opt1 = HUGE_VAL, opt2 = HUGE_VAL;
  biquad_t p = (biquad_t) effp->priv;
  
  /* Zero all numbers, set all bools to false: */
  memset(p, 0, sizeof(*p));

  /* Initialise non-zero numbers: */
  p->dcNormalise = dcNormalise;
  p->fc = fc;
  p->oomph = 0.5;

  /*
   * This block is a little complicated -- all because I wanted
   * to make it easy for the user i.e. order doesn't matter for
   * optional arguments.
   * This is made possible because slope (or oomph) <= 1 and by
   * insisting that the centre-frequency is > 1 (Hz).
   */
  if (n > 0 &&
     sscanf(argv[0], "%lf", &p->gain) &&
     (n < 2 || sscanf(argv[1], "%lf", &opt1))  &&
     (n < 3 || sscanf(argv[2], "%lf", &opt2))  &&
     (n < 4))
  do {
    if (opt1 != HUGE_VAL)
    {
      if (opt1 > 1)
      {
        p->fc = opt1;
        isFcSet = true;
      }
      else if (opt1 > 0)
      {
        p->oomph = opt1;
      }
      else break; /* error */
      if (opt2 != HUGE_VAL)
      {
        if (opt2 > 1)
        {
          if (isFcSet) break; /* error */
          p->fc = opt2;
        }
        else if (opt2 > 0)
        {
          if (!isFcSet) break; /* error */
          p->oomph = opt2;
        }
        else break; /* error */
      }
    }
    if (dcNormalise)
    {
      p->gain = -p->gain;
    }
    return ST_SUCCESS;
  } while (0);

  st_fail(effp->h->usage);
  return ST_EOF;
}



static int st_bass_getopts  (eff_t e, int n, char **a) {return getopts(e, n, a,  100, 0);}
static int st_treble_getopts(eff_t e, int n, char **a) {return getopts(e, n, a, 3000, 1);}



static int st_biquad_shelf_start(eff_t effp)
{
  biquad_t p = (biquad_t) effp->priv;

  /* Calculate intermediates: */
  double A  = exp(p->gain / 40 * log(10));
  double w0 = 2 * M_PI * p->fc / effp->ininfo.rate;
  double alpha = sin(w0)/2 * sqrt( (A + 1/A)*(1/p->oomph - 1) + 2 );
  double a0;

  if (p->gain == 0)
    return ST_EFF_NULL;

  /* Calculate filter coefficients: */
  p->b0 =    A*( (A+1) - (A-1)*cos(w0) + 2*sqrt(A)*alpha );  /* Numerator. */
  p->b1 =  2*A*( (A-1) - (A+1)*cos(w0)                   );
  p->b2 =    A*( (A+1) - (A-1)*cos(w0) - 2*sqrt(A)*alpha );
     a0 =        (A+1) + (A-1)*cos(w0) + 2*sqrt(A)*alpha;    /* Denominator. */
  p->a1 =   -2*( (A-1) + (A+1)*cos(w0)                   );
  p->a2 =        (A+1) + (A-1)*cos(w0) - 2*sqrt(A)*alpha;

  /* Simplify: */
  p->b2 = p->b2/a0;
  p->b1 = p->b1/a0;
  p->b0 = p->b0/a0;
  p->a2 = p->a2/a0;
  p->a1 = p->a1/a0;

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
      "title('SoX effect: %s gain=%g centre=%g slope=%g (rate=%u)')\n"
      "xlabel('Frequency (Hz)')\n"
      "ylabel('Amplitude Response (dB)')\n"
      "Fs=%u;minF=10;maxF=Fs/2;\n"
      "axis([minF maxF -25 25])\n"
      "sweepF=logspace(log10(minF),log10(maxF),200);\n"
      "grid on\n"
      "[h,w]=freqz([%f %f %f],[1 %f %f],sweepF,Fs);\n"
      "semilogx(w,20*log10(h),'b')\n"
      "pause\n"
      , effp->name, p->gain, p->fc, p->oomph
      , effp->ininfo.rate, effp->ininfo.rate
      , p->b0, p->b1, p->b2, p->a1, p->a2
      );
    exit(0);
  }
  return ST_SUCCESS;
}



static st_effect_t st_bass_effect = {
  "bass",
  "Usage: bass gain [frequency] [slope]",
  0,
  st_bass_getopts,
  st_biquad_shelf_start,
  st_biquad_flow,
  st_effect_nothing_drain,
  st_effect_nothing
};



st_effect_t const * st_bass_effect_fn(void)
{
  return &st_bass_effect;
}



static st_effect_t st_treble_effect = {
  "treble",
  "Usage: treble gain [frequency] [slope]",
  0,
  st_treble_getopts,
  st_biquad_shelf_start,
  st_biquad_flow,
  st_effect_nothing_drain,
  st_effect_nothing
};



st_effect_t const * st_treble_effect_fn(void)
{
  return &st_treble_effect;
}
