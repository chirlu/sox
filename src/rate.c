/* Effect: change sample rate     Copyright (c) 2008 robs@users.sourceforge.net
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* Based upon the techniques described in `The Quest For The Perfect Resampler'
 * by Laurent De Soras; http://ldesoras.free.fr/doc/articles/resampler-en.pdf */

#ifdef NDEBUG /* Enable assert always. */
#undef NDEBUG /* Must undef above assert.h or other that might include it. */
#endif

#include "sox_i.h"
#include "fft4g.h"
#include "getopt.h"
#define  FIFO_SIZE_T int
#include "fifo.h"
#include <assert.h>
#include <string.h>

#define  calloc     lsx_calloc
#define  malloc     lsx_malloc
#define  raw_coef_t double
#define  sample_t   double
#define  TO_SOX     SOX_FLOAT_64BIT_TO_SAMPLE
#define  FROM_SOX   SOX_SAMPLE_TO_FLOAT_64BIT
#define  coef(coef_p, interp_order, fir_len, phase_num, coef_interp_num, fir_coef_num) coef_p[(fir_len) * ((interp_order) + 1) * (phase_num) + ((interp_order) + 1) * (fir_coef_num) + (interp_order - coef_interp_num)]

static sample_t * prepare_coefs(raw_coef_t const * coefs, int num_coefs,
    int num_phases, int interp_order, int multiplier)
{
  int i, j, length = num_coefs * num_phases;
  sample_t * result = malloc(length * (interp_order + 1) * sizeof(*result));
  double fm1 = coefs[0], f1 = 0, f2 = 0;

  for (i = num_coefs - 1; i >= 0; --i)
    for (j = num_phases - 1; j >= 0; --j) {
      double f0 = fm1, b = 0, c = 0, d = 0; /* = 0 to kill compiler warning */
      int pos = i * num_phases + j - 1;
      fm1 = (pos > 0 ? coefs[pos - 1] : 0) * multiplier;
      switch (interp_order) {
        case 1: b = f1 - f0; break;
        case 2: b = f1 - (.5 * (f2+f0) - f1) - f0; c = .5 * (f2+f0) - f1; break;
        case 3: c=.5*(f1+fm1)-f0;d=(1/6.)*(f2-f1+fm1-f0-4*c);b=f1-f0-d-c; break;
        default: if (interp_order) assert(0);
      }
      #define coef_coef(x) \
        coef(result, interp_order, num_coefs, j, x, num_coefs - 1 - i)
      coef_coef(0) = f0;
      if (interp_order > 0) coef_coef(1) = b;
      if (interp_order > 1) coef_coef(2) = c;
      if (interp_order > 2) coef_coef(3) = d;
      #undef coef_coef
      f2 = f1, f1 = f0;
    }
  return result;
}

typedef struct {
  int        dft_length, num_taps, post_peak;
  sample_t   * coefs;
} half_band_t;      /* Note: not half-band as in symmetric about Fn/2 (Fs/4) */

typedef struct {    /* Data that are shared between channels and filters */
  sample_t   * poly_fir_coefs;
  half_band_t half_band[2];    /* [0]: halve; [1]: down/up: halve/double */
} rate_shared_t;

struct stage;
typedef void (* stage_fn_t)(struct stage * input, fifo_t * output);
typedef struct stage {
  rate_shared_t * shared;
  fifo_t     fifo;
  int        pre;              /* Number of past samples to store */
  int        pre_post;         /* pre + number of future samples to store */
  int        preload;          /* Number of zero samples to pre-load the fifo */
  int        which;            /* Which of the 2 half-band filters to use */
  stage_fn_t fn;
                               /* For poly_fir & spline: */
  union {                      /* 32bit.32bit fixed point arithmetic */
    #if defined(WORDS_BIGENDIAN)
    struct {int32_t integer; uint32_t fraction;} parts;
    #else
    struct {uint32_t fraction; int32_t integer;} parts;
    #endif
    int64_t all;
    #define MULT32 (65536. * 65536.)
  } at, step;
  int        divisor;          /* For step: > 1 for rational; 1 otherwise */
  double     out_in_ratio;
} stage_t;

#define stage_occupancy(s) max(0, fifo_occupancy(&(s)->fifo) - (s)->pre_post)
#define stage_read_p(s) ((sample_t *)fifo_read_ptr(&(s)->fifo) + (s)->pre)

static void cubic_spline(stage_t * p, fifo_t * output_fifo)
{
  int i, num_in = stage_occupancy(p), max_num_out = 1 + num_in*p->out_in_ratio;
  sample_t const * input = stage_read_p(p);
  sample_t * output = fifo_reserve(output_fifo, max_num_out);

  for (i = 0; p->at.parts.integer < num_in; ++i, p->at.all += p->step.all) {
    sample_t const * s = input + p->at.parts.integer;
    sample_t x = p->at.parts.fraction * (1 / MULT32);
    sample_t b = .5*(s[1]+s[-1])-*s, a = (1/6.)*(s[2]-s[1]+s[-1]-*s-4*b);
    sample_t c = s[1]-*s-a-b;
    output[i] = ((a*x + b)*x + c)*x + *s;
  }
  assert(max_num_out - i >= 0);
  fifo_trim_by(output_fifo, max_num_out - i);
  fifo_read(&p->fifo, p->at.parts.integer, NULL);
  p->at.parts.integer = 0;
}

static void half_sample(stage_t * p, fifo_t * output_fifo)
{
  sample_t * output;
  int i, j, num_in = max(0, fifo_occupancy(&p->fifo));
  rate_shared_t const * s = p->shared;
  half_band_t const * f = &s->half_band[p->which];
  int const overlap = f->num_taps - 1;

  while (num_in >= f->dft_length) {
    sample_t const * input = fifo_read_ptr(&p->fifo);
    fifo_read(&p->fifo, f->dft_length - overlap, NULL);
    num_in -= f->dft_length - overlap;

    output = fifo_reserve(output_fifo, f->dft_length);
    fifo_trim_by(output_fifo, (f->dft_length + overlap) >> 1);
    memcpy(output, input, f->dft_length * sizeof(*output));

    lsx_rdft(f->dft_length, 1, output, lsx_fft_br, lsx_fft_sc);
    output[0] *= f->coefs[0];
    output[1] *= f->coefs[1];
    for (i = 2; i < f->dft_length; i += 2) {
      sample_t tmp = output[i];
      output[i  ] = f->coefs[i  ] * tmp - f->coefs[i+1] * output[i+1];
      output[i+1] = f->coefs[i+1] * tmp + f->coefs[i  ] * output[i+1];
    }
    lsx_rdft(f->dft_length, -1, output, lsx_fft_br, lsx_fft_sc);

    for (j = 1, i = 2; i < f->dft_length - overlap; ++j, i += 2)
      output[j] = output[i];
  }
}

static void double_sample(stage_t * p, fifo_t * output_fifo)
{
  sample_t * output;
  int i, j, num_in = max(0, fifo_occupancy(&p->fifo));
  rate_shared_t const * s = p->shared;
  half_band_t const * f = &s->half_band[1];
  int const overlap = f->num_taps - 1;

  while (num_in > f->dft_length >> 1) {
    sample_t const * input = fifo_read_ptr(&p->fifo);
    fifo_read(&p->fifo, (f->dft_length - overlap) >> 1, NULL);
    num_in -= (f->dft_length - overlap) >> 1;

    output = fifo_reserve(output_fifo, f->dft_length);
    fifo_trim_by(output_fifo, overlap);
    for (j = i = 0; i < f->dft_length; ++j, i += 2)
      output[i] = input[j], output[i+1] = 0;

    lsx_rdft(f->dft_length, 1, output, lsx_fft_br, lsx_fft_sc);
    output[0] *= f->coefs[0];
    output[1] *= f->coefs[1];
    for (i = 2; i < f->dft_length; i += 2) {
      sample_t tmp = output[i];
      output[i  ] = f->coefs[i  ] * tmp - f->coefs[i+1] * output[i+1];
      output[i+1] = f->coefs[i+1] * tmp + f->coefs[i  ] * output[i+1];
    }
    lsx_rdft(f->dft_length, -1, output, lsx_fft_br, lsx_fft_sc);
  }
}

static double * make_lpf(int num_taps, double Fc, double beta, double scale)
{
  double * h = malloc(num_taps * sizeof(*h)), sum = 0;
  int i, m = num_taps - 1;
  assert(Fc >= 0 && Fc <= 1);
  for (i = 0; i <= m / 2; ++i) {
    double x = M_PI * (i - .5 * m), y = 2. * i / m - 1;
    h[i] = x? sin(Fc * x) / x : Fc;
    sum += h[i] *= lsx_bessel_I_0(beta * sqrt(1 - y * y));
    if (m - i != i)
      sum += h[m - i] = h[i];
  }
  for (i = 0; i < num_taps; ++i) h[i] *= scale / sum;
  return h;
}

#define TO_6dB .5869
#define TO_3dB ((2/3.) * (.5 + TO_6dB))
#define MAX_TBW0 36.
#define MAX_TBW0A (MAX_TBW0 / (1 + TO_3dB))
#define MAX_TBW3 floor(MAX_TBW0 * TO_3dB)
#define MAX_TBW3A floor(MAX_TBW0A * TO_3dB)

static double * design_lpf(
    double Fp,      /* End of pass-band; ~= 0.01dB point */
    double Fc,      /* Start of stop-band */
    double Fn,      /* Nyquist freq; e.g. 0.5, 1, PI */
    sox_bool allow_aliasing,
    double att,     /* Stop-band attenuation in dB */
    int * num_taps, /* (Single phase.)  0: value will be estimated */
    int k)          /* Number of phases; 0 for single-phase */
{
  double tr_bw, beta;

  if (allow_aliasing)
    Fc += (Fc - Fp) * TO_3dB;
  Fp /= Fn, Fc /= Fn;        /* Normalise to Fn = 1 */
  tr_bw = TO_6dB * (Fc - Fp); /* Transition band-width: 6dB to stop points */

  if (*num_taps == 0) {        /* TODO this could be cleaner, esp. for k != 0 */
    double n160 = (.0425* att - 1.4) /  tr_bw;    /* Half order for att = 160 */
    int n = n160 * (16.556 / (att - 39.6) + .8625) + .5;  /* For att [80,160) */
    *num_taps = k? 2 * n : 2 * (n + (n & 1)) + 1; /* =1 %4 (0 phase 1/2 band) */
  }
  assert(att >= 80);
  beta = att < 100 ? .1102 * (att - 8.7) : .1117 * att - 1.11;
  if (k)
    *num_taps = *num_taps * k - 1;
  else k = 1;
  return make_lpf(*num_taps, (Fc - tr_bw) / k, beta, (double)k);
}

static void fir_to_phase(double * * h, int * len,
    int * post_len, double phase0)
{
  double * work, phase = (phase0 > 50 ? 100 - phase0 : phase0) / 50;
  int work_len, begin, end, peak = 0, i = *len;

  for (work_len = 32; i > 1; work_len <<= 1, i >>= 1);
  work = calloc(work_len, sizeof(*work));
  for (i = 0; i < *len; ++i) work[i] = (*h)[i];

  lsx_safe_rdft(work_len, 1, work); /* Cepstral: */
  work[0] = log(fabs(work[0])), work[1] = log(fabs(work[1]));
  for (i = 2; i < work_len; i += 2) {
    work[i] = log(sqrt(sqr(work[i]) + sqr(work[i + 1])));
    work[i + 1] = 0;
  }
  lsx_safe_rdft(work_len, -1, work);
  for (i = 0; i < work_len; ++i) work[i] *= 2. / work_len;
  for (i = 1; i < work_len / 2; ++i) { /* Window to reject acausal components */
    work[i] *= 2;
    work[i + work_len / 2] = 0;
  }
  lsx_safe_rdft(work_len, 1, work);

  /* Some filters require phase unwrapping at this point.  Ours give dis-
   * continuities only in the stop band, so no need to unwrap in this case. */

  for (i = 2; i < work_len; i += 2) /* Interpolate between linear & min phase */
    work[i + 1] = phase * M_PI * .5 * i + (1 - phase) * work[i + 1];

  work[0] = exp(work[0]), work[1] = exp(work[1]);
  for (i = 2; i < work_len; i += 2) {
    double x = exp(work[i]);
    work[i    ] = x * cos(work[i + 1]);
    work[i + 1] = x * sin(work[i + 1]);
  }
  lsx_safe_rdft(work_len, -1, work);
  for (i = 0; i < work_len; ++i) work[i] *= 2. / work_len;

  for (i = 1; i < work_len; ++i) if (work[i] > work[peak])  /* Find peak pos. */
    peak = i;                                           /* N.B. peak val. > 0 */

  if (phase == 0)
    begin = 0;
  else if (phase == 1)
    begin = 1 + (work_len - *len) / 2;
  else {
    if (peak < work_len / 4) { /* Low phases can wrap impulse, so unwrap: */
      memmove(work + work_len / 4, work, work_len / 2 * sizeof(*work));
      memmove(work, work + work_len * 3 / 4, work_len / 4 * sizeof(*work));
      peak += work_len / 4;
    }
    begin = (.997 - (2 - phase) * .22) * *len + .5;
    end   = (.997 + (0 - phase) * .22) * *len + .5;
    begin = peak - begin - (begin & 1);
    end   = peak + 1 + end + (end & 1);
    *len = end - begin;
    *h = realloc(*h, *len * sizeof(**h));
  }
  for (i = 0; i < *len; ++i)
    (*h)[i] = work[begin + (phase0 > 50 ? *len - 1 - i : i)];
  *post_len = phase0 > 50 ? peak - begin : begin + *len - (peak + 1);
  free(work);
}

static void half_band_filter_init(rate_shared_t * p, unsigned which,
    int num_taps, sample_t const h[], double Fp, double atten, int multiplier,
    double phase, sox_bool allow_aliasing)
{
  half_band_t * f = &p->half_band[which];
  int dft_length, i;

  if (f->num_taps)
    return;
  if (h) {
    dft_length = lsx_set_dft_length(num_taps);
    f->coefs = calloc(dft_length, sizeof(*f->coefs));
    for (i = 0; i < num_taps; ++i)
      f->coefs[(i + dft_length - num_taps + 1) & (dft_length - 1)]
          = h[abs(num_taps / 2 - i)] / dft_length * 2 * multiplier;
    f->post_peak = num_taps / 2;
  }
  else {
    /* Adjustment to negate att degradation with intermediate phase */
    double att = phase && phase != 50 && phase != 100? atten * (34./33) : atten;
    double * h = design_lpf(Fp, 1., 2., allow_aliasing, att, &num_taps, 0);

    if (phase != 50)
      fir_to_phase(&h, &num_taps, &f->post_peak, phase);
    else f->post_peak = num_taps / 2;

    dft_length = lsx_set_dft_length(num_taps);
    f->coefs = calloc(dft_length, sizeof(*f->coefs));
    for (i = 0; i < num_taps; ++i)
      f->coefs[(i + dft_length - num_taps + 1) & (dft_length - 1)]
          = h[i] / dft_length * 2 * multiplier;
    free(h);
  }
  assert(num_taps & 1);
  f->num_taps = num_taps;
  f->dft_length = dft_length;
  lsx_debug("fir_len=%i dft_length=%i Fp=%g atten=%g mult=%i",
      num_taps, dft_length, Fp, atten, multiplier);
  lsx_safe_rdft(dft_length, 1, f->coefs);
}

#include "rate_filters.h"

typedef struct {
  double     factor;
  size_t     samples_in, samples_out;
  int        level, input_stage_num, output_stage_num;
  sox_bool   upsample;
  stage_t    * stages;
} rate_t;

#define pre_stage p->stages[-1]
#define last_stage p->stages[p->level]
#define post_stage p->stages[p->level + 1]

typedef enum {Default = -1, Quick, Low, Medium, High, Very} quality_t;

static void rate_init(rate_t * p, rate_shared_t * shared, double factor,
    quality_t quality, int interp_order, double phase, double bandwidth,
    sox_bool allow_aliasing)
{
  int i, mult, divisor = 1;

  assert(factor > 0);
  p->factor = factor;
  if (quality < Quick || quality > Very)
    quality = High;
  if (quality != Quick) {
    const int max_divisor = 2048;      /* Keep coef table size ~< 500kb */
    const double epsilon = 4 / MULT32; /* Scaled to half this at max_divisor */
    p->upsample = p->factor < 1;
    for (i = factor, p->level = 0; i >>= 1; ++p->level); /* log base 2 */
    factor /= 1 << (p->level + !p->upsample);
    for (i = 2; i <= max_divisor && divisor == 1; ++i) {
      double try_d = factor * i;
      int try = try_d + .5;
      if (fabs(try - try_d) < try * epsilon * (1 - (.5 / max_divisor) * i)) {
        if (try == i)  /* Rounded to 1:1? */
          factor = 1, divisor = 2, p->upsample = sox_false;
        else factor = try, divisor = i;
      }
    }
  }
  p->stages = (stage_t *)calloc((size_t)p->level + 4, sizeof(*p->stages)) + 1;
  for (i = -1; i <= p->level + 1; ++i) p->stages[i].shared = shared;
  last_stage.step.all = factor * MULT32 + .5;
  last_stage.out_in_ratio = MULT32 * divisor / last_stage.step.all;

  if (divisor != 1)
    assert(!last_stage.step.parts.fraction);
  else if (quality != Quick)
    assert(!last_stage.step.parts.integer);
  lsx_debug("i/o=%g; %.9g:%i @ level %i", p->factor, factor, divisor, p->level);

  mult = 1 + p->upsample; /* Compensate for zero-stuffing in double_sample */
  p->input_stage_num = -p->upsample;
  p->output_stage_num = p->level;
  if (quality == Quick) {
    ++p->output_stage_num;
    last_stage.fn = cubic_spline;
    last_stage.pre_post = max(3, last_stage.step.parts.integer);
    last_stage.preload = last_stage.pre = 1;
  }
  else if (last_stage.out_in_ratio != 2 || (p->upsample && quality == Low)) {
    poly_fir_t const * f;
    poly_fir1_t const * f1;
    int n = 4 * p->upsample + range_limit(quality, Medium, Very) - Medium;
    if (interp_order < 0)
      interp_order = quality > High;
    interp_order = divisor == 1? 1 + interp_order : 0;
    last_stage.divisor = divisor;
    p->output_stage_num += 2;
    if (p->upsample && quality == Low)
      mult = 1, ++p->input_stage_num, --p->output_stage_num, --n;
    f = &poly_firs[n];
    f1 = &f->interp[interp_order];
    if (!last_stage.shared->poly_fir_coefs) {
      int num_taps = 0, phases = divisor == 1? (1 << f1->phase_bits) : divisor;
      raw_coef_t * coefs =
          design_lpf(f->pass, f->stop, 1., sox_false, f->att, &num_taps, phases);
      assert(num_taps == f->num_coefs * phases - 1);
      last_stage.shared->poly_fir_coefs =
          prepare_coefs(coefs, f->num_coefs, phases, interp_order, mult);
      lsx_debug("fir_len=%i phases=%i coef_interp=%i mult=%i size=%s",
          f->num_coefs, phases, interp_order, mult,
          lsx_sigfigs3((num_taps + 1) * (interp_order + 1) * sizeof(sample_t)));
      free(coefs);
    }
    last_stage.fn = f1->fn;
    last_stage.pre_post = f->num_coefs - 1;
    last_stage.pre = 0;
    last_stage.preload = last_stage.pre_post >> 1;
    mult = 1;
  }
  if (quality > Low) {
    typedef struct {int len; sample_t const * h; double bw, a;} filter_t;
    static filter_t const filters[] = {
      {2 * array_length(half_fir_coefs_low) - 1, half_fir_coefs_low, 0,0},
      {0, NULL, .931, 110}, {0, NULL, .931, 125}, {0, NULL, .931, 170}};
    filter_t const * f = &filters[quality - Low];
    double att = allow_aliasing? (34./33)* f->a : f->a; /* negate att degrade */
    double bw = bandwidth? 1 - (1 - bandwidth / 100) / TO_3dB : f->bw;
    double min = 1 - (allow_aliasing? MAX_TBW0A : MAX_TBW0) / 100;
    assert((size_t)(quality - Low) < array_length(filters));
    half_band_filter_init(shared, p->upsample, f->len, f->h, bw, att, mult, phase, allow_aliasing);
    if (p->upsample) {
      pre_stage.fn = double_sample; /* Finish off setting up pre-stage */
      pre_stage.preload = shared->half_band[1].post_peak >> 1;
       /* Start setting up post-stage; TODO don't use dft for short filters */
      if ((1 - p->factor) / (1 - bw) > 2)
        half_band_filter_init(shared, 0, 0, NULL, max(p->factor, min), att, 1, phase, allow_aliasing);
      else shared->half_band[0] = shared->half_band[1];
    }
    else if (p->level > 0 && p->output_stage_num > p->level) {
      double pass = bw * divisor / factor / 2;
      if ((1 - pass) / (1 - bw) > 2)
        half_band_filter_init(shared, 1, 0, NULL, max(pass, min), att, 1, phase, allow_aliasing);
    }
    post_stage.fn = half_sample;
    post_stage.preload = shared->half_band[0].post_peak;
  }
  else if (quality == Low && !p->upsample) {    /* dft is slower here, so */
    post_stage.fn = half_sample_low;            /* use normal convolution */
    post_stage.pre_post = 2 * (array_length(half_fir_coefs_low) - 1);
    post_stage.preload = post_stage.pre = post_stage.pre_post >> 1;
  }
  if (p->level > 0) {
    stage_t * s = & p->stages[p->level - 1];
    if (shared->half_band[1].num_taps) {
      s->fn = half_sample;
      s->preload = shared->half_band[1].post_peak;
      s->which = 1;
    }
    else *s = post_stage;
  }
  for (i = p->input_stage_num; i <= p->output_stage_num; ++i) {
    stage_t * s = &p->stages[i];
    if (i >= 0 && i < p->level - 1) {
      s->fn = half_sample_25;
      s->pre_post = 2 * (array_length(half_fir_coefs_25) - 1);
      s->preload = s->pre = s->pre_post >> 1;
    }
    fifo_create(&s->fifo, (int)sizeof(sample_t));
    memset(fifo_reserve(&s->fifo, s->preload), 0, sizeof(sample_t)*s->preload);
    if (i < p->output_stage_num)
      lsx_debug("stage=%-3ipre_post=%-3ipre=%-3ipreload=%i",
          i, s->pre_post, s->pre, s->preload);
  }
}

static void rate_process(rate_t * p)
{
  stage_t * stage = p->stages + p->input_stage_num;
  int i;

  for (i = p->input_stage_num; i < p->output_stage_num; ++i, ++stage)
    stage->fn(stage, &(stage+1)->fifo);
}

static sample_t * rate_input(rate_t * p, sample_t const * samples, size_t n)
{
  p->samples_in += n;
  return fifo_write(&p->stages[p->input_stage_num].fifo, (int)n, samples);
}

static sample_t const * rate_output(rate_t * p, sample_t * samples, size_t * n)
{
  fifo_t * fifo = &p->stages[p->output_stage_num].fifo;
  p->samples_out += *n = min(*n, (size_t)fifo_occupancy(fifo));
  return fifo_read(fifo, (int)*n, samples);
}

static void rate_flush(rate_t * p)
{
  fifo_t * fifo = &p->stages[p->output_stage_num].fifo;
  size_t samples_out = p->samples_in / p->factor + .5;
  size_t remaining = samples_out - p->samples_out;
  sample_t * buff = calloc(1024, sizeof(*buff));

  if ((int)remaining > 0) {
    while ((size_t)fifo_occupancy(fifo) < remaining) {
      rate_input(p, buff, (size_t) 1024);
      rate_process(p);
    }
    fifo_trim_to(fifo, (int)remaining);
    p->samples_in = 0;
  }
  free(buff);
}

static void rate_close(rate_t * p)
{
  rate_shared_t * shared = p->stages[0].shared;
  int i;

  for (i = p->input_stage_num; i <= p->output_stage_num; ++i)
    fifo_delete(&p->stages[i].fifo);
  free(shared->half_band[0].coefs);
  if (shared->half_band[1].coefs != shared->half_band[0].coefs)
    free(shared->half_band[1].coefs);
  free(shared->poly_fir_coefs);
  memset(shared, 0, sizeof(*shared));
  free(p->stages - 1);
}

/*------------------------------- SoX Wrapper --------------------------------*/

typedef struct {
  sox_rate_t      out_rate;
  int             quality;
  double          coef_interp, phase, bandwidth;
  sox_bool        allow_aliasing;
  rate_t          rate;
  rate_shared_t   shared, * shared_ptr;
} priv_t;

static int create(sox_effect_t * effp, int argc, char **argv)
{
  priv_t * p = (priv_t *) effp->priv;
  int c;
  char * dummy_p, * found_at, * opts = "+i:b:p:MILasqlmhv", * qopts = opts +12;

  p->quality = -1;
  p->phase = 50;
  p->shared_ptr = &p->shared;

  while ((c = getopt(argc, argv, opts)) != -1) switch (c) {
    GETOPT_NUMERIC('i', coef_interp, 1 , 3)
    GETOPT_NUMERIC('p', phase,  0 , 100)
    GETOPT_NUMERIC('b', bandwidth,  100 - MAX_TBW3, 99.7)
    case 'M': p->phase =  0; break;
    case 'I': p->phase = 25; break;
    case 'L': p->phase = 50; break;
    case 's': p->bandwidth = 99; break;
    case 'a': p->allow_aliasing = sox_true; break;
    default: if ((found_at = strchr(qopts, c))) p->quality = found_at - qopts;
      else {lsx_fail("unknown option `-%c'", optopt); return lsx_usage(effp);}
  }
  argc -= optind, argv += optind;

  if ((unsigned)p->quality < 2 && (p->bandwidth || p->phase != 50 || p->allow_aliasing)) {
    lsx_fail("override options not allowed with this quality level");
    return SOX_EOF;
  }

  if (p->bandwidth && p->bandwidth < 100 - MAX_TBW3A && p->allow_aliasing) {
    lsx_fail("minimum allowed bandwidth with aliasing is %g%%", 100 - MAX_TBW3A);
    return SOX_EOF;
  }

  if (argc) {
    if ((p->out_rate = lsx_parse_frequency(*argv, &dummy_p)) <= 0 || *dummy_p)
      return lsx_usage(effp);
    argc--; argv++;
    effp->out_signal.rate = p->out_rate;
  }
  return argc? lsx_usage(effp) : SOX_SUCCESS;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;
  double out_rate = p->out_rate != 0 ? p->out_rate : effp->out_signal.rate;

  if (effp->in_signal.rate == out_rate)
    return SOX_EFF_NULL;

  effp->out_signal.channels = effp->in_signal.channels;
  effp->out_signal.rate = out_rate;
  rate_init(&p->rate, p->shared_ptr, effp->in_signal.rate / out_rate,
      p->quality, (int)p->coef_interp - 1, p->phase, p->bandwidth, p->allow_aliasing);
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_sample_t * ibuf,
                sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t i, odone = *osamp;

  sample_t const * s = rate_output(&p->rate, NULL, &odone);
  for (i = 0; i < odone; ++i) *obuf++ = TO_SOX(*s++, effp->clips);

  if (*isamp && odone < *osamp) {
    sample_t * t = rate_input(&p->rate, NULL, *isamp);
    for (i = *isamp; i; --i) *t++ = FROM_SOX(*ibuf++, effp->clips);
    rate_process(&p->rate);
  }
  else *isamp = 0;
  *osamp = odone;
  return SOX_SUCCESS;
}

static int drain(sox_effect_t * effp, sox_sample_t * obuf, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  static size_t isamp = 0;
  rate_flush(&p->rate);
  return flow(effp, 0, obuf, &isamp, osamp);
}

static int stop(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;
  rate_close(&p->rate);
  return SOX_SUCCESS;
}

sox_effect_handler_t const * sox_rate_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "rate", 0, SOX_EFF_RATE | SOX_EFF_GETOPT,
    create, start, flow, drain, stop, 0, sizeof(priv_t)
  };
  static char const * lines[] = {
    "[-q|-l|-m|-h|-v] [override-options] RATE[k]",
    "                    BAND-",
    "     QUALITY        WIDTH  REJ dB   TYPICAL USE",
    " -q  quick          n/a  ~30 @ Fs/4 playback on ancient hardware",
    " -l  low            80%     100     playback on old hardware",
    " -m  medium         95%     100     audio playback",
    " -h  high (default) 95%     125     16-bit mastering (use with dither)",
    " -v  very high      95%     175     24-bit mastering",
    "              OVERRIDE OPTIONS (only with -m, -h, -v)",
    " -M/-I/-L     Phase response = minimum/intermediate/linear(default)",
    " -s           Steep filter (band-width = 99%)",
    " -a           Allow aliasing above the pass-band",
    " -b 74-99.7   Any band-width %",
    " -p 0-100     Any phase response (0 = minimum, 25 = intermediate,",
    "              50 = linear, 100 = maximum)",
  };
  static char * usage;
  handler.usage = lsx_usage_lines(&usage, lines, array_length(lines));
  return &handler;
}
