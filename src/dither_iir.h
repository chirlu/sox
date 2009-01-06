#define _ output += p->coefs[j] * p->previous_errors[p->pos + j] \
                  - p->coefs[N + j] * p->previous_outputs[p->pos + j], ++j;
static int NAME(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t len = *isamp = *osamp = min(*isamp, *osamp);

  while (len--) {
    double r = p->am0 * RANQD1 + p->am1 * RANQD1;
    double error, d, output = 0;
    int j = 0;
    CONVOLVE
    assert(j == N);
    d = *ibuf++ - output;
    *obuf = d + r < 0? d + r <= SOX_SAMPLE_MIN - .5? SOX_SAMPLE_MIN: d + r - .5 
          : d + r >= SOX_SAMPLE_MAX - (1 << (31-PREC)) + 0.5?
          SOX_SAMPLE_MAX - (1 << (31-PREC)): d + r + .5;
    error = ((*obuf++ + (1 << (31-PREC))) & (-1 << (32-PREC))) - d;
    p->pos = p->pos? p->pos - 1 : p->pos - 1 + N;
    p->previous_errors[p->pos + N] = p->previous_errors[p->pos] = error;
    p->previous_outputs[p->pos + N] = p->previous_outputs[p->pos] = output;
  }
  return SOX_SUCCESS;
}
#undef CONVOLVE
#undef _
#undef NAME
#undef N
