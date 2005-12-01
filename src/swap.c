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


#include "st_i.h"

/* Private data for SKEL file */
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
int st_swap_getopts(eff_t effp, int n, char **argv) 
{
    swap_t swap = (swap_t) effp->priv;

    swap->order[0] = swap->order[1] = swap->order[2] = swap->order[3] = 0;
    if (n)
    {
        swap->def_opts = 0;
        if (n != 2 && n != 4)
        {
            st_fail("Usage: swap [1 2 | 1 2 3 4]");
            return (ST_EOF);
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

    return (ST_SUCCESS);
}

/*
 * Prepare processing.
 * Do all initializations.
 */
int st_swap_start(eff_t effp)
{
    swap_t swap = (swap_t) effp->priv;

    if (effp->outinfo.channels == 1)
    {
        st_fail("Can't swap channels on mono data.");
        return (ST_EOF);
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
            st_fail("invalid swap channel options used");
        }
        if (swap->order[0] != 1 && swap->order[0] != 2)
            st_fail("invalid swap channel options used");
        if (swap->order[1] != 1 && swap->order[1] != 2)
            st_fail("invalid swap channel options used");

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
            st_fail("invalid swap channel options used");
        if (swap->order[1] < 1 || swap->order[1] > 4)
            st_fail("invalid swap channel options used");
        if (swap->order[2] < 1 || swap->order[2] > 4)
            st_fail("invalid swap channel options used");
        if (swap->order[3] < 1 || swap->order[3] > 4)
            st_fail("invalid swap channel options used");

        /* Convert to array offsets */
        swap->order[0]--;
        swap->order[1]--;
        swap->order[2]--;
        swap->order[3]--;

    }

    return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
int st_swap_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                 st_size_t *isamp, st_size_t *osamp)
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
    return (ST_SUCCESS);
}

/*
 * Drain out remaining samples if the effect generates any.
 */

int st_swap_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
        *osamp = 0;
        return (ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.  
 *      (free allocated memory, etc.)
 */
int st_swap_stop(eff_t effp)
{
        /* nothing to do */
    return (ST_SUCCESS);
}
