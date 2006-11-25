#ifndef biquad_included
#define biquad_included

#include <math.h>
#include "st_i.h"

/* Private data for the biquad filter effects */
typedef struct biquad
{
  double gain;
  double fc;               /* Centre/corner/cutoff frequency */
  double oomph;            /* Q, BW, or Slope */
  bool dcNormalise;        /* A treble filter should normalise at DC */

  double b2, b1, b0;       /* Filter coefficients */
  double a2, a1;           /* Filter coefficients; a0 = 1 */

  st_sample_t i1, i2;      /* Filter memory */
  double o1, o2;           /* Filter memory */
} * biquad_t;

int st_biquad_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                        st_size_t *isamp, st_size_t *osamp);

assert_static(sizeof(struct biquad) <= ST_MAX_EFFECT_PRIVSIZE, 
    /* else */ biquad_PRIVSIZE_too_big);

#endif
