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
 
#include "st_i.h"

#ifdef HAVE_SAMPLERATE_H

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <samplerate.h>

static st_effect_t st_rabbit_effect;

/* Private data for resampling */
typedef struct {
  int converter_type;           /* SRC converter type */
  SRC_STATE *state;             /* SRC state struct */
  SRC_DATA *data;               /* SRC_DATA control struct */
  st_size_t samples;            /* Number of samples read so far */
  st_size_t outsamp;            /* Next output sample */
} *rabbit_t;

/*
 * Process options
 */
static int st_rabbit_getopts(eff_t effp, int n, char **argv)
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
    st_fail(st_rabbit_effect.usage);
    return ST_EOF;
  }

  return ST_SUCCESS;
}

/*
 * Prepare processing.
 */
static int st_rabbit_start(eff_t effp)
{
  rabbit_t r = (rabbit_t) effp->priv;

  /* The next line makes the "speed" effect accurate; it's needed because
   * ininfo.rate (st_rate_t) isn't floating point (but it's probably not worth
   * changing st_rate_t just because of this): */
  double in_rate = floor(effp->ininfo.rate / effp->globalinfo->speed + .5)
    * effp->globalinfo->speed;

  if (in_rate == effp->outinfo.rate)
    return ST_EFF_NULL;
          
  if (effp->ininfo.channels != effp->outinfo.channels) {
    st_fail("number of Input and Output channels must be equal to use rabbit effect");
    return ST_EOF;
  }

  r->data = (SRC_DATA *)xcalloc(1, sizeof(SRC_DATA));
  r->data->src_ratio = (double)effp->outinfo.rate / in_rate;
  r->data->input_frames_used = 0;
  r->data->output_frames_gen = 0;

  return ST_SUCCESS;
}

/*
 * Read all the data.
 */
static int st_rabbit_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf UNUSED,
                   st_size_t *isamp, st_size_t *osamp)
{
  rabbit_t r = (rabbit_t) effp->priv;
  int channels = effp->ininfo.channels;
  st_size_t i, newsamples;

  newsamples = r->samples + *isamp;
  if (newsamples / channels > INT_MAX) {
      st_fail("input data size %d too large for libsamplerate", newsamples);
      return ST_EOF;
  }

  r->data->data_in = (float *)xrealloc(r->data->data_in, newsamples * sizeof(float));

  for (i = 0 ; i < *isamp; i++)
    r->data->data_in[r->samples + i] = ST_SAMPLE_TO_FLOAT_DWORD(ibuf[i], effp->clips);

  r->samples = newsamples;
  r->data->input_frames = r->samples / channels;
  r->outsamp = 0;

  *osamp = 0;           /* Signal that we didn't produce any output */

  return ST_SUCCESS;
}

/*
 * Process samples and write output.
 */
static int st_rabbit_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
  rabbit_t r = (rabbit_t) effp->priv;
  int channels = effp->ininfo.channels;
  st_size_t i, outsamps;

  /* On first call, process the data */
  if (r->data->data_out == NULL) {
    /* Guess maximum number of possible output frames */
    st_size_t outframes = r->data->input_frames * (r->data->src_ratio + 0.01) + 8;
    int error;

    if (outframes > INT_MAX) {
      st_fail("too many output frames (%d) for libsamplerate", outframes);
      return ST_EOF;
    }
    r->data->output_frames = outframes;
    r->data->data_out = (float *)xmalloc(r->data->output_frames * channels * sizeof(float));

    /* Process the data */
    if ((error = src_simple(r->data, r->converter_type, channels))) {
      st_fail("libsamplerate processing failed: %s", src_strerror(error));
      return ST_EOF;
    }
  }

  /* Return the data one bufferful at a time */
  if (*osamp > INT_MAX) {
    st_fail("output buffer size %d too large for libsamplerate", *osamp);
    return ST_EOF;
  }

  outsamps = min(r->data->output_frames_gen * channels - r->outsamp, *osamp);
  for (i = 0; i < outsamps; i++)
    obuf[i] = ST_FLOAT_DWORD_TO_SAMPLE(r->data->data_out[r->outsamp + i], effp->clips);
  *osamp = (st_size_t)outsamps;
  r->outsamp += outsamps;

  return ST_SUCCESS;
}

/*
 * Do anything required when you stop reading samples.
 * Don't close input file!
 */
static int st_rabbit_stop(eff_t effp)
{
  rabbit_t r = (rabbit_t) effp->priv;

  free(r->data);
  src_delete(r->state);
  return ST_SUCCESS;
}

static st_effect_t st_rabbit_effect = {
  "rabbit",
  "Usage: rabbit [ -c0 | -c1 | -c2 | -c3 | -c4 ]",
  ST_EFF_RATE | ST_EFF_MCHAN,
  st_rabbit_getopts,
  st_rabbit_start,
  st_rabbit_flow,
  st_rabbit_drain,
  st_rabbit_stop,
  st_effect_nothing
};

const st_effect_t *st_rabbit_effect_fn(void)
{
  return &st_rabbit_effect;
}

#endif /* HAVE_SAMPLERATE */
