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
#include "getopt.h"
#include <string.h>

typedef struct {double mean_sqr, *log_mean_sqrs; unsigned trigger_done;} chan_t;

typedef struct {                /* Configuation parameters: */
  unsigned      power_boot_len;
  double        power_tc, buffer_time, power_dt, trigger_rise, trigger_time;
                                /* Working variables: */
  double        tc_mult;   /* Multiplier for decay time constant */
  sox_sample_t  * buffer;
  unsigned      buffer_len, buffer_ptr, flush_done, power_boot_done;
  unsigned      trigger_len, log_mean_sqrs_len, log_mean_sqrs_ptr;
  chan_t        * channels;
} priv_t;

static int create(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *)effp->priv;
  int c;

  p->power_tc       = .025; p->trigger_rise = 20;
  p->power_boot_len = 2;    p->trigger_time = .01;
  p->power_dt       = .1;   p->buffer_time  = .05;

  while ((c = getopt(argc, argv, "+c:b:d:r:u:p:")) != -1) switch (c) {
    GETOPT_NUMERIC('c', power_tc        ,.001 , 10)
    GETOPT_NUMERIC('b', power_boot_len  ,   0 , 10)
    GETOPT_NUMERIC('d', power_dt        ,.001 , 10)
    GETOPT_NUMERIC('r', trigger_rise    ,   1 , 100)
    GETOPT_NUMERIC('u', trigger_time    ,   0 , 10)
    GETOPT_NUMERIC('p', buffer_time     ,   0 , 10)
    default: lsx_fail("invalid option `-%c'", optopt); return lsx_usage(effp);
  }
  p->trigger_rise *= .1 * log(10.); /* Convert to natural log */
  return optind !=argc? lsx_usage(effp) : SOX_SUCCESS;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t i;

  p->tc_mult = exp(-1 / (p->power_tc * effp->in_signal.rate));
  p->trigger_len = 1 + p->trigger_time * effp->in_signal.rate + .5;

  p->log_mean_sqrs_len = p->power_dt * effp->in_signal.rate + .5;
  p->channels = lsx_calloc(effp->in_signal.channels, sizeof(*p->channels));
  for (i = 0; i < effp->in_signal.channels; ++i)
    lsx_Calloc(p->channels[i].log_mean_sqrs, p->log_mean_sqrs_len);

  p->buffer_len = p->trigger_len + p->buffer_time * effp->in_signal.rate + .5;
  p->buffer_len *= effp->in_signal.channels;
  p->buffer = lsx_calloc(p->buffer_len, sizeof(*p->buffer));
  p->power_boot_done = p->flush_done = p->log_mean_sqrs_ptr = p->buffer_ptr = 0;
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

static int flow_trigger(sox_effect_t * effp, sox_sample_t const * ibuf,
    sox_sample_t * obuf, size_t * ilen, size_t * olen)
{
  priv_t * p = (priv_t *)effp->priv;
  sox_bool triggered = sox_false;
  size_t i, idone = 0;

  while (idone < *ilen && !triggered) {
    for (i = 0; i < effp->in_signal.channels; ++i, ++idone) {
      chan_t * c = &p->channels[i];
      double d = SOX_SAMPLE_TO_FLOAT_64BIT(*ibuf,);
      p->buffer[p->buffer_ptr++] = *ibuf++;
      c->mean_sqr = p->tc_mult * c->mean_sqr + (1 - p->tc_mult) * sqr(d);
      d = log(c->mean_sqr);
      if (p->power_boot_done >= p->power_boot_len) {
        if (d - c->log_mean_sqrs[p->log_mean_sqrs_ptr] < p->trigger_rise)
          c->trigger_done = 0;
        else triggered |= ++c->trigger_done == p->trigger_len;
      }
      c->log_mean_sqrs[p->log_mean_sqrs_ptr] = d;
    }
    if (p->buffer_ptr == p->buffer_len)
      p->buffer_ptr = 0;
    if (++p->log_mean_sqrs_ptr == p->log_mean_sqrs_len)
      ++p->power_boot_done, p->log_mean_sqrs_ptr = 0;
  }
  if (triggered) {
    size_t ilen1 = *ilen - idone;
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
  size_t i;

  free(p->buffer);
  for (i = 0; i < effp->in_signal.channels; ++i)
    free(p->channels[i].log_mean_sqrs);
  free(p->channels);
  return SOX_SUCCESS;
}

sox_effect_handler_t const * lsx_vad_effect_fn(void)
{
  static sox_effect_handler_t handler = {"vad", "[options]"
    "\n\t-c power-time-constant (0.025 s)"
    "\n\t-d trigger-rise-time   (0.1 s)"
    "\n\t-r trigger-rise        (20 dB)"
    "\n\t-u trigger-up-time     (0.01 s)"
    "\n\t-p pre-trigger-buffer  (0.05 s)"
    , SOX_EFF_MCHAN | SOX_EFF_LENGTH | SOX_EFF_MODIFY,
    create, start, flow_trigger, drain, stop, NULL, sizeof(priv_t)
  };
  return &handler;
}
