#define _ output += p->coefs[j] * p->previous_errors[p->pos + j] \
                  - p->coefs[N + j] * p->previous_outputs[p->pos + j], ++j;
static int NAME(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t len = *isamp = *osamp = min(*isamp, *osamp);
  int dummy = 0;

  while (len--) {
    double r = p->am0 * RAND_ + p->am1 * RAND_;
    double error, d, output = 0;
    int j = 0;
    CONVOLVE
    assert(j == N);
    d = *ibuf++ - output;
    *obuf = SOX_ROUND_CLIP_COUNT(d + r, dummy);
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
