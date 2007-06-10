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

#include "sox_i.h"

#include <math.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>     /* For SEEK_* defines if not found in stdio */
#endif

/* Private data */
typedef struct reversestuff {
        FILE *fp;
        off_t pos;
        int phase;
} *reverse_t;

#define WRITING 0
#define READING 1

/*
 * Prepare processing: open temporary file.
 */

static int sox_reverse_start(sox_effect_t * effp)
{
        reverse_t reverse = (reverse_t) effp->priv;
        reverse->fp = tmpfile();
        if (reverse->fp == NULL)
        {
                sox_fail("Reverse effect can't create temporary file");
                return (SOX_EOF);
        }
        reverse->phase = WRITING;
        return (SOX_SUCCESS);
}

/*
 * Effect flow: a degenerate case: write input samples on temporary file,
 * don't generate any output samples.
 */
static int sox_reverse_flow(sox_effect_t * effp, const sox_ssample_t *ibuf, sox_ssample_t *obuf UNUSED, 
                    sox_size_t *isamp, sox_size_t *osamp)
{
        reverse_t reverse = (reverse_t) effp->priv;

        if (reverse->phase != WRITING)
        {
                sox_fail("Internal error: reverse_flow called in wrong phase");
                return(SOX_EOF);
        }
        if (fwrite((char *)ibuf, sizeof(sox_ssample_t), *isamp, reverse->fp)
            != *isamp)
        {
                sox_fail("Reverse effect write error on temporary file");
                return(SOX_EOF);
        }
        *osamp = 0;
        return(SOX_SUCCESS);
}

/*
 * Effect drain: generate the actual samples in reverse order.
 */

static int sox_reverse_drain(sox_effect_t * effp, sox_ssample_t *obuf, sox_size_t *osamp)
{
        reverse_t reverse = (reverse_t) effp->priv;
        sox_size_t len, nbytes;
        register int i, j;
        sox_ssample_t temp;

        if (reverse->phase == WRITING) {
                fflush(reverse->fp);
                fseeko(reverse->fp, (off_t)0, SEEK_END);
                reverse->pos = ftello(reverse->fp);
                if (reverse->pos % sizeof(sox_ssample_t) != 0)
                {
                        sox_fail("Reverse effect finds odd temporary file");
                        return(SOX_EOF);
                }
                reverse->phase = READING;
        }
        len = *osamp;
        nbytes = len * sizeof(sox_ssample_t);
        if (reverse->pos < nbytes) {
                nbytes = reverse->pos;
                len = nbytes / sizeof(sox_ssample_t);
        }
        reverse->pos -= nbytes;
        fseeko(reverse->fp, reverse->pos, SEEK_SET);
        if (fread((char *)obuf, sizeof(sox_ssample_t), len, reverse->fp) != len)
        {
                sox_fail("Reverse effect read error from temporary file");
                return(SOX_EOF);
        }
        for (i = 0, j = len-1; i < j; i++, j--) {
                temp = obuf[i];
                obuf[i] = obuf[j];
                obuf[j] = temp;
        }
        *osamp = len;
        if (reverse->pos == 0)
            return SOX_EOF;
        else
            return SOX_SUCCESS;
}

/*
 * Close and unlink the temporary file.
 */
static int sox_reverse_stop(sox_effect_t * effp)
{
        reverse_t reverse = (reverse_t) effp->priv;

        fclose(reverse->fp);
        return (SOX_SUCCESS);
}

static sox_effect_handler_t sox_reverse_effect = {
  "reverse",
  NULL,
  0,
  NULL,
  sox_reverse_start,
  sox_reverse_flow,
  sox_reverse_drain,
  sox_reverse_stop,
  NULL
};

const sox_effect_handler_t *sox_reverse_effect_fn(void)
{
    return &sox_reverse_effect;
}
