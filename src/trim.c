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
    char *end_str;
    sox_bool end_is_absolute;

    /* options converted to values */
    uint64_t start;
    uint64_t length;

    /* internal stuff */
    uint64_t index;
    uint64_t trimmed;
} priv_t;

/*
 * Process options
 */
static int sox_trim_getopts(sox_effect_t * effp, int argc, char **argv)
{
    char *end;
    priv_t * trim = (priv_t *) effp->priv;
    uint64_t samples;
    const char *n;
  --argc, ++argv;

    /* Do not know sample rate yet so hold off on completely parsing
     * time related strings.
     */
    switch (argc) {
        case 2:
            end = argv[1];
            if (*end == '=') {
                trim->end_is_absolute = sox_true;
                end++;
            } else trim->end_is_absolute = sox_false;
            trim->end_str = lsx_strdup(end);
            /* Do a dummy parse to see if it will fail */
            n = lsx_parsesamples(0., trim->end_str, &samples, 't');
            if (!n || *n)
              return lsx_usage(effp);
            trim->length = samples;
        case 1:
            trim->start_str = lsx_strdup(argv[0]);
            /* Do a dummy parse to see if it will fail */
            n = lsx_parsesamples(0., trim->start_str, &samples, 't');
            if (!n || *n)
              return lsx_usage(effp);
            trim->start = samples;
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
    uint64_t samples;

    if (lsx_parsesamples(effp->in_signal.rate, trim->start_str,
                        &samples, 't') == NULL)
      return lsx_usage(effp);
    trim->start = samples;

    if (trim->end_str)
    {
        if (lsx_parsesamples(effp->in_signal.rate, trim->end_str,
                    &samples, 't') == NULL)
          return lsx_usage(effp);
        trim->length = samples;
        if (trim->end_is_absolute) {
            if (trim->length < trim->start) {
                lsx_warn("end earlier than start");
                trim->length = 0;
                  /* with trim->end_str != NULL, this really means zero */
            } else
                trim->length -= trim->start;
        }
    }
    else
        trim->length = 0;
          /* with trim->end_str == NULL, this means indefinite length */

    lsx_debug("start at %" PRIu64 ", length %" PRIu64, trim->start, trim->length);

    /* Account for # of channels */
    trim->start *= effp->in_signal.channels;
    trim->length *= effp->in_signal.channels;

    trim->index = 0;
    trim->trimmed = 0;

    if (effp->in_signal.length) {
      if (trim->start >= effp->in_signal.length) {
        lsx_fail("start position after end of file");
        return SOX_EOF;
      } else if (trim->start + trim->length >= effp->in_signal.length) {
        lsx_fail("end position after end of file");
        return SOX_EOF;
      }
    }

    effp->out_signal.length = trim->length;
    if (!effp->out_signal.length && effp->in_signal.length > trim->start)
        effp->out_signal.length = effp->in_signal.length - trim->start;

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
        if (trim->end_str && ((trim->trimmed+done) >= trim->length)) {
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

static int lsx_kill(sox_effect_t * effp)
{
    priv_t * trim = (priv_t *) effp->priv;

    free(trim->start_str);
    free(trim->end_str);

    return (SOX_SUCCESS);
}

sox_uint64_t sox_trim_get_start(sox_effect_t * effp)
{
    priv_t * trim = (priv_t *)effp->priv;
    return trim->start;
}

void sox_trim_clear_start(sox_effect_t * effp)
{
    priv_t * trim = (priv_t *)effp->priv;
    trim->start = 0;
}

const sox_effect_handler_t *lsx_trim_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "trim", "start [length|=end]", SOX_EFF_MCHAN | SOX_EFF_LENGTH | SOX_EFF_MODIFY,
    sox_trim_getopts, sox_trim_start, sox_trim_flow,
    NULL, NULL, lsx_kill, sizeof(priv_t)
  };
  return &handler;
}
