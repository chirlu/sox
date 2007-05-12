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

/* FIXME: Make more efficient by resampling piece by piece rather than
   all in one go. The code is as it is at present because before SoX
   had global clipping detection, rabbit used to do its own based on
   sndfile-resample. */
 
#include "sox_i.h"

#ifdef HAVE_SAMPLERATE_H

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <samplerate.h>

static sox_effect_t sox_rabbit_effect;

/* Private data for resampling */
typedef struct {
  int converter_type;           /* SRC converter type */
  SRC_STATE *state;             /* SRC state struct */
  SRC_DATA *data;               /* SRC_DATA control struct */
  sox_size_t samples;            /* Number of samples read so far */
  sox_size_t outsamp;            /* Next output sample */
} *rabbit_t;

/*
 * Process options
 */
static int sox_rabbit_getopts(eff_t effp, int n, char **argv)
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

  if (n >= 1) {
    sox_fail(sox_rabbit_effect.usage);
    return SOX_EOF;
  }

  return SOX_SUCCESS;
}

/*
 * Prepare processing.
 */
static int sox_rabbit_start(eff_t effp)
{
  rabbit_t r = (rabbit_t) effp->priv;

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
  r->data->input_frames_used = 0;
  r->data->output_frames_gen = 0;

  return SOX_SUCCESS;
}

/*
 * Read all the data.
 */
static int sox_rabbit_flow(eff_t effp, const sox_ssample_t *ibuf, sox_ssample_t *obuf UNUSED,
                   sox_size_t *isamp, sox_size_t *osamp)
{
  rabbit_t r = (rabbit_t) effp->priv;
  int channels = effp->ininfo.channels;
  sox_size_t i, newsamples;

  newsamples = r->samples + *isamp;
  if (newsamples / channels > INT_MAX) {
      sox_fail("input data size %d too large for libsamplerate", newsamples);
      return SOX_EOF;
  }

  r->data->data_in = (float *)xrealloc(r->data->data_in, newsamples * sizeof(float));

  for (i = 0 ; i < *isamp; i++)
    r->data->data_in[r->samples + i] = SOX_SAMPLE_TO_FLOAT_32BIT(ibuf[i], effp->clips);

  r->samples = newsamples;
  r->data->input_frames = r->samples / channels;
  r->outsamp = 0;

  *osamp = 0;           /* Signal that we didn't produce any output */

  return SOX_SUCCESS;
}

/*
 * Process samples and write output.
 */
static int sox_rabbit_drain(eff_t effp, sox_ssample_t *obuf, sox_size_t *osamp)
{
  rabbit_t r = (rabbit_t) effp->priv;
  int channels = effp->ininfo.channels;
  sox_size_t i, outsamps;

  /* On first call, process the data */
  if (r->data->data_out == NULL) {
    /* Guess maximum number of possible output frames */
    sox_size_t outframes = r->data->input_frames * (r->data->src_ratio + 0.01) + 8;
    int error;

    if (outframes > INT_MAX) {
      sox_fail("too many output frames (%d) for libsamplerate", outframes);
      return SOX_EOF;
    }
    r->data->output_frames = outframes;
    r->data->data_out = (float *)xmalloc(r->data->output_frames * channels * sizeof(float));

    /* Process the data */
    if ((error = src_simple(r->data, r->converter_type, channels))) {
      sox_fail("libsamplerate processing failed: %s", src_strerror(error));
      return SOX_EOF;
    }
  }

  /* Return the data one bufferful at a time */
  if (*osamp > INT_MAX) {
    sox_fail("output buffer size %d too large for libsamplerate", *osamp);
    return SOX_EOF;
  }

  outsamps = min(r->data->output_frames_gen * channels - r->outsamp, *osamp);
  for (i = 0; i < outsamps; i++)
    obuf[i] = SOX_FLOAT_32BIT_TO_SAMPLE(r->data->data_out[r->outsamp + i], effp->clips);
  *osamp = (sox_size_t)outsamps;
  r->outsamp += outsamps;

  return SOX_SUCCESS;
}

/*
 * Do anything required when you stop reading samples.
 * Don't close input file!
 */
static int sox_rabbit_stop(eff_t effp)
{
  rabbit_t r = (rabbit_t) effp->priv;

  free(r->data);
  src_delete(r->state);
  return SOX_SUCCESS;
}

static sox_effect_t sox_rabbit_effect = {
  "rabbit",
  "Usage: rabbit [-c0|-c1|-c2|-c3|-c4]",
  SOX_EFF_RATE | SOX_EFF_MCHAN,
  sox_rabbit_getopts,
  sox_rabbit_start,
  sox_rabbit_flow,
  sox_rabbit_drain,
  sox_rabbit_stop,
  sox_effect_nothing
};

const sox_effect_t *sox_rabbit_effect_fn(void)
{
  return &sox_rabbit_effect;
}

#endif /* HAVE_SAMPLERATE */
