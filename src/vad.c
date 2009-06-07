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
  double        last_meas;
  double        meas, slope1, slope2; /* TC -controlled */
} chan_t;

typedef struct {                /* Configuration parameters: */
  double        hp_freq, lp_freq, measure_freq, search_step_time;
  double        measure_duration, search_time, pre_trigger_time, trigger_level;
  double        trigger_tc, slope_tc1, slope_tc2;
                                /* Working variables: */
  sox_sample_t  * buffer;
  unsigned      search_len, buffer_len, buffer_ptr, flush_done, search_step_len;

  double        * dft_buf, * window1, * window2;
  unsigned      dft_len, measure_period, measure_timer, measure_len;
  chan_t        * channels;
  double        trigger_meas_tc_mult, trigger_slope_tc_mult1, trigger_slope_tc_mult2;
  double        search_slope_tc_mult1, search_slope_tc_mult2;
  unsigned      start_bin, end_bin;
} priv_t;

static int create(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *)effp->priv;
  int c;

  p->hp_freq          = 300;
  p->lp_freq          = 12500;
  p->measure_duration = .2;
  p->measure_freq     = 10;
  p->trigger_tc       = .2;
  p->trigger_level    = 33;
  p->search_time      = 1;
  p->search_step_time = .05;
  p->slope_tc1        = .35;
  p->slope_tc2        = .075;

  while ((c = lsx_getopt(argc, argv, "+h:l:m:f:T:t:s:q:S:F:p:")) != -1) switch (c) {
    char * parse_ptr;
    case 'h': p->hp_freq = lsx_parse_frequency(lsx_optarg, &parse_ptr);
      if (p->hp_freq < 10 || *parse_ptr) return lsx_usage(effp);
      break;
    case 'l': p->lp_freq = lsx_parse_frequency(lsx_optarg, &parse_ptr);
      if (p->lp_freq < 1000 || *parse_ptr) return lsx_usage(effp);
      break;
    GETOPT_NUMERIC('m', measure_duration,  .02, 2)
    GETOPT_NUMERIC('f', measure_freq    ,   1 ,100)
    GETOPT_NUMERIC('T', trigger_tc      , .001, 1)
    GETOPT_NUMERIC('t', trigger_level   ,   0, 100)
    GETOPT_NUMERIC('s', search_time     ,   0 , 4)
    GETOPT_NUMERIC('q', search_step_time, .002, .02)
    GETOPT_NUMERIC('S', slope_tc1       , .001, 1)
    GETOPT_NUMERIC('F', slope_tc2       , .001, 1)
    GETOPT_NUMERIC('p', pre_trigger_time,   0 , 4)
    default: lsx_fail("invalid option `-%c'", optopt); return lsx_usage(effp);
  }
  return lsx_optind !=argc? lsx_usage(effp) : SOX_SUCCESS;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  unsigned i;

  unsigned pre_trigger_len = p->pre_trigger_time * effp->in_signal.rate + .5;
  pre_trigger_len *= effp->in_signal.channels;

  p->measure_len = effp->in_signal.rate * p->measure_duration + .5;
  p->measure_len *= effp->in_signal.channels;
  p->search_step_len = effp->in_signal.rate * p->search_step_time + .5;
  p->search_step_len *= effp->in_signal.channels;

  p->search_len = p->search_time * effp->in_signal.rate + .5;
  p->search_len *= effp->in_signal.channels;
  p->search_len += p->measure_len;

  p->buffer_len = pre_trigger_len + p->search_len;
  p->buffer = lsx_calloc(p->buffer_len, sizeof(*p->buffer));

  for (p->dft_len = 16; p->dft_len < p->measure_len; p->dft_len <<= 1);
  p->dft_buf = lsx_calloc(p->dft_len, sizeof(*p->dft_buf));

  p->window1 = lsx_calloc(p->measure_len, sizeof(*p->window1));
  for (i = 0; i < p->measure_len; ++i)
    p->window1[i] = -2. / SOX_SAMPLE_MIN / p->measure_len;
  lsx_apply_hann(p->window1, (int)p->measure_len);

  p->start_bin = p->hp_freq / effp->in_signal.rate * p->dft_len + .5;
  p->end_bin = p->lp_freq / effp->in_signal.rate * p->dft_len + .5;
  p->end_bin = min(p->end_bin, p->dft_len / 2);
  p->window2 = lsx_calloc(p->end_bin - p->start_bin, sizeof(*p->window2));
  for (i = 0; i < p->end_bin - p->start_bin; ++i)
    p->window2[i] = 2 * (p->dft_len / 2 + 1.) / (p->end_bin - p->start_bin);
  lsx_apply_hann(p->window2, (int)(p->end_bin - p->start_bin));

  p->flush_done = p->buffer_ptr = 0;
  p->measure_period = effp->in_signal.rate / p->measure_freq + .5;
  p->channels = lsx_calloc(effp->in_signal.channels, sizeof(*p->channels));
  p->trigger_meas_tc_mult = exp(-1 / (p->trigger_tc * p->measure_freq));
  p->trigger_slope_tc_mult1 = exp(-1 / (p->slope_tc1 * p->measure_freq));
  p->trigger_slope_tc_mult2 = exp(-1 / (p->slope_tc2 * p->measure_freq));
  p->search_slope_tc_mult1 = exp(-1 / (p->slope_tc1 / p->search_step_time));
  p->search_slope_tc_mult2 = exp(-1 / (p->slope_tc2 / p->search_step_time));
  lsx_debug("dft_len=%u measure_len=%u", p->dft_len, p->measure_len);
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

static double measure(sox_effect_t * effp, size_t x)
{
  priv_t * p = (priv_t *)effp->priv;
  double * buf = p->dft_buf;
  double mult, result = 0;
  size_t i;

  for (i = 0; i < p->measure_len; ++i) {
    buf[i] = p->buffer[x] * p->window1[i];
    x = (x + effp->in_signal.channels) % p->buffer_len;
  }
  memset(buf + i, 0, (p->dft_len - i) * sizeof(*buf));
  lsx_safe_rdft((int)p->dft_len, 1, buf);

  memset(buf, 0, p->start_bin * sizeof(*buf));
  for (i = p->start_bin; i < p->end_bin; ++i)
    buf[i] = (sqr(buf[2*i]) + sqr(buf[2*i+1])) * p->window2[i-p->start_bin];
  memset(buf + i, 0, ((p->dft_len >> 1) - i) * sizeof(*buf));
  lsx_safe_rdft((int)p->dft_len >> 1, 1, buf);

  i = max(1, (size_t)(.01 * p->dft_len + .5));
  mult = (p->dft_len / 4 + 1.) / (p->dft_len / 4 - i);
  for (; i < p->dft_len >> 2; ++i)
    result += sqr(buf[2*i]) + sqr(buf[2*i+1]);
  result = log(mult * result);
  result = max(result + 50, 0);
#if 0
  fprintf(stderr, "%g\n", result);
#endif
  return result;
}

static int flow_trigger(sox_effect_t * effp, sox_sample_t const * ibuf,
    sox_sample_t * obuf, size_t * ilen, size_t * olen)
{
  priv_t * p = (priv_t *)effp->priv;
  sox_bool triggered = sox_false;
  size_t i, idone = 0, to_flush = 0;

  while (idone < *ilen && !triggered) {
    for (i = 0; i < effp->in_signal.channels; ++i, ++idone) {
      chan_t * c = &p->channels[i];
      p->buffer[p->buffer_ptr++] = *ibuf++;
      if (p->measure_timer == p->measure_period - 1) {
        size_t flush = p->measure_len;
        size_t x = (p->buffer_ptr + p->buffer_len - flush) % p->buffer_len;
        double slope, meas, meas0 = measure(effp, x);
        c->meas = c->meas * p->trigger_meas_tc_mult + meas0 *(1 - p->trigger_meas_tc_mult);
        if (c->last_meas) {
          slope = (meas0 - c->last_meas) * p->measure_freq;
          c->slope1 = c->slope1? c->slope1 * p->trigger_slope_tc_mult1 + slope
            * (1  - p->trigger_slope_tc_mult1) : slope;
          c->slope2 = c->slope2? c->slope2 * p->trigger_slope_tc_mult2 + slope
            * (1  - p->trigger_slope_tc_mult2) : slope;
        }
        c->last_meas = meas0;
#if 0
        if (c->meas)
          fprintf(stderr, "%g\n", c->meas);
#endif
        if (triggered |= c->meas > p->trigger_level) {
          sox_bool started = sox_false;
          do {
            x = (x + p->buffer_len - p->search_step_len) % p->buffer_len;
            flush += p->search_step_len;
            meas = measure(effp, x);
#if 0
            fprintf(stderr, "%g %g %g\n", meas, c->slope1, c->slope2);
#endif
            slope = -(meas - c->last_meas) / p->search_step_time;
            c->last_meas = meas;
            if (slope > 0 || started) {
              c->slope1 = c->slope1 * p->search_slope_tc_mult1 +
                slope * (1  - p->search_slope_tc_mult1);
              c->slope2 = c->slope2 * p->search_slope_tc_mult2 +
                slope * (1  - p->search_slope_tc_mult2);
              started = sox_true;
            }
          } while (flush < p->search_len && (
                (meas > meas0 - 12 && (c->slope1 > 4 || c->slope2 > 2)) ||
                meas > p->trigger_level));
          to_flush = range_limit(flush, to_flush, p->search_len);
        }
      }
    }
    if (p->buffer_ptr == p->buffer_len)
      p->buffer_ptr = 0;
    if (++p->measure_timer == p->measure_period)
      p->measure_timer = 0;
  }
  if (triggered) {
    size_t ilen1 = *ilen - idone;
    p->flush_done = p->search_len - to_flush;
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
  free(p->channels);
  free(p->window2);
  free(p->window1);
  free(p->dft_buf);
  free(p->buffer);
  return SOX_SUCCESS;
}

sox_effect_handler_t const * lsx_vad_effect_fn(void)
{
  static sox_effect_handler_t handler = {"vad", "[options]"
    "\n\t-h high-pass-filter         (300 Hz)"
    "\n\t-l low-pass-filter          (12500 Hz)"
    "\n\t-m measure-duration         (0.2 s)"
    "\n\t-f measure-frequency        (10 Hz)"
    "\n\t-T trigger-time-constant    (0.2 s)"
    "\n\t-t trigger-level            (33)"
    "\n\t-s search-time              (1 s)"
    "\n\t-q search-step-time         (0.05 s)"
    "\n\t-S slope-slow-time-constant (0.35 s)"
    "\n\t-F slope-fast-time-constant (0.075 s)"
    "\n\t-p pre-trigger-buffer       (0 s)"
    , SOX_EFF_MCHAN | SOX_EFF_LENGTH | SOX_EFF_MODIFY | SOX_EFF_ALPHA,
    create, start, flow_trigger, drain, stop, NULL, sizeof(priv_t)
  };
  return &handler;
}
