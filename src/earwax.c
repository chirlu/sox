/*
 * November 9, 2000
 * Copyright (C) 2000 Edward Beingessner And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Edward Beingessner And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

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
*/

#include "st_i.h"

#define EARWAX_SCALE 64

/* A stereo fir filter. One side filters as if the signal was from
   30 degrees from the ear, the other as if 330 degrees. */
/*                           30   330  */
static const st_sample_t filt[]    =
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
  st_sample_t *tap; /* taps are z^-1 delays for the FIR filter */
} *earwax_t;

/*
 * Process options
 */
int st_earwax_getopts(eff_t effp, int n, char **argv) 
{
  /* no options */
  if (n){
    st_fail("The earwax filtering effect takes no options.\n");
    return (ST_EOF);
  }
  return (ST_SUCCESS);
}

/*
 * Prepare for processing.
 */
int st_earwax_start(eff_t effp)
{
  earwax_t earwax = (earwax_t) effp->priv;
  int i;

  /* check the input format */
  if (effp->ininfo.encoding != ST_ENCODING_SIGN2
      || effp->ininfo.rate != 44100
      || effp->ininfo.channels != 2){
    st_fail("the earwax effect works only with audio cd (44.1 kHz, twos-complement signed linear, stereo) samples.\n");
    return (ST_EOF);
  }

  /* allocate tap memory */
  earwax->tap = (st_sample_t*)malloc( sizeof(st_sample_t) * EARWAX_NUMTAPS );
  if( !earwax->tap ){
    st_fail("earwax: Cannot malloc %d bytes!\n", 
	    sizeof(st_sample_t) * EARWAX_NUMTAPS );
    return (ST_EOF);
  }

  /* zero out the delayed taps */
  for(i=0; i < EARWAX_NUMTAPS; i++ ){
    earwax->tap[i] = 0;
  }

  return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

int st_earwax_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                   st_size_t *isamp, st_size_t *osamp)
{
  earwax_t earwax = (earwax_t) effp->priv;
  int len, done;
  int i;
  st_sample_t output;

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
  return (ST_SUCCESS);
}

/*
 * Drain out taps.
 */
int st_earwax_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
  earwax_t earwax = (earwax_t) effp->priv;
  int i,j;
  st_sample_t output;  

  for(i = EARWAX_NUMTAPS-1; i >= 0; i--){
    output = 0;
    for(j = 0; j < i; j++ ){
      output += filt[j+(EARWAX_NUMTAPS-i)] * earwax->tap[j];
    } 
    *obuf++ = output;
  }
  *osamp = EARWAX_NUMTAPS-1;

  return (ST_SUCCESS);
}

/*
 * Clean up taps.
 */
int st_earwax_stop(eff_t effp)
{
  earwax_t earwax = (earwax_t) effp->priv;

  free((char *)earwax->tap);

  return (ST_SUCCESS);
}
