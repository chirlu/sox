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
#include <time.h>

EXPORT char *sizes[] = {
	"NONSENSE!",
	"bytes",
	"shorts",
	"NONSENSE",
	"longs",
	"32-bit floats",
	"64-bit floats",
	"IEEE floats"
};

EXPORT char *styles[] = {
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

/* Read short, little-endian: little end first. VAX/386 style. */
unsigned short
rlshort(ft)
ft_t ft;
{
	unsigned char uc, uc2;
	uc  = getc(ft->fp);
	uc2 = getc(ft->fp);
	return (uc2 << 8) | uc;
}

/* Read short, bigendian: big first. 68000/SPARC style. */
unsigned short
rbshort(ft)
ft_t ft;
{
	unsigned char uc, uc2;
	uc2 = getc(ft->fp);
	uc  = getc(ft->fp);
	return (uc2 << 8) | uc;
}

/* Write short, little-endian: little end first. VAX/386 style. */
unsigned short
#if	defined(__STDC__)
wlshort(ft_t ft, unsigned short us)
#else
wlshort(ft, us)
ft_t ft;
unsigned short us;
#endif
{
	putc(us, ft->fp);
	putc(us >> 8, ft->fp);
	if (ferror(ft->fp))
		fail(writerr);
	return(0);
}

/* Write short, big-endian: big end first. 68000/SPARC style. */
unsigned short
#if	defined(__STDC__)
wbshort(ft_t ft, unsigned short us)
#else
wbshort(ft, us)
ft_t ft;
unsigned short us;
#endif
{
	putc(us >> 8, ft->fp);
	putc(us, ft->fp);
	if (ferror(ft->fp))
		fail(writerr);
	return(0);
}

/* Read long, little-endian: little end first. VAX/386 style. */
ULONG
rllong(ft)
ft_t ft;
{
	unsigned char uc, uc2, uc3, uc4;
/*	if (feof(ft->fp))
		fail(readerr);	*/	/* No worky! */
	uc  = getc(ft->fp);
	uc2 = getc(ft->fp);
	uc3 = getc(ft->fp);
	uc4 = getc(ft->fp);
	return ((LONG)uc4 << 24) | ((LONG)uc3 << 16) | ((LONG)uc2 << 8) | (LONG)uc;
}

/* Read long, bigendian: big first. 68000/SPARC style. */
ULONG
rblong(ft)
ft_t ft;
{
	unsigned char uc, uc2, uc3, uc4;
/*	if (feof(ft->fp))
		fail(readerr);	 */	/* No worky! */
	uc  = getc(ft->fp);
	uc2 = getc(ft->fp);
	uc3 = getc(ft->fp);
	uc4 = getc(ft->fp);
	return ((LONG)uc << 24) | ((LONG)uc2 << 16) | ((LONG)uc3 << 8) | (LONG)uc4;
}

/* Write long, little-endian: little end first. VAX/386 style. */
ULONG
wllong(ft, ul)
ft_t ft;
ULONG ul;
{
char datum;

	datum = (char) (ul) & 0xff;
	putc(datum, ft->fp);
	datum = (char) (ul >> 8) & 0xff;
	putc(datum, ft->fp);
	datum = (char) (ul >> 16) & 0xff;
	putc(datum, ft->fp);
	datum = (char) (ul >> 24) & 0xff;
	putc(datum, ft->fp);
	if (ferror(ft->fp))
		fail(writerr);
	return(0);
}

/* Write long, big-endian: big end first. 68000/SPARC style. */
ULONG
wblong(ft, ul)
ft_t ft;
ULONG ul;
{
char datum;

	datum = (char) (ul >> 24) & 0xff;
	putc(datum, ft->fp);
	datum = (char) (ul >> 16) & 0xff;
	putc(datum, ft->fp);
	datum = (char) (ul >> 8) & 0xff;
	putc(datum, ft->fp);
	datum = (char) (ul) & 0xff;
	putc(datum, ft->fp);
	if (ferror(ft->fp))
		fail(writerr);
	return(0);
}

/* Read and write words and longs in "machine format".  Swap if indicated. */

/* Read short. */
unsigned short
rshort(ft)
ft_t ft;
{
	unsigned short us;

/*	if (feof(ft->fp))
		fail(readerr);	  */	/* No worky! */
	fread(&us, 2, 1, ft->fp);
	if (ft->swap)
		us = swapw(us);
	return us;
}

/* Write short. */
unsigned short
#if	defined(__STDC__)
wshort(ft_t ft, unsigned short us)
#else
wshort(ft, us)
ft_t ft;
unsigned short us;
#endif
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

/*	if (feof(ft->fp))
		fail(readerr);  */		/* No worky! */
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

/*    if (feof(ft->fp))
		fail(readerr);	*/	/* No worky! */
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

/*    if (feof(ft->fp))
		fail(readerr); */	  /* No worky! */
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


/* Byte swappers */

unsigned short
#if	defined(__STDC__)
swapw(unsigned short us)
#else
swapw(us)
unsigned short us;
#endif
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
#if	defined(__STDC__)
swapf(float uf)
#else
swapf(uf)
float uf;
#endif
{
	union {
	    ULONG l;
	    float f;
	} u;

	u.f= uf;
	u.l= (u.l>>24) | ((u.l>>8)&0xff00) | ((u.l<<8)&0xff0000L) | (u.l<<24);
	return u.f;
}

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
LONG gcd(a, b) 
LONG a, b;
{
	if (b == 0)
		return a;
	else
		return gcd(b, a % b);
}

LONG lcm(a, b)
LONG a, b;
{
	return (a * b) / gcd(a, b);
}

/* 
 * Cribbed from Unix SVR3 programmer's manual 
 */

static ULONG rand15_seed;

ULONG rand15() {
	rand15_seed = (rand15_seed * 1103515245L) + 12345L;
	return (ULONG) ((rand15_seed/65536L) % 32768L);
}

void srand15(seed) 
ULONG seed;
{
	rand15_seed = seed;
}

void newrand15() {
	time_t t;

	time(&t);
	srand15(t);
}

/* sine wave gen should be here, also */

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
