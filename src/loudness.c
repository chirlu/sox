/* Effect: loudness filter     Copyright (c) 2008 robs@users.sourceforge.net
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

#include "sox_i.h"
#include "fft4g.h"
#define  FIFO_SIZE_T int
#include "fifo.h"
#include <string.h>

typedef struct {
  int        dft_length, num_taps;
  double   * coefs;
} filter_t;

typedef struct {
  double     delta, start;
  int        n;
  size_t     samples_in, samples_out;
  fifo_t     input_fifo, output_fifo;
  filter_t   filter, * filter_ptr;
} priv_t;

static int create(sox_effect_t * effp, int argc, char **argv)
{
  priv_t * p = (priv_t *)effp->priv;
  p->filter_ptr = &p->filter;
  p->delta = -10;
  p->start = 65;
  p->n = 1023;
  do {                    /* break-able block */
    NUMERIC_PARAMETER(delta,-50 , 15) /* FIXME expand range */
    NUMERIC_PARAMETER(start, 50 , 75) /* FIXME expand range */
    NUMERIC_PARAMETER(n    ,127 ,2047)
  } while (0);
  p->n = 2 * p->n + 1;
  return argc?  lsx_usage(effp) : SOX_SUCCESS;
}

static double * make_filter(int n, double start, double delta, double rate)
{
  static const struct {double f, af, lu, tf;} iso226_table[] = {
    {   20,0.532,-31.6,78.5},{   25,0.506,-27.2,68.7},{ 31.5,0.480,-23.0,59.5},
    {   40,0.455,-19.1,51.1},{   50,0.432,-15.9,44.0},{   63,0.409,-13.0,37.5},
    {   80,0.387,-10.3,31.5},{  100,0.367, -8.1,26.5},{  125,0.349, -6.2,22.1},
    {  160,0.330, -4.5,17.9},{  200,0.315, -3.1,14.4},{  250,0.301, -2.0,11.4},
    {  315,0.288, -1.1, 8.6},{  400,0.276, -0.4, 6.2},{  500,0.267,  0.0, 4.4},
    {  630,0.259,  0.3, 3.0},{  800,0.253,  0.5, 2.2},{ 1000,0.250,  0.0, 2.4},
    { 1250,0.246, -2.7, 3.5},{ 1600,0.244, -4.1, 1.7},{ 2000,0.243, -1.0,-1.3},
    { 2500,0.243,  1.7,-4.2},{ 3150,0.243,  2.5,-6.0},{ 4000,0.242,  1.2,-5.4},
    { 5000,0.242, -2.1,-1.5},{ 6300,0.245, -7.1, 6.0},{ 8000,0.254,-11.2,12.6},
    {10000,0.271,-10.7,13.9},{12500,0.301, -3.1,12.3},
  };
  #define LEN (array_length(iso226_table) + 2)
  #define SPL(phon, t) (10 / t.af * log10(4.47e-3 * (pow(10., .025 * (phon)) - \
          1.15) + pow(.4 * pow(10., (t.tf + t.lu) / 10 - 9), t.af)) - t.lu + 94)
  double fs[LEN], spl[LEN], d[LEN], * work, * h;
  int i, work_len;
  
  fs[0] = log(1.);
  spl[0] = delta * .2;
  for (i = 0; i < (int)LEN - 2; ++i) {
    spl[i + 1] = SPL(start + delta, iso226_table[i]) -
                 SPL(start        , iso226_table[i]);
    fs[i + 1] = log(iso226_table[i].f);
  }
  fs[i + 1] = log(100000.);
  spl[i + 1] = spl[0];
  lsx_prepare_spline3(fs, spl, (int)LEN, HUGE_VAL, HUGE_VAL, d);

  for (work_len = 8192; work_len < rate / 2; work_len <<= 1);
  work = lsx_calloc(work_len, sizeof(*work));
  h = lsx_calloc(n, sizeof(*h));

  for (i = 1; i <= work_len / 2; ++i) {
    double f = rate * i / work_len;
    double spl1 = f < 1? spl[0] : lsx_spline3(fs, spl, d, (int)LEN, log(f));
    work[i < work_len / 2 ? 2 * i : 1] = dB_to_linear(spl1);
  }
  lsx_safe_rdft(work_len, -1, work);
  for (i = 0; i < n; ++i)
    h[i] = work[(work_len - n / 2 + i) % work_len] * 2. / work_len;
  lsx_apply_kaiser(h, n, lsx_kaiser_beta(40 + 2./3 * fabs(delta)));

  free(work);
  return h;
  #undef SPL
  #undef LEN
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;
  int i, half = p->n / 2, dft_length = lsx_set_dft_length(p->n);
  double * h;

  if (p->delta == 0)
    return SOX_EFF_NULL;

  if (!p->filter_ptr->num_taps) {
    h = make_filter(p->n, p->start, p->delta, effp->in_signal.rate);
    p->filter_ptr->coefs = lsx_calloc(dft_length, sizeof(*p->filter_ptr->coefs));
    for (i = 0; i < p->n; ++i)
      p->filter_ptr->coefs[(i + dft_length - p->n + 1) & (dft_length - 1)]
          = h[i] / dft_length * 2;
    free(h);
    p->filter_ptr->num_taps = p->n;
    p->filter_ptr->dft_length = dft_length;
    lsx_safe_rdft(dft_length, 1, p->filter_ptr->coefs);
  }
  fifo_create(&p->input_fifo, (int)sizeof(double));
  memset(fifo_reserve(&p->input_fifo, half), 0, sizeof(double) * half);
  fifo_create(&p->output_fifo, (int)sizeof(double));
  return SOX_SUCCESS;
}

static void filter(priv_t * p)
{
  int i, num_in = max(0, fifo_occupancy(&p->input_fifo));
  filter_t const * f = p->filter_ptr;
  int const overlap = f->num_taps - 1;
  double * output;

  while (num_in >= f->dft_length) {
    double const * input = fifo_read_ptr(&p->input_fifo);
    fifo_read(&p->input_fifo, f->dft_length - overlap, NULL);
    num_in -= f->dft_length - overlap;

    output = fifo_reserve(&p->output_fifo, f->dft_length);
    fifo_trim_by(&p->output_fifo, overlap);
    memcpy(output, input, f->dft_length * sizeof(*output));

    lsx_rdft(f->dft_length, 1, output, lsx_fft_br, lsx_fft_sc);
    output[0] *= f->coefs[0];
    output[1] *= f->coefs[1];
    for (i = 2; i < f->dft_length; i += 2) {
      double tmp = output[i];
      output[i  ] = f->coefs[i  ] * tmp - f->coefs[i+1] * output[i+1];
      output[i+1] = f->coefs[i+1] * tmp + f->coefs[i  ] * output[i+1];
    }
    lsx_rdft(f->dft_length, -1, output, lsx_fft_br, lsx_fft_sc);
  }
}

static int flow(sox_effect_t * effp, const sox_sample_t * ibuf,
                sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t i, odone = min(*osamp, (size_t)fifo_occupancy(&p->output_fifo));
  double const * s = fifo_read(&p->output_fifo, (int)odone, NULL);

  for (i = 0; i < odone; ++i)
    *obuf++ = SOX_FLOAT_64BIT_TO_SAMPLE(*s++, effp->clips);
  p->samples_out += odone;

  if (*isamp && odone < *osamp) {
    double * t = fifo_write(&p->input_fifo, (int)*isamp, NULL);
    p->samples_in += (int)*isamp;

    for (i = *isamp; i; --i)
      *t++ = SOX_SAMPLE_TO_FLOAT_64BIT(*ibuf++, effp->clips);
    filter(p);
  }
  else *isamp = 0;
  *osamp = odone;
  return SOX_SUCCESS;
}

static int drain(sox_effect_t * effp, sox_sample_t * obuf, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  static size_t isamp = 0;
  size_t samples_out = p->samples_in;
  size_t remaining = samples_out - p->samples_out;
  double * buff = lsx_calloc(1024, sizeof(*buff));

  if ((int)remaining > 0) {
    while ((size_t)fifo_occupancy(&p->output_fifo) < remaining) {
      fifo_write(&p->input_fifo, 1024, buff);
      p->samples_in += 1024;
      filter(p);
    }
    fifo_trim_to(&p->output_fifo, (int)remaining);
    p->samples_in = 0;
  }
  free(buff);
  return flow(effp, 0, obuf, &isamp, osamp);
}

static int stop(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;

  fifo_delete(&p->input_fifo);
  fifo_delete(&p->output_fifo);
  free(p->filter_ptr->coefs);
  memset(p->filter_ptr, 0, sizeof(*p->filter_ptr));
  return SOX_SUCCESS;
}

sox_effect_handler_t const * sox_loudness_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "loudness", "[gain [ref]]", 0,
    create, start, flow, drain, stop, NULL, sizeof(priv_t)
  };
  return &handler;
}
