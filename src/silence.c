
//	Silence effect for SoX
//	by Heikki Leinonen (heilei@iki.fi) 25.03.2001
//
//	This effect deletes samples from the start of the sound
//	file until a sample exceeds a given threshold (either
//	left or right channel in stereo files). This can be used
//	to filter out unwanted silence or low noise in the beginning
//	of a sound file. The threshold can be given either as a
//	percentage or in decibels.


#include <math.h>
#include "st.h"

#ifndef min
	#define min(s1,s2) ((s1)<(s2)?(s1):(s2))
#endif

#ifndef true
	#define	true	1
#endif

#ifndef false
	#define	false	0
#endif

//	Private data for silence effect.

#define	DEFAULT_THRESHOLD	1.0
#define	DEFAULT_UNIT		'%'

typedef struct silencestuff
{
	double	threshold;
	char	unit;	//	"d" for decibels or "%" for percent.
	int		wentAboveThreshold;
} *silence_t;


int st_silence_getopts(eff_t effp, int n, char **argv)
{
	silence_t	silence = (silence_t) effp->priv;

	silence->threshold = DEFAULT_THRESHOLD;
	silence->unit = DEFAULT_UNIT;
	switch (n)
	{
		case 0:	//	No arguments, use defaults given above.
		break;

		case 1:
			sscanf(argv[0], "%lf", &silence->threshold);
		break;

		default:
			sscanf(argv[0], "%lf", &silence->threshold);
			sscanf(argv[1], "%c", &silence->unit);
		break;
	}
	if ((silence->unit != '%') && (silence->unit != 'd'))
		st_fail("Usage: silence [threshold [d | %%]]");
	if ((silence->unit == '%') && ((silence->threshold < 0.0) || (silence->threshold > 100.0)))
		st_fail("silence threshold should be between 0.0 and 100.0 %%");
	if ((silence->unit == 'd') && (silence->threshold >= 0.0))
		st_fail("silence threshold should be less than 0.0 dB");
	return(ST_SUCCESS);
}

int st_silence_start(eff_t effp)
{
	silence_t	silence = (silence_t) effp->priv;

	silence->wentAboveThreshold = false;

	if ((effp->outinfo.channels != 1) && (effp->outinfo.channels != 2))
	{
		st_fail("Silence effect can only be run on mono or stereo data");
		return (ST_EOF);
	}
	return(ST_SUCCESS);
}

int aboveThreshold(LONG value, double threshold, char unit)
{
	double	maxLong = 2147483647.0;
	double	ratio, percentRatio, decibelRatio;

	ratio = (double) labs(value) / maxLong;
	percentRatio = ratio * 100.0;
	decibelRatio = log10(ratio) * 20.0;
	return((unit == '%') ? (percentRatio >= threshold) : (decibelRatio >= threshold));
}

//	Process signed long samples from ibuf to obuf.
//	Return number of samples processed in isamp and osamp.
int st_silence_flow(eff_t effp, LONG *ibuf, LONG *obuf, LONG *isamp, LONG *osamp)
{
	silence_t	silence = (silence_t) effp->priv;
	int			nrOfTicks, i;
	LONG		leftSample, rightSample, monoSample, nrOfInSamplesRead, nrOfOutSamplesWritten;

	nrOfInSamplesRead = 0;
	nrOfOutSamplesWritten = 0;

	switch (effp->outinfo.channels)
	{
	case 1:
		nrOfTicks = min((*isamp), (*osamp));
		for(i = 0; i < nrOfTicks; i++)
		{
			monoSample = ibuf[0];
			if (silence->wentAboveThreshold || aboveThreshold(monoSample, silence->threshold, silence->unit))
			{
				silence->wentAboveThreshold = true;
				obuf[0] = ibuf[0];	//	Copy data from input to output.
				obuf++;			//	Advance output buffers by 1 sample.
				nrOfOutSamplesWritten++;
			}
			ibuf++;	//	Always advance input buffers by 1 sample.
			nrOfInSamplesRead++;
		}
	break;

	case 2:
		nrOfTicks = min((*isamp), (*osamp)) / 2;
		for(i = 0; i < nrOfTicks; i++)
		{
			leftSample = ibuf[0];
			rightSample = ibuf[1];
			if (silence->wentAboveThreshold || aboveThreshold(leftSample, silence->threshold, silence->unit) || aboveThreshold(rightSample, silence->threshold, silence->unit))
			{
				silence->wentAboveThreshold = true;
				obuf[0] = ibuf[0];	//	Copy data from input to output.
				obuf[1] = ibuf[1];
				obuf += 2;			//	Advance output buffers by 2 samples.
				nrOfOutSamplesWritten += 2;
			}
			ibuf += 2;	//	Always advance input buffers by 2 samples.
			nrOfInSamplesRead += 2;
		}
	break;

	default:	//	We should never get here, but just in case...
		st_fail("Silence effect can only be run on mono or stereo data");
	break;
	}
	*isamp = nrOfInSamplesRead;
	*osamp = nrOfOutSamplesWritten;

	return (ST_SUCCESS);
}

int st_silence_drain(eff_t effp, LONG *obuf, LONG *osamp)
{
	*osamp = 0;
	return(ST_SUCCESS);
}

int st_silence_stop(eff_t effp)
{
	return(ST_SUCCESS);
}
