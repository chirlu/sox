/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools miscellaneous stuff.
 */

#include "st.h"
#include "version.h"
#include "patchlvl.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

char *sizes[] = {
	"NONSENSE!",
	"bytes",
	"shorts",
	"NONSENSE",
	"longs",
	"32-bit floats",
	"64-bit floats",
	"IEEE floats"
};

char *styles[] = {
	"NONSENSE!",
	"unsigned",
	"signed (2's complement)",
	"u-law",
	"a-law",
	"adpcm",
	"gsm"
};

char readerr[] = "Premature EOF while reading sample file.";
char writerr[] = "Error writing sample file.  You are probably out of disk space.";

/* Utilities */

/* Read and write words and longs in "machine format".  Swap if indicated. */

/* Read short. */
unsigned short
rshort(ft)
ft_t ft;
{
	unsigned short us;

	fread(&us, 2, 1, ft->fp);
	if (ft->swap)
		us = swapw(us);
	return us;
}

/* Write short. */
unsigned short
wshort(ft, us)
ft_t ft;
unsigned short us;
{
	if (ft->swap)
		us = swapw(us);
	if (fwrite(&us, 2, 1, ft->fp) != 1)
		fail(writerr);
	return(0);
}

/* Read long. */
ULONG
rlong(ft)
ft_t ft;
{
	ULONG ul;

	fread(&ul, 4, 1, ft->fp);
	if (ft->swap)
		ul = swapl(ul);
	return ul;
}

/* Write long. */
ULONG
wlong(ft, ul)
ft_t ft;
ULONG ul;
{
	if (ft->swap)
		ul = swapl(ul);
	if (fwrite(&ul, 4, 1, ft->fp) != 1)
		fail(writerr);
	return(0);
}

/* Read float. */
float
rfloat(ft)
ft_t ft;
{
	float f;

	fread(&f, sizeof(float), 1, ft->fp);
	if (ft->swap)
		f = swapf(f);
	return f;
}

void
wfloat(ft, f)
ft_t ft;
float f;
{
	float t = f;

	if (ft->swap)
		t = swapf(t);
	if (fwrite(&t, sizeof(float), 1, ft->fp) != 1)
		fail(writerr);
}

/* Read double. */
double
rdouble(ft)
ft_t ft;
{
	double d;

	fread(&d, sizeof(double), 1, ft->fp);
	if (ft->swap)
		d = swapd(d);
	return d;
}

/* Write double. */
void
wdouble(ft, d)
ft_t ft;
double d;
{
	if (ft->swap)
		d = swapd(d);
	if (fwrite(&d, sizeof(double), 1, ft->fp) != 1)
		fail(writerr);
}

/* generic swap routine */
static void
swapb(l, f, n)
char *l, *f;
int n;
{    register int i;

     for (i= 0; i< n; i++)
	f[i]= l[n-i-1];
}


/* Byte swappers, use libc optimized macro's if possible */
#ifndef HAVE_BYTESWAP_H

unsigned short
swapw(us)
unsigned short us;
{
	return ((us >> 8) | (us << 8)) & 0xffff;
}

ULONG
swapl(ul)
ULONG ul;
{
	return (ul >> 24) | ((ul >> 8) & 0xff00) | ((ul << 8) & 0xff0000L) | (ul << 24);
}

/* return swapped 32-bit float */
float
swapf(float f)
{
	union {
	    ULONG l;
	    float f;
	} u;

	u.f= f;
	u.l= (u.l>>24) | ((u.l>>8)&0xff00) | ((u.l<<8)&0xff0000L) | (u.l<<24);
	return u.f;
}

#endif

double
swapd(df)
double df;
{
	double sdf;
	swapb(&df, &sdf, sizeof(double));
	return (sdf);
}


/* dummy routines for do-nothing functions */
void nothing() {}
LONG nothing_success() {return(0);}

/* dummy drain routine for effects */
void null_drain(effp, obuf, osamp)
eff_t effp;
LONG *obuf;
LONG *osamp;
{
	*osamp = 0;
}

/* here for linear interp.  might be useful for other things */
LONG st_gcd(a, b) 
LONG a, b;
{
	if (b == 0)
		return a;
	else
		return st_gcd(b, a % b);
}

LONG st_lcm(a, b)
LONG a, b;
{
    /* parenthesize this way to avoid LONG overflow in the product term */
    return a * (b / st_gcd(a, b));
}

#ifndef HAVE_RAND
/* 
 * Portable random generator as defined by ANSI C Standard.
 * Don't ask me why not all C libraries include it.
 */

static ULONG rand_seed = 1;

int rand() {
	rand_seed = (rand_seed * 1103515245L) + 12345L;
	return ((ULONG)(rand_seed/65536L) % 32768L);
}

void srand(seed) 
unsigned int seed;
{
	rand_seed = seed;
}
#endif

/* Util to set initial seed so that we are a little less non-random */
void initrand() {
	time_t t;

	time(&t);
	srand(t);
}

char *
version()
{
	static char versionstr[20];
	
	sprintf(versionstr, "Version %d.%d", VERSION, PATCHLEVEL);
	return(versionstr);
}


#ifndef	HAVE_STRERROR
/* strerror function */
char *strerror(errcode)
int errcode;
{
	static char  nomesg[30];
	extern int sys_nerr;
	extern char *sys_errlist[];

	if (errcode < sys_nerr)
		return (sys_errlist[errcode]);
	else
	{
		sprintf (nomesg, "Undocumented error %d", errcode);
		return (nomesg);
	}
}
#endif
