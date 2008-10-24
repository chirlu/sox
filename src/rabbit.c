/* libsamplerate (aka Secret Rabbit Code) support for sox
 * (c) Reuben Thomas <rrt@sc3d.org> 2006
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

#ifdef HAVE_SAMPLERATE_H

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <samplerate.h>

/* Private data for resampling */
typedef struct {
  int converter_type;           /* SRC converter type */
  double out_rate;
  SRC_STATE *state;             /* SRC state struct */
  SRC_DATA *data;               /* SRC_DATA control struct */
  size_t i_alloc, o_alloc;  /* Samples allocated in data->data_{in,out} */
} priv_t;

/*
 * Process options
 */
static int getopts(sox_effect_t * effp, int argc, char **argv)
{
  priv_t * r = (priv_t *) effp->priv;
  char dummy;     /* To check for extraneous chars. */

  r->converter_type = SRC_SINC_BEST_QUALITY;

  if (argc) {
    if (!strcmp(argv[0], "-c0")) {
      r->converter_type = SRC_SINC_BEST_QUALITY;
      argc--; argv++;
    } else if (!strcmp(argv[0], "-c1")) {
      r->converter_type = SRC_SINC_MEDIUM_QUALITY;
      argc--; argv++;
    } else if (!strcmp(argv[0], "-c2")) {
      r->converter_type = SRC_SINC_FASTEST;
      argc--; argv++;
    } else if (!strcmp(argv[0], "-c3")) {
      r->converter_type = SRC_ZERO_ORDER_HOLD;
      argc--; argv++;
    } else if (!strcmp(argv[0], "-c4")) {
      r->converter_type = SRC_LINEAR;
      argc--; argv++;
    }
  }

  r->out_rate = HUGE_VAL;
  if (argc) {
    if (sscanf(*argv, "%lf %c", &r->out_rate, &dummy) != 1 || r->out_rate <= 0)
      return lsx_usage(effp);
    argc--; argv++;
  }

  return argc? lsx_usage(effp) : SOX_SUCCESS;
}

/*
 * Prepare processing.
 */
static int start(sox_effect_t * effp)
{
  priv_t * r = (priv_t *) effp->priv;
  int err = 0;
  double out_rate = r->out_rate != HUGE_VAL? r->out_rate : effp->out_signal.rate;

  if (effp->in_signal.rate == out_rate)
    return SOX_EFF_NULL;

  effp->out_signal.channels = effp->in_signal.channels;
  effp->out_signal.rate = out_rate;

  r->data = lsx_calloc(1, sizeof(SRC_DATA));
  r->data->src_ratio = out_rate / effp->in_signal.rate;
  r->i_alloc = r->o_alloc = 0;
  r->state = src_new(r->converter_type, (int)effp->in_signal.channels, &err);
  if (err) {
    free(r->data);
    lsx_fail("cannot initialise rabbit: %s", src_strerror(err));
    return SOX_EOF;
  }

  return SOX_SUCCESS;
}

/*
 * Read, convert, return data.
 */
static int flow(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf UNUSED,
                   size_t *isamp, size_t *osamp)
{
  priv_t * r = (priv_t *) effp->priv;
  SRC_DATA *d = r->data;
  unsigned int channels = effp->in_signal.channels;
  size_t i;
  size_t isamples0 = d->input_frames * channels;
  size_t isamples = isamples0 + *isamp;
  size_t osamples = isamples * (d->src_ratio + 0.01) + 8;

  if (osamples > *osamp) {
    osamples = *osamp;
    isamples = (osamples - 8) / (d->src_ratio + 0.01);
  }

  if (r->i_alloc < isamples) {
    d->data_in = lsx_realloc(d->data_in, isamples * sizeof(float));
    r->i_alloc = isamples;
  }
  if (r->o_alloc < osamples) {
    d->data_out = lsx_realloc(d->data_out, osamples * sizeof(float));
    r->o_alloc = osamples;
    d->output_frames = osamples / channels;
  }

  for (i = 0; i < isamples - isamples0; i++)
    d->data_in[isamples0 + i] = SOX_SAMPLE_TO_FLOAT_32BIT(ibuf[i], effp->clips);
  *isamp = i;

  d->input_frames = isamples / channels;

  *osamp = 0;
  while (d->input_frames > 0 || d->end_of_input != 0) {
    if (src_process(r->state, r->data) != 0) {
      lsx_fail("%s", src_strerror(src_error(r->state)));
      return SOX_EOF;
    }
    d->input_frames -= d->input_frames_used;
    if (d->input_frames)
       memmove(d->data_in, d->data_in + d->input_frames_used * sizeof(float),
           d->input_frames * sizeof(float));

    for (i = 0; i < (size_t)d->output_frames_gen * channels; i++)
      obuf[i] = SOX_FLOAT_32BIT_TO_SAMPLE(d->data_out[i], effp->clips);
    *osamp += i;

    if (!d->output_frames_gen || d->end_of_input)
      break;
  }

  return SOX_SUCCESS;
}

/*
 * Process samples and write output.
 */
static int drain(sox_effect_t * effp, sox_sample_t *obuf, size_t *osamp)
{
  priv_t * r = (priv_t *) effp->priv;
  static size_t isamp = 0;
  r->data->end_of_input = 1;
  return flow(effp, NULL, obuf, &isamp, osamp);
}

/*
 * Do anything required when you stop reading samples.
 */
static int stop(sox_effect_t * effp)
{
  priv_t * r = (priv_t *) effp->priv;

  free(r->data);
  src_delete(r->state);
  return SOX_SUCCESS;
}

const sox_effect_handler_t *sox_rabbit_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "rabbit", "[-c0|-c1|-c2|-c3|-c4] [rate]",
    SOX_EFF_RATE | SOX_EFF_MCHAN | SOX_EFF_DEPRECATED,
    getopts, start, flow, drain, stop, NULL, sizeof(priv_t)
  };

  return &handler;
}

#endif /* HAVE_SAMPLERATE */
