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
#include <string.h>

/* Time resolutin one millisecond */
#define TIMERES 1000

#define TRIM_USAGE "Trim usage: trim start [length]"

typedef struct
{
    /* options here */
    char *start_str;
    char *length_str;

    /* options converted to values */
    ULONG start;
    ULONG length;

    /* internal stuff */
    ULONG index;
    ULONG trimmed;
    int done;
} * trim_t;

/*
 * Process options
 */
int st_trim_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
    trim_t trim = (trim_t) effp->priv;

    trim->start = 0;
    trim->length = 0;

    /* Do not know sample rate yet so hold off on completely parsing
     * time related strings.
     */
    switch (n) {
	case 2:
	    trim->length_str = malloc(strlen(argv[1])+1);
	    if (!trim->length_str)
	    {
		st_fail("Could not allocate memory");
		return(ST_EOF);
	    }
	    strcpy(trim->length_str,argv[1]);
	    /* Do a dummy parse to see if it will fail */
	    if (st_parsesamples(0, trim->length_str,
			        &trim->length, 't') != ST_SUCCESS)
	    {
		st_fail(TRIM_USAGE);
		return(ST_EOF);
	    }
	case 1:
	    trim->start_str = malloc(strlen(argv[0])+1);
	    if (!trim->start_str)
	    {
		st_fail("Could not allocate memory");
		return(ST_EOF);
	    }
	    strcpy(trim->start_str,argv[0]);
	    /* Do a dummy parse to see if it will fail */
	    if (st_parsesamples(0, trim->start_str,
			        &trim->start, 't') != ST_SUCCESS)
	    {
		st_fail(TRIM_USAGE);
		return(ST_EOF);
	    }
	    break;
	default:
	    st_fail(TRIM_USAGE);
	    return ST_EOF;
	    break;

    }
    return (ST_SUCCESS);
}

/*
 * Start processing
 */
int st_trim_start(effp)
eff_t effp;
{
    trim_t trim = (trim_t) effp->priv;


    if (st_parsesamples(effp->ininfo.rate, trim->start_str,
		        &trim->start, 't') != ST_SUCCESS)
    {
	st_fail(TRIM_USAGE);
	return(ST_EOF);
    }
    if (st_parsesamples(effp->ininfo.rate, trim->length_str,
		        &trim->length, 't') != ST_SUCCESS)
    {
	st_fail(TRIM_USAGE);
	return(ST_EOF);
    }

    trim->done = 0;
    trim->index = 0;
    trim->trimmed = 0;

    return (ST_SUCCESS);
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

int st_trim_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
LONG *isamp, *osamp;
{
    int done;
    int offset;
    int start_trim = 0;

    trim_t trim = (trim_t) effp->priv;

    done = ((*isamp < *osamp) ? *isamp : *osamp);
    *isamp = done; /* always read this much */

    if (trim->done) {
	*osamp = 0;
	return (ST_EOF);
    }

    trim->index += done;
    offset = 0;
    if (! trim->trimmed) {
	if (trim->start > trim->index) {
	    *osamp = 0;
	    return (ST_SUCCESS);
	} else {
	    start_trim = 1;
	    offset =  (trim->start==0? 0: trim->index - trim->start);
	    done -= offset; /* adjust done */
	}
    }

    if (trim->trimmed || start_trim ) {

	if (trim->length && ( (trim->trimmed+done) > trim->length)) {
	    done = trim->length - trim->trimmed ;
	    *osamp = done;
	    trim->done = 1;
	}

	trim->trimmed += done;
    }

    memcpy(obuf, ibuf+offset, done * sizeof(LONG));
    *osamp = done;
    return (ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_trim_stop(effp)
eff_t effp;
{
    trim_t trim = (trim_t) effp->priv;

    if (trim->start_str)
	free(trim->start_str);
    if (trim->length_str)
	free(trim->length_str);

    return (ST_SUCCESS);
}





