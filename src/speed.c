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

/* Sound Tools Effect: Adjust the audio speed (pitch and tempo together)
 *
 * (c) 2006 robs@users.sourceforge.net
 *
 * Adjustment is given as the ratio of the new speed to the old speed, or as
 * a number of cents (100ths of a semitone) to change.  Speed change is
 * actually performed by whichever resampling effect is in effect.
 */

#include "st_i.h"
#include <math.h>
#include <string.h>

static int getopts(eff_t effp, int n, char * * argv)
{
  st_bool is_cents = st_false;
  double speed;

  /* Be quietly compatible with the old speed effect: */
  if (n != 0 && strcmp(*argv, "-c") == 0)
    is_cents = st_true, ++argv, --n;

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

st_effect_t const *st_speed_effect_fn(void)
{
  static st_effect_t driver = {
    "speed", "Usage: speed factor[c]", ST_EFF_NULL, getopts, 0, 0, 0, 0, 0};
  return &driver;
}
