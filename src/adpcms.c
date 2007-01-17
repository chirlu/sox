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

/* ADPCM CODECs: IMA, OKI.   (c) 2007 robs@users.sourceforge.net */

#include "adpcms.h"
#include "st_i.h"

#define range_limit(x,min,max)((x)<(min)?(min):(x)>(max)?(max):(x))

static int const ima_steps[89] = { /* ~16-bit precision */
  7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
  50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
  253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
  1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
  3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
  11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
  32767 };

static int const oki_steps[49] = { /* ~12-bit precision */
  256, 272, 304, 336, 368, 400, 448, 496, 544, 592, 656, 720, 800, 880, 960,
  1056, 1168, 1280, 1408, 1552, 1712, 1888, 2080, 2288, 2512, 2768, 3040, 3344,
  3680, 4048, 4464, 4912, 5392, 5936, 6528, 7184, 7904, 8704, 9568, 10528,
  11584, 12736, 14016, 15408, 16960, 18656, 20512, 22576, 24832 };

static int const step_changes[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

void adpcm_init(adpcm_t state, int type)
{
  state->last_output = 0;
  state->step_index = 0;
  state->max_step_index = type? 48 : 88;
  state->steps = type? oki_steps : ima_steps;
  state->mask = type? ~15 : ~0;
}

int adpcm_decode(int code, adpcm_t state)
{
  int s = ((code & 7) << 1) | 1;
  s = ((state->steps[state->step_index] * s) >> 3) & state->mask;
  if (code & 8)
    s = -s;
  s = state->last_output + s;
  s = range_limit(s, -0x8000, 0x7fff);
  state->step_index += step_changes[code & 0x07];
  state->step_index = range_limit(state->step_index, 0, state->max_step_index);
  return state->last_output = s;
}

int adpcm_encode(int sample, adpcm_t state)
{
  int delta = sample - state->last_output;
  int sign = 0;
  int code;
  if (delta < 0) {
    sign = 0x08;
    delta = -delta;
  }
  code = 4 * delta / state->steps[state->step_index];
  code = sign | min(code, 7);
  adpcm_decode(code, state); /* Update encoder state */
  return code;
}
