/*

    Butterworth effect file for SoX
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

    Code based on the butterworth implementation in

    Sound Processing Kit - A C++ Class Library for Audio Signal Processing
    Copyright (C) 1995-1998 Kai Lassfolk

    as described in

    Computer music: synthesis, composition, and performance
    Charles Dodge, Thomas A. Jerse
    [2nd ed.]
    Page 214

 */

int st_butterworth_start (eff_t effp);
int st_butterworth_flow (eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                         st_size_t *isamp, st_size_t *osamp);

typedef struct butterworth {
  double x [2];
  double y [2];

  double a [3];
  double b [2];

  /*
   * Cut off frequency for low-pass and high-pass,
   * center frequency for band-pass
   */
  double frequency;

  double bandwidth;
} *butterworth_t;
