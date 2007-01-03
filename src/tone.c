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

/* Sound Tools Tone Control Effects: bass & treble
 *
 * The bass and treble effects approximate to the venerable Baxandall
 * circuit used in the analogue world.
 *
 * Filter design by Robert Bristow-Johnson <rbj@audioimagination.com>
 * see http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
 *
 * This implementation (c) 2006 robs@users.sourceforge.net
 */

#include "biquad.h"



static int getopts(eff_t effp, int n, char **argv, double fc, int dcNormalise)
{
  bool isFcSet = false;
  double opt1 = HUGE_VAL, opt2 = HUGE_VAL;
  biquad_t p = (biquad_t) effp->priv;
  int ret = ST_SUCCESS;
  char dummy;
  
  /* Initialise non-zero numbers: */
  p->dcNormalise = dcNormalise;
  p->fc = fc;
  p->width.slope = 0.5;

  /*
   * This block is a little complicated -- all because I wanted
   * to make it easy for the user i.e. order doesn't matter for
   * optional arguments.
   * This is made possible because slope <= 1 and by
   * insisting that the centre-frequency is > 1 (Hz).
   */
  if (n > 0 &&
     sscanf(argv[0], "%lf %c", &p->gain, &dummy) == 1 &&
     (n < 2 || sscanf(argv[1], "%lf %c", &opt1, &dummy) == 1)  &&
     (n < 3 || sscanf(argv[2], "%lf %c", &opt2, &dummy) == 1)  &&
     (n < 4)) {
    if (opt1 != HUGE_VAL) {
      if (opt1 <= 0)
        ret = ST_EOF;
      else {
        if (opt1 > 1) {
          p->fc = opt1;
          isFcSet = true;
        } else
          p->width.slope = opt1;
        if (opt2 != HUGE_VAL) {
          if (opt2 > 1) {
            if (isFcSet)
              ret = ST_EOF;
            else
              p->fc = opt2;
          } else if (opt2 > 0) {
            if (!isFcSet)
              ret = ST_EOF;
            else
              p->width.slope = opt2;
          } else
            ret = ST_EOF;
        }
      }
    }
    if (dcNormalise)
      p->gain = -p->gain;
  }

  if (ret == ST_EOF)
    st_fail(effp->h->usage);
  return ret;
}



static int bass_getopts  (eff_t e,int n,char **a){return getopts(e,n,a, 100,0);}
static int treble_getopts(eff_t e,int n,char **a){return getopts(e,n,a,3000,1);}



static int st_biquad_shelf_start(eff_t effp)
{
  biquad_t p = (biquad_t) effp->priv;

  /* Calculate intermediates: */
  double A  = exp(p->gain / 40 * log(10));
  double w0 = 2 * M_PI * p->fc / effp->ininfo.rate;
  double alpha = sin(w0)/2 * sqrt( (A + 1/A)*(1/p->width.slope - 1) + 2 );

  /* Calculate filter coefficients: */
  p->b0 =    A*( (A+1) - (A-1)*cos(w0) + 2*sqrt(A)*alpha );
  p->b1 =  2*A*( (A-1) - (A+1)*cos(w0)                   );
  p->b2 =    A*( (A+1) - (A-1)*cos(w0) - 2*sqrt(A)*alpha );
  p->a0 =        (A+1) + (A-1)*cos(w0) + 2*sqrt(A)*alpha;
  p->a1 =   -2*( (A-1) + (A+1)*cos(w0)                   );
  p->a2 =        (A+1) + (A-1)*cos(w0) - 2*sqrt(A)*alpha;

  return st_biquad_start(effp, "slope");
}



st_effect_t const * st_bass_effect_fn(void)
{
  static st_effect_t driver = {
    "bass",
    "Usage: bass gain [frequency] [slope]",
    0,
    bass_getopts,
    st_biquad_shelf_start,
    st_biquad_flow,
    st_effect_nothing_drain,
    st_effect_nothing,
    st_effect_nothing
  };
  return &driver;
}



st_effect_t const * st_treble_effect_fn(void)
{
  static st_effect_t driver = {
    "treble",
    "Usage: treble gain [frequency] [slope]",
    0,
    treble_getopts,
    st_biquad_shelf_start,
    st_biquad_flow,
    st_effect_nothing_drain,
    st_effect_nothing,
    st_effect_nothing
  };
  return &driver;
}
