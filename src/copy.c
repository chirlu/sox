/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools skeleton effect file.
 */

#include "st.h"

/*
 * Process options
 */
int st_copy_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
	if (n)
	{
		st_fail("Copy effect takes no options.");
		return (ST_EOF);
	}
	return (ST_SUCCESS);
}

/*
 * Start processing
 */
int st_copy_start(effp)
eff_t effp;
{
	/* nothing to do */
	/* stuff data into delaying effects here */
    return (ST_SUCCESS);
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

int st_copy_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
LONG *isamp, *osamp;
{
	int done;
	
	done = ((*isamp < *osamp) ? *isamp : *osamp);
	memcpy(obuf, ibuf, done * sizeof(LONG));
	*isamp = *osamp = done;
	return (ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_copy_stop(effp)
eff_t effp;
{
	/* nothing to do */
    return (ST_SUCCESS);
}




