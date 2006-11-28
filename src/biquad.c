#include "biquad.h"



int st_biquad_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
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
    *obuf++ = ST_EFF_ROUND_CLIP_COUNT(o0);
  }
  return ST_SUCCESS;
}
