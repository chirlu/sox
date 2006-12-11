/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools skeleton effect file.
 */

#include "st_i.h"
#include "string.h" /* memcpy() */

static st_effect_t st_copy_effect;

/*
 * Process options
 */
static int st_copy_getopts(eff_t effp UNUSED, int n, char **argv UNUSED) 
{
        if (n)
        {
                st_fail(st_copy_effect.usage);
                return (ST_EOF);
        }
        return (ST_SUCCESS);
}

/*
 * Start processing
 */
static int st_copy_start(eff_t effp UNUSED)
{
        /* nothing to do */
        /* stuff data into delaying effects here */
        return (ST_SUCCESS);
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */
static int st_copy_flow(eff_t effp UNUSED, const st_sample_t *ibuf, st_sample_t *obuf, 
                 st_size_t *isamp, st_size_t *osamp)
{
        int done;
        
        done = ((*isamp < *osamp) ? *isamp : *osamp);
        memcpy(obuf, ibuf, done * sizeof(st_sample_t));
        *isamp = *osamp = done;
        return (ST_SUCCESS);
}

static st_effect_t st_copy_effect = {
  "copy",
  "Usage: Copy effect takes no options",
  ST_EFF_MCHAN,
  st_copy_getopts,
  st_copy_start,
  st_copy_flow,
  st_effect_nothing_drain,
  st_effect_nothing
};

const st_effect_t *st_copy_effect_fn(void)
{
    return &st_copy_effect;
}
