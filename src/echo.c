/*
 * August 24, 1998
 * Copyright (C) 1998 Juergen Mueller And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Juergen Mueller And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * This is the "echo.c" while the old "echo.c" from version 12 moves to
 * "reverb.c" satisfying the defintions made in the Guitar FX FAQ.
 *
 *
 * Echo effect for dsp.
 * 
 * Flow diagram scheme for n delays ( 1 <= n <= MAX_ECHOS ):
 *
 *        * gain-in                                              ___
 * ibuff -----------+------------------------------------------>|   |
 *                  |       _________                           |   |
 *                  |      |         |                * decay 1 |   |
 *                  +----->| delay 1 |------------------------->|   |
 *                  |      |_________|                          |   |
 *                  |            _________                      | + |
 *                  |           |         |           * decay 2 |   |
 *                  +---------->| delay 2 |-------------------->|   |
 *                  |           |_________|                     |   |
 *                  :                 _________                 |   |
 *                  |                |         |      * decay n |   |
 *                  +--------------->| delay n |--------------->|___|
 *                                   |_________|                  | 
 *                                                                | * gain-out
 *                                                                |
 *                                                                +----->obuff
 *
 * Usage: 
 *   echo gain-in gain-out delay-1 decay-1 [delay-2 decay-2 ... delay-n decay-n]
 *
 * Where:
 *   gain-in, decay-1 ... decay-n :  0.0 ... 1.0      volume
 *   gain-out :  0.0 ...      volume
 *   delay-1 ... delay-n :  > 0.0 msec
 *
 * Note:
 *   when decay is close to 1.0, the samples can begin clipping and the output
 *   can saturate! 
 *
 * Hint:
 *   1 / out-gain > gain-in ( 1 + decay-1 + ... + decay-n )
 *
*/

/*
 * Sound Tools reverb effect file.
 */

#include <stdlib.h> /* Harmless, and prototypes atof() etc. --dgc */
#include <math.h>
#include "st_i.h"

#define DELAY_BUFSIZ ( 50L * ST_MAXRATE )
#define MAX_ECHOS 7     /* 24 bit x ( 1 + MAX_ECHOS ) = */
                        /* 24 bit x 8 = 32 bit !!!      */

/* Private data for SKEL file */
typedef struct echostuff {
        int     counter;                        
        int     num_delays;
        double  *delay_buf;
        float   in_gain, out_gain;
        float   delay[MAX_ECHOS], decay[MAX_ECHOS];
        st_ssize_t samples[MAX_ECHOS], maxsamples;
        st_size_t fade_out;
} *echo_t;

/* Private data for SKEL file */


/*
 * Process options
 */
int st_echo_getopts(eff_t effp, int n, char **argv) 
{
        echo_t echo = (echo_t) effp->priv;
        int i;

        echo->num_delays = 0;

        if ((n < 4) || (n % 2))
        {
            st_fail("Usage: echo gain-in gain-out delay decay [ delay decay ... ]");
            return (ST_EOF);
        }

        i = 0;
        sscanf(argv[i++], "%f", &echo->in_gain);
        sscanf(argv[i++], "%f", &echo->out_gain);
        while (i < n) {
                if ( echo->num_delays >= MAX_ECHOS )
                        st_fail("echo: to many delays, use less than %i delays",
                                MAX_ECHOS);
                /* Linux bug and it's cleaner. */
                sscanf(argv[i++], "%f", &echo->delay[echo->num_delays]);
                sscanf(argv[i++], "%f", &echo->decay[echo->num_delays]);
                echo->num_delays++;
        }
        return (ST_SUCCESS);
}

/*
 * Prepare for processing.
 */
int st_echo_start(eff_t effp)
{
        echo_t echo = (echo_t) effp->priv;
        int i;
        float sum_in_volume;
        long j;

        echo->maxsamples = 0L;
        if ( echo->in_gain < 0.0 )
        {
                st_fail("echo: gain-in must be positive!\n");
                return (ST_EOF);
        }
        if ( echo->in_gain > 1.0 )
        {
                st_fail("echo: gain-in must be less than 1.0!\n");
                return (ST_EOF);
        }
        if ( echo->out_gain < 0.0 )
        {
                st_fail("echo: gain-in must be positive!\n");
                return (ST_EOF);
        }
        for ( i = 0; i < echo->num_delays; i++ ) {
                echo->samples[i] = echo->delay[i] * effp->ininfo.rate / 1000.0;
                if ( echo->samples[i] < 1 )
                {
                    st_fail("echo: delay must be positive!\n");
                    return (ST_EOF);
                }
                if ( echo->samples[i] > DELAY_BUFSIZ )
                {
                        st_fail("echo: delay must be less than %g seconds!\n",
                                DELAY_BUFSIZ / (float) effp->ininfo.rate );
                        return (ST_EOF);
                }
                if ( echo->decay[i] < 0.0 )
                {
                    st_fail("echo: decay must be positive!\n" );
                    return (ST_EOF);
                }
                if ( echo->decay[i] > 1.0 )
                {
                    st_fail("echo: decay must be less than 1.0!\n" );
                    return (ST_EOF);
                }
                if ( echo->samples[i] > echo->maxsamples )
                        echo->maxsamples = echo->samples[i];
        }
        if (! (echo->delay_buf = (double *) malloc(sizeof (double) * echo->maxsamples)))
        {
                st_fail("echo: Cannot malloc %d bytes!\n", 
                        sizeof(long) * echo->maxsamples);
                return (ST_EOF);
        }
        for ( j = 0; j < echo->maxsamples; ++j )
                echo->delay_buf[j] = 0.0;
        /* Be nice and check the hint with warning, if... */
        sum_in_volume = 1.0;
        for ( i = 0; i < echo->num_delays; i++ ) 
                sum_in_volume += echo->decay[i];
        if ( sum_in_volume * echo->in_gain > 1.0 / echo->out_gain )
                st_warn("echo: warning >>> gain-out can cause saturation of output <<<");
        echo->counter = 0;
        echo->fade_out = echo->maxsamples;
        return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
int st_echo_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                 st_size_t *isamp, st_size_t *osamp)
{
        echo_t echo = (echo_t) effp->priv;
        int len, done;
        int j;
        
        double d_in, d_out;
        st_sample_t out;

        len = ((*isamp > *osamp) ? *osamp : *isamp);
        for(done = 0; done < len; done++) {
                /* Store delays as 24-bit signed longs */
                d_in = (double) *ibuf++ / 256;
                /* Compute output first */
                d_out = d_in * echo->in_gain;
                for ( j = 0; j < echo->num_delays; j++ ) {
                        d_out += echo->delay_buf[ 
(echo->counter + echo->maxsamples - echo->samples[j]) % echo->maxsamples] 
                        * echo->decay[j];
                }
                /* Adjust the output volume and size to 24 bit */
                d_out = d_out * echo->out_gain;
                out = st_clip24((st_sample_t) d_out);
                *obuf++ = out * 256;
                /* Store input in delay buffer */
                echo->delay_buf[echo->counter] = d_in;
                /* Adjust the counter */
                echo->counter = ( echo->counter + 1 ) % echo->maxsamples;
        }
        /* processed all samples */
        return (ST_SUCCESS);
}

/*
 * Drain out reverb lines. 
 */
int st_echo_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
        echo_t echo = (echo_t) effp->priv;
        double d_in, d_out;
        st_sample_t out;
        int j;
        st_size_t done;

        done = 0;
        /* drain out delay samples */
        while ( ( done < *osamp ) && ( done < echo->fade_out ) ) {
                d_in = 0;
                d_out = 0;
                for ( j = 0; j < echo->num_delays; j++ ) {
                        d_out += echo->delay_buf[ 
(echo->counter + echo->maxsamples - echo->samples[j]) % echo->maxsamples] 
                        * echo->decay[j];
                }
                /* Adjust the output volume and size to 24 bit */
                d_out = d_out * echo->out_gain;
                out = st_clip24((st_sample_t) d_out);
                *obuf++ = out * 256;
                /* Store input in delay buffer */
                echo->delay_buf[echo->counter] = d_in;
                /* Adjust the counters */
                echo->counter = ( echo->counter + 1 ) % echo->maxsamples;
                done++;
                echo->fade_out--;
        };
        /* samples played, it remains */
        *osamp = done;
        return (ST_SUCCESS);
}

/*
 * Clean up reverb effect.
 */
int st_echo_stop(eff_t effp)
{
        echo_t echo = (echo_t) effp->priv;

        free((char *) echo->delay_buf);
        echo->delay_buf = (double *) -1;   /* guaranteed core dump */
        return (ST_SUCCESS);
}
