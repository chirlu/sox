/*
 * adpcm.h
 */
#ifndef SAMPL
#define SAMPL short
#endif
#ifndef LONG
#define LONG long
#endif
#ifndef ULONG
#define ULONG u_long
#endif

/* default coef sets */
extern short iCoef[7][2];

/* AdpcmBlockExpandI() outputs interleaved samples into one output buffer */
extern const char *AdpcmBlockExpandI(
	int chans,          /* total channels             */
	const u_char *ibuff,/* input buffer[blockAlign]   */
	SAMPL *obuff,       /* output samples, n*chans    */
	int n               /* samples to decode PER channel, REQUIRE n % 8 == 1  */
);

extern void AdpcmMashI(
	int chans,          /* total channels */
	const SAMPL *ip,    /* ip[n*chans] is interleaved input samples */
	int n,              /* samples to encode PER channel, REQUIRE */
	int *st,            /* input/output steps, 16<=st[i] */
	u_char *obuff,      /* output buffer[blockAlign] */
	int blockAlign,     /* >= 7*chans + n/2          */
	int opt             /* non-zero allows some cpu-intensive code to improve output */
);

