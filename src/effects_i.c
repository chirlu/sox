/* Implements a libSoX internal interface for implementing effects.
 * All public functions & data are prefixed with lsx_ .
 *
 * (c) 2005-8 Chris Bagwell and SoX contributors
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

#undef lsx_fail
#define lsx_fail sox_globals.subsystem=effp->handler.name,lsx_fail

int lsx_usage(sox_effect_t * effp)
{
  if (effp->handler.usage)
    lsx_fail("usage: %s", effp->handler.usage);
  else
    lsx_fail("this effect takes no parameters");
  return SOX_EOF;
}

char * lsx_usage_lines(char * * usage, char const * const * lines, size_t n)
{
  if (!*usage) {
    size_t i, len;
    for (len = i = 0; i < n; len += strlen(lines[i++]) + 1);
    *usage = lsx_malloc(len);
    strcpy(*usage, lines[0]);
    for (i = 1; i < n; ++i) {
      strcat(*usage, "\n");
      strcat(*usage, lines[i]);
    }
  }
  return *usage;
}

/* here for linear interp.  might be useful for other things */
unsigned lsx_gcd(unsigned a, unsigned b)
{
  if (b == 0)
    return a;
  else
    return lsx_gcd(b, a % b);
}

unsigned lsx_lcm(unsigned a, unsigned b)
{
  /* parenthesize this way to avoid unsigned overflow in product term */
  return a * (b / lsx_gcd(a, b));
}

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

lsx_enum_item const lsx_wave_enum[] = {
  LSX_ENUM_ITEM(SOX_WAVE_,SINE)
  LSX_ENUM_ITEM(SOX_WAVE_,TRIANGLE)
  {0, 0}};

void lsx_generate_wave_table(
    lsx_wave_t wave_type,
    sox_data_t data_type,
    void *table,
    size_t table_size,
    double min,
    double max,
    double phase)
{
  uint32_t t;
  uint32_t phase_offset = phase / M_PI / 2 * table_size + 0.5;

  for (t = 0; t < table_size; t++)
  {
    uint32_t point = (t + phase_offset) % table_size;
    double d;
    switch (wave_type)
    {
      case SOX_WAVE_SINE:
      d = (sin((double)point / table_size * 2 * M_PI) + 1) / 2;
      break;

      case SOX_WAVE_TRIANGLE:
      d = (double)point * 2 / table_size;
      switch (4 * point / table_size)
      {
        case 0:         d = d + 0.5; break;
        case 1: case 2: d = 1.5 - d; break;
        case 3:         d = d - 1.5; break;
      }
      break;

      default: /* Oops! FIXME */
        d = 0.0; /* Make sure we have a value */
      break;
    }
    d  = d * (max - min) + min;
    switch (data_type)
    {
      case SOX_FLOAT:
        {
          float *fp = (float *)table;
          *fp++ = (float)d;
          table = fp;
          continue;
        }
      case SOX_DOUBLE:
        {
          double *dp = (double *)table;
          *dp++ = d;
          table = dp;
          continue;
        }
      default: break;
    }
    d += d < 0? -0.5 : +0.5;
    switch (data_type)
    {
      case SOX_SHORT:
        {
          short *sp = table;
          *sp++ = (short)d;
          table = sp;
          continue;
        }
      case SOX_INT:
        {
          int *ip = table;
          *ip++ = (int)d;
          table = ip;
          continue;
        }
      default: break;
    }
  }
}

/*
 * lsx_parsesamples
 *
 * Parse a string for # of samples.  If string ends with a 's'
 * then the string is interpreted as a user calculated # of samples.
 * If string contains ':' or '.' or if it ends with a 't' then its
 * treated as an amount of time.  This is converted into seconds and
 * fraction of seconds and then use the sample rate to calculate
 * # of samples.
 * Returns NULL on error, pointer to next char to parse otherwise.
 */
char const * lsx_parsesamples(sox_rate_t rate, const char *str0, size_t *samples, int def)
{
  int i, found_samples = 0, found_time = 0;
  char const * end;
  char const * pos;
  sox_bool found_colon, found_dot;
  char * str = (char *)str0;

  for (;*str == ' '; ++str);
  for (end = str; *end && strchr("0123456789:.ets", *end); ++end);
  if (end == str)
    return NULL;

  pos = strchr(str, ':');
  found_colon = pos && pos < end;

  pos = strchr(str, '.');
  found_dot = pos && pos < end;

  if (found_colon || found_dot || *(end-1) == 't')
    found_time = 1;
  else if (*(end-1) == 's')
    found_samples = 1;

  if (found_time || (def == 't' && !found_samples)) {
    for (*samples = 0, i = 0; *str != '.' && i < 3; ++i) {
      char * last_str = str;
      long part = strtol(str, &str, 10);
      if (!i && str == last_str)
        return NULL;
      *samples += rate * part;
      if (i < 2) {
        if (*str != ':')
          break;
        ++str;
        *samples *= 60;
      }
    }
    if (*str == '.') {
      char * last_str = str;
      double part = strtod(str, &str);
      if (str == last_str)
        return NULL;
      *samples += rate * part + .5;
    }
    return *str == 't'? str + 1 : str;
  }
  {
    char * last_str = str;
    double part = strtod(str, &str);
    if (str == last_str)
      return NULL;
    *samples = part + .5;
    return *str == 's'? str + 1 : str;
  }
}

#if 0

#define TEST(st, samp, len) \
  str = st; \
  next = lsx_parsesamples(10000, str, &samples, 't'); \
  assert(samples == samp && next == str + len);

int main(int argc, char * * argv)
{
  char const * str, * next;
  size_t samples;

  TEST("0"  , 0, 1)
  TEST("1" , 10000, 1)

  TEST("0s" , 0, 2)
  TEST("0s,", 0, 2)
  TEST("0s/", 0, 2)
  TEST("0s@", 0, 2)

  TEST("0t" , 0, 2)
  TEST("0t,", 0, 2)
  TEST("0t/", 0, 2)
  TEST("0t@", 0, 2)

  TEST("1s" , 1, 2)
  TEST("1s,", 1, 2)
  TEST("1s/", 1, 2)
  TEST("1s@", 1, 2)
  TEST(" 01s" , 1, 4)
  TEST("1e6s" , 1000000, 4)

  TEST("1t" , 10000, 2)
  TEST("1t,", 10000, 2)
  TEST("1t/", 10000, 2)
  TEST("1t@", 10000, 2)
  TEST("1.1t" , 11000, 4)
  TEST("1.1t,", 11000, 4)
  TEST("1.1t/", 11000, 4)
  TEST("1.1t@", 11000, 4)
  TEST("1e6t" , 10000, 1)

  TEST(".0", 0, 2)
  TEST("0.0", 0, 3)
  TEST("0:0.0", 0, 5)
  TEST("0:0:0.0", 0, 7)

  TEST(".1", 1000, 2)
  TEST(".10", 1000, 3)
  TEST("0.1", 1000, 3)
  TEST("1.1", 11000, 3)
  TEST("1:1.1", 611000, 5)
  TEST("1:1:1.1", 36611000, 7)
  TEST("1:1", 610000, 3)
  TEST("1:01", 610000, 4)
  TEST("1:1:1", 36610000, 5)
  TEST("1:", 600000, 2)
  TEST("1::", 36000000, 3)

  TEST("0.444444", 4444, 8)
  TEST("0.555555", 5556, 8)

  assert(!lsx_parsesamples(10000, "x", &samples, 't'));
  return 0;
}
#endif 

/* a note is given as an int,
 * 0   => 440 Hz = A
 * >0  => number of half notes 'up',
 * <0  => number of half notes down,
 * example 12 => A of next octave, 880Hz
 *
 * calculated by freq = 440Hz * 2**(note/12)
 */
static double calc_note_freq(double note)
{
  return 440.0 * pow(2.0, note / 12.0);
}

/* Read string 'text' and convert to frequency.
 * 'text' can be a positive number which is the frequency in Hz.
 * If 'text' starts with a hash '%' and a following number the corresponding
 * note is calculated.
 * Return -1 on error.
 */
double lsx_parse_frequency(char const * text, char * * end_ptr)
{
  double result;

  if (*text == '%') {
    result = strtod(text + 1, end_ptr);
    if (*end_ptr == text + 1)
      return -1;
    return calc_note_freq(result);
  }
  result = strtod(text, end_ptr);
  if (end_ptr) {
    if (*end_ptr == text)
      return -1;
    if (**end_ptr == 'k') {
      result *= 1000;
      ++*end_ptr;
    }
  }
  return result < 0 ? -1 : result;
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
int * lsx_fft_br;
double * lsx_fft_sc;

static void update_fft_cache(int len)
{
  static int n;

  if (!len) {
    free(lsx_fft_br);
    free(lsx_fft_sc);
    lsx_fft_sc = NULL;
    lsx_fft_br = NULL;
    n = 0;
    return;
  }
  if (len > n) {
    int old_n = n;
    n = len;
    lsx_fft_br = lsx_realloc(lsx_fft_br, dft_br_len(n) * sizeof(*lsx_fft_br));
    lsx_fft_sc = lsx_realloc(lsx_fft_sc, dft_sc_len(n) * sizeof(*lsx_fft_sc));
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
}

void lsx_safe_cdft(int len, int type, double * d)
{
  assert(is_power_of_2(len));
  update_fft_cache(len);
  lsx_cdft(len, type, d, lsx_fft_br, lsx_fft_sc);
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

