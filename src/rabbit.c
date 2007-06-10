/*
 * libsamplerate (aka Secret Rabbit Code) support for sox
 * (c) Reuben Thomas <rrt@sc3d.org> 2006
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, write to the Free Software
 * Foundation, Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301,
 * USA.  */

#include "sox_i.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <samplerate.h>

/* Private data for resampling */
typedef struct {
  int converter_type;           /* SRC converter type */
  SRC_STATE *state;             /* SRC state struct */
  SRC_DATA *data;               /* SRC_DATA control struct */
  sox_size_t i_alloc, o_alloc;  /* Samples allocated in data->data_{in,out} */
} *rabbit_t;

/*
 * Process options
 */
static int getopts(sox_effect_t * effp, int n, char **argv)
{
  rabbit_t r = (rabbit_t) effp->priv;

  r->converter_type = SRC_SINC_BEST_QUALITY;

  if (n >= 1) {
    if (!strcmp(argv[0], "-c0")) {
      r->converter_type = SRC_SINC_BEST_QUALITY;
      n--; argv++;
    } else if (!strcmp(argv[0], "-c1")) {
      r->converter_type = SRC_SINC_MEDIUM_QUALITY;
      n--; argv++;
    } else if (!strcmp(argv[0], "-c2")) {
      r->converter_type = SRC_SINC_FASTEST;
      n--; argv++;
    } else if (!strcmp(argv[0], "-c3")) {
      r->converter_type = SRC_ZERO_ORDER_HOLD;
      n--; argv++;
    } else if (!strcmp(argv[0], "-c4")) {
      r->converter_type = SRC_LINEAR;
      n--; argv++;
    }
  }

  if (n >= 1)
    return sox_usage(effp);

  return SOX_SUCCESS;
}

/*
 * Prepare processing.
 */
static int start(sox_effect_t * effp)
{
  rabbit_t r = (rabbit_t) effp->priv;
  int err = 0;

  /* The next line makes the "speed" effect accurate; it's needed because
   * ininfo.rate (sox_rate_t) isn't floating point (but it's probably not worth
   * changing sox_rate_t just because of this): */
  double in_rate = floor(effp->ininfo.rate / effp->global_info->speed + .5)
    * effp->global_info->speed;

  if (in_rate == effp->outinfo.rate)
    return SOX_EFF_NULL;
          
  if (effp->ininfo.channels != effp->outinfo.channels) {
    sox_fail("number of Input and Output channels must be equal to use rabbit effect");
    return SOX_EOF;
  }

  r->data = (SRC_DATA *)xcalloc(1, sizeof(SRC_DATA));
  r->data->src_ratio = (double)effp->outinfo.rate / in_rate;
  r->i_alloc = r->o_alloc = 0;
  r->state = src_new(r->converter_type, effp->ininfo.channels, &err);
  if (err) {
    free(r->data);
    sox_fail("cannot initialise rabbit: %s", src_strerror(err));
    return SOX_EOF;
  }

  return SOX_SUCCESS;
}

/*
 * Read, convert, return data.
 */
static int flow(sox_effect_t * effp, const sox_ssample_t *ibuf, sox_ssample_t *obuf UNUSED,
                   sox_size_t *isamp, sox_size_t *osamp)
{
  rabbit_t r = (rabbit_t) effp->priv;
  SRC_DATA *d = r->data;
  unsigned int channels = effp->ininfo.channels;
  sox_size_t i;

  if (isamp && *isamp > 0) {
    sox_size_t isamples0 = d->input_frames * channels;
    sox_size_t isamples = isamples0 + *isamp;
    sox_size_t osamples = isamples * (d->src_ratio + 0.01) + 8;

    if (osamples > sox_bufsiz) {
      osamples = sox_bufsiz;
      isamples = (osamples - 8) / (d->src_ratio + 0.01);
    }

    if (r->i_alloc < isamples) {
      d->data_in = xrealloc(d->data_in, isamples * sizeof(float));
      r->i_alloc = isamples;
    }
    if (r->o_alloc < osamples) {
      d->data_out = xrealloc(d->data_out, osamples * sizeof(float));
      r->o_alloc = osamples;
      d->output_frames = osamples / channels;
    }

    for (i = 0; i < isamples - isamples0; i++)
      d->data_in[isamples0 + i] = SOX_SAMPLE_TO_FLOAT_32BIT(ibuf[i], effp->clips);

    *isamp = isamples - isamples0;
    d->input_frames = isamples / channels;
  }

  *osamp = 0;
  while (d->input_frames > 0 || d->end_of_input != 0) {
    if (src_process(r->state, r->data) != 0) {
      sox_fail("%s", src_strerror(src_error(r->state)));
      return SOX_EOF;
    }
    if (d->input_frames_used) {
      d->input_frames -= d->input_frames_used;
      if (d->input_frames)
       memcpy(d->data_in,
              d->data_in + d->input_frames_used * sizeof(float),
              d->input_frames * sizeof(float));
    }

    *osamp = d->output_frames_gen * channels;
    if (! *osamp)
      break;

    for (i = 0; i < (sox_size_t)d->output_frames_gen * channels; i++)
      obuf[i] = SOX_FLOAT_32BIT_TO_SAMPLE(d->data_out[i], effp->clips);

    if (d->end_of_input)
      break;
  }

  return SOX_SUCCESS;
}

/*
 * Process samples and write output.
 */
static int drain(sox_effect_t * effp, sox_ssample_t *obuf, sox_size_t *osamp)
{
  rabbit_t r = (rabbit_t) effp->priv;
  r->data->end_of_input = 1;
  return flow(effp, NULL, obuf, NULL, osamp);
}

/*
 * Do anything required when you stop reading samples.
 */
static int stop(sox_effect_t * effp)
{
  rabbit_t r = (rabbit_t) effp->priv;

  free(r->data);
  src_delete(r->state);
  return SOX_SUCCESS;
}

const sox_effect_handler_t *sox_rabbit_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "rabbit", "[-c0|-c1|-c2|-c3|-c4]",
    SOX_EFF_RATE | SOX_EFF_MCHAN,
    getopts, start, flow, drain, stop, NULL
  };

  return &handler;
}
