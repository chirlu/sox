/*
 * CD-R format handler
 *
 * David Elliott, Sony Microsystems -  July 5, 1991
 *
 * Copyright 1991 David Elliott And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 *
 * This code automatically handles endianness differences
 *
 * cbagwell (cbagwell@sprynet.com) - 20 April 1998
 *
 *   Changed endianness handling.  Seemed to be reversed (since format
 *   is in big endian) and made it so that user could always override
 *   swapping no matter what endian machine they are one.
 *
 *   Fixed bug were trash could be appended to end of file for certain
 *   endian machines.
 *
 */

#include "st.h"

#define SECTORSIZE	(2352 / 2)

/* Private data for SKEL file */
typedef struct cdrstuff {
	LONG	samples;	/* number of samples written */
} *cdr_t;

LONG rawread(P3(ft_t, LONG *, LONG));
void rawwrite(P3(ft_t, LONG *, LONG));

/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *	Find out sampling rate, 
 *	size and style of samples, 
 *	mono/stereo/quad.
 */

void cdrstartread(ft) 
ft_t ft;
{

	int     littlendian = 1;
	char    *endptr;

	/* Needed because of rawread() */
	rawstartread(ft);

	endptr = (char *) &littlendian;
	/* CDR is in Big Endian format.  Swap whats read in on */
        /* Little Endian machines.                             */
	if (*endptr)
	{ 
	    ft->swap = ft->swap ? 0 : 1;
	}

	ft->info.rate = 44100L;
	ft->info.size = WORD;
	ft->info.style = SIGN2;
	ft->info.channels = 2;
	ft->comment = NULL;
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

LONG cdrread(ft, buf, len) 
ft_t ft;
LONG *buf, len;
{

	return rawread(ft, buf, len);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
void cdrstopread(ft) 
ft_t ft;
{
	/* Needed because of rawread() */
	rawstopread(ft);
}

void cdrstartwrite(ft) 
ft_t ft;
{
	cdr_t cdr = (cdr_t) ft->priv;

	int     littlendian = 1;
	char    *endptr;

	endptr = (char *) &littlendian;
	/* CDR is in Big Endian format.  Swap whats written out on */
	/* Little Endian Machines.                                 */
	if (*endptr) 
	{
	    ft->swap = ft->swap ? 0 : 1;
	}

	/* Needed because of rawwrite() */
	rawstartwrite(ft);

	cdr->samples = 0;

	ft->info.rate = 44100L;
	ft->info.size = WORD;
	ft->info.style = SIGN2;
	ft->info.channels = 2;
}

void cdrwrite(ft, buf, len) 
ft_t ft;
LONG *buf, len;
{
	cdr_t cdr = (cdr_t) ft->priv;

	cdr->samples += len;

	rawwrite(ft, buf, len);
}

/*
 * A CD-R file needs to be padded to SECTORSIZE, which is in terms of
 * samples.  We write -32768 for each sample to pad it out.
 */

void cdrstopwrite(ft) 
ft_t ft;
{
	cdr_t cdr = (cdr_t) ft->priv;
	int padsamps = SECTORSIZE - (cdr->samples % SECTORSIZE);
	short zero;

	/* Flush buffer before writing anything else */
	rawstopwrite(ft);

	zero = 0;

	if (padsamps != SECTORSIZE) 
	{
		while (padsamps > 0) {
			wshort(ft, zero);
			padsamps--;
		}
	}
}

