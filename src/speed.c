/*  Sound Tools Effect: Adjust the audio speed (pitch and tempo together)
 *
 *  (c) 2006 robs@users.sourceforge.net
 *
 *  See LICENSE file for further copyright information.
 *
 *  Adjustment is given as the ratio of the new speed to the old speed,
 *  or as a number of cents (100ths of a semitone) to change.  Speed change
 *  is actually performed by whichever resampling effect is in effect.
 */

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
    char c, dummy;
    int scanned = sscanf(*argv, "%lf%c %c", &speed, &c, &dummy);
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

st_effect_t const * st_speed_effect_fn(void)
{
  static st_effect_t driver = {
    "speed", "Usage: speed factor[c]", ST_EFF_NULL,
    getopts, NULL, NULL, NULL, NULL};
  return &driver;
}
