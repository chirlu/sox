
/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools cut effect file.
 *
 * Pull loop #n from a looped sound file.
 * Not finished, don't use it yet.
 */

#include <math.h>
#include "st.h"

/* Private data for SKEL file */
typedef struct cutstuff {
	int	which;			/* Loop # to pull */
	int	where;			/* current sample # */
	ULONG start;			/* first wanted sample */
	ULONG end;			/* last wanted sample + 1 */
} *cut_t;

/*
 * Process options
 */
void cut_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
	cut_t cut = (cut_t) effp->priv;

	/* parse it */
	cut->which = 0;	/* for now */
}

/*
 * Prepare processing.
 */
void cut_start(effp)
eff_t effp;
{
	cut_t cut = (cut_t) effp->priv;
	/* nothing to do */

	cut->where = 0;
	cut->start = effp->loops[0].start;
	cut->end = effp->loops[0].start + effp->loops[0].length;
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

void cut_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
LONG *isamp, *osamp;
{
	cut_t cut = (cut_t) effp->priv;
	int len, done;
	
	len = ((*isamp > *osamp) ? *osamp : *isamp);
	if ((cut->where + len <= cut->start) ||
			(cut->where >= cut->end)) {
		*isamp = len;
		*osamp = 0;
		cut->where += len;
		return;
	}
	*isamp = len;		/* We will have processed all inputs */
	if (cut->where < cut->start) {
		/* skip */
		ibuf += cut->start - cut->where;
		len -= cut->start - cut->where;
	}
	if (cut->where + len >= cut->end) {
		/* shorten */
		len = cut->end - cut->where;
	}
	for(done = 0; done < len; done++) {
		*obuf++ = *ibuf++;
	}
	*osamp = len;
}

/*
 * Drain out remaining samples if the effect generates any.
 */

void cut_drain(effp, obuf, osamp)
eff_t effp;
LONG *obuf;
LONG *osamp;
{
	*osamp = 0;
}

/*
 * Do anything required when you stop reading samples.  
 *	(free allocated memory, etc.)
 */
void cut_stop(effp)
eff_t effp;
{
	/* nothing to do */
}


