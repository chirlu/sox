/*
 * August 24, 1998
 * Copyright (C) 1998 Juergen Mueller And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Juergen Mueller And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 *      Phaser effect.
 * 
 * Flow diagram scheme:
 *
 *        * gain-in  +---+                     * gain-out
 * ibuff ----------->|   |----------------------------------> obuff
 *                   | + |  * decay
 *                   |   |<------------+
 *                   +---+  _______    |
 *                     |   |       |   |
 *                     +---| delay |---+
 *                         |_______|
 *                            /|\
 *                             |
 *                     +---------------+      +------------------+
 *                     | Delay control |<-----| modulation speed |
 *                     +---------------+      +------------------+
 *
 *
 * The delay is controled by a sine or triangle modulation.
 *
 * Usage: 
 *   phaser gain-in gain-out delay decay speed [ -s | -t ]
 *
 * Where:
 *   gain-in, decay :  0.0 ... 1.0      volume
 *   gain-out :  0.0 ...      volume
 *   delay :  0.0 ... 5.0 msec
 *   speed :  0.1 ... 2.0 Hz       modulation
 *   -s : modulation by sine (default)
 *   -t : modulation by triangle
 *
 * Note:
 *   when decay is close to 1.0, the samples may begin clipping or the output
 *   can saturate! 
 *
 * Hint:
 *   in-gain < ( 1 - decay * decay )
 *   1 / out-gain > gain-in / ( 1 - decay )
 *
*/

/*
 * libSoX phaser effect file.
 */

#include <stdlib.h> /* Harmless, and prototypes atof() etc. --dgc */
#include <math.h>
#include <string.h>
#include "sox_i.h"

#define MOD_SINE        0
#define MOD_TRIANGLE    1

/* Private data for SKEL file */
typedef struct phaserstuff {
        int     modulation;
        int     counter;                        
        int     phase;
        double  *phaserbuf;
        float   in_gain, out_gain;
        float   delay, decay;
        float   speed;
        sox_size_t length;
        int     *lookup_tab;
        sox_size_t maxsamples, fade_out;
} *phaser_t;

/*
 * Process options
 */
static int sox_phaser_getopts(sox_effect_t * effp, int n, char **argv) 
{
        phaser_t phaser = (phaser_t) effp->priv;

        if (!((n == 5) || (n == 6)))
        {
            sox_fail(effp->handler.usage);
            return (SOX_EOF);
        }

        sscanf(argv[0], "%f", &phaser->in_gain);
        sscanf(argv[1], "%f", &phaser->out_gain);
        sscanf(argv[2], "%f", &phaser->delay);
        sscanf(argv[3], "%f", &phaser->decay);
        sscanf(argv[4], "%f", &phaser->speed);
        phaser->modulation = MOD_SINE;
        if ( n == 6 ) {
                if ( !strcmp(argv[5], "-s"))
                        phaser->modulation = MOD_SINE;
                else if ( ! strcmp(argv[5], "-t"))
                        phaser->modulation = MOD_TRIANGLE;
                else
                {
                        sox_fail(effp->handler.usage);
                        return (SOX_EOF);
                }
        }
        return (SOX_SUCCESS);
}

/*
 * Prepare for processing.
 */
static int sox_phaser_start(sox_effect_t * effp)
{
        phaser_t phaser = (phaser_t) effp->priv;
        unsigned int i;

        phaser->maxsamples = phaser->delay * effp->ininfo.rate / 1000.0;

        if ( phaser->delay < 0.0 )
        {
            sox_fail("phaser: delay must be positive!");
            return (SOX_EOF);
        }
        if ( phaser->delay > 5.0 )
        {
            sox_fail("phaser: delay must be less than 5.0 msec!");
            return (SOX_EOF);
        }
        if ( phaser->speed < 0.1 )
        {
            sox_fail("phaser: speed must be more than 0.1 Hz!");
            return (SOX_EOF);
        }
        if ( phaser->speed > 2.0 )
        {
            sox_fail("phaser: speed must be less than 2.0 Hz!");
            return (SOX_EOF);
        }
        if ( phaser->decay < 0.0 )
        {
            sox_fail("phaser: decay must be positive!" );
            return (SOX_EOF);
        }
        if ( phaser->decay >= 1.0 )
        {
            sox_fail("phaser: decay must be less that 1.0!" );
            return (SOX_EOF);
        }
        /* Be nice and check the hint with warning, if... */
        if ( phaser->in_gain > ( 1.0 - phaser->decay * phaser->decay ) )
                sox_warn("phaser: warning >>> gain-in can cause saturation or clipping of output <<<");
        if ( phaser->in_gain / ( 1.0 - phaser->decay ) > 1.0 / phaser->out_gain )
                sox_warn("phaser: warning >>> gain-out can cause saturation or clipping of output <<<");

        phaser->length = effp->ininfo.rate / phaser->speed;
        phaser->phaserbuf = (double *) xmalloc(sizeof (double) * phaser->maxsamples);
        for ( i = 0; i < phaser->maxsamples; i++ )
                phaser->phaserbuf[i] = 0.0;
        phaser->lookup_tab = (int *) xmalloc(sizeof (int) * phaser->length);

        if (phaser->modulation == MOD_SINE)
          sox_generate_wave_table(SOX_WAVE_SINE, SOX_INT, phaser->lookup_tab,
              phaser->length, 0., (double)(phaser->maxsamples - 1), 0.);
        else
          sox_generate_wave_table(SOX_WAVE_TRIANGLE, SOX_INT, phaser->lookup_tab,
              phaser->length, 0., (double)(2 * (phaser->maxsamples - 1)), 3 * M_PI_2);
        phaser->counter = 0;
        phaser->phase = 0;
        phaser->fade_out = phaser->maxsamples;
        return (SOX_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
static int sox_phaser_flow(sox_effect_t * effp, const sox_ssample_t *ibuf, sox_ssample_t *obuf, 
                   sox_size_t *isamp, sox_size_t *osamp)
{
        phaser_t phaser = (phaser_t) effp->priv;
        double d_in, d_out;
        sox_ssample_t out;
        sox_size_t len = min(*isamp, *osamp);
        *isamp = *osamp = len;

        while (len--) {
                /* Store delays as 24-bit signed longs */
                d_in = (double) *ibuf++ / 256;
                /* Compute output first */
                d_in = d_in * phaser->in_gain;
                d_in += phaser->phaserbuf[(phaser->maxsamples + 
        phaser->counter - phaser->lookup_tab[phaser->phase]) % 
        phaser->maxsamples] * phaser->decay * -1.0;
                /* Adjust the output volume and size to 24 bit */
                d_out = d_in * phaser->out_gain;
                out = SOX_24BIT_CLIP_COUNT((sox_ssample_t) d_out, effp->clips);
                *obuf++ = out * 256;
                /* Mix decay of delay and input */
                phaser->phaserbuf[phaser->counter] = d_in;
                phaser->counter = 
                        ( phaser->counter + 1 ) % phaser->maxsamples;
                phaser->phase  = ( phaser->phase + 1 ) % phaser->length;
        }
        /* processed all samples */
        return (SOX_SUCCESS);
}

/*
 * Drain out reverb lines. 
 */
static int sox_phaser_drain(sox_effect_t * effp, sox_ssample_t *obuf, sox_size_t *osamp)
{
        phaser_t phaser = (phaser_t) effp->priv;
        sox_size_t done;
        
        double d_in, d_out;
        sox_ssample_t out;

        done = 0;
        while ( ( done < *osamp ) && ( done < phaser->fade_out ) ) {
                d_in = 0;
                d_out = 0;
                /* Compute output first */
                d_in += phaser->phaserbuf[(phaser->maxsamples + 
        phaser->counter - phaser->lookup_tab[phaser->phase]) % 
        phaser->maxsamples] * phaser->decay * -1.0;
                /* Adjust the output volume and size to 24 bit */
                d_out = d_in * phaser->out_gain;
                out = SOX_24BIT_CLIP_COUNT((sox_ssample_t) d_out, effp->clips);
                *obuf++ = out * 256;
                /* Mix decay of delay and input */
                phaser->phaserbuf[phaser->counter] = d_in;
                phaser->counter = 
                        ( phaser->counter + 1 ) % phaser->maxsamples;
                phaser->phase  = ( phaser->phase + 1 ) % phaser->length;
                done++;
                phaser->fade_out--;
        }
        /* samples played, it remains */
        *osamp = done;
        if (phaser->fade_out == 0)
            return SOX_EOF;
        else
            return SOX_SUCCESS;
}

/*
 * Clean up phaser effect.
 */
static int sox_phaser_stop(sox_effect_t * effp)
{
        phaser_t phaser = (phaser_t) effp->priv;

        free(phaser->phaserbuf);
        free(phaser->lookup_tab);
        return (SOX_SUCCESS);
}

static sox_effect_handler_t sox_phaser_effect = {
  "phaser",
  "Usage: phaser gain-in gain-out delay decay speed [ -s | -t ]",
  SOX_EFF_LENGTH,
  sox_phaser_getopts,
  sox_phaser_start,
  sox_phaser_flow,
  sox_phaser_drain,
  sox_phaser_stop,
  NULL
};

const sox_effect_handler_t *sox_phaser_effect_fn(void)
{
    return &sox_phaser_effect;
}
