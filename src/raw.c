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

#include <string.h>
#include <stdlib.h>

#ifndef HAVE_MEMMOVE
#define memmove(dest, src, len) bcopy((src), (dest), (len))
#endif

#define MAXWSPEED 1

static void rawdefaults(P1(ft_t ft));

int st_rawstartread(ft) 
ft_t ft;
{
	ft->file.buf = malloc(BUFSIZ);
	if (!ft->file.buf)
	{
	    st_fail("Unable to alloc resources");
	    return(ST_EOF);
	}
	ft->file.size = BUFSIZ;
	ft->file.count = 0;
	ft->file.pos = 0;
	ft->file.eof = 0;

	return(ST_SUCCESS);
}

int st_rawstartwrite(ft) 
ft_t ft;
{
	ft->file.buf = malloc(BUFSIZ);
	if (!ft->file.buf)
	{
	    st_fail("Unable to alloc resources");
	    return(ST_EOF);
	}
	ft->file.size = BUFSIZ;
	ft->file.pos = 0;
	ft->file.eof = 0;

	return(ST_SUCCESS);
}

/* Read raw file data, and convert it to */
/* the sox internal signed long format. */

static unsigned char blockgetc(ft)
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

/* Util to reverse the n chars starting at p. */
/* FIXME: Move to misc.c */
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

static int blockr_sw(p0, n, ft)
LONG *p0,n;
ft_t ft;
{
    LONG x, done;
		short s;

		for (done=0; done < n;) {
			blockr(&s, sizeof(short), ft);
			x = s;
			if (ft->file.eof) break;
			/* scale signed up to long's range */
			*p0++ = LEFT(x, 16);
			done++;
		}
	return done;
}

LONG st_rawread(ft, buf, nsamp) 
ft_t ft;
LONG *buf, nsamp;
{
	int done = 0;

	switch(ft->info.size) {
		case ST_SIZE_BYTE:
		    switch(ft->info.encoding)
		    {
			case ST_ENCODING_SIGN2:
				while(done < nsamp) {
					LONG datum;
					datum = blockgetc(ft);
					if (ft->file.eof)
						return done;
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 24);
					done++;
				}
				return done;
			case ST_ENCODING_UNSIGNED:
				while(done < nsamp) {
					LONG datum;
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
			case ST_ENCODING_ULAW:
				while(done < nsamp) {
					LONG datum;
					datum = blockgetc(ft);
					if (ft->file.eof)
						return done;
					datum = st_ulaw_to_linear(datum);
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 16);
					done++;
				}
				return done;
			case ST_ENCODING_ALAW:
				while(done < nsamp) {
					LONG datum;
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
		case ST_SIZE_WORD:
		    switch(ft->info.encoding)
		    {
			case ST_ENCODING_SIGN2:
				return blockr_sw(buf, nsamp, ft);
			case ST_ENCODING_UNSIGNED:
				while(done < nsamp) {
					LONG x;
					unsigned short s;
					blockr(&s, sizeof(short), ft);
					x = s;
					if (ft->file.eof)
						return done;
					/* Convert to signed */
					x ^= 0x8000;
					/* scale signed up to long's range */
					*buf++ = LEFT(x, 16);
					done++;
				}
				return done;
			case ST_ENCODING_ULAW:
				st_fail("No U-Law support for shorts");
				return 0;
			case ST_ENCODING_ALAW:
				st_fail("No A-Law support for shorts");
				return 0;
		    }
		    break;
		case ST_SIZE_DWORD:
		    switch(ft->info.encoding)
		    {
			case ST_ENCODING_SIGN2:
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
		case ST_SIZE_FLOAT:
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
			break;
	}
	st_fail("Sorry, don't have code to read %s, %s",
		st_encodings_str[ft->info.encoding], st_sizes_str[ft->info.size]);
	return(0);
}

int st_rawstopread(ft)
ft_t ft;
{
	free(ft->file.buf);

	return(ST_SUCCESS);
}

static void blockflush(ft)
ft_t ft;
{
	if (fwrite(ft->file.buf, 1, ft->file.pos, ft->fp) != ft->file.pos)
	{
		st_fail("Error writing data to file");
	}
	ft->file.pos = 0;
}

static void blockputc(ft,c)
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
		swapn(ft->file.buf + ft->file.pos, n);
	ft->file.pos += n;
}

static LONG blockw_sw(ft, buf, nsamp)
ft_t ft;
LONG *buf, nsamp;
{
	short *top;
	LONG save_nsamp = nsamp;

	top = (short*)(ft->file.buf + ft->file.size);
	while (nsamp) {
		short *p, *q;
		p = (short*)(ft->file.buf + ft->file.pos);
		if (p >= top) {
			blockflush(ft);
			continue;
		}
		q = p+nsamp; if (q>top) q = top;
		ft->file.pos += (q-p)*sizeof(short);
		nsamp -= (q-p);
		if (ft->swap) {
#	ifdef MAXWSPEED
			q -= 4;
			while (p<q) {
				p[0] = st_swapw((buf[0] + 0x8000) >> 16); /* round for 16 bit */
				p[1] = st_swapw((buf[1] + 0x8000) >> 16); /* round for 16 bit */
				p[2] = st_swapw((buf[2] + 0x8000) >> 16); /* round for 16 bit */
				p[3] = st_swapw((buf[3] + 0x8000) >> 16); /* round for 16 bit */
				p += 4; buf += 4;
			}
			q += 4;
#	endif
			while (p<q) {
				*p++ = st_swapw(((*buf++) + 0x8000) >> 16); /* round for 16 bit */
			}
		} else {
#	ifdef MAXWSPEED
			q -= 4;
			while (p<q) {
				p[0] = (buf[0] + 0x8000) >> 16; /* round for 16 bit */
				p[1] = (buf[1] + 0x8000) >> 16; /* round for 16 bit */
				p[2] = (buf[2] + 0x8000) >> 16; /* round for 16 bit */
				p[3] = (buf[3] + 0x8000) >> 16; /* round for 16 bit */
				p += 4; buf += 4;
			}
			q += 4;
#	endif
			while (p<q) {
				*p++ = ((*buf++) + 0x8000) >> 16; /* round for 16 bit */
			}
		}
	}
	return(save_nsamp - nsamp);
}

/* Convert the sox internal signed long format */
/* to the raw file data, and write it. */

LONG st_rawwrite(ft, buf, nsamp) 
ft_t ft;
LONG *buf, nsamp;
{
	int done = 0;

	switch(ft->info.size) {
		case ST_SIZE_BYTE:
		    switch(ft->info.encoding)
		    {
			case ST_ENCODING_SIGN2:
				while(done < nsamp) {
					int datum;
					/* scale signed up to long's range */
					datum = (int) RIGHT(*buf++, 24);
					blockputc(ft, datum);
					done++;
				}
				return done;
			case ST_ENCODING_UNSIGNED:
				while(done < nsamp) {
					int datum;
					/* scale signed up to long's range */
					datum = (int) RIGHT(*buf++, 24);
					/* Convert to unsigned */
					datum ^= 128;
					blockputc(ft, datum);
					done++;
				}
				return done;
			case ST_ENCODING_ULAW:
				while(done < nsamp) {
					int datum;
					/* scale signed up to long's range */
					datum = (int) RIGHT(*buf++, 16);
					/* round up to 12 bits of data */
					datum += 0x8;	/* + 0b1000 */
					datum = st_linear_to_ulaw(datum);
					blockputc(ft, datum);
					done++;
				}
				return done;
			case ST_ENCODING_ALAW:
				while(done < nsamp) {
					int datum;
					/* scale signed up to long's range */
					datum = (int) RIGHT(*buf++, 16);
					/* round up to 12 bits of data */
					datum += 0x8;	/* + 0b1000 */
					datum = st_linear_to_Alaw(datum);
					blockputc(ft, datum);
					done++;
				}
				return done;
		    }
		    break;
		case ST_SIZE_WORD:
		    switch(ft->info.encoding)
		    {
			case ST_ENCODING_SIGN2:
				return blockw_sw(ft,buf,nsamp);
			case ST_ENCODING_UNSIGNED:
				while(done < nsamp) {
					int datum;
					unsigned short s;
					/* scale signed up to long's range */
					datum = *buf++ + 0x8000; /* round for 16 bit */
					s = RIGHT(datum, 16) ^ 0x8000;
					/* Convert to unsigned */
					blockw(&s, sizeof(short),ft);
					done++;
				}
				return done;
			case ST_ENCODING_ULAW:
				st_fail("No U-Law support for shorts");
				return 0;
			case ST_ENCODING_ALAW:
				st_fail("No A-Law support for shorts");
				return 0;
		    }
		    break;
		case ST_SIZE_DWORD:
		    switch(ft->info.encoding)
		    {
			case ST_ENCODING_SIGN2:
				while(done < nsamp) {
					/* scale signed up to long's range */
					blockw(buf, sizeof(LONG), ft);
					buf++;
					done++;
				}
				return done;
		    }
		    break;
		case ST_SIZE_FLOAT:
			while(done < nsamp) {
				float f;
				/* scale signed up to long's range */
				f = (float)*buf++ / 0x10000;
			 	blockw(&f, sizeof(float), ft);
				done++;
			}
			return done;
		default:
			break;
	}
	st_fail("Sorry, don't have code to write %s, %s",
		st_encodings_str[ft->info.encoding], st_sizes_str[ft->info.size]);
	return 0;
}

int st_rawstopwrite(ft)
ft_t ft;
{
	blockflush(ft);
	free(ft->file.buf);
	return(ST_SUCCESS);
}

/*
* Set parameters to the fixed parameters known for this format,
* and change format to raw format.
*/

#define STARTREAD(NAME,SIZE,STYLE) \
int NAME(ft) \
ft_t ft; \
{ \
	ft->info.size = SIZE; \
	ft->info.encoding = STYLE; \
	rawdefaults(ft); \
	return st_rawstartread(ft); \
}

#define STARTWRITE(NAME,SIZE,STYLE)\
int NAME(ft) \
ft_t ft; \
{ \
	ft->info.size = SIZE; \
	ft->info.encoding = STYLE; \
	rawdefaults(ft); \
	return st_rawstartwrite(ft); \
}

STARTREAD(st_sbstartread,ST_SIZE_BYTE,ST_ENCODING_SIGN2) 
STARTWRITE(st_sbstartwrite,ST_SIZE_BYTE,ST_ENCODING_SIGN2) 

STARTREAD(st_ubstartread,ST_SIZE_BYTE,ST_ENCODING_UNSIGNED) 
STARTWRITE(st_ubstartwrite,ST_SIZE_BYTE,ST_ENCODING_UNSIGNED) 

STARTREAD(st_uwstartread,ST_SIZE_WORD,ST_ENCODING_UNSIGNED) 
STARTWRITE(st_uwstartwrite,ST_SIZE_WORD,ST_ENCODING_UNSIGNED) 

STARTREAD(st_swstartread,ST_SIZE_WORD,ST_ENCODING_SIGN2) 
STARTWRITE(st_swstartwrite,ST_SIZE_WORD,ST_ENCODING_SIGN2) 

STARTREAD(st_slstartread,ST_SIZE_DWORD,ST_ENCODING_SIGN2) 
STARTWRITE(st_slstartwrite,ST_SIZE_DWORD,ST_ENCODING_SIGN2) 

STARTREAD(st_ulstartread,ST_SIZE_BYTE,ST_ENCODING_ULAW) 
STARTWRITE(st_ulstartwrite,ST_SIZE_BYTE,ST_ENCODING_ULAW) 

STARTREAD(st_alstartread,ST_SIZE_BYTE,ST_ENCODING_ALAW) 
STARTWRITE(st_alstartwrite,ST_SIZE_BYTE,ST_ENCODING_ALAW) 

void rawdefaults(ft)
ft_t ft;
{
	if (ft->info.rate == 0)
		ft->info.rate = 8000;
	if (ft->info.channels == -1)
		ft->info.channels = 1;
}

