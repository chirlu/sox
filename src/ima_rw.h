/*
	ima_rw.h -- codex utilities for WAV_FORMAT_IMA_ADPCM
	 
	Copyright (C) 1999 Stanley J. Brooks <stabro@megsinet.net> 

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 
*/
#ifndef SAMPL
#	define SAMPL short
#endif

/* #undef STRICT_IMA makes code a bit faster, but not
 * strictly compatible with the real IMA spec, which
 * defines an encoding on approximate multiply/divide
 * using shift/adds instead of the (now) faster mult/div
 * Probably the roundof inexactness is inaudibly small,
 * so you might consider it worthwhile to #undef STRICT_IMA
 */
#define STRICT_IMA
/* #undef STRICT_IMA */

/*
 * call initImaTable() before any other Ima* functions,
 * to create the fast lookup tables
 */
extern void initImaTable(void);

/* ImaBlockExpandI() outputs interleaved samples into one output buffer */
extern void ImaBlockExpandI(
	int chans,          /* total channels             */
	const u_char *ibuff,/* input buffer[blockAlign]   */
	SAMPL *obuff,       /* output samples, n*chans    */
	int n               /* samples to decode PER channel, REQUIRE n % 8 == 1  */
);

/* ImaBlockExpandM() outputs non-interleaved samples into chan separate output buffers */
extern void ImaBlockExpandM(
	int chans,          /* total channels             */
	const u_char *ibuff,/* input buffer[blockAlign]   */
	SAMPL **obuffs,     /* chan output sample buffers, each takes n samples */
	int n               /* samples to decode PER channel, REQUIRE n % 8 == 1  */
);

extern void ImaMashChannel(
	int ch,             /* channel number to encode, REQUIRE 0 <= ch < chans  */
	int chans,          /* total channels */
	const SAMPL *ip,    /* ip[] is interleaved input samples */
	int n,              /* samples to encode PER channel, REQUIRE n % 8 == 1 */
	int *st,            /* input/output state, REQUIRE 0 <= *st <= ISSTMAX */
	u_char *obuff,      /* output buffer[blockAlign] */
	int opt             /* non-zero allows some cpu-intensive code to improve output */
);

