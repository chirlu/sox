/*	Silence effect for SoX
 *	by Heikki Leinonen (heilei@iki.fi) 25.03.2001
 *	Major Modifications by Chris Bagwell 06.08.2001
 *
 *	This effect deletes samples from the start of the sound
 *	file until a sample exceeds a given threshold (either
 *	left or right channel in stereo files). This can be used
 *	to filter out unwanted silence or low noise in the beginning
 *	of a sound file. The threshold can be given either as a
 *	percentage or in decibels.
 */


#include <string.h>
#include <math.h>
#include "st.h"

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

#define SILENCE_START  0
#define SILENCE_TRIM   1 
#define SILENCE_COPY   2
#define SILENCE_FLUSH  3
#define SILENCE_STOP   4

typedef struct silencestuff
{
    char	trim;
    double	trim_threshold;
    char	trim_unit; /* "d" for decibels or "%" for percent. */
    char	stop;
    int		stop_count;
    ULONG	stop_duration;
    double	stop_threshold;
    char	stop_unit;
    LONG	*holdoff;
    ULONG	holdoff_count;
    ULONG	holdoff_offset;
    char	mode;
    char	crossings;
} *silence_t;

/*#define SILENCE_USAGE "Usage: silence count duration thershold [d | %% | s] [ -notrim ] [ count duration threshold [ d | %% | s ]]" */

#define SILENCE_USAGE "Usage: silence count duration threshold [d | %%]"

int st_silence_getopts(eff_t effp, int n, char **argv)
{
	silence_t	silence = (silence_t) effp->priv;
	ULONG duration;

	if (n < 3)
	{
	    st_fail(SILENCE_USAGE);
    	    return (ST_EOF);
	}

	silence->trim = FALSE;
	silence->trim_threshold = 1.0;
	silence->trim_unit = '%';

	silence->stop = TRUE;
	if (sscanf(argv[0], "%d", &silence->stop_count) != 1)
	{
	    st_fail(SILENCE_USAGE);
	    return ST_EOF;
	}
	if (sscanf(argv[1], "%ld", &duration) != 1)
	{
	    st_fail(SILENCE_USAGE);
	    return ST_EOF;
	}
	silence->stop_duration = duration;
	if (sscanf(argv[2], "%lf", &silence->stop_threshold) != 1)
	{
	    st_fail(SILENCE_USAGE);
	    return ST_EOF;
	}

	if (n > 3 && strlen(argv[3]) == 1)
	{
	    silence->stop_unit = argv[3][0];
	}

	if (silence->stop_count != 1)
       	{
	    st_warn("Only support a stop count of 1 currently");
	    st_fail(SILENCE_USAGE);
	    return(ST_EOF);
	}

	if ((silence->stop_unit != '%') && (silence->stop_unit != 'd') &&
		(silence->stop_unit != 's'))
	{
		st_fail(SILENCE_USAGE);
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
	return(ST_SUCCESS);
}

int st_silence_start(eff_t effp)
{
	silence_t	silence = (silence_t) effp->priv;

    	silence->mode = SILENCE_START;

	silence->holdoff = malloc(sizeof(LONG)*silence->stop_duration);
	silence->holdoff_count = 0;
	silence->holdoff_offset = 0;

	silence->crossings = 0;

	return(ST_SUCCESS);
}

int aboveThreshold(LONG value, double threshold, char unit)
{
    double ratio;
    int rc = 0;

    ratio = (double)labs(value) / (double)MAXLONG;

    if (unit == 's')
    {
	rc = (labs(value) >= threshold);
    }
    else
    {
	if (unit == '%')
	    ratio *= 100.0;
	else if (unit == 'd')
	    ratio = log10(ratio) * 20.0;
    	rc = (ratio >= threshold);
    }

    return rc;
}

/* Process signed long samples from ibuf to obuf. */
/* Return number of samples processed in isamp and osamp. */
int st_silence_flow(eff_t effp, LONG *ibuf, LONG *obuf, LONG *isamp, LONG *osamp)
{
    silence_t silence = (silence_t) effp->priv;
    int	threshold, i, j;
    LONG nrOfTicks, nrOfInSamplesRead, nrOfOutSamplesWritten;

    nrOfInSamplesRead = 0;
    nrOfOutSamplesWritten = 0;

    switch (silence->mode)
    {
	case SILENCE_START:
	    if (!silence->trim)
	    {
		/* If no trimming then copy over starting from here */
		silence->mode = SILENCE_COPY;
		goto silence_copy;
	    }
	    else
		/* Fall through and trim audio */
		silence->mode = SILENCE_TRIM;

        /* Reads and discards all input data until it detects a
         * sample that is above the specified threshold.  Turns on
	 * copy mode when detected.
	 */
	case SILENCE_TRIM:
	    nrOfTicks = min((*isamp), (*osamp)) / effp->ininfo.channels;
	    for(i = 0; i < nrOfTicks; i++)
	    {
		threshold = 1;
		for (j = 0; j < effp->ininfo.channels; j++)
		{
		    threshold &= aboveThreshold(ibuf[j], silence->trim_threshold, 
			                        silence->trim_unit);
		}
		if (threshold)
		{
		    silence->mode = SILENCE_COPY;
		    goto silence_copy;
		}
		ibuf += effp->ininfo.channels;
		nrOfInSamplesRead += effp->ininfo.channels;
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
	                    (*osamp-nrOfOutSamplesWritten)) / effp->ininfo.channels;
	    if (silence->stop)
	    {
	        for(i = 0; i < nrOfTicks; i++)
	        {
		    threshold = 0;
		    for (j = 0; j < effp->ininfo.channels; j++)
		    {
		        threshold |= aboveThreshold(ibuf[j], 
				                    silence->stop_threshold, 
			                            silence->stop_unit);
		    }
		    if (threshold && silence->holdoff_count)
		    {
			silence->mode = SILENCE_FLUSH;
			goto silence_flush;
		    }
		    else if (threshold)
		    {
			/* Not holding off so copy into output buffer */
			memcpy(obuf,ibuf,sizeof(LONG)*effp->ininfo.channels);
			nrOfInSamplesRead += effp->ininfo.channels;
			nrOfOutSamplesWritten += effp->ininfo.channels;
			ibuf += effp->ininfo.channels;
		    }
		    else if (!threshold)
		    {
			/* Add to holdoff buffer */
		        for (j = 0; j < effp->ininfo.channels; j++)
		        {
			    silence->holdoff[silence->holdoff_count++] = 
				*ibuf++;
			    nrOfInSamplesRead++;
		        }
			/* Check if holdoff buffer is greater than duration, 
			 * if so then stop processing.
			 */
			if (silence->holdoff_count >= 
				silence->stop_duration)
			{
			    silence->mode = SILENCE_STOP;
			    silence->holdoff_count = 0;
		    	    *isamp = nrOfInSamplesRead;
			    *osamp = nrOfOutSamplesWritten;
			    /* Return ST_EOF to indicate no more processing */
			    return (ST_EOF);
			    break;
			}
		    }
	        }
	    }
	    else
	    {
	        memcpy(obuf, ibuf, sizeof(LONG)*nrOfTicks);
	        nrOfInSamplesRead += nrOfTicks;
	        nrOfOutSamplesWritten += nrOfTicks;
	    }
	    break;

	case SILENCE_FLUSH:
silence_flush:
	    nrOfTicks = min((silence->holdoff_count - silence->holdoff_offset), 
	                    (*osamp-nrOfOutSamplesWritten)) / effp->ininfo.channels;
	    for(i = 0; i < nrOfTicks; i++)
	    {
		*obuf++ = silence->holdoff[silence->holdoff_offset++];
		nrOfOutSamplesWritten++;
	    }

	    if (silence->holdoff_offset == silence->holdoff_count)
	    {
		silence->holdoff_offset = 0;
		silence->holdoff_count = 0;
		silence->mode = SILENCE_COPY;
		/* Return to copy mode incase there are is more room in output buffer
		 * to copy some more data from input buffer.
		 */
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

int st_silence_drain(eff_t effp, LONG *obuf, LONG *osamp)
{
    silence_t silence = (silence_t) effp->priv;
    int i;
    LONG nrOfTicks, nrOfOutSamplesWritten = 0;

    /* Only if in flush mode will there be possible samples to write
     * out during drain() call.
     */
    if (silence->mode == SILENCE_FLUSH)
    {
        nrOfTicks = min((silence->holdoff_count - silence->holdoff_offset), 
	                *osamp) / effp->ininfo.channels;
	for(i = 0; i < nrOfTicks; i++)
	{
	    *obuf++ = silence->holdoff[silence->holdoff_offset++];
	    nrOfOutSamplesWritten++;
        }

	if (silence->holdoff_offset == silence->holdoff_count)
	{
	    silence->holdoff_offset = 0;
	    silence->holdoff_count = 0;
	    silence->mode = SILENCE_STOP;
	}
    }

    *osamp = nrOfOutSamplesWritten;
    return(ST_SUCCESS);
}

int st_silence_stop(eff_t effp)
{
    silence_t silence = (silence_t) effp->priv;

    free(silence->holdoff);
    return(ST_SUCCESS);
}
