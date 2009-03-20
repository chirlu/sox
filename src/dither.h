#ifdef IIR
#define _ output += p->coefs[j] * p->previous_errors[p->pos + j] \
                  - p->coefs[N + j] * p->previous_outputs[p->pos + j], ++j;
#else
#define _ d -= p->coefs[j] * p->previous_errors[p->pos + j], ++j;
#endif
static int NAME(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t len = *isamp = *osamp = min(*isamp, *osamp);

  while (len--) {
    if (!p->auto_detect || (p->history = ((p->history << 1) + !!(*ibuf & (-1u >> p->prec))))) {
      double d1, r = (RANQD1 >> p->prec) + (RANQD1 >> p->prec);
#ifdef IIR
      double d, output = 0;
#else
      double d = *ibuf++;
#endif 
      int j = 0;
      CONVOLVE
      assert(j == N);
      p->pos = p->pos? p->pos - 1 : p->pos - 1 + N;
#ifdef IIR
      d = *ibuf++ - output;
      p->previous_outputs[p->pos + N] = p->previous_outputs[p->pos] = output;
#endif
      d1 = (d + r) / (1 << (32 - p->prec));
      d1 = (int)(d1 < 0? d1 - .5 : d1 + .5);
      p->previous_errors[p->pos + N] = p->previous_errors[p->pos] =
          d1 * (1 << (32 - p->prec)) - d;
      *obuf = d1 < (-1 << (p->prec-1))? ++effp->clips, -1 << (p->prec-1) :
          d1 > SOX_INT_MAX(p->prec)? ++effp->clips, SOX_INT_MAX(p->prec) : d1;
      *obuf++ <<= 32 - p->prec;

      if (p->dither_off)
        lsx_debug("flow %u: on  @ %u", effp->flow, (unsigned)p->num_output);
      p->dither_off = sox_false;
    }
    else {
      *obuf++ = *ibuf++;
      if (!p->dither_off) {
        lsx_debug("flow %u: off @ %u", effp->flow, (unsigned)p->num_output);
        memset(p->previous_errors, 0, sizeof(p->previous_errors));
        memset(p->previous_outputs, 0, sizeof(p->previous_outputs));
      }
      p->dither_off = sox_true;
    }
    ++p->num_output;
  }
  return SOX_SUCCESS;
}
#undef CONVOLVE
#undef _
#undef NAME
#undef N
