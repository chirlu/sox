/*

  Band-pass effect file for SoX
  Copyright (C) 1999 Jan Paul Schmidt <jps@fundament.org>

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

  Code based on the band-pass implementation in

  Sound Processing Kit - A C++ Class Library for Audio Signal Processing
  Copyright (C) 1995-1998 Kai Lassfolk

  as described in

  Computer music: synthesis, composition, and performance
  Charles Dodge, Thomas A. Jerse
  [2nd ed.]
  Page 216

 */

#include <math.h>

#include "st_i.h"
#include "btrworth.h"

int st_bandpass_getopts (eff_t effp, int n, char **argv)
{
  butterworth_t butterworth = (butterworth_t)effp->priv;

  if (n != 2) {
    st_fail("Usage: bandpass FREQUENCY BANDWIDTH");
    return (ST_EOF);
  }

  st_butterworth_start (effp);

  if (!(sscanf (argv [0], "%lf", &butterworth->frequency))) {
    st_fail("bandpass: illegal frequency");
    return (ST_EOF);
  }

  if (!(sscanf (argv [1], "%lf", &butterworth->bandwidth))) {
    st_fail("bandpass: illegal bandwidth");
    return (ST_EOF);
  }
  return (ST_SUCCESS);
}

int st_bandpass_start (eff_t effp)
{
  butterworth_t butterworth = (butterworth_t) effp->priv;
  double c;
  double d;

  c = 1.0 / tan (M_PI * butterworth->bandwidth / effp->ininfo.rate);
  d = 2 * cos (2 * M_PI * butterworth->frequency / effp->ininfo.rate);

  butterworth->a [0] = 1.0 / (1.0 + c);
  butterworth->a [1] = 0.0;
  butterworth->a [2] = -butterworth->a [0];

  butterworth->b [0] = -c * d * butterworth->a [0];
  butterworth->b [1] = (c - 1.0) * butterworth->a[0];
  return (ST_SUCCESS);
}
