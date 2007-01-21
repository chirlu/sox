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

/* Biquad filter common definitions   (c) 2006-7 robs@users.sourceforge.net */

#ifndef biquad_included
#define biquad_included

#include "st_i.h"


typedef enum {
  filter_LPF, 
  filter_HPF, 
  filter_BPF_CSG, 
  filter_BPF, 
  filter_notch, 
  filter_APF, 
  filter_peakingEQ, 
  filter_lowShelf, 
  filter_highShelf,
  filter_LPF_1, 
  filter_HPF_1, 
  filter_BPF_SPK, 
  filter_BPF_SPK_N,
  filter_AP1,
  filter_AP2,
  filter_deemph
} filter_t;


typedef enum {
  width_bw_Hz,
  /* The old, non-RBJ, non-freq-warped band-pass/reject response;
   * leaving here for now just in case anybody misses it: */
  width_bw_old,
  width_bw_oct,
  width_Q,
  width_slope
} width_t;


/* Private data for the biquad filter effects */
typedef struct biquad
{
  double gain;             /* For EQ filters */
  double fc;               /* Centre/corner/cutoff frequency */
  double width;            /* Filter width; interpreted as per width_type */
  width_t width_type;      

  filter_t filter_type;

  double b2, b1, b0;       /* Filter coefficients */
  double a2, a1, a0;       /* Filter coefficients */

  st_sample_t i1, i2;      /* Filter memory */
  double      o1, o2;      /* Filter memory */
} * biquad_t;


assert_static(sizeof(struct biquad) <= ST_MAX_EFFECT_PRIVSIZE, 
    /* else */ biquad_PRIVSIZE_too_big);

int st_biquad_getopts(eff_t effp, int n, char **argv,
    int min_args, int max_args, int fc_pos, int width_pos, int gain_pos,
    char const * allowed_width_types, filter_t filter_type);
int st_biquad_start(eff_t effp);
int st_biquad_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                        st_size_t *isamp, st_size_t *osamp);

#undef st_fail
#define st_fail st_message_filename=effp->name,st_fail

#endif
