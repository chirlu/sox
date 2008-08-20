/* libSoX effect: Delay one or more channels (c) 2008 robs@users.sourceforge.net
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
#include <string.h>

typedef struct {
  size_t argc;
  char * * argv, * max_arg;
  size_t delay, pad, buffer_size, buffer_index;
  sox_sample_t * buffer;
} priv_t;

static int kill(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  unsigned i;

  for (i = 0; i < p->argc; ++i)
    free(p->argv[i]);
  free(p->argv);
  return SOX_SUCCESS;
}

static int create(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t delay, max_samples = 0;
  unsigned i;

  p->argv = lsx_calloc(p->argc = argc, sizeof(*p->argv));
  for (i = 0; i < p->argc; ++i) {
    char const * next = lsx_parsesamples(96000., p->argv[i] = lsx_strdup(argv[i]), &delay, 't');
    if (!next || *next) {
      kill(effp);
      return lsx_usage(effp);
    }
    if (delay > max_samples) {
      max_samples = delay;
      p->max_arg = p->argv[i];
    }
  }
  return SOX_SUCCESS;
}

static int stop(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  free(p->buffer);
  return SOX_SUCCESS;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t max_delay;

  if (!p->max_arg)
    return SOX_EFF_NULL;
  if (effp->flow < p->argc)
    lsx_parsesamples(effp->in_signal.rate, p->argv[effp->flow], &p->buffer_size, 't');
  lsx_parsesamples(effp->in_signal.rate, p->max_arg, &max_delay, 't');
  p->buffer_index = p->delay = 0;
  p->pad = max_delay - p->buffer_size;
  p->buffer = lsx_malloc(p->buffer_size * sizeof(*p->buffer));
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t len = *isamp = *osamp = min(*isamp, *osamp);

  if (!p->buffer_size)
    memcpy(obuf, ibuf, len * sizeof(*obuf));
  else for (; len; --len) {
    if (p->delay < p->buffer_size) {
      p->buffer[p->delay++] = *ibuf++;
      *obuf++ = 0;
    } else {
      *obuf++ = p->buffer[p->buffer_index];
      p->buffer[p->buffer_index++] = *ibuf++;
      p->buffer_index %= p->buffer_size;
    }
  }
  return SOX_SUCCESS;
}

static int drain(sox_effect_t * effp, sox_sample_t * obuf, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t len = *osamp = min(p->delay + p->pad, *osamp);

  for (; p->delay && len; --p->delay, --len) {
    *obuf++ = p->buffer[p->buffer_index++];
    p->buffer_index %= p->buffer_size;
  }
  for (; p->pad && len; --p->pad, --len)
    *obuf++ = 0;
  return SOX_SUCCESS;
}

sox_effect_handler_t const * sox_delay_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "delay", "{length}", SOX_EFF_LENGTH,
    create, start, flow, drain, stop, kill, sizeof(priv_t)
  };
  return &handler;
}
