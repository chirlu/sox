
/*
 * June 1, 1992
 * Copyright 1992 Guido van Rossum And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Guido van Rossum And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * "reverse" effect, uses a temporary file created by tmpfile().
 */

#include <math.h>
#include "st.h"

IMPORT FILE *tmpfile();

#ifndef SEEK_SET
#define SEEK_SET        0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR        1
#endif
#ifndef SEEK_END
#define SEEK_END        2
#endif

/* Private data */
typedef struct reversestuff {
	FILE *fp;
	LONG pos;
	int phase;
} *reverse_t;

#define WRITING 0
#define READING 1

/*
 * Process options: none in our case.
 */

void reverse_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
	if (n)
		fail("Reverse effect takes no options.");
}

/*
 * Prepare processing: open temporary file.
 */

void reverse_start(effp)
eff_t effp;
{
	reverse_t reverse = (reverse_t) effp->priv;
	reverse->fp = tmpfile();
	if (reverse->fp == NULL)
		fail("Reverse effect can't create temporary file\n");
	reverse->phase = WRITING;
}

/*
 * Effect flow: a degenerate case: write input samples on temporary file,
 * don't generate any output samples.
 */

void reverse_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
int *isamp, *osamp;
{
	reverse_t reverse = (reverse_t) effp->priv;

	if (reverse->phase != WRITING)
		fail("Internal error: reverse_flow called in wrong phase");
	if (fwrite((char *)ibuf, sizeof(LONG), *isamp, reverse->fp)
	    != *isamp)
		fail("Reverse effect write error on temporary file\n");
	*osamp = 0;
}

/*
 * Effect drain: generate the actual samples in reverse order.
 */

void reverse_drain(effp, obuf, osamp)
eff_t effp;
LONG *obuf;
int *osamp;
{
	reverse_t reverse = (reverse_t) effp->priv;
	int len, nbytes;
	register int i, j;
	LONG temp;

	if (reverse->phase == WRITING) {
		fflush(reverse->fp);
		fseek(reverse->fp, 0L, SEEK_END);
		reverse->pos = ftell(reverse->fp);
		if (reverse->pos % sizeof(LONG) != 0)
			fail("Reverse effect finds odd temporary file\n");
		reverse->phase = READING;
	}
	len = *osamp;
	nbytes = len * sizeof(LONG);
	if (reverse->pos < nbytes) {
		nbytes = reverse->pos;
		len = nbytes / sizeof(LONG);
	}
	reverse->pos -= nbytes;
	fseek(reverse->fp, reverse->pos, SEEK_SET);
	if (fread((char *)obuf, sizeof(LONG), len, reverse->fp) != len)
		fail("Reverse effect read error from temporary file\n");
	for (i = 0, j = len-1; i < j; i++, j--) {
		temp = obuf[i];
		obuf[i] = obuf[j];
		obuf[j] = temp;
	}
	*osamp = len;
}

/*
 * Close and unlink the temporary file.
 */
void reverse_stop(effp)
eff_t effp;
{
	reverse_t reverse = (reverse_t) effp->priv;

	fclose(reverse->fp);
}

