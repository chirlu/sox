/*
 * swap - effect to swap ordering of channels in multi-channel audio.
 *
 * Written by Chris Bagwell (cbagwell@sprynet.com) - March 16, 1999
 *
  * Copyright 1999 Chris Bagwell And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Chris Bagwell And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */


#include "sox_i.h"

static sox_effect_t sox_swap_effect;

typedef struct swapstuff {
    int         order[4];
    int         def_opts;
} *swap_t;

/*
 * Process options
 *
 * Don't do initialization now.
 * The 'info' fields are not yet filled in.
 */
static int sox_swap_getopts(eff_t effp, int n, char **argv) 
{
    swap_t swap = (swap_t) effp->priv;

    swap->order[0] = swap->order[1] = swap->order[2] = swap->order[3] = 0;
    if (n)
    {
        swap->def_opts = 0;
        if (n != 2 && n != 4)
        {
            sox_fail(sox_swap_effect.usage);
            return (SOX_EOF);
        }
        else if (n == 2)
        {
            sscanf(argv[0],"%d",&swap->order[0]);
            sscanf(argv[1],"%d",&swap->order[1]);
        }
        else
        {
            sscanf(argv[0],"%d",&swap->order[0]);
            sscanf(argv[1],"%d",&swap->order[1]);
            sscanf(argv[2],"%d",&swap->order[2]);
            sscanf(argv[3],"%d",&swap->order[3]);
        }
    }
    else
        swap->def_opts = 1;

    return (SOX_SUCCESS);
}

/*
 * Prepare processing.
 * Do all initializations.
 */
static int sox_swap_start(eff_t effp)
{
    swap_t swap = (swap_t) effp->priv;
    int i;

    if (effp->outinfo.channels == 1)
    {
        sox_fail("Can't swap channels on mono data.");
        return (SOX_EOF);
    }

    if (effp->outinfo.channels == 2)
    {
        if (swap->def_opts)
        {
            swap->order[0] = 2;
            swap->order[1] = 1;
        }

        if (swap->order[2] || swap->order[3])
        {
            sox_fail("invalid swap channel options used");
        }
        if (swap->order[0] != 1 && swap->order[0] != 2)
            sox_fail("invalid swap channel options used");
        if (swap->order[1] != 1 && swap->order[1] != 2)
            sox_fail("invalid swap channel options used");

        /* Convert to array offsets */
        swap->order[0]--;
        swap->order[1]--;
    }

    if (effp->outinfo.channels == 4)
    {
        if (swap->def_opts)
        {
            swap->order[0] = 2;
            swap->order[1] = 1;
            swap->order[2] = 4;
            swap->order[3] = 3;
        }

        if (swap->order[0] < 1 || swap->order[0] > 4)
            sox_fail("invalid swap channel options used");
        if (swap->order[1] < 1 || swap->order[1] > 4)
            sox_fail("invalid swap channel options used");
        if (swap->order[2] < 1 || swap->order[2] > 4)
            sox_fail("invalid swap channel options used");
        if (swap->order[3] < 1 || swap->order[3] > 4)
            sox_fail("invalid swap channel options used");

        /* Convert to array offsets */
        swap->order[0]--;
        swap->order[1]--;
        swap->order[2]--;
        swap->order[3]--;

    }

    for (i = 0; i < (int)effp->outinfo.channels; ++i)
      if (swap->order[i] != i)
        return SOX_SUCCESS;

    return SOX_EFF_NULL;
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
static int sox_swap_flow(eff_t effp, const sox_ssample_t *ibuf, sox_ssample_t *obuf, 
                 sox_size_t *isamp, sox_size_t *osamp)
{
    swap_t swap = (swap_t) effp->priv;
    int len, done;

    switch (effp->outinfo.channels)
    {
    case 2:
        /* Length to process will be buffer length / 2 since we
         * work with two samples at a time.
         */
        len = ((*isamp > *osamp) ? *osamp : *isamp) / 2;
        for(done = 0; done < len; done++)
        {
            obuf[0] = ibuf[swap->order[0]];
            obuf[1] = ibuf[swap->order[1]];
            /* Advance buffer by 2 samples */
            ibuf += 2;
            obuf += 2;
        }
        
        *isamp = len * 2;
        *osamp = len * 2;
        
        break;
        
    case 4:
        /* Length to process will be buffer length / 4 since we
         * work with four samples at a time.
         */
        len = ((*isamp > *osamp) ? *osamp : *isamp) / 4;
        for(done = 0; done < len; done++)
        {
            obuf[0] = ibuf[swap->order[0]];
            obuf[1] = ibuf[swap->order[1]];
            obuf[2] = ibuf[swap->order[2]];
            obuf[3] = ibuf[swap->order[3]];
            /* Advance buffer by 2 samples */
            ibuf += 4;
            obuf += 4;
        }
        *isamp = len * 4;
        *osamp = len * 4;
        
        break;
    }
    return (SOX_SUCCESS);
}

static sox_effect_t sox_swap_effect = {
  "swap",
  "Usage: swap [1 2 | 1 2 3 4]",
  SOX_EFF_MCHAN,
  sox_swap_getopts,
  sox_swap_start,
  sox_swap_flow,
  sox_effect_nothing_drain,
  sox_effect_nothing,
  sox_effect_nothing  
};

const sox_effect_t *sox_swap_effect_fn(void)
{
    return &sox_swap_effect;
}
