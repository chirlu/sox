/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools skeleton effect file.
 */

#include "st_i.h"
#include <string.h>

/* Time resolutin one millisecond */
#define TIMERES 1000

#define TRIM_USAGE "Trim usage: trim start [length]"

typedef struct
{
    /* options here */
    char *start_str;
    char *length_str;

    /* options converted to values */
    st_size_t start;
    st_size_t length;

    /* internal stuff */
    st_size_t index;
    st_size_t trimmed;
} * trim_t;

/*
 * Process options
 */
int st_trim_getopts(eff_t effp, int n, char **argv) 
{
    trim_t trim = (trim_t) effp->priv;

    /* Do not know sample rate yet so hold off on completely parsing
     * time related strings.
     */
    switch (n) {
        case 2:
            trim->length_str = (char *)malloc(strlen(argv[1])+1);
            if (!trim->length_str)
            {
                st_fail("Could not allocate memory");
                return(ST_EOF);
            }
            strcpy(trim->length_str,argv[1]);
            /* Do a dummy parse to see if it will fail */
            if (st_parsesamples(0, trim->length_str,
                                &trim->length, 't') != ST_SUCCESS)
            {
                st_fail(TRIM_USAGE);
                return(ST_EOF);
            }
        case 1:
            trim->start_str = (char *)malloc(strlen(argv[0])+1);
            if (!trim->start_str)
            {
                st_fail("Could not allocate memory");
                return(ST_EOF);
            }
            strcpy(trim->start_str,argv[0]);
            /* Do a dummy parse to see if it will fail */
            if (st_parsesamples(0, trim->start_str,
                                &trim->start, 't') != ST_SUCCESS)
            {
                st_fail(TRIM_USAGE);
                return(ST_EOF);
            }
            break;
        default:
            st_fail(TRIM_USAGE);
            return ST_EOF;
            break;

    }
    return (ST_SUCCESS);
}

/*
 * Start processing
 */
int st_trim_start(eff_t effp)
{
    trim_t trim = (trim_t) effp->priv;

    if (st_parsesamples(effp->ininfo.rate, trim->start_str,
                        &trim->start, 't') != ST_SUCCESS)
    {
        st_fail(TRIM_USAGE);
        return(ST_EOF);
    }
    /* Account for # of channels */
    trim->start *= effp->ininfo.channels;

    if (trim->length_str)
    {
        if (st_parsesamples(effp->ininfo.rate, trim->length_str,
                    &trim->length, 't') != ST_SUCCESS)
        {
            st_fail(TRIM_USAGE);
            return(ST_EOF);
        }
    }
    else
        trim->length = 0;

    /* Account for # of channels */
    trim->length *= effp->ininfo.channels;

    trim->index = 0;
    trim->trimmed = 0;

    return (ST_SUCCESS);
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */
int st_trim_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                 st_size_t *isamp, st_size_t *osamp)
{
    int finished = 0;
    int start_trim = 0;
    int offset = 0;
    int done;

    trim_t trim = (trim_t) effp->priv;

    /* Compute the most samples we can process this time */
    done = ((*isamp < *osamp) ? *isamp : *osamp);

    /* Quick check to see if we are trimming off the back side yet.
     * If so then we can skip trimming from the front side.
     */
    if (!trim->trimmed) {
        if ((trim->index+done) <= trim->start) {
            /* If we haven't read more then "start" samples, return that
             * we've read all this buffer without outputing anything
             */
            *osamp = 0;
            *isamp = done;
            trim->index += done;
            return (ST_SUCCESS);
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
            finished = 1;
        }

        trim->trimmed += done;
    }

    memcpy(obuf, ibuf+offset, done * sizeof(st_sample_t));

    *osamp = done;
    *isamp = offset + done;
    trim->index += done;

    /* return ST_EOF when nothing consumed and we detect
     * we are finished.
     */
    if (finished && !done)
        return (ST_EOF);
    else
        return (ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_trim_stop(eff_t effp)
{
    trim_t trim = (trim_t) effp->priv;

    if (trim->start_str)
        free(trim->start_str);
    if (trim->length_str)
        free(trim->length_str);

    return (ST_SUCCESS);
}

st_size_t st_trim_get_start(eff_t effp)
{
    trim_t trim = (trim_t)effp->priv;
    return trim->start;
}

void st_trim_clear_start(eff_t effp)
{
    trim_t trim = (trim_t)effp->priv;
    trim->start = 0;
}
