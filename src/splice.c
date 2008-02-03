/*
 * Effect: splice with a WSOL method.
 * Copyright (c) 2008 robs@users.sourceforge.net
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

static float difference(const float * a, const float * b, size_t length)
{
  float diff = 0;
  size_t i = 0;

  #define _ diff += sqr(a[i] - b[i]), ++i; /* Loop optimisation */
  do {_ _ _ _ _ _ _ _} while (i < length); /* N.B. length ≡ 0 (mod 8) */
  #undef _
  return diff;
}

/* Find where the two segments are most alike over the overlap period. */
static size_t best_overlap_position(float const * f1, float const * f2,
    sox_size_t overlap, sox_size_t search, sox_size_t channels)
{
  size_t i, best_pos = 0;
  float diff, least_diff = difference(f2, f1, channels * overlap);

  for (i = 1; i < search; ++i) { /* linear search */
    diff = difference(f2 + channels * i, f1, channels * overlap);
    if (diff < least_diff)
      least_diff = diff, best_pos = i;
  }
  return best_pos;
}

static void splice(const float * in1, const float * in2,
    float * output, sox_size_t overlap, sox_size_t channels)
{
  size_t i, j, k = 0;
  float fade_step = 1.0f / (float) overlap;

  for (i = 0; i < overlap; ++i) {
    float fade_in  = fade_step * (float) i;
    float fade_out = 1.0f - fade_in;
    for (j = 0; j < channels; ++j, ++k)
      output[k] = in1[k] * fade_out + in2[k] * fade_in;
  }
}

static sox_size_t do_splice(float * f, sox_size_t overlap, sox_size_t search, sox_size_t channels)
{
  size_t offset = search? best_overlap_position(
      f, f + overlap * channels, overlap, search, channels) : 0;
  splice(f, f + (overlap + offset) * channels,
      f + (overlap + offset) * channels, overlap, channels);
  return overlap + offset;
}

typedef struct splice
{
  unsigned nsplices;     /* Number of splices requested */
  struct {
    char * str;          /* Command-line argument to parse for this splice */
    sox_size_t overlap;  /* Number of samples to overlap */
    sox_size_t search;   /* Number of samples to search */
    sox_size_t start;    /* Start splicing when in_pos equals this */
  } * splices;

  sox_size_t in_pos;     /* Number of samples read from the input stream */
  unsigned splices_pos;  /* Number of splices completed so far */
  sox_size_t buffer_pos; /* Number of samples through the current splice */
  sox_size_t max_buffer_size;
  float * buffer;
  unsigned state;
} * splice_t;

assert_static(sizeof(struct splice) <= SOX_MAX_EFFECT_PRIVSIZE,
              /* else */ splice_PRIVSIZE_too_big);

static int parse(sox_effect_t * effp, char * * argv, sox_rate_t rate)
{
  splice_t p = (splice_t) effp->priv;
  char const * next;
  sox_size_t i, buffer_size;

  p->max_buffer_size = 0;
  for (i = 0; i < p->nsplices; ++i) {
    if (argv) /* 1st parse only */
      p->splices[i].str = xstrdup(argv[i]);
    
    p->splices[i].overlap = p->splices[i].search = rate * 0.01 + .5;

    next = sox_parsesamples(rate, p->splices[i].str, &p->splices[i].start, 't');
    if (next == NULL) break;
    
    if (*next == ',') {
      next = sox_parsesamples(rate, next + 1, &p->splices[i].overlap, 't');
      if (next == NULL) break;
      p->splices[i].overlap *= 2;
      if (*next == ',') {
        next = sox_parsesamples(rate, next + 1, &p->splices[i].search, 't');
        if (next == NULL) break;
        p->splices[i].search *= 2;
      }
    }
    if (*next != '\0') break;
    p->splices[i].overlap = max(p->splices[i].overlap + 4, 16);
    p->splices[i].overlap &= ~7; /* Make divisible by 8 for loop optimisation */
    
    if (i > 0 && p->splices[i].start <= p->splices[i-1].start) break;
    if (p->splices[i].start < p->splices[i].overlap) break;
    p->splices[i].start -= p->splices[i].overlap;
    buffer_size = 2 * p->splices[i].overlap + p->splices[i].search;
    p->max_buffer_size = max(p->max_buffer_size, buffer_size);
  }
  if (i < p->nsplices)
    return sox_usage(effp);
  return SOX_SUCCESS;
}

static int create(sox_effect_t * effp, int n, char * * argv)
{
  splice_t p = (splice_t) effp->priv;
  p->splices = xcalloc(p->nsplices = n, sizeof(*p->splices));
  return parse(effp, argv, SOX_MAXRATE); /* No rate yet; parse with dummy */
}

static int start(sox_effect_t * effp)
{
  splice_t p = (splice_t) effp->priv;
  unsigned i;

  parse(effp, 0, effp->ininfo.rate); /* Re-parse now rate is known */
  p->buffer = xcalloc(p->max_buffer_size * effp->ininfo.channels, sizeof(*p->buffer));
  p->in_pos = p->buffer_pos = p->splices_pos = 0;
  p->state = p->splices_pos != p->nsplices && p->in_pos == p->splices[p->splices_pos].start;
  for (i = 0; i < p->nsplices; ++i)
    if (p->splices[i].overlap)
      return SOX_SUCCESS;
  return SOX_EFF_NULL;
}

static int flow(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, sox_size_t * isamp, sox_size_t * osamp)
{
  splice_t p = (splice_t) effp->priv;
  sox_size_t c, idone = 0, odone = 0;
  *isamp /= effp->ininfo.channels;
  *osamp /= effp->ininfo.channels;

  while (sox_true) {
copying:
    if (p->state == 0) {
      for (; idone < *isamp && odone < *osamp; ++idone, ++odone, ++p->in_pos) {
        if (p->splices_pos != p->nsplices && p->in_pos == p->splices[p->splices_pos].start) {
          p->state = 1;
          goto buffering;
        }
        for (c = 0; c < effp->ininfo.channels; ++c)
          *obuf++ = *ibuf++;
      }
      break;
    }

buffering:
    if (p->state == 1) {
      sox_size_t buffer_size = (2 * p->splices[p->splices_pos].overlap + p->splices[p->splices_pos].search) * effp->ininfo.channels;
      for (; idone < *isamp; ++idone, ++p->in_pos) {
        if (p->buffer_pos == buffer_size) {
          p->buffer_pos = do_splice(p->buffer,
              p->splices[p->splices_pos].overlap,
              p->splices[p->splices_pos].search,
              effp->ininfo.channels) * effp->ininfo.channels;
          p->state = 2;
          goto flushing;
          break;
        }
        for (c = 0; c < effp->ininfo.channels; ++c)
          p->buffer[p->buffer_pos++] = SOX_SAMPLE_TO_FLOAT_32BIT(*ibuf++, effp->clips);
      }
      break;
    }

flushing:
    if (p->state == 2) {
      sox_size_t buffer_size = (2 * p->splices[p->splices_pos].overlap + p->splices[p->splices_pos].search) * effp->ininfo.channels;
      for (; odone < *osamp; ++odone) {
        if (p->buffer_pos == buffer_size) {
          p->buffer_pos = 0;
          ++p->splices_pos;
          p->state = p->splices_pos != p->nsplices && p->in_pos == p->splices[p->splices_pos].start;
          goto copying;
        }
        for (c = 0; c < effp->ininfo.channels; ++c)
          *obuf++ = SOX_FLOAT_32BIT_TO_SAMPLE(p->buffer[p->buffer_pos++], effp->clips);
      }
      break;
    }
  }

  *isamp = idone * effp->ininfo.channels;
  *osamp = odone * effp->ininfo.channels;
  return SOX_SUCCESS;
}

static int drain(sox_effect_t * effp, sox_sample_t * obuf, sox_size_t * osamp)
{
  sox_size_t isamp = 0;
  return flow(effp, 0, obuf, &isamp, osamp);
}

static int stop(sox_effect_t * effp)
{
  splice_t p = (splice_t) effp->priv;
  if (p->splices_pos != p->nsplices)
    sox_warn("Input audio too short; splices not made: %u", p->nsplices - p->splices_pos);
  free(p->buffer);
  return SOX_SUCCESS;
}

static int kill(sox_effect_t * effp)
{
  splice_t p = (splice_t) effp->priv;
  unsigned i;
  for (i = 0; i < p->nsplices; ++i)
    free(p->splices[i].str);
  free(p->splices);
  return SOX_SUCCESS;
}

sox_effect_handler_t const * sox_splice_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "splice", "{position[,excess[,leaway]]}", SOX_EFF_MCHAN|SOX_EFF_LENGTH,
    create, start, flow, drain, stop, kill
  };
  return &handler;
}
