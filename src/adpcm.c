/*
 * adpcm.c  codex functions for MS_ADPCM data
 *          (hopefully) provides interoperability with
 *          Microsoft's ADPCM format, but, as usual,
 *          see LACK-OF-WARRANTY information below.
 *
 *      Copyright (C) 1999 Stanley J. Brooks <stabro@megsinet.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * November 20, 1999
 *  specs I've seen are unclear about ADPCM supporting more than 2 channels,
 *  but these routines support more channels in a manner which looks (IMHO)
 *  like the most natural extension.
 *
 *  Remark: code still turbulent, encoding to be added RSN
 */

#include <sys/types.h>
#include <math.h>
#include <stdio.h>
#include "adpcm.h"

typedef struct MsState {
	LONG  step;	/* step size */
	short iCoef[2];
} MsState_t;

#define lsbshortldi(x,p) { (x)=((short)((int)(p)[0] + ((int)(p)[1]<<8))); (p) += 2; }

/*
 * Lookup tables for MS ADPCM format
 */

/* these are step-size adjust factors, where
 * 1.0 is scaled to 0x100
 */
static LONG stepAdjustTable[] = {
	230, 230, 230, 230, 307, 409, 512, 614,
	768, 614, 512, 409, 307, 230, 230, 230
};

/* TODO : The first 7 iCoef sets are always hardcoded and must
   appear in the actual WAVE file.  They should be read in
   in case a sound program added extras to the list. */

static short iCoef[7][2] = {
			{ 256,   0},
			{ 512,-256},
			{   0,   0},
			{ 192,  64},
			{ 240,   0},
			{ 460,-208},
			{ 392,-232}
};

#if 0
static LONG AdpcmDecode(LONG, MsState_t*, LONG, LONG)__attribute__((regparm(3)));
#endif

#ifdef __GNUC__
inline
#endif
static LONG AdpcmDecode(c, state, sample1, sample2)
LONG c, sample1, sample2;
MsState_t *state;
{
	LONG predict;
	LONG sample;
	LONG step, step1;

	/** Compute next step value **/
	step = state->step;
	step1 = (stepAdjustTable[c] * step) >> 8;
	state->step = (step1 < 16)? 16:step1;

	/** make linear prediction for next sample **/
	predict =
			((sample1 * state->iCoef[0]) +
			 (sample2 * state->iCoef[1])) >> 8;
	/** then add the code*step adjustment **/
	c -= (c & 0x08) << 1;
	sample = (c * step) + predict;

	if (sample > 32767)
		sample = 32767;
	else if (sample < -32768)
		sample = -32768;

	return (sample);
}

/* AdpcmBlockExpandI() outputs interleaved samples into one output buffer */
const char *AdpcmBlockExpandI(
	int chans,          /* total channels             */
	const u_char *ibuff,/* input buffer[blockAlign]   */
	SAMPL *obuff,       /* output samples, n*chans    */
	int n               /* samples to decode PER channel */
)
{
	const u_char *ip;
	int ch;
	const char *errmsg = NULL;
	MsState_t state[4];						/* One decompressor state for each channel */

	/* Read the four-byte header for each channel */
	ip = ibuff;
	for (ch = 0; ch < chans; ch++) {
		u_char bpred = *ip++;
		if (bpred >= 7) {
			errmsg = "MSADPCM bpred >= 7, arbitrarily using 0\n";
			bpred = 0;
		}
		state[ch].iCoef[0] = iCoef[(int)bpred][0];
		state[ch].iCoef[1] = iCoef[(int)bpred][1];
	}

	for (ch = 0; ch < chans; ch++)
		lsbshortldi(state[ch].step, ip);

	/* sample1's directly into obuff */
	for (ch = 0; ch < chans; ch++)
		lsbshortldi(obuff[chans+ch], ip);

	/* sample2's directly into obuff */
	for (ch = 0; ch < chans; ch++)
		lsbshortldi(obuff[ch], ip);

	{
		int ch;
		u_char b;
		short *op, *top;

		/* already have 1st 2 samples from block-header */
		op = obuff + 2*chans;
		top = obuff + n*chans;

		ch = 0;
		while (op < top) {
			b = *ip++;
			*op++ = AdpcmDecode(b >> 4, state+ch, op[-chans], op[-2*chans]);
			if (++ch == chans) ch = 0;
			/* ch = ++ch % chans; */
			*op++ = AdpcmDecode(b&0x0f, state+ch, op[-chans], op[-2*chans]);
			if (++ch == chans) ch = 0;
			/* ch = ++ch % chans; */
		}
	}
	return errmsg;
}
