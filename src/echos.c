/*
 * August 24, 1998
 * Copyright (C) 1998 Juergen Mueller And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Juergen Mueller And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Echos effect for dsp.
 * 
 * Flow diagram scheme for n delays ( 1 <= n <= MAX_ECHOS ):
 *
 *                                                    * gain-in  ___
 * ibuff --+--------------------------------------------------->|   |
 *         |                                          * decay 1 |   |
 *         |               +----------------------------------->|   |
 *         |               |                          * decay 2 | + |
 *         |               |             +--------------------->|   |
 *         |               |             |            * decay n |   |
 *         |    _________  |  _________  |     _________   +--->|___|
 *         |   |         | | |         | |    |         |  |      | 
 *         +-->| delay 1 |-+-| delay 2 |-+...-| delay n |--+      | * gain-out
 *             |_________|   |_________|      |_________|         |
 *                                                                +----->obuff
 *
 * Usage: 
 *   echos gain-in gain-out delay-1 decay-1 [delay-2 decay-2 ... delay-n decay-n]
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
typedef struct echosstuff {
        int     counter[MAX_ECHOS];                     
        int     num_delays;
        double  *delay_buf;
        float   in_gain, out_gain;
        float   delay[MAX_ECHOS], decay[MAX_ECHOS];
        st_ssize_t samples[MAX_ECHOS], pointer[MAX_ECHOS];
        st_size_t sumsamples;
} *echos_t;

/* Private data for SKEL file */

/*
 * Process options
 */
int st_echos_getopts(eff_t effp, int n, char **argv) 
{
        echos_t echos = (echos_t) effp->priv;
        int i;

        echos->num_delays = 0;

        if ((n < 4) || (n % 2))
        {
            st_fail("Usage: echos gain-in gain-out delay decay [ delay decay ... ]");
            return (ST_EOF);
        }

        i = 0;
        sscanf(argv[i++], "%f", &echos->in_gain);
        sscanf(argv[i++], "%f", &echos->out_gain);
        while (i < n) {
                /* Linux bug and it's cleaner. */
                sscanf(argv[i++], "%f", &echos->delay[echos->num_delays]);
                sscanf(argv[i++], "%f", &echos->decay[echos->num_delays]);
                echos->num_delays++;
                if ( echos->num_delays > MAX_ECHOS )
                {
                        st_fail("echos: to many delays, use less than %i delays",
                                MAX_ECHOS);
                        return (ST_EOF);
                }
        }
        echos->sumsamples = 0;
        return (ST_SUCCESS);
}

/*
 * Prepare for processing.
 */
int st_echos_start(eff_t effp)
{
        echos_t echos = (echos_t) effp->priv;
        int i;
        float sum_in_volume;
        unsigned long j;

        if ( echos->in_gain < 0.0 )
        {
                st_fail("echos: gain-in must be positive!\n");
                return (ST_EOF);
        }
        if ( echos->in_gain > 1.0 )
        {
                st_fail("echos: gain-in must be less than 1.0!\n");
                return (ST_EOF);
        }
        if ( echos->out_gain < 0.0 )
        {
                st_fail("echos: gain-in must be positive!\n");
                return (ST_EOF);
        }
        for ( i = 0; i < echos->num_delays; i++ ) {
                echos->samples[i] = echos->delay[i] * effp->ininfo.rate / 1000.0;
                if ( echos->samples[i] < 1 )
                {
                    st_fail("echos: delay must be positive!\n");
                    return (ST_EOF);
                }
                if ( echos->samples[i] > DELAY_BUFSIZ )
                {
                        st_fail("echos: delay must be less than %g seconds!\n",
                                DELAY_BUFSIZ / (float) effp->ininfo.rate );
                        return (ST_EOF);
                }
                if ( echos->decay[i] < 0.0 )
                {
                    st_fail("echos: decay must be positive!\n" );
                    return (ST_EOF);
                }
                if ( echos->decay[i] > 1.0 )
                {
                    st_fail("echos: decay must be less than 1.0!\n" );
                    return (ST_EOF);
                }
                echos->counter[i] = 0;
                echos->pointer[i] = echos->sumsamples;
                echos->sumsamples += echos->samples[i];
        }
        if (! (echos->delay_buf = (double *) malloc(sizeof (double) * echos->sumsamples)))
        {
                st_fail("echos: Cannot malloc %d bytes!\n", 
                        sizeof(double) * echos->sumsamples);
                return(ST_EOF);
        }
        for ( j = 0; j < echos->sumsamples; ++j )
                echos->delay_buf[j] = 0.0;
        /* Be nice and check the hint with warning, if... */
        sum_in_volume = 1.0;
        for ( i = 0; i < echos->num_delays; i++ ) 
                sum_in_volume += echos->decay[i];
        if ( sum_in_volume * echos->in_gain > 1.0 / echos->out_gain )
                st_warn("echos: warning >>> gain-out can cause saturation of output <<<");
        return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
int st_echos_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                st_size_t *isamp, st_size_t *osamp)
{
        echos_t echos = (echos_t) effp->priv;
        int len, done;
        int j;
        
        double d_in, d_out;
        st_sample_t out;

        len = ((*isamp > *osamp) ? *osamp : *isamp);
        for(done = 0; done < len; done++) {
                /* Store delays as 24-bit signed longs */
                d_in = (double) *ibuf++ / 256;
                /* Compute output first */
                d_out = d_in * echos->in_gain;
                for ( j = 0; j < echos->num_delays; j++ ) {
                        d_out += echos->delay_buf[echos->counter[j] + echos->pointer[j]] * echos->decay[j];
                }
                /* Adjust the output volume and size to 24 bit */
                d_out = d_out * echos->out_gain;
                out = st_clip24((st_sample_t) d_out);
                *obuf++ = out * 256;
                /* Mix decay of delays and input */
                for ( j = 0; j < echos->num_delays; j++ ) {
                        if ( j == 0 )
                                echos->delay_buf[echos->counter[j] + echos->pointer[j]] = d_in;
                        else
                                echos->delay_buf[echos->counter[j] + echos->pointer[j]] = 
                                   echos->delay_buf[echos->counter[j-1] + echos->pointer[j-1]] + d_in;
                }
                /* Adjust the counters */
                for ( j = 0; j < echos->num_delays; j++ )
                        echos->counter[j] = 
                           ( echos->counter[j] + 1 ) % echos->samples[j];
        }
        /* processed all samples */
        return (ST_SUCCESS);
}

/*
 * Drain out reverb lines. 
 */
int st_echos_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
        echos_t echos = (echos_t) effp->priv;
        double d_in, d_out;
        st_sample_t out;
        int j;
        st_size_t done;

        done = 0;
        /* drain out delay samples */
        while ( ( done < *osamp ) && ( done < echos->sumsamples ) ) {
                d_in = 0;
                d_out = 0;
                for ( j = 0; j < echos->num_delays; j++ ) {
                        d_out += echos->delay_buf[echos->counter[j] + echos->pointer[j]] * echos->decay[j];
                }
                /* Adjust the output volume and size to 24 bit */
                d_out = d_out * echos->out_gain;
                out = st_clip24((st_sample_t) d_out);
                *obuf++ = out * 256;
                /* Mix decay of delays and input */
                for ( j = 0; j < echos->num_delays; j++ ) {
                        if ( j == 0 )
                                echos->delay_buf[echos->counter[j] + echos->pointer[j]] = d_in;
                        else
                                echos->delay_buf[echos->counter[j] + echos->pointer[j]] = 
                                   echos->delay_buf[echos->counter[j-1] + echos->pointer[j-1]];
                }
                /* Adjust the counters */
                for ( j = 0; j < echos->num_delays; j++ )
                        echos->counter[j] = 
                           ( echos->counter[j] + 1 ) % echos->samples[j];
                done++;
                echos->sumsamples--;
        };
        /* samples played, it remains */
        *osamp = done;
        return (ST_SUCCESS);
}

/*
 * Clean up echos effect.
 */
int st_echos_stop(eff_t effp)
{
        echos_t echos = (echos_t) effp->priv;

        free((char *) echos->delay_buf);
        echos->delay_buf = (double *) -1;   /* guaranteed core dump */
        return (ST_SUCCESS);
}

