/*
 * Effect: reverb           Copyright (c) 2007 robs@users.sourceforge.net
 * Algorithm based on freeverb by Jezar @ Dreampoint.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, write to the Free Software Foundation,
 * Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301, USA.
 */

#include "sox_i.h"
#include "xmalloc.h"
#include <math.h>
#include <string.h>

#define FLOAT float
#define filter_create(p, n) (p)->ptr=Xcalloc((p)->buffer, (p)->size=(size_t)(n))
#define filter_delete(p) free((p)->buffer)
#define ADVANCE_PTR(ptr) if (--p->ptr < p->buffer) p->ptr += p->size

typedef struct {
  size_t  size;
  FLOAT   * buffer, * ptr;
  FLOAT   store;
} filter_t;

static FLOAT comb_process(filter_t * p,  /* gcc -O2 will inline this */
    FLOAT input, FLOAT feedback, FLOAT hf_damping)
{
  FLOAT output = *p->ptr;
  p->store = output + (p->store - output) * hf_damping;
  *p->ptr = input + p->store * feedback;
  ADVANCE_PTR(ptr);
  return output;
}

static FLOAT allpass_process(filter_t * p,  /* gcc -O2 will inline this */
    FLOAT input, FLOAT feedback)
{
  FLOAT output = *p->ptr;
  *p->ptr = input + output * feedback;
  ADVANCE_PTR(ptr);
  return output - input;
}

static const size_t /* Filter delay lengths in samples (44100Hz sample-rate) */
  comb_lengths[] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617},
  allpass_lengths[] = {225, 341, 441, 556};
#define stereo_adjust 12

typedef struct filter_array {
  filter_t comb   [array_length(comb_lengths)];
  filter_t allpass[array_length(allpass_lengths)];
} filter_array_t;

static void filter_array_create(filter_array_t * p, double rate,
    double scale, double offset)
{
  size_t i;
  double r = rate * (1 / 44100.);

  for (i = 0; i < array_length(comb_lengths); ++i, offset = -offset)
    filter_create(&p->comb[i], scale * r * (comb_lengths[i] + stereo_adjust * offset) + .5);
  for (i = 0; i < array_length(allpass_lengths); ++i, offset = -offset)
    filter_create(&p->allpass[i], r * (allpass_lengths[i] + stereo_adjust * offset) + .5);
}

static void filter_array_delete(filter_array_t * p)
{
  size_t i;

  for (i = 0; i < array_length(allpass_lengths); ++i)
    filter_delete(&p->allpass[i]);
  for (i = 0; i < array_length(comb_lengths); ++i)
    filter_delete(&p->comb[i]);
}

static void filter_array_process(filter_array_t * p,
    size_t length, FLOAT const * input, FLOAT * output,
    FLOAT feedback, FLOAT hf_damping, FLOAT gain)
{
  while (length--) {
    FLOAT out = 0, in = *input++;

    size_t i = array_length(comb_lengths) - 1;
    do out += comb_process(p->comb + i, in, feedback, hf_damping);
    while (i--);

    i = array_length(allpass_lengths) - 1;
    do out = allpass_process(p->allpass + i, out, .5f);
    while (i--);

    *output++ = out * gain;
  }
}

static const size_t block_size = 1024;

typedef struct reverb {
  FLOAT feedback;
  FLOAT hf_damping;
  FLOAT gain;
  size_t delay, num_delay_blocks;
  FLOAT * * in, * out[2];
  filter_array_t chan[2];
} reverb_t;

static FLOAT * reverb_create(reverb_t * p, double sample_rate_Hz,
    double wet_gain_dB,
    double room_scale,     /* % */
    double reverberance,   /* % */
    double hf_damping,     /* % */
    double pre_delay_ms,
    FLOAT * * out,
    double stereo_depth)
{
  size_t i;
  double scale = room_scale / 100 * .9 + .1;
  double depth = stereo_depth / 100;
  double a, b;

  memset(p, 0, sizeof(*p));

  b = -1 / log(1 - .5); a = 100 / (1 + log(1 - .98) * b); b *= a;
  p->feedback = 1 - exp((reverberance - a) / b);
  p->hf_damping = hf_damping / 100 * .3 + .2;
  p->gain = exp(wet_gain_dB / 20 * log(10.)) * .015;
  p->delay = pre_delay_ms / 1000 * sample_rate_Hz + .5;
  p->num_delay_blocks = (p->delay + block_size - 1) / block_size;
  Xcalloc(p->in, 1 + p->num_delay_blocks);
  for (i = 0; i <= p->num_delay_blocks; ++i)
    Xcalloc(p->in[i], block_size);
  for (i = 0; i <= ceil(depth); ++i) {
    filter_array_create(p->chan + i, sample_rate_Hz, scale, i * depth);
    out[i] = Xcalloc(p->out[i], block_size);
  }
  return p->in[0];
}

static void reverb_delete(reverb_t * p)
{
  size_t i;
  for (i = 0; i < 2 && p->out[i]; ++i) {
    free(p->out[i]);
    filter_array_delete(p->chan + i);
  }
  for (i = 0; i <= p->num_delay_blocks; ++i)
    free(p->in[i]);
  free(p->in);
}

static FLOAT * reverb_process(reverb_t * p, size_t length)
{
  FLOAT * oldest_in = p->in[p->num_delay_blocks];
  size_t len1 = p->delay % block_size;
  size_t len2 = length - len1, i;

  for (i = 0; i < 2 && p->out[i]; ++i) {
    FLOAT * * b = p->in + p->num_delay_blocks;

    if (len1)
      filter_array_process(p->chan + i, len1, *b-- + block_size - len1, p->out[i], p->feedback, p->hf_damping, p->gain);
    filter_array_process(p->chan + i, len2, *b, p->out[i] + len1, p->feedback, p->hf_damping, p->gain);
  }
  for (i = p->num_delay_blocks; i; --i)
    p->in[i] = p->in[i - 1];
  return p->in[i] = oldest_in;
}

/*------------------------------- SoX Wrapper --------------------------------*/

typedef struct priv {
  double reverberance, hf_damping, pre_delay_ms;
  double stereo_depth, wet_gain_dB, room_scale;
  sox_bool wet_only;

  size_t ichannels, ochannels;
  struct {
    reverb_t reverb;
    FLOAT * dry, * dry1, * wet[2];
  } f[2];
} priv_t;

assert_static(sizeof(struct priv) <= SOX_MAX_EFFECT_PRIVSIZE,
              /* else */ EFFECT_PRIVSIZE_too_big);

static int getopts(sox_effect_t * effp, int argc, char **argv)
{
  priv_t * p = (priv_t *) effp->priv;

  p->reverberance = p->hf_damping = 50; /* Set non-zero defaults */
  p->stereo_depth = p->room_scale = 100;

  p->wet_only = argc && (!strcmp(*argv, "-w") || !strcmp(*argv, "--wet-only"))
    && (--argc, ++argv, sox_true);
  do {  /* break-able block */
    NUMERIC_PARAMETER(reverberance, 0, 100)
    NUMERIC_PARAMETER(hf_damping, 0, 100)
    NUMERIC_PARAMETER(room_scale, 0, 100)
    NUMERIC_PARAMETER(stereo_depth, 0, 100)
    NUMERIC_PARAMETER(pre_delay_ms, 0, 500)
    NUMERIC_PARAMETER(wet_gain_dB, -10, 10)
  } while (0);

  return argc ? sox_usage(effp) : SOX_SUCCESS;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;
  size_t i;
  
  p->ichannels = p->ochannels = 1;

  effp->outinfo.rate = effp->ininfo.rate;
  if (effp->ininfo.channels > 2 && p->stereo_depth) {
    sox_warn("stereo-depth not applicable with >2 channels");
    p->stereo_depth = 0;
  }
  if (effp->ininfo.channels == 1 && p->stereo_depth)
    effp->outinfo.channels = p->ochannels = 2;
  else effp->outinfo.channels = effp->ininfo.channels;
  if (effp->ininfo.channels == 2 && p->stereo_depth)
    p->ichannels = p->ochannels = 2;
  else effp->flows = effp->ininfo.channels;
  for (i = 0; i < p->ichannels; ++i)
    p->f[i].dry = reverb_create(&p->f[i].reverb, effp->ininfo.rate,
        p->wet_gain_dB, p->room_scale, p->reverberance, p->hf_damping,
        p->pre_delay_ms, p->f[i].wet, p->stereo_depth);
  return sox_effect_set_imin(effp, block_size);
}

static int flow(sox_effect_t * effp, const sox_ssample_t * ibuf,
                sox_ssample_t * obuf, sox_size_t * isamp, sox_size_t * osamp)
{
  priv_t * p = (priv_t *) effp->priv;
  sox_size_t c, i, w;

  if (*isamp < block_size || *osamp < block_size * p->ochannels / p->ichannels)
    *isamp = *osamp = 0;
  else {
    *osamp = (*isamp = block_size) * p->ochannels / p->ichannels;

    for (i = 0; i < *isamp; ++i) for (c = 0; c < p->ichannels; ++c) 
      p->f[c].dry[i] = SOX_SAMPLE_TO_FLOAT_32BIT(*ibuf++, effp->clips);
    for (c = 0; c < p->ichannels; ++c) {
      p->f[c].dry1 = p->f[c].dry;
      p->f[c].dry = reverb_process(&p->f[c].reverb, *isamp / p->ichannels);
    }
    if (p->ichannels == 2)
      for (i = 0; i < *isamp; ++i) for (w = 0; w < 2; ++w) {
        FLOAT x = (1 - p->wet_only) * p->f[w].dry1[i] + .5 *
          (p->f[0].wet[w][i] + p->f[1].wet[w][i]);
        *obuf++ = SOX_FLOAT_32BIT_TO_SAMPLE(x, effp->clips);
      }
    else
      for (i = 0; i < *isamp; ++i) for (w = 0; w < min(2, p->ochannels); ++w) {
        FLOAT x = (1 - p->wet_only) * p->f[0].dry1[i] + p->f[0].wet[w][i];
        *obuf++ = SOX_FLOAT_32BIT_TO_SAMPLE(x, effp->clips);
      }
  }
  return SOX_SUCCESS;
}

static int stop(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;
  size_t i;
  for (i = 0; i < p->ichannels; ++i)
    reverb_delete(&p->f[i].reverb);
  return SOX_SUCCESS;
}

sox_effect_handler_t const *sox_reverb_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "reverb", 
    "[-w|--wet-only]"
    " [reverberance (50%)"
    " [HF-damping (50%)"
    " [room-scale (100%)"
    " [stereo-depth (100%)"
    " [pre-delay (0ms)"
    " [wet-gain (0dB)"
    "]]]]]]", SOX_EFF_MCHAN, getopts, start, flow, NULL, stop, NULL
  };
  return &handler;
}
