
/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools Map effect file.
 *
 * Print out map of sound file instrument specifications.
 */

#include <math.h>
#include "st.h"

/*
 * Process options
 */
void map_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
	if (n)
		fail("Map effect takes no options.");
}

/*
 * Prepare processing.
 */
void map_start(effp)
eff_t effp;
{
	int i;

	fprintf(stderr, "Loop info:\n");
	for(i = 0; i < 8; i++) {
		fprintf(stderr, "Loop %d: start:  %6d",i,effp->loops[i].start);
		fprintf(stderr, " length: %6d", effp->loops[i].length);
		fprintf(stderr, " count: %6d", effp->loops[i].count);
		fprintf(stderr, " type:  ");
		switch(effp->loops[i].type) {
			case 0: fprintf(stderr, "off\n"); break;
			case 1: fprintf(stderr, "forward\n"); break;
			case 2: fprintf(stderr, "forward/backward\n"); break;
		}
	}
	fprintf(stderr, "MIDI note: %d\n", effp->instr.MIDInote);
	fprintf(stderr, "MIDI low : %d\n", effp->instr.MIDIlow);
	fprintf(stderr, "MIDI hi  : %d\n", effp->instr.MIDIhi);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

void map_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
int *isamp, *osamp;
{
}
