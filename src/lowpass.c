/*

    Low-pass effect file for SoX
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

 */

#include <math.h>

#include "st_i.h"
#include "btrworth.h"

int st_lowpass_getopts(eff_t effp, int n, char **argv)
{
  butterworth_t butterworth = (butterworth_t)effp->priv;

  if (n != 1) {
    st_fail("Usage: lowpass FREQUENCY");
    return (ST_EOF);
  }

  st_butterworth_start (effp);

  if (!(sscanf (argv [0], "%lf", &butterworth->frequency))) {
    st_fail("lowpass: illegal frequency");
    return (ST_EOF);
  }
  return (ST_SUCCESS);
}

int st_lowpass_start(eff_t effp)
{
  butterworth_t butterworth = (butterworth_t) effp->priv;
  double c;

  c = 1.0 / tan (M_PI * butterworth->frequency / effp->ininfo.rate);

  butterworth->a [0] = 1.0 / (1.0 + sqrt(2.0) * c + c * c);
  butterworth->a [1] = 2.0 * butterworth->a [0];
  butterworth->a [2] = butterworth->a [0];

  butterworth->b [0] = 2 * (1.0 - c * c) * butterworth->a[0];
  butterworth->b [1] = (1.0 - sqrt(2.0) * c + c * c) * butterworth->a [0];
  return (ST_SUCCESS);
}
