/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools raw format file.
 *
 * Includes .ub, .uw, .sb, .sw, and .ul formats at end
 */

/*
 * Notes: most of the headerless formats set their handlers to raw
 * in their startread/write routines.  
 *
 */

#include "st.h"
#include "libst.h"

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include <string.h>
#include <stdlib.h>

#ifndef HAVE_MEMMOVE
#define memmove(dest, src, len) bcopy((src), (dest), (len))
#endif

void rawstartread(ft) 
ft_t ft;
{
	ft->file.buf = malloc(BUFSIZ);
	ft->file.size = BUFSIZ;
	ft->file.count = 0;
	ft->file.pos = 0;
	ft->file.eof = 0;
}

void rawstartwrite(ft) 
ft_t ft;
{
	ft->file.buf = malloc(BUFSIZ);
	ft->file.size = BUFSIZ;
	ft->file.pos = 0;
	ft->file.eof = 0;
}

/* Read raw file data, and convert it to */
/* the sox internal signed long format. */

unsigned char blockgetc(ft)
ft_t ft;
{
	char rval;

	if (ft->file.pos == ft->file.count)
	{
		if (ft->file.eof)
			return(0);
		ft->file.count = fread(ft->file.buf, 1, ft->file.size, ft->fp);
		ft->file.pos = 0;
		if (ft->file.count == 0)
		{
			ft->file.eof = 1;
			return(0);
		}
	}
	rval = *(ft->file.buf + ft->file.pos++);
	return (rval);
}

/* Util to swap every 2 chars up to 'n' imes. */
static void swapn(p, n)
char *p;
int n;
{
	char *q;
	if (n>1) {
		q = p+n-1;
		while (q>p) {
			char t = *q;
			*q-- = *p;
			*p++ = t;
		}
	}
}

/* Reads a block of 'n' characters and possibly byte swaps */
static void blockr(p0, n, ft)
ft_t ft;
int n;
void *p0;
{
	int rest;
	char *p;
	p = p0;
	
	while ((rest = ft->file.count - ft->file.pos) < n)
	{
		if (ft->file.eof) {
			memset(p,0,n);
			return;
		}

		memmove(ft->file.buf, ft->file.buf+ft->file.pos, rest);

		ft->file.pos = 0;
		ft->file.count = rest;
		ft->file.count += fread(ft->file.buf+rest, 1, ft->file.size-rest, ft->fp);
		ft->file.eof = (ft->file.count < n);
	}
	memcpy(p, ft->file.buf + ft->file.pos, n);
	ft->file.pos += n;
	/* FIXME: For speed, there should be a version of this funciton
	 * for WORDS, LONGS, and FLOATS.  This is because swapping
	 * bytes is an expensive operation and should be done in batch
	 * mode.
	 */
	if (ft->swap)
		swapn(p,n);
}

LONG rawread(ft, buf, nsamp) 
ft_t ft;
LONG *buf, nsamp;
{
	register LONG datum;
	int done = 0;

	switch(ft->info.size) {
		case BYTE:
		    switch(ft->info.style)
		    {
			case SIGN2:
				while(done < nsamp) {
					datum = blockgetc(ft);
					if (ft->file.eof)
						return done;
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 24);
					done++;
				}
				return done;
			case UNSIGNED:
				while(done < nsamp) {
					datum = blockgetc(ft);
					if (ft->file.eof)
						return done;
					/* Convert to signed */
					datum ^= 128;
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 24);
					done++;
				}
				return done;
			case ULAW:
				while(done < nsamp) {
					datum = blockgetc(ft);
					if (ft->file.eof)
						return done;
					datum = st_ulaw_to_linear(datum);
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 16);
					done++;
				}
				return done;
			case ALAW:
				while(done < nsamp) {
				        datum = blockgetc(ft);
				        if (ft->file.eof)
				                return done;
				        datum = st_Alaw_to_linear(datum);
				        /* scale signed up to long's range */
				        *buf++ = LEFT(datum, 16);
				        done++;
				}

				return done;
		    }
		    break;
		case WORD:
		    switch(ft->info.style)
		    {
			case SIGN2:
				while(done < nsamp) {
					short s;
					blockr(&s, sizeof(short), ft);
					datum = s;
					if (ft->file.eof)
						return done;
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 16);
					done++;
				}
				return done;
			case UNSIGNED:
				while(done < nsamp) {
					unsigned short s;
					blockr(&s, sizeof(short), ft);
					datum = s;
					if (ft->file.eof)
						return done;
					/* Convert to signed */
					datum ^= 0x8000;
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 16);
					done++;
				}
				return done;
			case ULAW:
				fail("No U-Law support for shorts");
				return done;
			case ALAW:
				fail("No A-Law support for shorts");
				return done;
		    }
		    break;
		case DWORD:
		    switch(ft->info.style)
		    {
			case SIGN2:
				while(done < nsamp) {
					blockr(buf, sizeof(LONG), ft);
					if (ft->file.eof)
						return done;
					/* scale signed up to long's range */
					buf++;
					done++;
				}
				return done;
		    }
		    break;
		case FLOAT:
			while(done < nsamp) {
				float f;
				blockr(&f, sizeof(float), ft);
				if (ft->file.eof)
					return done;
				*buf++ = f * 0x10000; /* hmm... */
				done++;
			}
			return done;
		default:
			fail("Drop through in rawread!");
	}
	fail("Sorry, don't have code to read %s, %s",
		styles[ft->info.style], sizes[ft->info.size]);
	return(0);
}

void rawstopread(ft)
ft_t ft;
{
	free(ft->file.buf);
}

static void blockflush(ft)
ft_t ft;
{
	if (fwrite(ft->file.buf, 1, ft->file.pos, ft->fp) != ft->file.pos)
	{
		fail("Error writing data to file");
	}
	ft->file.pos = 0;
}

void blockputc(ft,c)
ft_t ft;
int c;
{
	if (ft->file.pos == ft->file.size) blockflush(ft);
	*(ft->file.buf + ft->file.pos) = c;
	ft->file.pos++;
}

static void blockw(p0, n, ft)
void *p0;
int n;
ft_t ft;
{
	if (ft->file.pos > ft->file.size-n) blockflush(ft);
	memcpy(ft->file.buf + ft->file.pos, p0, n);
	/* FIXME: Should be a version for every data type.  This
	 * is because swap is an expensive operation.  We should
	 * only swap the buffer after its full.
	 */
	if (ft->swap)
		swapn(p0, n);
	ft->file.pos += n;
}

/* Convert the sox internal signed long format */
/* to the raw file data, and write it. */

void rawwrite(ft, buf, nsamp) 
ft_t ft;
LONG *buf, nsamp;
{
	register int datum;
	int done = 0;

	switch(ft->info.size) {
		case BYTE:
		    switch(ft->info.style)
		    {
			case SIGN2:
				while(done < nsamp) {
					/* scale signed up to long's range */
					datum = (int) RIGHT(*buf++, 24);
					blockputc(ft, datum);
					done++;
				}
				return;
			case UNSIGNED:
				while(done < nsamp) {
					/* scale signed up to long's range */
					datum = (int) RIGHT(*buf++, 24);
					/* Convert to unsigned */
					datum ^= 128;
					blockputc(ft, datum);
					done++;
				}
				return;
			case ULAW:
				while(done < nsamp) {
					/* scale signed up to long's range */
					datum = (int) RIGHT(*buf++, 16);
					/* round up to 12 bits of data */
					datum += 0x8;	/* + 0b1000 */
					datum = st_linear_to_ulaw(datum);
					blockputc(ft, datum);
					done++;
				}
				return;
			case ALAW:
				while(done < nsamp) {
					/* scale signed up to long's range */
					datum = (int) RIGHT(*buf++, 16);
					/* round up to 12 bits of data */
					datum += 0x8;	/* + 0b1000 */
					datum = st_linear_to_Alaw(datum);
					blockputc(ft, datum);
					done++;
				}
				return;
		    }
		    break;
		case WORD:
		    switch(ft->info.style)
		    {
			case SIGN2:
				while(done < nsamp) {
					short s;
					/* scale signed up to long's range */
					datum = *buf++ + 0x8000; /* round for 16 bit */
					s = RIGHT(datum, 16);
					blockw(&s, sizeof(short),ft);
					done++;
				}
				return;
			case UNSIGNED:
				while(done < nsamp) {
					unsigned short s;
					/* scale signed up to long's range */
					datum = *buf++ + 0x8000; /* round for 16 bit */
					s = RIGHT(datum, 16) ^ 0x8000;
					/* Convert to unsigned */
					blockw(&s, sizeof(short),ft);
					done++;
				}
				return;
			case ULAW:
fail("No U-Law support for shorts (try -b option ?)");
				return;
			case ALAW:
fail("No A-Law support for shorts (try -b option ?)");
				return;
		    }
		    break;
		case DWORD:
		    switch(ft->info.style)
		    {
			case SIGN2:
				while(done < nsamp) {
					/* scale signed up to long's range */
					blockw(buf, sizeof(LONG), ft);
					buf++;
					done++;
				}
				return;
		    }
		    break;
		case FLOAT:
			while(done < nsamp) {
				float f;
				/* scale signed up to long's range */
				f = (float)*buf++ / 0x10000;
			 	blockw(&f, sizeof(float), ft);
				done++;
			}
			return;
		default:
		    fail("Drop through in rawwrite!");
	}
	fail("Sorry, don't have code to write %s, %s",
		styles[ft->info.style], sizes[ft->info.size]);
}

void rawstopwrite(ft)
ft_t ft;
{
	blockflush(ft);
	free(ft->file.buf);
}

/*
* Set parameters to the fixed parameters known for this format,
* and change format to raw format.
*/

void rawdefaults();

#define STARTREAD(NAME,SIZE,STYLE) \
void NAME(ft) \
ft_t ft; \
{ \
	ft->info.size = SIZE; \
	ft->info.style = STYLE; \
	rawstartread(ft); \
	rawdefaults(ft); \
}

#define STARTWRITE(NAME,SIZE,STYLE)\
void NAME(ft) \
ft_t ft; \
{ \
	ft->info.size = SIZE; \
	ft->info.style = STYLE; \
	rawstartwrite(ft); \
	rawdefaults(ft); \
}

STARTREAD(sbstartread,BYTE,SIGN2) 
STARTWRITE(sbstartwrite,BYTE,SIGN2) 

STARTREAD(ubstartread,BYTE,UNSIGNED) 
STARTWRITE(ubstartwrite,BYTE,UNSIGNED) 

STARTREAD(uwstartread,WORD,UNSIGNED) 
STARTWRITE(uwstartwrite,WORD,UNSIGNED) 

STARTREAD(swstartread,WORD,SIGN2) 
STARTWRITE(swstartwrite,WORD,SIGN2) 

STARTREAD(slstartread,DWORD,SIGN2) 
STARTWRITE(slstartwrite,DWORD,SIGN2) 

STARTREAD(ulstartread,BYTE,ULAW) 
STARTWRITE(ulstartwrite,BYTE,ULAW) 

STARTREAD(alstartread,BYTE,ALAW) 
STARTWRITE(alstartwrite,BYTE,ALAW) 

void rawdefaults(ft)
ft_t ft;
{
	if (ft->info.rate == 0)
		ft->info.rate = 8000;
	if (ft->info.channels == -1)
		ft->info.channels = 1;
}

