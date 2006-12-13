/*
 * skeleff - Skelton Effect.  Use as sample for new effects.
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

#include "st_i.h"

/* Private data for effect */
typedef struct skeleff {
        int  localdata;
} *skeleff_t;

assert_static(sizeof(struct skeleff) <= ST_MAX_EFFECT_PRIVSIZE, 
    /* else */ skelleff_PRIVSIZE_too_big);

/*
 * Process options
 *
 * Don't do initialization now.
 * The 'info' fields are not yet filled in.
 */
static int st_skeleff_getopts(eff_t effp, int n, char **argv) 
{
    skeleff_t skeleff = (skeleff_t) effp->priv;

    if (n && n != 1) {
      st_fail("Usage: skeleff [option]");
      return ST_EOF;
    }

    return ST_SUCCESS;
}

/*
 * Prepare processing.
 * Do all initializations.
 * If there's nothing to do, use st_effect_nothing instead.
 */
static int st_skeleff_start(eff_t effp)
{
    if (effp->outinfo.channels == 1) {
        st_fail("Can't run skeleff on mono data.");
        return ST_EOF;
    }

    return ST_SUCCESS;
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
static int st_skeleff_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                    st_size_t *isamp, st_size_t *osamp)
{
    skeleff_t skeleff = (skeleff_t) effp->priv;
    int len, done;

    switch (effp->outinfo.channels) {
    case 2:
        /* Length to process will be buffer length / 2 since we
         * work with two samples at a time.
         */
        len = ((*isamp > *osamp) ? *osamp : *isamp) / 2;
        for(done = 0; done < len; done++)
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

    return ST_SUCCESS;
}

/*
 * Drain out remaining samples if the effect generates any.
 * If there's nothing to do, use st_effect_nothing_drain instead.
 */
static int st_skeleff_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
        *osamp = 0;
        /* Help out application and return ST_EOF when drain
         * will not return any mre information.  *osamp == 0
         * also indicates that.
         */
        return ST_EOF;
}

/*
 * Do anything required when you stop reading samples.  
 *      (free allocated memory, etc.)
 * If there's nothing to do, use st_effect_nothing instead.
 */
static int st_skeleff_stop(eff_t effp)
{
    return ST_SUCCESS;
}


/*
 * Effect descriptor.
 * If one of the methods does nothing, use the relevant
 * st_effect_nothing* method.
 */
static st_effect_t st_skel_effect = {
  "skel",
  "Usage: skel -x",
  ST_EFF_MCHAN,
  st_skel_getopts,
  st_skel_start,
  st_skel_flow,
  st_skel_drain,
  st_skel_stop
};

/*
 * Function returning effect descriptor. This should be the only
 * externally visible object.
 */
const st_effect_t *st_skel_effect_fn(void)
{
  return &st_skel_effect;
}
