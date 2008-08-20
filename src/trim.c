/* July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"
#include <string.h>

typedef struct {
    /* options here */
    char *start_str;
    char *length_str;

    /* options converted to values */
    size_t start;
    size_t length;

    /* internal stuff */
    size_t index;
    size_t trimmed;
} priv_t;

/*
 * Process options
 */
static int sox_trim_getopts(sox_effect_t * effp, int n, char **argv)
{
    priv_t * trim = (priv_t *) effp->priv;

    /* Do not know sample rate yet so hold off on completely parsing
     * time related strings.
     */
    switch (n) {
        case 2:
            trim->length_str = lsx_malloc(strlen(argv[1])+1);
            strcpy(trim->length_str,argv[1]);
            /* Do a dummy parse to see if it will fail */
            if (lsx_parsesamples(0., trim->length_str, &trim->length, 't') == NULL)
              return lsx_usage(effp);
        case 1:
            trim->start_str = lsx_malloc(strlen(argv[0])+1);
            strcpy(trim->start_str,argv[0]);
            /* Do a dummy parse to see if it will fail */
            if (lsx_parsesamples(0., trim->start_str, &trim->start, 't') == NULL)
              return lsx_usage(effp);
            break;
        default:
            return lsx_usage(effp);

    }
    return (SOX_SUCCESS);
}

/*
 * Start processing
 */
static int sox_trim_start(sox_effect_t * effp)
{
    priv_t * trim = (priv_t *) effp->priv;

    if (lsx_parsesamples(effp->in_signal.rate, trim->start_str,
                        &trim->start, 't') == NULL)
      return lsx_usage(effp);
    /* Account for # of channels */
    trim->start *= effp->in_signal.channels;

    if (trim->length_str)
    {
        if (lsx_parsesamples(effp->in_signal.rate, trim->length_str,
                    &trim->length, 't') == NULL)
          return lsx_usage(effp);
    }
    else
        trim->length = 0;

    /* Account for # of channels */
    trim->length *= effp->in_signal.channels;

    trim->index = 0;
    trim->trimmed = 0;

    return (SOX_SUCCESS);
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */
static int sox_trim_flow(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf,
                 size_t *isamp, size_t *osamp)
{
    int result = SOX_SUCCESS;
    int start_trim = 0;
    int offset = 0;
    int done;

    priv_t * trim = (priv_t *) effp->priv;

    /* Compute the most samples we can process this time */
    done = ((*isamp < *osamp) ? *isamp : *osamp);

    /* Quick check to see if we are trimming off the back side yet.
     * If so then we can skip trimming from the front side.
     */
    if (!trim->trimmed) {
        if ((trim->index+done) <= trim->start) {
            /* If we haven't read more than "start" samples, return that
             * we've read all this buffer without outputing anything
             */
            *osamp = 0;
            *isamp = done;
            trim->index += done;
            return (SOX_SUCCESS);
        } else {
            start_trim = 1;
            /* We've read at least "start" samples.  Now find
             * out where our target data begins and subtract that
             * from the total to be copied this round.
             */
            offset = trim->start - trim->index;
            done -= offset;
        }
    } /* !trimmed */

    if (trim->trimmed || start_trim) {
        if (trim->length && ((trim->trimmed+done) >= trim->length)) {
            /* Since we know the end is in this block, we set done
             * to the desired length less the amount already read.
             */
            done = trim->length - trim->trimmed;
            result = SOX_EOF;
        }

        trim->trimmed += done;
    }
    memcpy(obuf, ibuf+offset, done * sizeof(*obuf));
    *osamp = done;
    *isamp = offset + done;
    trim->index += done;

    return result;
}

static int kill(sox_effect_t * effp)
{
    priv_t * trim = (priv_t *) effp->priv;

    free(trim->start_str);
    free(trim->length_str);

    return (SOX_SUCCESS);
}

size_t sox_trim_get_start(sox_effect_t * effp)
{
    priv_t * trim = (priv_t *)effp->priv;
    return trim->start;
}

void sox_trim_clear_start(sox_effect_t * effp)
{
    priv_t * trim = (priv_t *)effp->priv;
    trim->start = 0;
}

const sox_effect_handler_t *sox_trim_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "trim", "start [length]", SOX_EFF_MCHAN|SOX_EFF_LENGTH,
    sox_trim_getopts, sox_trim_start, sox_trim_flow,
    NULL, NULL, kill, sizeof(priv_t)
  };
  return &handler;
}
