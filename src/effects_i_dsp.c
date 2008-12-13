/* libSoX internal DSP functions.
 * All public functions & data are prefixed with lsx_ .
 *
 * Copyright (c) 2008 robs@users.sourceforge.net
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

#ifdef NDEBUG /* Enable assert always. */
#undef NDEBUG /* Must undef above assert.h or other that might include it. */
#endif

#include "sox_i.h"
#include <assert.h>
#include <string.h>

/* Numerical Recipes cubic spline */

void lsx_prepare_spline3(double const * x, double const * y, int n,
    double start_1d, double end_1d, double * y_2d)
{
  double p, qn, sig, un, * u = lsx_malloc((n - 1) * sizeof(*u));
  int i;

  if (start_1d == HUGE_VAL)
    y_2d[0] = u[0] = 0;      /* Start with natural spline or */
  else {                     /* set the start first derivative */
    y_2d[0] = -.5;
    u[0] = (3 / (x[1] - x[0])) * ((y[1] - y[0]) / (x[1] - x[0]) - start_1d);
  }

  for (i = 1; i < n - 1; ++i) {
    sig = (x[i] - x[i - 1]) / (x[i + 1] - x[i - 1]);
    p = sig * y_2d[i - 1] + 2;
    y_2d[i] = (sig - 1) / p;
    u[i] = (y[i + 1] - y[i]) / (x[i + 1] - x[i]) -
           (y[i] - y[i - 1]) / (x[i] - x[i - 1]);
    u[i] = (6 * u[i] / (x[i + 1] - x[i - 1]) - sig * u[i - 1]) / p;
  }
  if (end_1d == HUGE_VAL)
    qn = un = 0;             /* End with natural spline or */
  else {                     /* set the end first derivative */
    qn = .5;
    un = 3 / (x[n - 1] - x[n - 2]) * (end_1d - (y[n - 1] - y[n - 2]) / (x[n - 1] - x[n - 2]));
  }
  y_2d[n - 1] = (un - qn * u[n - 2]) / (qn * y_2d[n - 2] + 1);
  for (i = n - 2; i >= 0; --i)
    y_2d[i] = y_2d[i] * y_2d[i + 1] + u[i];
  free(u);
}

double lsx_spline3(double const * x, double const * y, double const * y_2d,
    int n, double x1)
{
  int     t, i[2] = {0, 0};
  double  d, a, b;

  for (i[1] = n - 1; i[1] - i[0] > 1; t = (i[1] + i[0]) >> 1, i[x[t] > x1] = t);
  d = x[i[1]] - x[i[0]];
  assert(d != 0);
  a = (x[i[1]] - x1) / d;
  b = (x1 - x[i[0]]) / d;
  return a * y[i[0]] + b * y[i[1]] +
    ((a * a * a - a) * y_2d[i[0]] + (b * b * b - b) * y_2d[i[1]]) * d * d / 6;
}

double lsx_bessel_I_0(double x)
{
  double term = 1, sum = 1, last_sum, x2 = x / 2;
  int i = 1;
  do {
    double y = x2 / i++;
    last_sum = sum, sum += term *= y * y;
  } while (sum != last_sum);
  return sum;
}

int lsx_set_dft_length(int num_taps) /* Set to 4 x nearest power of 2 */
{
  int result, n = num_taps;
  for (result = 8; n > 2; result <<= 1, n >>= 1);
  result = range_limit(result, 4096, 131072);
  assert(num_taps * 2 < result);
  return result;
}

#include "fft4g.h"
int    * lsx_fft_br;
double * lsx_fft_sc;
static int fft_len;
#ifdef HAVE_OPENMP
static omp_lock_t fft_cache_lock;
#endif

void init_fft_cache(void)
{
  omp_init_lock(&fft_cache_lock);
}

void clear_fft_cache(void)
{
  omp_destroy_lock(&fft_cache_lock);
  free(lsx_fft_br);
  free(lsx_fft_sc);
  lsx_fft_sc = NULL;
  lsx_fft_br = NULL;
  fft_len = 0;
}

static void update_fft_cache(int len)
{
  omp_set_lock(&fft_cache_lock);
  if (len > fft_len) {
    int old_n = fft_len;
    fft_len = len;
    lsx_fft_br = lsx_realloc(lsx_fft_br, dft_br_len(fft_len) * sizeof(*lsx_fft_br));
    lsx_fft_sc = lsx_realloc(lsx_fft_sc, dft_sc_len(fft_len) * sizeof(*lsx_fft_sc));
    if (!old_n)
      lsx_fft_br[0] = 0;
  }
}

static sox_bool is_power_of_2(int x)
{
  return !(x < 2 || (x & (x - 1)));
}

void lsx_safe_rdft(int len, int type, double * d)
{
  assert(is_power_of_2(len));
  update_fft_cache(len);
  lsx_rdft(len, type, d, lsx_fft_br, lsx_fft_sc);
  omp_unset_lock(&fft_cache_lock);
}

void lsx_safe_cdft(int len, int type, double * d)
{
  assert(is_power_of_2(len));
  update_fft_cache(len);
  lsx_cdft(len, type, d, lsx_fft_br, lsx_fft_sc);
  omp_unset_lock(&fft_cache_lock);
}

void lsx_power_spectrum(int n, double const * in, double * out)
{
  int i;
  double * work = lsx_memdup(in, n * sizeof(*work));
  lsx_safe_rdft(n, 1, work);
  out[0] = sqr(work[0]);
  for (i = 2; i < n; i += 2)
    out[i >> 1] = sqr(work[i]) + sqr(work[i + 1]);
  out[i >> 1] = sqr(work[1]);
  free(work);
}

void lsx_power_spectrum_f(int n, float const * in, float * out)
{
  int i;
  double * work = lsx_malloc(n * sizeof(*work));
  for (i = 0; i< n; ++i) work[i] = in[i];
  lsx_safe_rdft(n, 1, work);
  out[0] = sqr(work[0]);
  for (i = 2; i < n; i += 2)
    out[i >> 1] = sqr(work[i]) + sqr(work[i + 1]);
  out[i >> 1] = sqr(work[1]);
  free(work);
}

void lsx_apply_hann_f(float h[], const int num_points)
{
  int i, m = num_points - 1;
  for (i = 0; i < num_points; ++i) {
    double x = 2 * M_PI * i / m;
    h[i] *= .5 - .5 * cos(x);
  }
}

void lsx_apply_hann(double h[], const int num_points)
{
  int i, m = num_points - 1;
  for (i = 0; i < num_points; ++i) {
    double x = 2 * M_PI * i / m;
    h[i] *= .5 - .5 * cos(x);
  }
}

void lsx_apply_hamming(double h[], const int num_points)
{
  int i, m = num_points - 1;
  for (i = 0; i < num_points; ++i) {
    double x = 2 * M_PI * i / m;
    h[i] *= .53836 - .46164 * cos(x);
  }
}

void lsx_apply_bartlett(double h[], const int num_points)
{
  int i, m = num_points - 1;
  for (i = 0; i < num_points; ++i) {
    h[i] *= 2. / m * (m / 2. - fabs(i - m / 2.));
  }
}

void lsx_apply_blackman(double h[], const int num_points, double alpha /*.16*/)
{
  int i, m = num_points - 1;
  for (i = 0; i < num_points; ++i) {
    double x = 2 * M_PI * i / m;
    h[i] *= (1 - alpha) *.5 - .5 * cos(x) + alpha * .5 * cos(2 * x);
  }
}

void lsx_apply_blackman_nutall(double h[], const int num_points)
{
  int i, m = num_points - 1;
  for (i = 0; i < num_points; ++i) {
    double x = 2 * M_PI * i / m;
    h[i] *= .3635819 - .4891775 * cos(x) + .1365995 * cos(2 * x) - .0106411 * cos(3 * x);
  }
}

double lsx_kaiser_beta(double att)
{
  if (att > 100  ) return .1117 * att - 1.11;
  if (att > 50   ) return .1102 * (att - 8.7);
  if (att > 20.96) return .58417 * pow(att -20.96, .4) + .07886 * (att - 20.96);
  return 0;
}

void lsx_apply_kaiser(double h[], const int num_points, double beta)
{
  int i, m = num_points - 1;
  for (i = 0; i <= m; ++i) {
    double x = 2. * i / m - 1;
    h[i] *= lsx_bessel_I_0(beta * sqrt(1 - x * x)) / lsx_bessel_I_0(beta);
  }
}

double * lsx_make_lpf(int num_taps, double Fc, double beta, double scale)
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

double * lsx_design_lpf(
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
    Fc += (Fc - Fp) * LSX_TO_3dB;
  Fp /= Fn, Fc /= Fn;        /* Normalise to Fn = 1 */
  tr_bw = LSX_TO_6dB * (Fc - Fp); /* Transition band-width: 6dB to stop points */

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
  return lsx_make_lpf(*num_taps, (Fc - tr_bw) / k, beta, (double)k);
}

void lsx_fir_to_phase(double * * h, int * len,
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
