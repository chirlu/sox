/*
 * adpcm.h
 */
#include "st.h"

#ifndef SAMPL
#define SAMPL short
#endif

/* default coef sets */
extern const short iCoef[7][2];

/* AdpcmBlockExpandI() outputs interleaved samples into one output buffer */
extern const char *AdpcmBlockExpandI(
	int chans,          /* total channels             */
	int nCoef,
	const short *iCoef,
	const unsigned char *ibuff,/* input buffer[blockAlign]   */
	SAMPL *obuff,       /* output samples, n*chans    */
	int n               /* samples to decode PER channel, REQUIRE n % 8 == 1  */
);

extern void AdpcmBlockMashI(
	int chans,          /* total channels */
	const SAMPL *ip,    /* ip[n*chans] is interleaved input samples */
	int n,              /* samples to encode PER channel, REQUIRE */
	int *st,            /* input/output steps, 16<=st[i] */
	unsigned char *obuff,      /* output buffer[blockAlign] */
	int blockAlign,     /* >= 7*chans + n/2          */
	int opt             /* non-zero allows some cpu-intensive code to improve output */
);

/* Some helper functions for computing samples/block and blockalign */

/*
 * AdpcmSamplesIn(dataLen, chans, blockAlign, samplesPerBlock)
 *  returns the number of samples/channel which would be
 *  in the dataLen, given the other parameters ...
 *  if input samplesPerBlock is 0, then returns the max
 *  samplesPerBlock which would go into a block of size blockAlign
 *  Yes, it is confusing usage.
 */
extern st_size_t AdpcmSamplesIn(
	st_size_t dataLen,
	unsigned short chans,
	unsigned short blockAlign,
	unsigned short samplesPerBlock
);

/*
 * st_size_t AdpcmBytesPerBlock(chans, samplesPerBlock)
 *   return minimum blocksize which would be required
 *   to encode number of chans with given samplesPerBlock
 */
extern st_size_t AdpcmBytesPerBlock(
	unsigned short chans,
	unsigned short samplesPerBlock
);
