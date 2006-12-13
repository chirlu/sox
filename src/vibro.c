/*
 * Sound Tools Vibro effect file.
 *
 * Modeled on world-famous Fender(TM) Amp Vibro knobs.
 * 
 * Algorithm: generate a sine wave ranging from
 * 0 + depth to 1.0, where signal goes from -1.0 to 1.0.
 * Multiply signal with sine wave.  I think.
 *
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 *
 * April 28, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *
 *  Rearranged some functions so that they are declared before they are
 *  used.  Clears up some compiler warnings.  Because this functions passed
 *  foats, it helped out some dump compilers pass stuff on the stack
 *  correctly.
 *
 */


#include <math.h>
#include <stdlib.h>
#include "st_i.h"

static st_effect_t st_vibro_effect;

/* Private data for Vibro effect */
typedef struct vibrostuff {
        float           speed;
        float           depth;
        short           *sinetab;               /* sine wave to apply */
        int             mult;                   /* multiplier */
        unsigned        length;                 /* length of table */
        int             counter;                /* current counter */
} *vibro_t;

/*
 * Process options
 */
static int st_vibro_getopts(eff_t effp, int n, char **argv) 
{
        vibro_t vibro = (vibro_t) effp->priv;

        vibro->depth = 0.5;
        if ((n == 0) || !sscanf(argv[0], "%f", &vibro->speed) ||
                ((n == 2) && !sscanf(argv[1], "%f", &vibro->depth)))
        {
                st_fail(st_vibro_effect.usage);
                return (ST_EOF);
        }
        if ((vibro->speed <= 0.001) || (vibro->speed > 30.0) || 
                        (vibro->depth < 0.0) || (vibro->depth > 1.0))
        {
                st_fail("Vibro: speed must be < 30.0, 0.0 < depth < 1.0");
                return (ST_EOF);
        }
        return (ST_SUCCESS);
}

/*
 * Prepare processing.
 */
static int st_vibro_start(eff_t effp)
{
        vibro_t vibro = (vibro_t) effp->priv;

        vibro->length = effp->ininfo.rate / vibro->speed;
        vibro->sinetab = (short*) xmalloc(vibro->length * sizeof(short));

        st_generate_wave_table(ST_WAVE_SINE, ST_SHORT,
            vibro->sinetab, vibro->length, (1 - vibro->depth) * 256, 256, 0);
        vibro->counter = 0;
        return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

static int st_vibro_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                  st_size_t *isamp, st_size_t *osamp)
{
        vibro_t vibro = (vibro_t) effp->priv;
        register int counter, tablen;
        int len, done;
        short *sinetab;
        st_sample_t l;

        len = ((*isamp > *osamp) ? *osamp : *isamp);

        sinetab = vibro->sinetab;
        counter = vibro->counter;
        tablen = vibro->length;
        for(done = 0; done < len; done++) {
                l = *ibuf++;
                /* 24x8 gives 32-bit result */
                *obuf++ = ((l / 256) * sinetab[counter++ % tablen]);
        }
        vibro->counter = counter;
        /* processed all samples */
        *isamp = *osamp = len;
        return (ST_SUCCESS);
}

static st_effect_t st_vibro_effect = {
  "vibro",
  "Usage: vibro speed [ depth ]",
  0,
  st_vibro_getopts,
  st_vibro_start,
  st_vibro_flow,
  st_effect_nothing_drain,
  st_effect_nothing
};

const st_effect_t *st_vibro_effect_fn(void)
{
    return &st_vibro_effect;
}
