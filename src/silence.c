/* Silence effect for SoX
 * by Heikki Leinonen (heilei@iki.fi) 25.03.2001
 * Major Modifications by Chris Bagwell 06.08.2001
 * Minor addition by Donnie Smith 13.08.2003
 *
 * This effect can delete samples from the start of a sound file
 * until it sees a specified count of samples exceed a given threshold 
 * (any of the channels).
 * This effect can also delete samples from the end of a sound file
 * when it sees a specified count of samples below a given threshold
 * (all channels).
 * It may also be used to delete samples anywhere in a sound file.
 * Theshold's can be given as either a percentage or in decibels.
 */


#include <string.h>
#include <math.h>
#include "st_i.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef min
#define min(s1,s2) ((s1)<(s2)?(s1):(s2))
#endif

/* Private data for silence effect. */

#define SILENCE_TRIM        0
#define SILENCE_TRIM_FLUSH  1
#define SILENCE_COPY        2
#define SILENCE_COPY_FLUSH  3
#define SILENCE_STOP        4

typedef struct silencestuff
{
    char        start;
    int         start_periods;
    char        *start_duration_str;
    st_size_t   start_duration;
    double      start_threshold;
    char        start_unit; /* "d" for decibels or "%" for percent. */
    int         restart;

    st_sample_t *start_holdoff;
    st_size_t   start_holdoff_offset;
    st_size_t   start_holdoff_end;
    int         start_found_periods;

    char        stop;
    int         stop_periods;
    char        *stop_duration_str;
    st_size_t   stop_duration;
    double      stop_threshold;
    char        stop_unit;

    st_sample_t *stop_holdoff;
    st_size_t   stop_holdoff_offset;
    st_size_t   stop_holdoff_end;
    int         stop_found_periods;

    double      *window;
    double      *window_current;
    double      *window_end;
    st_size_t   window_size;
    double      rms_sum;

    /* State Machine */
    char        mode;
} *silence_t;

#define SILENCE_USAGE "Usage: silence above_periods [ duration thershold[d | %% ] ] [ below_periods duration threshold[ d | %% ]]"

int st_silence_getopts(eff_t effp, int n, char **argv)
{
    silence_t   silence = (silence_t) effp->priv;
    int parse_count;

    if (n < 1)
    {
        st_fail(SILENCE_USAGE);
        return (ST_EOF);
    }

    /* Parse data related to trimming front side */
    silence->start = FALSE;
    if (sscanf(argv[0], "%d", &silence->start_periods) != 1)
    {
        st_fail(SILENCE_USAGE);
        return(ST_EOF);
    }
    if (silence->start_periods < 0)
    {
        st_fail("Periods must not be negative");
        return(ST_EOF);
    }
    argv++;
    n--;

    if (silence->start_periods > 0)
    {
        silence->start = TRUE;
        if (n < 2)
        {
            st_fail(SILENCE_USAGE);
            return ST_EOF;
        }

        /* We do not know the sample rate so we can not fully
         * parse the duration info yet.  So save argument off
         * for future processing.
         */
        silence->start_duration_str = (char *)malloc(strlen(argv[0])+1);
        if (!silence->start_duration_str)
        {
            st_fail("Could not allocate memory");
            return(ST_EOF);
        }
        strcpy(silence->start_duration_str,argv[0]);
        /* Perform a fake parse to do error checking */
        if (st_parsesamples(0,silence->start_duration_str,
                    &silence->start_duration,'s') !=
                ST_SUCCESS)
        {
            st_fail(SILENCE_USAGE);
            return(ST_EOF);
        }

        parse_count = sscanf(argv[1], "%lf%c", &silence->start_threshold, 
                &silence->start_unit);
        if (parse_count < 1)
        {
            st_fail(SILENCE_USAGE);
            return ST_EOF;
        }
        else if (parse_count < 2)
            silence->start_unit = '%';

        argv++; argv++;
        n--; n--;
    }

    silence->stop = FALSE;
    /* Parse data needed for trimming of backside */
    if (n > 0)
    {
        if (n < 3)
        {
            st_fail(SILENCE_USAGE);
            return ST_EOF;
        }
        if (sscanf(argv[0], "%d", &silence->stop_periods) != 1)
        {
            st_fail(SILENCE_USAGE);
            return ST_EOF;
        }
        if (silence->stop_periods < 0)
        {
            silence->stop_periods = -silence->stop_periods;
            silence->restart = 1;
        }
        else
            silence->restart = 0;
        silence->stop = TRUE;
        argv++;
        n--;

        /* We do not know the sample rate so we can not fully
         * parse the duration info yet.  So save argument off
         * for future processing.
         */
        silence->stop_duration_str = (char *)malloc(strlen(argv[0])+1);
        if (!silence->stop_duration_str)
        {
            st_fail("Could not allocate memory");
            return(ST_EOF);
        }
        strcpy(silence->stop_duration_str,argv[0]);
        /* Perform a fake parse to do error checking */
        if (st_parsesamples(0,silence->stop_duration_str,
                    &silence->stop_duration,'s') !=
                ST_SUCCESS)
        {
            st_fail(SILENCE_USAGE);
            return(ST_EOF);
        }

        parse_count = sscanf(argv[1], "%lf%c", &silence->stop_threshold, 
                             &silence->stop_unit);
        if (parse_count < 1)
        {
            st_fail(SILENCE_USAGE);
            return ST_EOF;
        }
        else if (parse_count < 2)
            silence->stop_unit = '%';

        argv++; argv++;
        n--; n--;
    }

    /* Error checking */
    if (silence->start)
    {
        if ((silence->start_unit != '%') && (silence->start_unit != 'd'))
        {
            st_fail("Invalid unit specified");
            st_fail(SILENCE_USAGE);
            return(ST_EOF);
        }
        if ((silence->start_unit == '%') && ((silence->start_threshold < 0.0)
            || (silence->start_threshold > 100.0)))
        {
            st_fail("silence threshold should be between 0.0 and 100.0 %%");
            return (ST_EOF);
        }
        if ((silence->start_unit == 'd') && (silence->start_threshold >= 0.0))
        {
            st_fail("silence threshold should be less than 0.0 dB");
            return(ST_EOF);
        }
    }

    if (silence->stop)
    {
        if ((silence->stop_unit != '%') && (silence->stop_unit != 'd'))
        {
            st_fail("Invalid unit specified");
            return(ST_EOF);
        }
        if ((silence->stop_unit == '%') && ((silence->stop_threshold < 0.0) || 
                    (silence->stop_threshold > 100.0)))
        {
            st_fail("silence threshold should be between 0.0 and 100.0 %%");
            return (ST_EOF);
        }
        if ((silence->stop_unit == 'd') && (silence->stop_threshold >= 0.0))
        {
            st_fail("silence threshold should be less than 0.0 dB");
            return(ST_EOF);
        }
    }
    return(ST_SUCCESS);
}

int st_silence_start(eff_t effp)
{
        silence_t       silence = (silence_t) effp->priv;

        silence->window_size = (effp->ininfo.rate / 10) * effp->ininfo.channels;
        silence->window = (double *)malloc(silence->window_size *
                                           sizeof(double));

        if (!silence->window)
        {
            st_fail("Unable to allocate memory");
            return(ST_EOF);
        }

        memset(silence->window, 0, 
               silence->window_size * sizeof(double));

        silence->window_current = silence->window;
        silence->window_end = silence->window + silence->window_size;
        silence->rms_sum = 0;

        /* Now that we now sample rate, reparse duration. */
        if (silence->start)
        {
            if (st_parsesamples(effp->ininfo.rate, silence->start_duration_str,
                                &silence->start_duration, 's') !=
                    ST_SUCCESS)
            {
                st_fail(SILENCE_USAGE);
                return(ST_EOF);
            }
        }
        if (silence->stop)
        {
            if (st_parsesamples(effp->ininfo.rate,silence->stop_duration_str,
                                &silence->stop_duration,'s') !=
                    ST_SUCCESS)
            {
                st_fail(SILENCE_USAGE);
                return(ST_EOF);
            }
        }

        if (silence->start)
            silence->mode = SILENCE_TRIM;
        else
            silence->mode = SILENCE_COPY;

        silence->start_holdoff = (st_sample_t *)malloc(sizeof(st_sample_t)*silence->start_duration);
        if (!silence->start_holdoff)
        {
            st_fail("Could not allocate memory");
            return(ST_EOF);
        }
        silence->start_holdoff_offset = 0;
        silence->start_holdoff_end = 0;
        silence->start_found_periods = 0;

        silence->stop_holdoff = (st_sample_t *)malloc(sizeof(st_sample_t)*silence->stop_duration);
        if (!silence->stop_holdoff)
        {
            st_fail("Could not allocate memory");
            return(ST_EOF);
        }
        silence->stop_holdoff_offset = 0;
        silence->stop_holdoff_end = 0;
        silence->stop_found_periods = 0;

        return(ST_SUCCESS);
}

int aboveThreshold(eff_t effp, st_sample_t value, double threshold, char unit)
{
    double ratio;
    int rc;

    /* When scaling low bit data, noise values got scaled way up */
    /* Only consider the original bits when looking for silence */
    switch(effp->ininfo.size)
    {
        case ST_SIZE_BYTE:
            value = ST_SAMPLE_TO_SIGNED_BYTE(value);
            ratio = (double)abs(value) / (double)ST_INT8_MAX;
            break;
        case ST_SIZE_WORD:
            value = ST_SAMPLE_TO_SIGNED_WORD(value);
            ratio = (double)abs(value) / (double)ST_INT16_MAX;
            break;
        case ST_SIZE_DWORD:
            value = ST_SAMPLE_TO_SIGNED_DWORD(value);
            ratio = (double)labs(value) / (double)ST_INT32_MAX;
            break;
        default:
            ratio = 0;
    }

    if (unit == '%')
        ratio *= 100.0;
    else if (unit == 'd')
        ratio = log10(ratio) * 20.0;
    rc = (ratio >= threshold);

    return rc;
}

st_sample_t compute_rms(eff_t effp, st_sample_t sample)
{
    silence_t silence = (silence_t) effp->priv;
    double new_sum;
    st_sample_t rms;

    new_sum = silence->rms_sum;
    new_sum -= *silence->window_current;
    new_sum += ((double)sample * (double)sample);

    rms = sqrt(new_sum / silence->window_size);

    return (rms);
}

void update_rms(eff_t effp, st_sample_t sample)
{
    silence_t silence = (silence_t) effp->priv;

    silence->rms_sum -= *silence->window_current;
    *silence->window_current = ((double)sample * (double)sample);
    silence->rms_sum += *silence->window_current;

    silence->window_current++;
    if (silence->window_current >= silence->window_end)
        silence->window_current = silence->window;
}

/* Process signed long samples from ibuf to obuf. */
/* Return number of samples processed in isamp and osamp. */
int st_silence_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                    st_size_t *isamp, st_size_t *osamp)
{
    silence_t silence = (silence_t) effp->priv;
    int threshold, j;
    st_size_t i;
    st_size_t nrOfTicks, nrOfInSamplesRead, nrOfOutSamplesWritten;

    nrOfInSamplesRead = 0;
    nrOfOutSamplesWritten = 0;

    switch (silence->mode)
    {
        case SILENCE_TRIM:
            /* Reads and discards all input data until it detects a
             * sample that is above the specified threshold.  Turns on
             * copy mode when detected.
             * Need to make sure and copy input in groups of "channels" to
             * prevent getting buffers out of sync.
             */
            nrOfTicks = min((*isamp), (*osamp)) / effp->ininfo.channels;
            for(i = 0; i < nrOfTicks; i++)
            {
                threshold = 0;
                for (j = 0; j < effp->ininfo.channels; j++)
                {
                    threshold |= aboveThreshold(effp,
                                                compute_rms(effp, ibuf[j]),
                                                silence->start_threshold, 
                                                silence->start_unit);
                }

                if (threshold)
                {
                    /* Add to holdoff buffer */
                    for (j = 0; j < effp->ininfo.channels; j++)
                    {
                        update_rms(effp, *ibuf);
                        silence->start_holdoff[
                            silence->start_holdoff_end++] = *ibuf++;
                        nrOfInSamplesRead++;
                    }

                    if (silence->start_holdoff_end >=
                            silence->start_duration)
                    {
                        if (++silence->start_found_periods >=
                                silence->start_periods)
                        {
                            silence->mode = SILENCE_TRIM_FLUSH;
                            goto silence_trim_flush;
                        }
                        /* Trash holdoff buffer since its not
                         * needed.  Start looking again.
                         */
                        silence->start_holdoff_offset = 0;
                        silence->start_holdoff_end = 0;
                    }
                }
                else /* !above Threshold */
                {
                    silence->start_holdoff_end = 0;
                    for (j = 0; j < effp->ininfo.channels; j++)
                    {
                        update_rms(effp, ibuf[j]);
                    }
                    ibuf += effp->ininfo.channels; 
                    nrOfInSamplesRead += effp->ininfo.channels;
                }
            } /* for nrOfTicks */
            break;

        case SILENCE_TRIM_FLUSH:
silence_trim_flush:
            nrOfTicks = min((silence->start_holdoff_end -
                             silence->start_holdoff_offset), 
                             (*osamp-nrOfOutSamplesWritten)); 
            for(i = 0; i < nrOfTicks; i++)
            {
                *obuf++ = silence->start_holdoff[silence->start_holdoff_offset++];
                nrOfOutSamplesWritten++;
            }

            /* If fully drained holdoff then switch to copy mode */
            if (silence->start_holdoff_offset == silence->start_holdoff_end)
            {
                silence->start_holdoff_offset = 0;
                silence->start_holdoff_end = 0;
                silence->mode = SILENCE_COPY;
                goto silence_copy;
            }
            break;

        case SILENCE_COPY:
            /* Attempts to copy samples into output buffer.  If not
             * looking for silence to terminate copy then blindly
             * copy data into output buffer.
             *
             * If looking for silence, then see if input sample is above
             * threshold.  If found then flush out hold off buffer
             * and copy over to output buffer.  Tell user about
             * input and output processing.
             *
             * If not above threshold then store in hold off buffer
             * and do not write to output buffer.  Tell user input
             * was processed.
             *
             * If hold off buffer is full then stop copying data and
             * discard data in hold off buffer.
             */
silence_copy:
            nrOfTicks = min((*isamp-nrOfInSamplesRead), 
                            (*osamp-nrOfOutSamplesWritten)) / 
                           effp->ininfo.channels;
            if (silence->stop)
            {
                for(i = 0; i < nrOfTicks; i++)
                {
                    threshold = 1;
                    for (j = 0; j < effp->ininfo.channels; j++)
                    {
                        threshold &= aboveThreshold(effp, 
                                                    compute_rms(effp, ibuf[j]),
                                                    silence->stop_threshold, 
                                                    silence->stop_unit);
                    }

                    /* If above threshold, check to see if we where holding
                     * off previously.  If so then flush this buffer.
                     * We haven't incremented any pointers yet so nothing
                     * is lost.
                     */
                    if (threshold && silence->stop_holdoff_end)
                    {
                        silence->mode = SILENCE_COPY_FLUSH;
                        goto silence_copy_flush;
                    }
                    else if (threshold)
                    {
                        /* Not holding off so copy into output buffer */
                        for (j = 0; j < effp->ininfo.channels; j++)
                        {
                            update_rms(effp, *ibuf);
                            *obuf++ = *ibuf++;
                            nrOfInSamplesRead++;
                            nrOfOutSamplesWritten++;
                        }
                    }
                    else if (!threshold)
                    {
                        /* Add to holdoff buffer */
                        for (j = 0; j < effp->ininfo.channels; j++)
                        {
                            update_rms(effp, *ibuf);
                            silence->stop_holdoff[
                                silence->stop_holdoff_end++] = *ibuf++;
                            nrOfInSamplesRead++;
                        }

                        /* Check if holdoff buffer is greater than duration 
                         */
                        if (silence->stop_holdoff_end >= 
                                silence->stop_duration)
                        {
                            /* Increment found counter and see if this
                             * is the last period.  If so then exit.
                             */
                            if (++silence->stop_found_periods >= 
                                    silence->stop_periods)
                            {
                                silence->stop_holdoff_offset = 0;
                                silence->stop_holdoff_end = 0;
                                if (!silence->restart)
                                {
                                    silence->mode = SILENCE_STOP;
                                    *isamp = nrOfInSamplesRead;
                                    *osamp = nrOfOutSamplesWritten;
                                    /* Return ST_EOF since no more processing */
                                    return (ST_EOF);
                                }
                                else
                                {
                                    silence->mode = SILENCE_TRIM;
                                    return (ST_SUCCESS);
                                }
                            }
                            else
                            {
                                /* Flush this buffer and start 
                                 * looking again.
                                 */
                                silence->mode = SILENCE_COPY_FLUSH;
                                goto silence_copy_flush;
                            }
                            break;
                        } /* Filled holdoff buffer */
                    } /* Detected silence */
                } /* For # of samples */
            } /* Trimming off backend */
            else /* !(silence->stop) */
            {
                memcpy(obuf, ibuf, sizeof(st_sample_t)*nrOfTicks*
                                   effp->ininfo.channels);
                nrOfInSamplesRead += (nrOfTicks*effp->ininfo.channels);
                nrOfOutSamplesWritten += (nrOfTicks*effp->ininfo.channels);
            }
            break;

        case SILENCE_COPY_FLUSH:
silence_copy_flush:
            nrOfTicks = min((silence->stop_holdoff_end -
                                silence->stop_holdoff_offset), 
                            (*osamp-nrOfOutSamplesWritten));

            for(i = 0; i < nrOfTicks; i++)
            {
                *obuf++ = silence->stop_holdoff[silence->stop_holdoff_offset++];
                nrOfOutSamplesWritten++;
            }

            /* If fully drained holdoff then return to copy mode */
            if (silence->stop_holdoff_offset == silence->stop_holdoff_end)
            {
                silence->stop_holdoff_offset = 0;
                silence->stop_holdoff_end = 0;
                silence->mode = SILENCE_COPY;
                goto silence_copy;
            }
            break;

        case SILENCE_STOP:
            nrOfInSamplesRead = *isamp;
            break;
        }

        *isamp = nrOfInSamplesRead;
        *osamp = nrOfOutSamplesWritten;

        return (ST_SUCCESS);
}

int st_silence_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
    silence_t silence = (silence_t) effp->priv;
    st_size_t i;
    st_size_t nrOfTicks, nrOfOutSamplesWritten = 0;

    /* Only if in flush mode will there be possible samples to write
     * out during drain() call.
     */
    if (silence->mode == SILENCE_COPY_FLUSH || 
        silence->mode == SILENCE_COPY)
    {
        nrOfTicks = min((silence->stop_holdoff_end - 
                            silence->stop_holdoff_offset), *osamp);
        for(i = 0; i < nrOfTicks; i++)
        {
            *obuf++ = silence->stop_holdoff[silence->stop_holdoff_offset++];
            nrOfOutSamplesWritten++;
        }

        /* If fully drained holdoff then stop */
        if (silence->stop_holdoff_offset == silence->stop_holdoff_end)
        {
            silence->stop_holdoff_offset = 0;
            silence->stop_holdoff_end = 0;
            silence->mode = SILENCE_STOP;
        }
    }

    *osamp = nrOfOutSamplesWritten;
    return(ST_SUCCESS);
}

int st_silence_stop(eff_t effp)
{
    silence_t silence = (silence_t) effp->priv;

    if (silence->window)
        free(silence->window);
    if (silence->start_holdoff)
        free(silence->start_holdoff);
    if (silence->stop_holdoff)
        free(silence->stop_holdoff);
    return(ST_SUCCESS);
}
