/* libSoX effect: Voice Activity Detector  (c) 2009 robs@users.sourceforge.net
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
#include "sgetopt.h"
#include <string.h>

typedef struct {
  double    * dft_buf, * noise_buf, * spectrum, * meas_buf, mean_meas;
} chan_t;

typedef struct {                /* Configuration parameters: */
  double    noise_tc_up, noise_tc_down, noise_reduction_amount;
  double    measure_freq, measure_duration, measure_tc, pre_trigger_time;
  double    hp_filter_freq, lp_filter_freq, hp_lifter_freq, lp_lifter_freq;
  double    trigger_tc, trigger_level1, search_time, gap_time;
                                /* Working variables: */
  sox_sample_t  * buffer;
  unsigned  dft_len, buffer_len, buffer_ptr, flush_done, gap_count;
  unsigned  measure_period_len, measure_len, search_count, search_ptr;
  unsigned  spectrum_start, spectrum_end, cepstrum_start, cepstrum_end;
  int       measure_timer, booting;
  double    measure_tc_mult, trigger_meas_tc_mult;
  double    noise_tc_up_mult, noise_tc_down_mult;
  double    * spectrum_window, * cepstrum_window;
  chan_t    * channels;
} priv_t;

#define GETOPT_FREQ(c, name, min) \
    case c: p->name = lsx_parse_frequency(lsx_optarg, &parse_ptr); \
      if (p->name < min || *parse_ptr) return lsx_usage(effp); \
      break;

static int create(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *)effp->priv;
  #define opt_str "+N:n:r:f:m:M:h:l:H:L:T:t:s:g:p:"
  int c;

  p->noise_tc_up      = .1;
  p->noise_tc_down    = .01;
  p->noise_reduction_amount = 1.35;

  p->measure_freq     = 20;
  p->measure_duration = 2 / p->measure_freq;
  p->measure_tc       = .4;

  p->hp_filter_freq   = 50;
  p->lp_filter_freq   = 6000;
  p->hp_lifter_freq   = 150;
  p->lp_lifter_freq   = 2000;

  p->trigger_tc       = .25;
  p->trigger_level1   = 7;

  p->search_time      = 1;
  p->gap_time         = .25;

  while ((c = lsx_getopt(argc, argv, opt_str)) != -1) switch (c) {
    char * parse_ptr;
    GETOPT_NUMERIC('N', noise_tc_up     ,  .1 , 10)
    GETOPT_NUMERIC('n', noise_tc_down   ,.001 , .1)
    GETOPT_NUMERIC('r', noise_reduction_amount   ,0 , 2)
    GETOPT_NUMERIC('f', measure_freq    ,   5 , 50)
    GETOPT_NUMERIC('m', measure_duration, .01 , 1)
    GETOPT_NUMERIC('M', measure_tc      ,  .1 , 1)
    GETOPT_FREQ(   'h', hp_filter_freq  ,  10)
    GETOPT_FREQ(   'l', lp_filter_freq  ,  1000)
    GETOPT_FREQ(   'H', hp_lifter_freq  ,  10)
    GETOPT_FREQ(   'L', lp_lifter_freq  ,  1000)
    GETOPT_NUMERIC('T', trigger_tc      , .01 , 1)
    GETOPT_NUMERIC('t', trigger_level1  ,   0 , 10)
    GETOPT_NUMERIC('s', search_time     ,  .1 , 4)
    GETOPT_NUMERIC('g', gap_time        ,  .1 , 1)
    GETOPT_NUMERIC('p', pre_trigger_time,   0 , 4)
    default: lsx_fail("invalid option `-%c'", optopt); return lsx_usage(effp);
  }
  return lsx_optind !=argc? lsx_usage(effp) : SOX_SUCCESS;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  unsigned i, pre_trigger_len, search_len;

  pre_trigger_len = p->pre_trigger_time * effp->in_signal.rate + .5;
  pre_trigger_len *= effp->in_signal.channels;

  p->measure_len = effp->in_signal.rate * p->measure_duration + .5;
  p->measure_len *= effp->in_signal.channels;

  p->measure_period_len = effp->in_signal.rate / p->measure_freq + .5;
  p->measure_period_len *= effp->in_signal.channels;
  p->search_count = ceil(p->search_time * p->measure_freq);
  search_len = p->search_count * p->measure_period_len;
  p->gap_count = p->gap_time * p->measure_freq + .5;

  p->buffer_len = pre_trigger_len + p->measure_len + search_len;
  lsx_Calloc(p->buffer, p->buffer_len);

  for (p->dft_len = 16; p->dft_len < p->measure_len; p->dft_len <<= 1);
  lsx_debug("dft_len=%u measure_len=%u", p->dft_len, p->measure_len);

  lsx_Calloc(p->channels, effp->in_signal.channels);
  for (i = 0; i < effp->in_signal.channels; ++i) {
    chan_t * c = &p->channels[i];
    lsx_Calloc(c->dft_buf, p->dft_len);
    lsx_Calloc(c->spectrum, p->dft_len);
    lsx_Calloc(c->noise_buf, p->dft_len);
    lsx_Calloc(c->meas_buf, p->search_count);
  }

  lsx_Calloc(p->spectrum_window, p->measure_len);
  for (i = 0; i < p->measure_len; ++i)
    p->spectrum_window[i] = -2. / SOX_SAMPLE_MIN / sqrt((double)p->measure_len);
  lsx_apply_hann(p->spectrum_window, (int)p->measure_len);

  p->spectrum_start = p->hp_filter_freq / effp->in_signal.rate * p->dft_len + .5;
  p->spectrum_start = max(p->spectrum_start, 1);
  p->spectrum_end = p->lp_filter_freq / effp->in_signal.rate * p->dft_len + .5;
  p->spectrum_end = min(p->spectrum_end, p->dft_len / 2);

  lsx_Calloc(p->cepstrum_window, p->spectrum_end - p->spectrum_start);
  for (i = 0; i < p->spectrum_end - p->spectrum_start; ++i)
    p->cepstrum_window[i] = 2 / sqrt((double)p->spectrum_end - p->spectrum_start);
  lsx_apply_hann(p->cepstrum_window, (int)(p->spectrum_end - p->spectrum_start));
  
  p->cepstrum_start = ceil(effp->in_signal.rate * .5 / p->lp_lifter_freq);
  p->cepstrum_end = floor(effp->in_signal.rate * .5 / p->hp_lifter_freq);
  p->cepstrum_end = min(p->cepstrum_end, p->dft_len / 4);
  if (p->cepstrum_end <= p->cepstrum_start)
    return SOX_EOF;

  p->noise_tc_up_mult     = exp(-1 / (p->noise_tc_up   * p->measure_freq));
  p->noise_tc_down_mult   = exp(-1 / (p->noise_tc_down * p->measure_freq));
  p->measure_tc_mult      = exp(-1 / (p->measure_tc    * p->measure_freq));
  p->trigger_meas_tc_mult = exp(-1 / (p->trigger_tc    * p->measure_freq));

  p->measure_timer = -p->measure_len;
  p->flush_done = p->buffer_ptr = 0;
  return SOX_SUCCESS;
}

static int flow_flush(sox_effect_t * effp, sox_sample_t const * ibuf,
    sox_sample_t * obuf, size_t * ilen, size_t * olen)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t odone = min(p->buffer_len - p->flush_done, *olen);
  size_t odone1 = min(odone, p->buffer_len - p->buffer_ptr);

  memcpy(obuf, p->buffer + p->buffer_ptr, odone1 * sizeof(*obuf));
  if ((p->buffer_ptr += odone1) == p->buffer_len) {
    memcpy(obuf + odone1, p->buffer, (odone - odone1) * sizeof(*obuf));
    p->buffer_ptr = odone - odone1;
  }
  if ((p->flush_done += odone) == p->buffer_len) {
    size_t olen1 = *olen - odone;
    (effp->handler.flow = lsx_flow_copy)(effp, ibuf, obuf +odone, ilen, &olen1);
    odone += olen1;
  }
  else *ilen = 0;
  *olen = odone;
  return SOX_SUCCESS;
}

static double measure(
    priv_t * p, chan_t * c, size_t index, size_t step, int booting)
{
  double mult, result = 0;
  size_t i;

  for (i = 0; i < p->measure_len; ++i, index = (index + step) % p->buffer_len)
    c->dft_buf[i] = p->buffer[index] * p->spectrum_window[i];
  memset(c->dft_buf + i, 0, (p->dft_len - i) * sizeof(*c->dft_buf));
  lsx_safe_rdft((int)p->dft_len, 1, c->dft_buf);

  memset(c->dft_buf, 0, p->spectrum_start * sizeof(*c->dft_buf));
  for (i = p->spectrum_start; i < p->spectrum_end; ++i) {
    double d = sqrt(sqr(c->dft_buf[2 * i]) + sqr(c->dft_buf[2 * i + 1]));
    mult = booting >= 0? booting / (1. + booting) : p->measure_tc_mult;
    c->spectrum[i] = c->spectrum[i] * mult + d * (1 - mult);
    d = sqr(c->spectrum[i]);
    mult = booting >= 0? 0 :
        d > c->noise_buf[i]? p->noise_tc_up_mult : p->noise_tc_down_mult;
    c->noise_buf[i] = c->noise_buf[i] * mult + d * (1 - mult);
    d = sqrt(max(0, d - p->noise_reduction_amount * c->noise_buf[i]));
    c->dft_buf[i] = d * p->cepstrum_window[i - p->spectrum_start];
  }
  memset(c->dft_buf + i, 0, ((p->dft_len >> 1) - i) * sizeof(*c->dft_buf));
  lsx_safe_rdft((int)p->dft_len >> 1, 1, c->dft_buf);

  for (i = p->cepstrum_start; i < p->cepstrum_end; ++i)
    result += sqr(c->dft_buf[2 * i]) + sqr(c->dft_buf[2 * i + 1]);
  result = log(result / (p->cepstrum_end - p->cepstrum_start));
  return max(0, 21 + result);
}

static int flow_trigger(sox_effect_t * effp, sox_sample_t const * ibuf,
    sox_sample_t * obuf, size_t * ilen, size_t * olen)
{
  priv_t * p = (priv_t *)effp->priv;
  sox_bool triggered = sox_false;
  size_t i, idone = 0, to_flush = 0;

  while (idone < *ilen && !triggered) {
    p->measure_timer += effp->in_signal.channels;
    for (i = 0; i < effp->in_signal.channels; ++i, ++idone) {
      chan_t * c = &p->channels[i];
      p->buffer[p->buffer_ptr++] = *ibuf++;
      if (!p->measure_timer) {
        size_t x = (p->buffer_ptr + p->buffer_len - p->measure_len) % p->buffer_len;
        double meas = measure(p, c, x, effp->in_signal.channels, p->booting);
        c->meas_buf[p->search_ptr] = meas;
        c->mean_meas = c->mean_meas * p->trigger_meas_tc_mult +
            meas *(1 - p->trigger_meas_tc_mult);

        if (triggered |= c->mean_meas > p->trigger_level1) {
          unsigned n = p->search_count, ptr = p->search_ptr;
          unsigned j, trigger_j = n, zero_j = n;
          for (j = 0; j < n; ++j, ptr = (ptr + n - 1) % n)
            if (c->meas_buf[ptr] > p->trigger_level1 && j <= trigger_j + p->gap_count)
              zero_j = trigger_j = j;
            else if (!c->meas_buf[ptr] && trigger_j >= zero_j)
              zero_j = j;
          j = min(j, zero_j);
          to_flush = range_limit(j, to_flush, n);
        }
        lsx_debug_more("%12g %12g %u", meas, c->mean_meas, to_flush);
      }
    }
    if (p->buffer_ptr == p->buffer_len)
      p->buffer_ptr = 0;
    if (!p->measure_timer) {
      p->measure_timer = -p->measure_period_len;
      p->search_ptr = (p->search_ptr + 1) % p->search_count;
      if (p->booting >= 0)
        p->booting = p->booting == 6? -1 : p->booting + 1;
    }
  }
  if (triggered) {
    size_t ilen1 = *ilen - idone;
    p->flush_done = (p->search_count - to_flush) * p->measure_period_len;
    p->buffer_ptr = (p->buffer_ptr + p->flush_done) % p->buffer_len;
    (effp->handler.flow = flow_flush)(effp, ibuf, obuf, &ilen1, olen);
    idone += ilen1;
  }
  else *olen = 0;
  *ilen = idone;
  return SOX_SUCCESS;
}

static int drain(sox_effect_t * effp, sox_sample_t * obuf, size_t * olen)
{
  size_t ilen = 0;
  return effp->handler.flow(effp, NULL, obuf, &ilen, olen);
}

static int stop(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  unsigned i;

  for (i = 0; i < effp->in_signal.channels; ++i) {
    chan_t * c = &p->channels[i];
    free(c->meas_buf);
    free(c->noise_buf);
    free(c->spectrum);
    free(c->dft_buf);
  }
  free(p->channels);
  free(p->cepstrum_window);
  free(p->spectrum_window);
  free(p->buffer);
  return SOX_SUCCESS;
}

sox_effect_handler_t const * lsx_vad_effect_fn(void)
{
  static sox_effect_handler_t handler = {"vad", NULL,
    SOX_EFF_MCHAN | SOX_EFF_LENGTH | SOX_EFF_MODIFY,
    create, start, flow_trigger, drain, stop, NULL, sizeof(priv_t)
  };
  static char const * lines[] = {
    "[options]",
    "\t-N noise-tc-up              (0.1 s)",
    "\t-n noise-tc-down            (0.01 s)",
    "\t-r noise-reduction-amount   (1.35)",
    "\t-f measure-frequency        (20 Hz)",
    "\t-m measure-duration         (0.1 s)",
    "\t-M measure-tc               (0.4 s)",
    "\t-h high-pass-filter         (50 Hz)",
    "\t-l low-pass-filter          (6000 Hz)",
    "\t-H high-pass-lifter         (150 Hz)",
    "\t-L low-pass-lifter          (2000 Hz)",
    "\t-T trigger-time-constant    (0.25 s)",
    "\t-t trigger-level            (7)",
    "\t-s search-time              (1 s)",
    "\t-g allowed-gap              (0.25 s)",
    "\t-p pre-trigger-buffer       (0 s)",
  };
  static char * usage;
  handler.usage = lsx_usage_lines(&usage, lines, array_length(lines));
  return &handler;
}
