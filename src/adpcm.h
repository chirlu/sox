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

/* AdpcmBlockExpandI() outputs interleaved samples into one output buffer */
extern const char *AdpcmBlockExpandI(
	int chans,          /* total channels             */
	const u_char *ibuff,/* input buffer[blockAlign]   */
	SAMPL *obuff,       /* output samples, n*chans    */
	int n               /* samples to decode PER channel, REQUIRE n % 8 == 1  */
);

