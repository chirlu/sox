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

/* Addressible FIFO buffer */

#include "sox_i.h"
#include "xmalloc.h"

#include <string.h>

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
 * Change tempo (alter duration, maintain pitch) using a time domain
 * WSOLA-like method.  Based on TDStretch.cpp revision 1.24 from The
 * SoundTouch Library Copyright (c) Olli Parviainen 2001-2005.
 */

#include <string.h>
#include <assert.h>
#ifndef max
#define max(a, b) ((a) >= (b) ? (a) : (b))
#endif

typedef enum {FALSE, TRUE} BOOL;

typedef struct {
  size_t samples_in;
  size_t samples_out;
  double factor;
  size_t channels;
  size_t sampleReq;
  float * pMidBuffer;
  float * pRefMidBuffer;
  float * pRefMidBufferUnaligned;
  size_t overlapLength;
  size_t seekLength;
  size_t seekWindowLength;
  size_t maxOffset;
  double nominalSkip;
  double skipFract;
  fifo_t outputBuffer;
  fifo_t inputBuffer;
  BOOL bQuickseek;
  BOOL bMidBufferDirty;
} TDStretch;

static void clearMidBuffer(TDStretch * p)
{
  if (p->bMidBufferDirty) {
    memset((p->pMidBuffer), 0, 2 * sizeof(float) * p->overlapLength);
    p->bMidBufferDirty = FALSE;
  }
}

static void clearInput(TDStretch * p)
{
  p->samples_in = 0;
  fifo_clear(&p->inputBuffer);
  clearMidBuffer(p);
}

static void clear(TDStretch * p)
{
  fifo_clear(&p->outputBuffer);
  fifo_clear(&p->inputBuffer);
  clearMidBuffer(p);
}

/* Slopes the amplitude of the 'midBuffer' samples so that cross correlation */
/* is faster to calculate */
static void precalcCorrReferenceMono(TDStretch * p)
{
  int i;
  float temp;

  for (i = 0; i < (int) p->overlapLength; i++) {
    temp = (float) i *(float) (p->overlapLength - i);

    (p->pRefMidBuffer)[i] = (float) ((p->pMidBuffer)[i] * temp);
  }
}

static void precalcCorrReferenceStereo(TDStretch * p)
{
  int i, cnt2;
  float temp;

  for (i = 0; i < (int) p->overlapLength; i++) {
    temp = (float) i *(float) (p->overlapLength - i);

    cnt2 = i * 2;
    (p->pRefMidBuffer)[cnt2] = (float) ((p->pMidBuffer)[cnt2] * temp);
    (p->pRefMidBuffer)[cnt2 + 1] = (float) ((p->pMidBuffer)[cnt2 + 1] * temp);
  }
}

static double calcCrossCorrMono(
    TDStretch * p, const float * mixingPos, const float * compare)
{
  double corr = 0;
  size_t i = 0;

  /* Loop optimisation: */
  #define _ corr += mixingPos[i] * compare[i], ++i;
  do {_ _ _ _ _ _ _ _} while (i < p->overlapLength);
  #undef _
  return corr;
}

static double calcCrossCorrStereo(
    TDStretch * p, const float * mixingPos, const float * compare)
{
  double corr = 0;
  size_t i = 0;

  /* Loop optimisation: */
  #define _ corr += mixingPos[i]*compare[i] + mixingPos[i+1]*compare[i+1], i+=2;
  do {_ _ _ _ _ _ _ _} while (i < 2 * p->overlapLength);
  #undef _
  return corr;
}

/* Seeks for the optimal overlap-mixing position.  The best position is
 * determined as the position where the two overlapped sample sequences are
 * 'most alike', in terms of the highest cross-correlation value over the
 * overlapping period.  4 variants exist for mono/stereo, quick/accurate */

static size_t seekBestOverlapPositionMono(
    TDStretch * p, const float * refPos)
{
  size_t bestOffs;
  double bestCorr, corr;
  size_t tempOffset;
  const float *compare;

  /* Slopes the amplitude of the 'midBuffer' samples */
  precalcCorrReferenceMono(p);
  bestCorr = INT_MIN;
  bestOffs = 0;

  /* Scans for the best correlation value by testing each possible position */
  /* over the permitted range. */
  for (tempOffset = 0; tempOffset < p->seekLength; tempOffset++) {
    compare = refPos + tempOffset;

    /* Calculates correlation value for the mixing position corresponding */
    /* to 'tempOffset' */
    corr = calcCrossCorrMono(p, p->pRefMidBuffer, compare);

    /* Checks for the highest correlation value */
    if (corr > bestCorr) {
      bestCorr = corr;
      bestOffs = tempOffset;
    }
  }
  return bestOffs;
}

static size_t seekBestOverlapPositionStereo(TDStretch * p,
                                            const float * refPos)
{
  size_t bestOffs;
  double bestCorr, corr;
  size_t i;

  precalcCorrReferenceStereo(p);
  bestCorr = INT_MIN;
  bestOffs = 0;

  for (i = 0; i < p->seekLength; i++) {
    corr = calcCrossCorrStereo(p, refPos + 2 * i, p->pRefMidBuffer);
    if (corr > bestCorr) {
      bestCorr = corr;
      bestOffs = i;
    }
  }
  return bestOffs;
}

/* Table for the quick hierarchical mixing position seeking algorithm */
static int const scanOffsets[4][24] = {
  { 124,  186,  248,  310,  372,  434,  496,  558,  620,  682,  744, 806,
    868,  930,  992, 1054, 1116, 1178, 1240, 1302, 1364, 1426, 1488,   0},
  {-100,  -75,  -50,  -25,   25,   50,   75,  100,    0,    0,    0,   0,
      0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0},
  { -20,  -15,  -10,   -5,    5,   10,   15,   20,    0,    0,    0,   0,
      0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0},
  {  -4,   -3,   -2,   -1,    1,    2,    3,    4,    0,    0,    0,   0,
      0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0}};

static size_t seekBestOverlapPositionMonoQuick(TDStretch * p,
                                               const float * refPos)
{
  size_t j;
  size_t bestOffs;
  double bestCorr, corr;
  size_t scanCount, corrOffset, tempOffset;

  /* Slopes the amplitude of the 'midBuffer' samples */
  precalcCorrReferenceMono(p);
  bestCorr = INT_MIN;
  bestOffs = 0;
  corrOffset = 0;
  tempOffset = 0;

  /* Scans for the best correlation value using four-pass hierarchical
   * search.  The look-up table 'scans' has hierarchical position adjusting
   * steps.  In first pass the routine searhes for the highest correlation
   * with relatively coarse steps, then rescans the neighbourhood of the
   * highest correlation with better resolution and so on. */
  for (scanCount = 0; scanCount < 4; scanCount++) {
    j = 0;
    while (scanOffsets[scanCount][j]) {
      tempOffset = corrOffset + scanOffsets[scanCount][j];
      if (tempOffset >= p->seekLength)
        break;

      /* Calculates correlation value for the mixing position corresponding */
      /* to 'tempOffset' */
      corr = calcCrossCorrMono(p, refPos + tempOffset, (p->pRefMidBuffer));

      /* Checks for the highest correlation value */
      if (corr > bestCorr) {
        bestCorr = corr;
        bestOffs = tempOffset;
      }
      j++;
    }
    corrOffset = bestOffs;
  }

  return bestOffs;
}

static size_t seekBestOverlapPositionStereoQuick(TDStretch * p,
                                                 const float * refPos)
{
  size_t j;
  size_t bestOffs;
  double bestCorr, corr;
  size_t scanCount, corrOffset, tempOffset;

  precalcCorrReferenceStereo(p);

  bestCorr = INT_MIN;
  bestOffs = 0;
  corrOffset = 0;
  tempOffset = 0;

  for (scanCount = 0; scanCount < 4; scanCount++) {
    j = 0;
    while (scanOffsets[scanCount][j]) {
      tempOffset = corrOffset + scanOffsets[scanCount][j];
      if (tempOffset >= p->seekLength)
        break;

      corr =
          calcCrossCorrStereo(p, refPos + 2 * tempOffset, p->pRefMidBuffer);

      if (corr > bestCorr) {
        bestCorr = corr;
        bestOffs = tempOffset;
      }
      j++;
    }
    corrOffset = bestOffs;
  }
  return bestOffs;
}

static size_t seekBestOverlapPosition(TDStretch * p,
                                      const float * refPos)
{
  if (p->channels == 2) {
    if (p->bQuickseek)
      return seekBestOverlapPositionStereoQuick(p, refPos);
    return seekBestOverlapPositionStereo(p, refPos);
  } else if (p->bQuickseek)
    return seekBestOverlapPositionMonoQuick(p, refPos);
  return seekBestOverlapPositionMono(p, refPos);
}


/* Overlaps samples in 'midBuffer' with the samples in 'input' */
static void overlapMono(TDStretch * p, float * output,
                        const float * input)
{
  int i, itemp;

  for (i = 0; i < (int) p->overlapLength; i++) {
    itemp = p->overlapLength - i;
    output[i] = (input[i] * i + (p->pMidBuffer)[i] * itemp) / p->overlapLength;
  }
}

static void overlapStereo(TDStretch * p, float * output,
                          const float * input)
{
  int i;
  size_t cnt2;
  float fTemp;
  float fScale;
  float fi;

  fScale = 1.0f / (float) p->overlapLength;

  for (i = 0; i < (int) p->overlapLength; i++) {
    fTemp = (float) (p->overlapLength - i) * fScale;
    fi = (float) i *fScale;

    cnt2 = 2 * i;
    output[cnt2 + 0] = input[cnt2 + 0] * fi + p->pMidBuffer[cnt2 + 0] * fTemp;
    output[cnt2 + 1] = input[cnt2 + 1] * fi + p->pMidBuffer[cnt2 + 1] * fTemp;
  }
}

/* Overlaps samples in 'midBuffer' with the samples in 'inputBuffer' at
 * position of 'ovlPos'. */
static inline void overlap(TDStretch * p, float * output,
                           const float * input, size_t ovlPos)
{
  if (p->channels == 2)
    overlapStereo(p, output, input + 2 * ovlPos);
  else
    overlapMono(p, output, input + ovlPos);
}

/* Processes as many processing frames of the samples 'inputBuffer', store */
/* the result into 'outputBuffer' */
static void processSamples(TDStretch * p)
{
  size_t ovlSkip, offset, temp;

  if (p->bMidBufferDirty == FALSE) {
    /* if midBuffer is empty, move the first samples of the input stream
     * into it */
    if (fifo_occupancy(&p->inputBuffer) < p->overlapLength)
      return;   /* wait until we've got p->overlapLength samples */
    fifo_read(&p->inputBuffer, p->overlapLength, p->pMidBuffer);
    p->bMidBufferDirty = TRUE;
  }
  /* Process samples as long as there are enough samples in 'inputBuffer'
   * to form a processing frame. */
  while (fifo_occupancy(&p->inputBuffer) >= p->sampleReq) {
    /* If tempo differs from the normal SCALE, scan for the best overlapping
     * position */
    offset = seekBestOverlapPosition(p, fifo_read_ptr(&p->inputBuffer));

    /* Mix the samples in the 'inputBuffer' at position of 'offset' with the
     * samples in 'midBuffer' using sliding overlapping ... first partially
     * overlap with the end of the previous sequence (that's in 'midBuffer') */
    overlap(p, fifo_reserve(&p->outputBuffer, p->overlapLength),
            fifo_read_ptr(&p->inputBuffer), offset);

    /* ... then copy sequence samples from 'inputBuffer' to output */
    temp = (p->seekWindowLength - 2 * p->overlapLength);    /* & 0xfffffffe; */
    if ((int)temp > 0) {
      fifo_write(&p->outputBuffer, temp,
                 (float *) fifo_read_ptr(&p->inputBuffer) +
                 p->channels * (offset + p->overlapLength));
    }
    /* Copies the end of the current sequence from 'inputBuffer' to
     * 'midBuffer' for being mixed with the beginning of the next
     * processing sequence and so on */
    assert(offset + p->seekWindowLength <= fifo_occupancy(&p->inputBuffer));
    memcpy(p->pMidBuffer,
           (float *) fifo_read_ptr(&p->inputBuffer) +
           p->channels * (offset + p->seekWindowLength - p->overlapLength),
           p->channels * sizeof(float) * p->overlapLength);
    p->bMidBufferDirty = TRUE;

    /* Remove the processed samples from the input buffer. Update
     * the difference between integer & nominal skip step to 'p->skipFract'
     * in order to prevent the error from accumulating over time. */
    p->skipFract += p->nominalSkip;     /* real skip size */
    ovlSkip = (int) p->skipFract;       /* rounded to integer skip */
    p->skipFract -= ovlSkip;    /* maintain the fraction part, i.e. real vs. integer skip */
    fifo_read(&p->inputBuffer, ovlSkip, NULL);
  }
}

/* Set new overlap length parameter & reallocate RefMidBuffer if necessary. */
static void acceptNewOverlapLength(TDStretch * p, size_t newOverlapLength)
{
  size_t prevOvl;

  prevOvl = p->overlapLength;
  p->overlapLength = newOverlapLength;
  if (p->overlapLength > prevOvl) {
    free(p->pMidBuffer);
    free(p->pRefMidBufferUnaligned);
    p->pMidBuffer = xcalloc(p->overlapLength * 2, sizeof(float));
    p->bMidBufferDirty = TRUE;
    clearMidBuffer(p);
    p->pRefMidBufferUnaligned = xcalloc(
        2 * p->overlapLength + 16 / sizeof(float), sizeof(float));

    /* For efficiency, align 'pRefMidBuffer' to 16 byte boundary */
    p->pRefMidBuffer = (float *)
      ((((unsigned long) (p->pRefMidBufferUnaligned)) + 15ul) & ~15ul);
  }
}

/*  Sets routine control parameters. These control are certain time constants
 *  defining how the sound is stretched to the desired duration.
 *    'sampleRate' = sample rate of the sound
 *    'sequenceMS' = one processing sequence length in milliseconds
 *    'seekwindowMS' = seeking window length for scanning the best overlapping
 *       position
 *    'overlapMS' = overlapping length
 *    'tempo' = 1 for no change, < 1 for slower, > 1 for faster.
 *    'quickSeek' = whether to use a quick seek for the best overlapping
 *    position.
 */
static void setParameters(TDStretch * p, double sampleRate, double tempo,
    double sequenceMs, double seekWindowMs, double overlapMs, BOOL quickSeek)
{
  size_t newOvl;
  size_t intskip;

  p->factor = tempo;
  p->bQuickseek = quickSeek;
  p->maxOffset = p->seekLength = sampleRate * seekWindowMs / 1000 + .5;
  p->seekWindowLength = sampleRate * sequenceMs / 1000 + .5;
  newOvl = max(sampleRate * overlapMs / 1000 + 4.5, 16);
  newOvl &= ~7; /* must be divisible by 8 */
  acceptNewOverlapLength(p, newOvl);

  /* Calculate ideal skip length (according to tempo value)  */
  p->nominalSkip = tempo * (p->seekWindowLength - p->overlapLength);
  p->skipFract = 0;
  intskip = (int) (p->nominalSkip + 0.5);

  /* Calculate how many samples are needed in the 'inputBuffer' to  */
  /* process another batch of samples */
  p->sampleReq =
      max(intskip + p->overlapLength, p->seekWindowLength) + p->maxOffset;
}

static float * putSamples(TDStretch * p, float const *samples, size_t n)
{
  p->samples_in += n;
  return fifo_write(&p->inputBuffer, n, samples);
}

static float const * receiveSamples(
    TDStretch * p, float * samples, size_t * n)
{
  p->samples_out += *n = min(*n, fifo_occupancy(&p->outputBuffer));
  return fifo_read(&p->outputBuffer, *n, samples);
}

/* Flushes the last samples from the processing pipeline to the output.
 * Clears also the internal processing buffers.
 *
 * Note: This function is meant for extracting the last samples of a sound
 * stream. This function may introduce additional blank samples in the end
 * of the sound stream, and thus it's not recommended to call this function
 * in the middle of a sound stream. */
static void flush(TDStretch * p)
{
  size_t samples_out = p->samples_in / p->factor + .5;

  if (p->samples_out < samples_out) {
    size_t remaining = p->samples_in / p->factor + .5 - p->samples_out;
    float buff[128];
    memset(buff, 0, sizeof(buff));

    while (fifo_occupancy(&p->outputBuffer) < remaining) {
      putSamples(p, buff, sizeof(buff)/sizeof(buff[0])/p->channels);
      processSamples(p);
    }
    fifo_trim(&p->outputBuffer, remaining);
    clearInput(p);
  }
}

static void deleteTDStretch(TDStretch * p)
{
  free(p->pMidBuffer);
  free(p->pRefMidBufferUnaligned);
  fifo_delete(&p->outputBuffer);
  fifo_delete(&p->inputBuffer);
  free(p);
}

static TDStretch * newTDStretch(size_t channels)
{
  TDStretch * p = xcalloc(1, sizeof(*p));
  p->channels = channels;
  fifo_create(&p->inputBuffer, p->channels * sizeof(float));
  fifo_create(&p->outputBuffer, p->channels * sizeof(float));
  return p;
}

/*
 * libSoX tempo effect: adjust the audio tempo (but not key)
 *
 * Adjustment is given as the ratio of the new tempo to the old tempo.
 */

#include "sox_i.h"
#include <math.h>

typedef struct tempo {
  TDStretch   * tdstretch;
  sox_bool    quick_seek;
  double      factor, sequence_ms, seek_window_ms, overlap_ms;
} priv_t;

assert_static(sizeof(struct tempo) <= SOX_MAX_EFFECT_PRIVSIZE,
              /* else */ tempo_PRIVSIZE_too_big);

static int getopts(sox_effect_t * effp, int argc, char **argv)
{
  priv_t * p = (priv_t *) effp->priv;

  p->sequence_ms    = 82; /* Set non-zero defaults: */
  p->seek_window_ms = 14;
  p->overlap_ms     = 12;

  p->quick_seek = !argc || strcmp(*argv, "-l") || (--argc, ++argv, sox_false);
  do {                    /* break-able block */
    NUMERIC_PARAMETER(factor        ,0.25, 4  )
    NUMERIC_PARAMETER(sequence_ms   , 10 , 120)
    NUMERIC_PARAMETER(seek_window_ms, 7  , 28 )
    NUMERIC_PARAMETER(overlap_ms    , 6  , 24 )
  } while (0);

  sox_debug("factor:%g sequence:%g seek:%g overlap:%g quick:%i", p->factor,
      p->sequence_ms, p->seek_window_ms, p->overlap_ms, p->quick_seek);
  return argc || !p->factor? sox_usage(effp) : SOX_SUCCESS;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;

  if (p->factor == 1)
    return SOX_EFF_NULL;

  if (effp->ininfo.channels > 2) {
    sox_fail("supports only mono or stereo audio");
    return SOX_EOF;
  }
  p->tdstretch = newTDStretch(effp->ininfo.channels);
  setParameters(p->tdstretch, effp->ininfo.rate, p->factor, p->sequence_ms,
                p->seek_window_ms, p->overlap_ms, p->quick_seek);
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_ssample_t * ibuf,
                sox_ssample_t * obuf, sox_size_t * isamp, sox_size_t * osamp)
{
  priv_t * p = (priv_t *) effp->priv;
  sox_size_t i;
  sox_size_t odone = *osamp /= effp->ininfo.channels;
  float const * s = receiveSamples(p->tdstretch, NULL, &odone);

  for (i = 0; i < odone * effp->ininfo.channels; ++i)
    *obuf++ = SOX_FLOAT_32BIT_TO_SAMPLE(*s++, effp->clips);

  if (*isamp && odone < *osamp) {
    float * t = putSamples(p->tdstretch, NULL, *isamp / effp->ininfo.channels);
    for (i = *isamp; i; --i)
      *t++ = SOX_SAMPLE_TO_FLOAT_32BIT(*ibuf++, effp->clips);
    processSamples(p->tdstretch);
  }
  else *isamp = 0;

  *osamp = odone * effp->ininfo.channels;
  return SOX_SUCCESS;
}

static int drain(sox_effect_t * effp, sox_ssample_t * obuf, sox_size_t * osamp)
{
  static sox_size_t isamp = 0;
  flush(((priv_t *)effp->priv)->tdstretch);
  return flow(effp, 0, obuf, &isamp, osamp);
}

static int stop(sox_effect_t * effp)
{
  deleteTDStretch(((priv_t *)effp->priv)->tdstretch);
  return SOX_SUCCESS;
}

sox_effect_handler_t const * sox_tempo_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "tempo", "[-l] factor [window-ms [seek-ms [overlap-ms]]]",
    SOX_EFF_MCHAN | SOX_EFF_LENGTH,
    getopts, start, flow, drain, stop, NULL
  };
  return &handler;
}
