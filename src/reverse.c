/*
 * June 1, 1992
 * Copyright 1992 Guido van Rossum And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Guido van Rossum And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * "reverse" effect, uses a temporary file created by tmpfile().
 */

#include <math.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>     /* For SEEK_* defines if not found in stdio */
#endif

#include "st_i.h"

/* Private data */
typedef struct reversestuff {
        FILE *fp;
        st_size_t pos;
        int phase;
} *reverse_t;

#define WRITING 0
#define READING 1

/*
 * Process options: none in our case.
 */

int st_reverse_getopts(eff_t effp, int n, char **argv) 
{
        if (n)
        {
                st_fail("Reverse effect takes no options.");
                return (ST_EOF);
        }
        return(ST_SUCCESS);
}

/*
 * Prepare processing: open temporary file.
 */

int st_reverse_start(eff_t effp)
{
        reverse_t reverse = (reverse_t) effp->priv;
        reverse->fp = tmpfile();
        if (reverse->fp == NULL)
        {
                st_fail("Reverse effect can't create temporary file\n");
                return (ST_EOF);
        }
        reverse->phase = WRITING;
        return (ST_SUCCESS);
}

/*
 * Effect flow: a degenerate case: write input samples on temporary file,
 * don't generate any output samples.
 */
int st_reverse_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                    st_size_t *isamp, st_size_t *osamp)
{
        reverse_t reverse = (reverse_t) effp->priv;

        if (reverse->phase != WRITING)
        {
                st_fail("Internal error: reverse_flow called in wrong phase");
                return(ST_EOF);
        }
        if (fwrite((char *)ibuf, sizeof(st_sample_t), *isamp, reverse->fp)
            != *isamp)
        {
                st_fail("Reverse effect write error on temporary file\n");
                return(ST_EOF);
        }
        *osamp = 0;
        return(ST_SUCCESS);
}

/*
 * Effect drain: generate the actual samples in reverse order.
 */

int st_reverse_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
        reverse_t reverse = (reverse_t) effp->priv;
        st_size_t len, nbytes;
        register int i, j;
        st_sample_t temp;

        if (reverse->phase == WRITING) {
                fflush(reverse->fp);
                fseek(reverse->fp, 0L, SEEK_END);
                reverse->pos = ftell(reverse->fp);
                if (reverse->pos % sizeof(st_sample_t) != 0)
                {
                        st_fail("Reverse effect finds odd temporary file\n");
                        return(ST_EOF);
                }
                reverse->phase = READING;
        }
        len = *osamp;
        nbytes = len * sizeof(st_sample_t);
        if (reverse->pos < nbytes) {
                nbytes = reverse->pos;
                len = nbytes / sizeof(st_sample_t);
        }
        reverse->pos -= nbytes;
        fseek(reverse->fp, reverse->pos, SEEK_SET);
        if (fread((char *)obuf, sizeof(st_sample_t), len, reverse->fp) != len)
        {
                st_fail("Reverse effect read error from temporary file\n");
                return(ST_EOF);
        }
        for (i = 0, j = len-1; i < j; i++, j--) {
                temp = obuf[i];
                obuf[i] = obuf[j];
                obuf[j] = temp;
        }
        *osamp = len;
        return(ST_SUCCESS);
}

/*
 * Close and unlink the temporary file.
 */
int st_reverse_stop(eff_t effp)
{
        reverse_t reverse = (reverse_t) effp->priv;

        fclose(reverse->fp);
        return (ST_SUCCESS);
}
