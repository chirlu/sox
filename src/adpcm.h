/*
 * adpcm.h
 */
#include "sox.h"

#ifndef SAMPL
#define SAMPL short
#endif

/* default coef sets */
extern const short lsx_ms_adpcm_i_coef[7][2];

/* lsx_ms_adpcm_block_expand_i() outputs interleaved samples into one output buffer */
extern const char *lsx_ms_adpcm_block_expand_i(
	unsigned chans,          /* total channels             */
	int nCoef,
	const short *lsx_ms_adpcm_i_coef,
	const unsigned char *ibuff,/* input buffer[blockAlign]   */
	SAMPL *obuff,       /* output samples, n*chans    */
	int n               /* samples to decode PER channel, REQUIRE n % 8 == 1  */
);

extern void lsx_ms_adpcm_block_mash_i(
	unsigned chans,          /* total channels */
	const SAMPL *ip,    /* ip[n*chans] is interleaved input samples */
	int n,              /* samples to encode PER channel, REQUIRE */
	int *st,            /* input/output steps, 16<=st[i] */
	unsigned char *obuff,      /* output buffer[blockAlign] */
	int blockAlign      /* >= 7*chans + n/2          */
);

/* Some helper functions for computing samples/block and blockalign */

/*
 * lsx_ms_adpcm_samples_in(dataLen, chans, blockAlign, samplesPerBlock)
 *  returns the number of samples/channel which would be
 *  in the dataLen, given the other parameters ...
 *  if input samplesPerBlock is 0, then returns the max
 *  samplesPerBlock which would go into a block of size blockAlign
 *  Yes, it is confusing usage.
 */
extern sox_size_t lsx_ms_adpcm_samples_in(
	sox_size_t dataLen,
	sox_size_t chans,
	sox_size_t blockAlign,
	sox_size_t samplesPerBlock
);

/*
 * sox_size_t lsx_ms_adpcm_bytes_per_block(chans, samplesPerBlock)
 *   return minimum blocksize which would be required
 *   to encode number of chans with given samplesPerBlock
 */
extern sox_size_t lsx_ms_adpcm_bytes_per_block(
	sox_size_t chans,
	sox_size_t samplesPerBlock
);
