/*
 * earwax - makes listening to headphones easier
 * 
 * This effect takes a stereo sound that is meant to be listened to
 * on headphones, and adds audio cues to move the soundstage from inside
 * your head (standard for headphones) to outside and in front of the
 * listener (standard for speakers). This makes the sound much easier to
 * listen to on headphones. See www.geocities.com/beinges for a full
 * explanation.
 * 
 * Usage: 
 *   earwax
 *
 * Note:
 *   This filter only works for 44.1 kHz stereo signals (cd format)
 * 
 * November 9, 2000
 * Copyright (C) 2000 Edward Beingessner And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Edward Beingessner And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

#include "sox_i.h"

static sox_effect_t sox_earwax_effect;

#define EARWAX_SCALE 64

/* A stereo fir filter. One side filters as if the signal was from
   30 degrees from the ear, the other as if 330 degrees. */
/*                           30   330  */
static const sox_ssample_t filt[]    =
{   4,  -6,
    4,  -11,
    -1,  -5,
    3,   3,
    -2,   5,
    -5, 0,
    9,  1,
    6,  3,
    -4, -1,
    -5, -3, 
    -2, -5,
    -7,  1,
    6,   -7,
    30,  -29,
    12,  -3,
    -11,  4,
    -3,   7,
    -20,  23,
    2,    0,
    1,    -6,
    -14,  -5,
    15,   -18,
    6,    7,
    15,   -10,
    -14,  22,
    -7,   -2,
    -4,   9,
    6,    -12,
    6,    -6,
    0,    -11,
    0,    -5, 
    4,     0};   

/* 32 tap stereo FIR filter needs 64 taps */
#define EARWAX_NUMTAPS  64

typedef struct earwaxstuff {
  sox_ssample_t *tap; /* taps are z^-1 delays for the FIR filter */
} *earwax_t;

/*
 * Prepare for processing.
 */
static int sox_earwax_start(eff_t effp)
{
  earwax_t earwax = (earwax_t) effp->priv;
  int i;

  /* check the input format */
  if (effp->ininfo.rate != 44100 || effp->ininfo.channels != 2) {
    sox_fail("The earwax effect works only with 44.1 kHz, stereo audio.");
    return (SOX_EOF);
  }

  /* allocate tap memory */
  earwax->tap = (sox_ssample_t*)xmalloc( sizeof(sox_ssample_t) * EARWAX_NUMTAPS );

  /* zero out the delayed taps */
  for(i=0; i < EARWAX_NUMTAPS; i++ ){
    earwax->tap[i] = 0;
  }

  return (SOX_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

static int sox_earwax_flow(eff_t effp, const sox_ssample_t *ibuf, sox_ssample_t *obuf, 
                   sox_size_t *isamp, sox_size_t *osamp)
{
  earwax_t earwax = (earwax_t) effp->priv;
  int len, done;
  int i;
  sox_ssample_t output;

  len = ((*isamp > *osamp) ? *osamp : *isamp);

  for(done = 0; done < len; done++) {

    /* update taps and calculate output */
    output = 0;
    for(i = EARWAX_NUMTAPS-1; i > 0; i--) {
      earwax->tap[i] = earwax->tap[i-1];
      output += earwax->tap[i] * filt[i];
    }
    earwax->tap[0] = *ibuf++ / EARWAX_SCALE;
    output += earwax->tap[0] * filt[i];

    /* store scaled output */
    *obuf++ = output; 
  }

  *isamp = *osamp = len;
  return (SOX_SUCCESS);
}

/*
 * Drain out taps.
 */
static int sox_earwax_drain(eff_t effp, sox_ssample_t *obuf, sox_size_t *osamp)
{
  earwax_t earwax = (earwax_t) effp->priv;
  int i,j;
  sox_ssample_t output;  

  for(i = EARWAX_NUMTAPS-1; i >= 0; i--){
    output = 0;
    for(j = 0; j < i; j++ ){
      output += filt[j+(EARWAX_NUMTAPS-i)] * earwax->tap[j];
    } 
    *obuf++ = output;
  }
  *osamp = EARWAX_NUMTAPS-1;

  return (SOX_EOF);
}

/*
 * Clean up taps.
 */
static int sox_earwax_stop(eff_t effp)
{
  earwax_t earwax = (earwax_t) effp->priv;

  free((char *)earwax->tap);

  return (SOX_SUCCESS);
}

static sox_effect_t sox_earwax_effect = {
  "earwax",
  "Usage: The earwax filtering effect takes no options",
  SOX_EFF_MCHAN|SOX_EFF_LENGTH,
  sox_effect_nothing_getopts,
  sox_earwax_start,
  sox_earwax_flow,
  sox_earwax_drain,
  sox_earwax_stop,
  sox_effect_nothing
};

const sox_effect_t *sox_earwax_effect_fn(void)
{
    return &sox_earwax_effect;
}
