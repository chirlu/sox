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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

const char *st_sizes_str[] = {
	"NONSENSE!",
	"bytes",
	"shorts",
	"NONSENSE",
	"longs",
	"32-bit floats",
	"64-bit floats",
	"IEEE floats"
};

const char *st_encodings_str[] = {
	"NONSENSE!",
	"unsigned",
	"signed (2's complement)",
	"u-law",
	"a-law",
	"adpcm",
	"ima_adpcm",
	"gsm",
};

static const char readerr[] = "Premature EOF while reading sample file.";
static const char writerr[] = "Error writing sample file.  You are probably out of disk space.";

/* Utilities */

/* Read in a buffer of data of length len and each element is size bytes.
 * Returns number of elements read, not bytes read.
 */

LONG st_read(ft, buf, size, len)
ft_t ft;
void *buf;
int size;
LONG len;
{
    return fread(buf, size, len, ft->fp);
}

/* Write a buffer of data of length len and each element is size bytes.
 * Returns number of elements writen, not bytes writen.
 */

LONG st_write(ft, buf, size, len)
ft_t ft;
void *buf;
int size;
LONG len;
{
    return fwrite(buf, size, len, ft->fp);
}

/* Read and write known datatypes in "machine format".  Swap if indicated.
 * They all return ST_EOF on error and ST_SUCCESS on success.
 */
/* Read n-char string (and possibly null-terminating). 
 * Stop reading and null-terminate string if either a 0 or \n is reached. 
 */
int
st_reads(ft, c, len)
ft_t ft;
char *c;
int len;
{
    char *sc;
    char in;

    sc = c;
    do
    {
	if (fread(&in, 1, 1, ft->fp) != 1)
	{
	    *sc = 0;
	    return (ST_EOF);
	}
	if (in == 0 || in == '\n')
	{
	    break;
	}

	*sc = in;
	sc++;
    } while (sc - c < len);
    *sc = 0;
    return(ST_SUCCESS);
}

/* Write null-terminated string (without \0). */
int
st_writes(ft, c)
ft_t ft;
char *c;
{
	if (fwrite(c, 1, strlen(c), ft->fp) != strlen(c))
	{
		st_fail(writerr);
		return(ST_EOF);
	}
	return(ST_SUCCESS);
}

/* Read byte. */
int
st_readb(ft, uc)
ft_t ft;
unsigned char *uc;
{
	if (fread(uc, 1, 1, ft->fp) != 1)
	{
	    return(ST_EOF);
	}
	return ST_SUCCESS;
}

/* Write byte. */
int
st_writeb(ft, uc)
ft_t ft;
unsigned char uc;
{
	if (fwrite(&uc, 1, 1, ft->fp) != 1)
	{
		st_fail(writerr);
		return(ST_EOF);
	}
	return(ST_SUCCESS);
}

/* Read word. */
int
st_readw(ft, us)
ft_t ft;
unsigned short *us;
{
	if (fread(us, 2, 1, ft->fp) != 1)
	{
	    return (ST_EOF);
	}
	if (ft->swap)
		*us = st_swapw(*us);
	return ST_SUCCESS;
}

/* Write word. */
int
st_writew(ft, us)
ft_t ft;
unsigned short us;
{
	if (ft->swap)
		us = st_swapw(us);
	if (fwrite(&us, 2, 1, ft->fp) != 1)
	{
		st_fail(writerr);
		return (ST_EOF);
	}
	return(ST_SUCCESS);
}

/* Read double word. */
int
st_readdw(ft, ul)
ft_t ft;
ULONG *ul;
{
	if (fread(ul, 4, 1, ft->fp) != 1)
	{
	    return (ST_EOF);
	}
	if (ft->swap)
		*ul = st_swapl(*ul);
	return ST_SUCCESS;
}

/* Write double word. */
int
st_writedw(ft, ul)
ft_t ft;
ULONG ul;
{
	if (ft->swap)
		ul = st_swapl(ul);
	if (fwrite(&ul, 4, 1, ft->fp) != 1)
	{
		st_fail(writerr);
		return (ST_EOF);
	}
	return(ST_SUCCESS);
}

/* Read float. */
int
st_readf(ft, f)
ft_t ft;
float *f;
{
	if (fread(f, sizeof(float), 1, ft->fp) != 1)
	{
	    return(ST_EOF);
	}
	if (ft->swap)
		*f = st_swapf(*f);
	return ST_SUCCESS;
}

/* Write float. */
int
st_writef(ft, f)
ft_t ft;
float f;
{
	float t = f;

	if (ft->swap)
		t = st_swapf(t);
	if (fwrite(&t, sizeof(float), 1, ft->fp) != 1)
	{
		st_fail(writerr);
		return (ST_EOF);
	}
	return (ST_SUCCESS);
}

/* Read double. */
int
st_readdf(ft, d)
ft_t ft;
double *d;
{
	if (fread(d, sizeof(double), 1, ft->fp) != 1)
	{
	    return(ST_EOF);
	}
	if (ft->swap)
		*d = st_swapd(*d);
	return ST_SUCCESS;
}

/* Write double. */
int
st_writedf(ft, d)
ft_t ft;
double d;
{
	if (ft->swap)
		d = st_swapd(d);
	if (fwrite(&d, sizeof(double), 1, ft->fp) != 1)
	{
		st_fail(writerr);
		return (ST_EOF);
	}
	return (ST_SUCCESS);
}

/* generic swap routine. Swap l and place in to f (datatype length = n) */
static void
st_swapb(l, f, n)
char *l, *f;
int n;
{    register int i;

     for (i= 0; i< n; i++)
	f[i]= l[n-i-1];
}


/* Byte swappers, use libc optimized macro's if possible */
#ifndef HAVE_BYTESWAP_H

unsigned short
st_swapw(us)
unsigned short us;
{
	return ((us >> 8) | (us << 8)) & 0xffff;
}

ULONG
st_swapl(ul)
ULONG ul;
{
	return (ul >> 24) | ((ul >> 8) & 0xff00) | ((ul << 8) & 0xff0000L) | (ul << 24);
}

/* return swapped 32-bit float */
float
st_swapf(float f)
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
st_swapd(df)
double df;
{
	double sdf;
	st_swapb(&df, &sdf, sizeof(double));
	return (sdf);
}


/* dummy routines for do-nothing functions */
void st_nothing(P0) {}
LONG st_nothing_success(P0) {return(0);}

/* dummy drain routine for effects */
void st_null_drain(effp, obuf, osamp)
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
	return ((unsigned int)(rand_seed/65536L) % 32768L);
}

void srand(seed) 
unsigned int seed;
{
	rand_seed = seed;
}
#endif

/* Util to set initial seed so that we are a little less non-random */
void st_initrand(P0) {
	time_t t;

	time(&t);
	srand(t);
}

LONG st_clip24(l)
LONG l;
{
    if (l >= ((LONG)1 << 23))
	return ((LONG)1 << 23) - 1;
    else if (l <= -((LONG)1 << 23))
        return -((LONG)1 << 23) + 1;
    else
        return l;
}

/* This was very painful.  We need a sine library. */

void st_sine(buf, len, max, depth)
int *buf;
LONG len;
int max;
int depth;
{
	long i;
	int offset;
	double val;

	offset = max - depth;
	for (i = 0; i < len; i++) {
		val = sin((double)i/(double)len * 2.0 * M_PI);
		buf[i] = (int) ((1.0 + val) * depth / 2.0);
	}
}

void st_triangle(buf, len, max, depth)
int *buf;
LONG len;
int max;
int depth;
{
	LONG i;
	int offset;
	double val;

	offset = max - 2 * depth;
	for (i = 0; i < len / 2; i++) {
		val = i * 2.0 / len;
		buf[i] = offset + (int) (val * 2.0 * (double)depth);
	}
	for (i = len / 2; i < len ; i++) {
		val = (len - i) * 2.0 / len;
		buf[i] = offset + (int) (val * 2.0 * (double)depth);
	}
}

const char *
st_version()
{
	static char versionstr[20];
	
	sprintf(versionstr, "Version %s", ST_LIB_VERSION);
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
