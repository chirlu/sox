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
 * Sound Tools phaser effect file.
 */

#include <stdlib.h> /* Harmless, and prototypes atof() etc. --dgc */
#include <math.h>
#include <string.h>
#include "st_i.h"

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
        st_size_t length;
        int     *lookup_tab;
        st_size_t maxsamples, fade_out;
} *phaser_t;

/*
 * Process options
 */
int st_phaser_getopts(eff_t effp, int n, char **argv) 
{
        phaser_t phaser = (phaser_t) effp->priv;

        if (!((n == 5) || (n == 6)))
        {
            st_fail("Usage: phaser gain-in gain-out delay decay speed [ -s | -t ]");
            return (ST_EOF);
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
                        st_fail("Usage: phaser gain-in gain-out delay decay speed [ -s | -t ]");
                        return (ST_EOF);
                }
        }
        return (ST_SUCCESS);
}

/*
 * Prepare for processing.
 */
int st_phaser_start(eff_t effp)
{
        phaser_t phaser = (phaser_t) effp->priv;
        unsigned int i;

        phaser->maxsamples = phaser->delay * effp->ininfo.rate / 1000.0;

        if ( phaser->delay < 0.0 )
        {
            st_fail("phaser: delay must be positive!\n");
            return (ST_EOF);
        }
        if ( phaser->delay > 5.0 )
        {
            st_fail("phaser: delay must be less than 5.0 msec!\n");
            return (ST_EOF);
        }
        if ( phaser->speed < 0.1 )
        {
            st_fail("phaser: speed must be more than 0.1 Hz!\n");
            return (ST_EOF);
        }
        if ( phaser->speed > 2.0 )
        {
            st_fail("phaser: speed must be less than 2.0 Hz!\n");
            return (ST_EOF);
        }
        if ( phaser->decay < 0.0 )
        {
            st_fail("phaser: decay must be positive!\n" );
            return (ST_EOF);
        }
        if ( phaser->decay >= 1.0 )
        {
            st_fail("phaser: decay must be less that 1.0!\n" );
            return (ST_EOF);
        }
        /* Be nice and check the hint with warning, if... */
        if ( phaser->in_gain > ( 1.0 - phaser->decay * phaser->decay ) )
                st_warn("phaser: warning >>> gain-in can cause saturation or clipping of output <<<");
        if ( phaser->in_gain / ( 1.0 - phaser->decay ) > 1.0 / phaser->out_gain )
                st_warn("phaser: warning >>> gain-out can cause saturation or clipping of output <<<");

        phaser->length = effp->ininfo.rate / phaser->speed;

        if (! (phaser->phaserbuf = 
                (double *) malloc(sizeof (double) * phaser->maxsamples)))
        {
                st_fail("phaser: Cannot malloc %d bytes!\n", 
                        sizeof(double) * phaser->maxsamples);
                return (ST_EOF);
        }
        for ( i = 0; i < phaser->maxsamples; i++ )
                phaser->phaserbuf[i] = 0.0;
        if (! (phaser->lookup_tab = 
                (int *) malloc(sizeof (int) * phaser->length)))
        {
                st_fail("phaser: Cannot malloc %d bytes!\n", 
                        sizeof(int) * phaser->length);
                return (ST_EOF);
        }

        if ( phaser->modulation == MOD_SINE )
                st_sine(phaser->lookup_tab, phaser->length, 
                        phaser->maxsamples - 1,
                        phaser->maxsamples - 1);
        else
                st_triangle(phaser->lookup_tab, phaser->length, 
                        (phaser->maxsamples - 1) * 2,
                        phaser->maxsamples - 1);
        phaser->counter = 0;
        phaser->phase = 0;
        phaser->fade_out = phaser->maxsamples;
        return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
int st_phaser_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                   st_size_t *isamp, st_size_t *osamp)
{
        phaser_t phaser = (phaser_t) effp->priv;
        int len, done;
        
        double d_in, d_out;
        st_sample_t out;

        len = ((*isamp > *osamp) ? *osamp : *isamp);
        for(done = 0; done < len; done++) {
                /* Store delays as 24-bit signed longs */
                d_in = (double) *ibuf++ / 256;
                /* Compute output first */
                d_in = d_in * phaser->in_gain;
                d_in += phaser->phaserbuf[(phaser->maxsamples + 
        phaser->counter - phaser->lookup_tab[phaser->phase]) % 
        phaser->maxsamples] * phaser->decay * -1.0;
                /* Adjust the output volume and size to 24 bit */
                d_out = d_in * phaser->out_gain;
                out = st_clip24((st_sample_t) d_out);
                *obuf++ = out * 256;
                /* Mix decay of delay and input */
                phaser->phaserbuf[phaser->counter] = d_in;
                phaser->counter = 
                        ( phaser->counter + 1 ) % phaser->maxsamples;
                phaser->phase  = ( phaser->phase + 1 ) % phaser->length;
        }
        /* processed all samples */
        return (ST_SUCCESS);
}

/*
 * Drain out reverb lines. 
 */
int st_phaser_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
        phaser_t phaser = (phaser_t) effp->priv;
        st_size_t done;
        
        double d_in, d_out;
        st_sample_t out;

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
                out = st_clip24((st_sample_t) d_out);
                *obuf++ = out * 256;
                /* Mix decay of delay and input */
                phaser->phaserbuf[phaser->counter] = d_in;
                phaser->counter = 
                        ( phaser->counter + 1 ) % phaser->maxsamples;
                phaser->phase  = ( phaser->phase + 1 ) % phaser->length;
                done++;
                phaser->fade_out--;
        }
        /* samples playd, it remains */
        *osamp = done;
        return (ST_SUCCESS);
}

/*
 * Clean up phaser effect.
 */
int st_phaser_stop(eff_t effp)
{
        phaser_t phaser = (phaser_t) effp->priv;

        free((char *) phaser->phaserbuf);
        phaser->phaserbuf = (double *) -1;   /* guaranteed core dump */
        free((char *) phaser->lookup_tab);
        phaser->lookup_tab = (int *) -1;   /* guaranteed core dump */
        return (ST_SUCCESS);
}

