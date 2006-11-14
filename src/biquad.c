#include "biquad.h"



int st_biquad_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                        st_size_t *isamp, st_size_t *osamp)
{
  biquad_t p = (biquad_t) effp->priv;
  st_size_t len = (*isamp > *osamp)? *osamp : *isamp;
  *isamp = *osamp = len;

  while (len--)
  {
    double o0 = *ibuf*p->b0 +p->i1*p->b1 +p->i2*p->b2 -p->o1*p->a1 -p->o2*p->a2;
    p->i2 = p->i1, p->i1 = *ibuf++;
    p->o2 = p->o1, p->o1 = o0;
    *obuf++ = ST_ROUND_CLIP_COUNT(o0, p->clippedCount);
  }
  return ST_SUCCESS;
}



int st_biquad_stop(eff_t effp) /* Stop processing, warn if overflows */
{
  biquad_t p = (biquad_t) effp->priv;
  if (p->clippedCount != 0)
  {
    st_warn("%s: %d samples clipped", effp->name, p->clippedCount);
  }
  return ST_SUCCESS;
}
