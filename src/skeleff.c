/*
 * skeleff - Skeleton Effect.  Use as sample for new effects.
 *
 * Copyright 1999 Chris Bagwell And Sundry Contributors
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

/* Private data for effect */
typedef struct skeleff {
  int  localdata;
} *skeleff_t;

assert_static(sizeof(struct skeleff) <= SOX_MAX_EFFECT_PRIVSIZE, 
              /* else */ skeleff_PRIVSIZE_too_big);

/*
 * Process command-line options but don't do other
 * initialization now: effp->ininfo & effp->outinfo are not
 * yet filled in.
 */
static int getopts(sox_effect_t * effp, int n, char **argv)
{
  skeleff_t skeleff = (skeleff_t)effp->priv;

  if (n && n != 1)
    return sox_usage(effp);

  return SOX_SUCCESS;
}

/*
 * Prepare processing.
 * Do all initializations.
 */
static int start(sox_effect_t * effp)
{
  if (effp->outinfo.channels == 1) {
    sox_fail("Can't run on mono data.");
    return SOX_EOF;
  }

  return SOX_SUCCESS;
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
static int flow(sox_effect_t * effp, const sox_ssample_t *ibuf, sox_ssample_t *obuf, 
                           sox_size_t *isamp, sox_size_t *osamp)
{
  skeleff_t skeleff = (skeleff_t)effp->priv;
  sox_size_t len, done;

  switch (effp->outinfo.channels) {
  case 2:
    /* Length to process will be buffer length / 2 since we
     * work with two samples at a time.
     */
    len = min(*isamp, *osamp) / 2;
    for (done = 0; done < len; done++)
      {
        obuf[0] = ibuf[0];
        obuf[1] = ibuf[1];
        /* Advance buffer by 2 samples */
        ibuf += 2;
        obuf += 2;
      }
        
    *isamp = len * 2;
    *osamp = len * 2;
        
    break;
  }

  return SOX_SUCCESS;
}

/*
 * Drain out remaining samples if the effect generates any.
 */
static int drain(sox_effect_t * effp, sox_ssample_t *obuf, sox_size_t *osamp)
{
  *osamp = 0;
  /* Return SOX_EOF when drain
   * will not output any more samples.
   * *osamp == 0 also indicates that.
   */
  return SOX_EOF;
}

/*
 * Do anything required when you stop reading samples.  
 */
static int stop(sox_effect_t * effp)
{
  return SOX_SUCCESS;
}

/*
 * Do anything required when you kill an effect.  
 *      (free allocated memory, etc.)
 */
static int kill(sox_effect_t * effp)
{
  return SOX_SUCCESS;
}


/*
 * Effect descriptor.
 * If no specific processing is needed for any of
 * the 6 functions, then the function above can be deleted
 * and 0 used in place of the its name below.
 */
static sox_effect_handler_t sox_skel_effect = {
  "skel",
  "[OPTION]",
  SOX_EFF_MCHAN,
  getopts,
  start,
  flow,
  drain,
  stop,
  kill,
};

/*
 * Function returning effect descriptor. This should be the only
 * externally visible object.
 */
const sox_effect_handler_t *sox_skel_effect_fn(void)
{
  return &sox_skel_effect;
}
