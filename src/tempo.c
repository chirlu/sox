/*
 * Effect: change the audio tempo (but not key)
 *
 * Copyright (c) 2007 robs@users.sourceforge.net
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
#include <assert.h>

/* Addressible FIFO buffer */

typedef struct {
  char * data;
  size_t allocation;   /* Number of bytes allocated for data. */
  size_t item_size;    /* Size of each item in data */
  size_t begin;        /* Offset of the first byte to read. */
  size_t end;          /* 1 + Offset of the last byte byte to read. */
} fifo_t;

#define FIFO_MIN 0x4000

static void fifo_clear(fifo_t * f)
{
  f->end = f->begin = 0;
}

static void * fifo_reserve(fifo_t * f, size_t n)
{
  n *= f->item_size;

  if (f->begin == f->end)
    fifo_clear(f);

  while (1) {
    if (f->end + n <= f->allocation) {
      void *p = (char *) f->data + f->end;

      f->end += n;
      return p;
    }
    if (f->begin > FIFO_MIN) {
      memmove(f->data, f->data + f->begin, f->end - f->begin);
      f->end -= f->begin;
      f->begin = 0;
      continue;
    }
    f->allocation += n;
    f->data = xrealloc(f->data, f->allocation);
  }
}

static void * fifo_write(fifo_t * f, size_t n, void const * data)
{
  void * s = fifo_reserve(f, n);
  if (data)
    memcpy(s, data, n * f->item_size);
  return s;
}

static void fifo_trim(fifo_t * f, size_t n)
{
  n *= f->item_size;
  f->end = f->begin + n;
}

static size_t fifo_occupancy(fifo_t * f)
{
  return (f->end - f->begin) / f->item_size;
}

static void * fifo_read(fifo_t * f, size_t n, void * data)
{
  char * ret = f->data + f->begin;
  n *= f->item_size;
  if (n > f->end - f->begin)
    return NULL;
  if (data)
    memcpy(data, ret, n);
  f->begin += n;
  return ret;
}

#define fifo_read_ptr(f) fifo_read(f, 0, NULL)

static void fifo_delete(fifo_t * f)
{
  free(f->data);
}

static void fifo_create(fifo_t * f, size_t item_size)
{
  f->item_size = item_size;
  f->allocation = FIFO_MIN;
  f->data = xmalloc(f->allocation);
  fifo_clear(f);
}

/*
 * Change tempo (alter duration, maintain pitch) using a WSOLA algorithm.
 * Based on ideas from Olli Parviainen's SoundTouch Library.
 */

typedef struct {
  size_t samples_in;
  size_t samples_out;
  double factor;
  size_t channels;
  size_t process_size;
  float * prev_win_end;
  size_t overlap;
  size_t seek;
  size_t window;
  size_t windows_total;
  size_t skip_total;
  fifo_t output_fifo;
  fifo_t input_fifo;
  sox_bool quick_seek;
} Stretch;

/* For the Wave Similarity bit of WSOLA */
static double difference(const float * a, const float * b, size_t length)
{
  double diff = 0;
  size_t i = 0;

  #define _ diff += sqr(a[i] - b[i]), ++i; /* Loop optimisation */
  do {_ _ _ _ _ _ _ _} while (i < length); /* N.B. length ≡ 0 (mod 8) */
  #undef _
  return diff;
}

/* Find where the two windows are most alike over the overlap period. */
static size_t stretch_best_overlap_position(Stretch * p, float const * new_win)
{
  float * f = p->prev_win_end;
  size_t j, best_pos, prev_best_pos = (p->seek + 1) >> 1, step = 64;
  size_t i = best_pos = p->quick_seek? prev_best_pos : 0;
  double diff, least_diff = difference(new_win + p->channels * i, f, p->channels * p->overlap);
  int k = 0;

  if (p->quick_seek) do { /* hierarchical search */
    for (k = -1; k <= 1; k += 2) for (j = 1; j < 4 || step == 64; ++j) {
      i = prev_best_pos + k * j * step;
      if ((int)i < 0 || i >= p->seek)
        break;
      diff = difference(new_win + p->channels * i, f, p->channels * p->overlap);
      if (diff < least_diff)
        least_diff = diff, best_pos = i;
    }
    prev_best_pos = best_pos;
  } while (step >>= 2);
  else for (i = 1; i < p->seek; i++) { /* linear search */
    diff = difference(new_win + p->channels * i, f, p->channels * p->overlap);
    if (diff < least_diff)
      least_diff = diff, best_pos = i;
  }
  return best_pos;
}

/* For the Over-Lap bit of WSOLA */
static void stretch_overlap(Stretch * p, const float * in1, const float * in2, float * output)
{
  size_t i, j, k = 0;
  float fade_step = 1.0f / (float) p->overlap;

  for (i = 0; i < p->overlap; ++i) {
    float fade_in  = fade_step * (float) i;
    float fade_out = 1.0f - fade_in;
    for (j = 0; j < p->channels; ++j, ++k)
      output[k] = in1[k] * fade_out + in2[k] * fade_in;
  }
}

static void stretch_process_windows(Stretch * p)
{
  while (fifo_occupancy(&p->input_fifo) >= p->process_size) {
    size_t skip, offset = 0;

    /* Copy or overlap the first bit to the output */
    if (!p->windows_total)
      fifo_write(&p->output_fifo, p->overlap, fifo_read_ptr(&p->input_fifo));
    else {
      offset = stretch_best_overlap_position(p, fifo_read_ptr(&p->input_fifo));
      stretch_overlap(p, p->prev_win_end,
          (float *) fifo_read_ptr(&p->input_fifo) + p->channels * offset,
          fifo_write(&p->output_fifo, p->overlap, NULL));
    }
    /* Copy the middle bit to the output */
    if (p->window > 2 * p->overlap)
      fifo_write(&p->output_fifo, p->window - 2 * p->overlap,
                 (float *) fifo_read_ptr(&p->input_fifo) +
                 p->channels * (offset + p->overlap));

    /* Copy the end bit to prev_win_end ready to be mixed with
     * the beginning of the next window. */
    assert(offset + p->window <= fifo_occupancy(&p->input_fifo));
    memcpy(p->prev_win_end,
           (float *) fifo_read_ptr(&p->input_fifo) +
           p->channels * (offset + p->window - p->overlap),
           p->channels * p->overlap * sizeof(*(p->prev_win_end)));

    /* The Advance bit of WSOLA */
    skip = p->factor * (++p->windows_total * (p->window - p->overlap)) + 0.5;
    skip -= (p->seek + 1) >> 1; /* So seek straddles the nominal skip point. */
    p->skip_total += skip -= p->skip_total;
    fifo_read(&p->input_fifo, skip, NULL);

    sox_debug("%3u %u", offset, skip);
  }
}

static void stretch_setup(Stretch * p,
  double sample_rate,
  double factor,      /* 1 for no change, < 1 for slower, > 1 for faster. */
  double window_ms,   /* Processing window length in milliseconds. */
  double seek_ms,     /* Milliseconds to seek for the best overlap position. */
  double overlap_ms,  /* Overlap length in milliseconds. */
  sox_bool quick_seek)/* Whether to quick seek for the best overlap position.*/
{
  size_t max_skip;

  p->factor = factor;
  p->window = sample_rate * window_ms / 1000 + .5;
  p->seek   = sample_rate * seek_ms / 1000 + .5;
  p->overlap = max(sample_rate * overlap_ms / 1000 + 4.5, 16);
  p->overlap &= ~7; /* must be divisible by 8 */
  p->prev_win_end = xmalloc(p->overlap * p->channels * sizeof(*p->prev_win_end));
  p->quick_seek = quick_seek;

  /* # of samples needed in input fifo to process a window */
  max_skip = ceil(factor * (p->window - p->overlap));
  p->process_size = max(max_skip + p->overlap, p->window) + p->seek;
}

static float * stretch_input(Stretch * p, float const *samples, size_t n)
{
  p->samples_in += n;
  return fifo_write(&p->input_fifo, n, samples);
}

static float const * stretch_output(
    Stretch * p, float * samples, size_t * n)
{
  p->samples_out += *n = min(*n, fifo_occupancy(&p->output_fifo));
  return fifo_read(&p->output_fifo, *n, samples);
}

/* Flush samples remaining in the processing pipeline to the output. */
static void stretch_flush(Stretch * p)
{
  size_t samples_out = p->samples_in / p->factor + .5;

  if (p->samples_out < samples_out) {
    size_t remaining = p->samples_in / p->factor + .5 - p->samples_out;
    float * buff = xcalloc(128 * p->channels, sizeof(*buff));

    while (fifo_occupancy(&p->output_fifo) < remaining) {
      stretch_input(p, buff, 128);
      stretch_process_windows(p);
    }
    free(buff);
    fifo_trim(&p->output_fifo, remaining);
    p->samples_in = 0;
  }
}

static void stretch_delete(Stretch * p)
{
  free(p->prev_win_end);
  fifo_delete(&p->output_fifo);
  fifo_delete(&p->input_fifo);
  free(p);
}

static Stretch * stretch_new(size_t channels)
{
  Stretch * p = xcalloc(1, sizeof(*p));
  p->channels = channels;
  fifo_create(&p->input_fifo, p->channels * sizeof(float));
  fifo_create(&p->output_fifo, p->channels * sizeof(float));
  return p;
}

typedef struct tempo {
  Stretch     * stretch;
  sox_bool    quick_seek;
  double      factor, window_ms, seek_ms, overlap_ms;
} priv_t;

assert_static(sizeof(struct tempo) <= SOX_MAX_EFFECT_PRIVSIZE,
              /* else */ tempo_PRIVSIZE_too_big);

static int getopts(sox_effect_t * effp, int argc, char **argv)
{
  priv_t * p = (priv_t *) effp->priv;

  p->window_ms    = 82; /* Set non-zero defaults: */
  p->seek_ms      = 14;
  p->overlap_ms   = 12;

  p->quick_seek = argc && !strcmp(*argv, "-q") && (--argc, ++argv, sox_true);
  do {                    /* break-able block */
    NUMERIC_PARAMETER(factor      ,0.25, 4  )
    NUMERIC_PARAMETER(window_ms   , 10 , 120)
    NUMERIC_PARAMETER(seek_ms     , 3  , 28 )
    NUMERIC_PARAMETER(overlap_ms  , 2  , 24 )
  } while (0);
  return argc || !p->factor || p->overlap_ms + p->seek_ms >= p->window_ms ?
    sox_usage(effp) : SOX_SUCCESS;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;

  if (p->factor == 1)
    return SOX_EFF_NULL;
  p->stretch = stretch_new(effp->ininfo.channels);
  stretch_setup(p->stretch, effp->ininfo.rate, p->factor, p->window_ms,
                p->seek_ms, p->overlap_ms, p->quick_seek);
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_ssample_t * ibuf,
                sox_ssample_t * obuf, sox_size_t * isamp, sox_size_t * osamp)
{
  priv_t * p = (priv_t *) effp->priv;
  sox_size_t i;
  sox_size_t odone = *osamp /= effp->ininfo.channels;
  float const * s = stretch_output(p->stretch, NULL, &odone);

  for (i = 0; i < odone * effp->ininfo.channels; ++i)
    *obuf++ = SOX_FLOAT_32BIT_TO_SAMPLE(*s++, effp->clips);

  if (*isamp && odone < *osamp) {
    float * t = stretch_input(p->stretch, NULL, *isamp / effp->ininfo.channels);
    for (i = *isamp; i; --i)
      *t++ = SOX_SAMPLE_TO_FLOAT_32BIT(*ibuf++, effp->clips);
    stretch_process_windows(p->stretch);
  }
  else *isamp = 0;

  *osamp = odone * effp->ininfo.channels;
  return SOX_SUCCESS;
}

static int drain(sox_effect_t * effp, sox_ssample_t * obuf, sox_size_t * osamp)
{
  static sox_size_t isamp = 0;
  stretch_flush(((priv_t *)effp->priv)->stretch);
  return flow(effp, 0, obuf, &isamp, osamp);
}

static int stop(sox_effect_t * effp)
{
  stretch_delete(((priv_t *)effp->priv)->stretch);
  return SOX_SUCCESS;
}

sox_effect_handler_t const * sox_tempo_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "tempo", "[-q] factor [window-ms [seek-ms [overlap-ms]]]",
    SOX_EFF_MCHAN | SOX_EFF_LENGTH,
    getopts, start, flow, drain, stop, NULL
  };
  return &handler;
}
