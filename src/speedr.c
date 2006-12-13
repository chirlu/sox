/*  Sound Tools Effect: speedr
 *
 *  (c) 2006 robs@users.sourceforge.net
 *
 *  See LICENSE file for further copyright information.
 */

static char const usage[] =
  "\n"
  "Usage: speedr factor[c]\n"
  "\n"
  "Use resampling to adjust the audio  speed  (pitch  and  tempo\n"
  "together).  ‘factor’ is  either the ratio of the new speed to\n"
  "the old speed: > 1 speeds up, < 1 slows down, or, if appended\n"
  "with  ‘c’,  the number of cents (i.e 100ths of a semitone) by\n"
  "which  the  pitch  (and  tempo)  should  be  adjusted:  >   0\n"
  "increases, < 0 decreases.\n"
  "   By  default, the speed change is performed by the resample\n"
  "effect with  its  default  parameters.   For  higher  quality\n"
  "resampling,  in addition to the speedr effect, specify either\n"
  "the resample or the rabbit effect  with  appropriate  parame-\n"
  "ters.";

#include "st_i.h"
#include <math.h>
#include <string.h>

static int getopts(eff_t effp, int n, char * * argv)
{
  bool is_cents = false;
  double speed;

  /* Be quietly compatible with the old speed effect: */
  if (n != 0 && strcmp(*argv, "-c") == 0)
    is_cents = true, ++argv, --n;

  if (n == 1) {
    char c;
    int scanned = sscanf(*argv, "%lf%c%*c", &speed, &c);
    if (scanned == 1 || (scanned == 2 && c == 'c')) {
      is_cents |= scanned == 2;
      if (is_cents || speed > 0) {
        effp->globalinfo->speed *= is_cents? pow(2, speed/1200) : speed;
        return ST_SUCCESS;
      }
    }
  }
  st_fail(effp->h->usage);
  return ST_EOF;
}

st_effect_t const * st_speedr_effect_fn(void)
{
  static st_effect_t driver = {"speedr", usage, ST_EFF_NULL, getopts};
  return &driver;
}
