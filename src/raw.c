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

#include "st_i.h"
#include "libst.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifndef HAVE_MEMMOVE
#define memmove(dest, src, len) bcopy((src), (dest), (len))
#endif

#define MAXWSPEED 1

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

static void rawdefaults(ft_t ft);

int st_rawseek(ft_t ft, st_size_t offset) 
{
	int sample_size = 0;

	switch(ft->info.size) {
		case ST_SIZE_BYTE:
			sample_size = 1;
		        break;
		case ST_SIZE_WORD:
			sample_size = sizeof(short);
		        break;
		case ST_SIZE_DWORD:
			sample_size = sizeof(LONG);
		        break;
		case ST_SIZE_FLOAT:
			sample_size = sizeof(float);
		        break;
		default:
			st_fail_errno(ft,ST_ENOTSUP,"Can't seek this data size");
			return(ft->st_errno);
	}

	ft->st_errno = st_seek(ft,offset*sample_size,SEEK_SET);

	return(ft->st_errno);
}

int st_rawstartread(ft_t ft) 
{
	ft->file.buf = malloc(BUFSIZ);
	if (!ft->file.buf)
	{
	    st_fail_errno(ft,ST_ENOMEM,"Unable to alloc resources");
	    return(ST_EOF);
	}
	ft->file.size = BUFSIZ;
	ft->file.count = 0;
	ft->file.pos = 0;
	ft->file.eof = 0;

	return(ST_SUCCESS);
}

int st_rawstartwrite(ft_t ft) 
{
	ft->file.buf = malloc(BUFSIZ);
	if (!ft->file.buf)
	{
	    st_fail_errno(ft,ST_ENOMEM,"Unable to alloc resources");
	    return(ST_EOF);
	}
	ft->file.size = BUFSIZ;
	ft->file.pos = 0;
	ft->file.eof = 0;

	return(ST_SUCCESS);
}

/* Util to reverse the n chars starting at p. */
/* FIXME: This is already in misc.c */
static void swapn(char *p, int n)
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

void st_ub_copy_buf(LONG *buf1, char *buf2, ULONG len, char swap)
{
    while (len)
    {
	unsigned char datum;

	datum = *((unsigned char *)buf2);
        buf2++;

	*buf1++ = ST_UNSIGNED_BYTE_TO_SAMPLE(datum);
	len--;
    }
}

void st_sb_copy_buf(LONG *buf1, char *buf2, ULONG len, char swap)
{
    while (len)
    {
	unsigned char datum;

	datum = *((unsigned char *)buf2);
        buf2++;

	*buf1++ = ST_SIGNED_BYTE_TO_SAMPLE(datum);
	len--;
    }
}

void st_ulaw_copy_buf(LONG *buf1, char *buf2, ULONG len, char swap)
{
    while (len)
    {
	unsigned char datum;

	datum = *((unsigned char *)buf2);
        buf2++;

	*buf1++ = ST_ULAW_BYTE_TO_SAMPLE(datum);
	len--;
    }
}

void st_alaw_copy_buf(LONG *buf1, char *buf2, ULONG len, char swap)
{
    while (len)
    {
	unsigned char datum;

	datum = *((unsigned char *)buf2);
        buf2++;

	*buf1++ = ST_ALAW_BYTE_TO_SAMPLE(datum);
	len--;
    }
}

void st_uw_copy_buf(LONG *buf1, char *buf2, ULONG len, char swap)
{
    while (len)
    {
	unsigned short datum;

	datum = *((unsigned short *)buf2);
        buf2++; buf2++;
	if (swap)
	    datum = st_swapw(datum);

	*buf1++ = ST_UNSIGNED_WORD_TO_SAMPLE(datum);
	len--;
    }
}

void st_sw_copy_buf(LONG *buf1, char *buf2, ULONG len, char swap)
{
    while (len)
    {
	unsigned short datum;

	datum = *((unsigned short *)buf2);
        buf2++; buf2++;
	if (swap)
	    datum = st_swapw(datum);

	*buf1++ = ST_SIGNED_WORD_TO_SAMPLE(datum);
	len--;
    }
}

void st_udw_copy_buf(LONG *buf1, char *buf2, ULONG len, char swap)
{
    while (len)
    {
	ULONG datum;

	datum = *((ULONG *)buf2);
        buf2++; buf2++; buf2++; buf2++;
	if (swap)
	    datum = st_swapl(datum);

	*buf1++ = ST_UNSIGNED_DWORD_TO_SAMPLE(datum);
	len--;
    }
}

void st_sl_copy_buf(LONG *buf1, char *buf2, ULONG len, char swap)
{
    while (len)
    {
	ULONG datum;

	datum = *((ULONG *)buf2);
        buf2++; buf2++; buf2++; buf2++;
	if (swap)
	    datum = st_swapl(datum);

	*buf1++ = ST_SIGNED_DWORD_TO_SAMPLE(datum);
	len--;
    }
}

void st_f32_copy_buf(LONG *buf1, char *buf2, ULONG len, char swap)
{
    while (len)
    {
	float datum;

	datum = *((float *)buf2);
        buf2++; buf2++; buf2++; buf2++;
	if (swap)
	    datum = st_swapf(datum);

	*buf1++ = ST_FLOAT_DWORD_TO_SAMPLE(datum);
	len--;
    }
}

void st_f64_copy_buf(LONG *buf1, char *buf2, ULONG len, char swap)
{
    while (len)
    {
	double datum;

	datum = *((double *)buf2);
        buf2++; buf2++; buf2++; buf2++;
        buf2++; buf2++; buf2++; buf2++;
	if (swap)
	    datum = st_swapd(datum);

	*buf1++ = ST_FLOAT_DDWORD_TO_SAMPLE(datum);
	len--;
    }
}

/* Reads a buffer of different data types into SoX's internal buffer
 * format.
 */
/* FIXME:  This function adds buffering on top of stdio's buffering.
 * Mixing st_readbuf's and freads or fgetc or even SoX's other util
 * functions will cause a loss of data!  Need to have sox implement
 * a consistent buffering protocol.
 */
ULONG st_readbuf(LONG *p, int n, int size, int encoding, ft_t ft)
{
    ULONG len, done = 0;
    void (*copy_buf)(LONG *, char *, ULONG, char) = 0;
    int i;

    switch(size) {
	case ST_SIZE_BYTE:
	    switch(encoding)
	    {
		case ST_ENCODING_SIGN2:
		    copy_buf = st_sb_copy_buf;
		    break;
		case ST_ENCODING_UNSIGNED:
		    copy_buf = st_ub_copy_buf;
		    break;
		case ST_ENCODING_ULAW:
		    copy_buf = st_ulaw_copy_buf;
		    break;
		case ST_ENCODING_ALAW:
		    copy_buf = st_alaw_copy_buf;
		    break;
		default:
		    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size.");
		    return(0);
	    }
	    break;

	case ST_SIZE_WORD:
	    switch(encoding)
	    {
		case ST_ENCODING_SIGN2:
		    copy_buf = st_sw_copy_buf;
		    break;
		case ST_ENCODING_UNSIGNED:
		    copy_buf = st_uw_copy_buf;
		    break;
		default:
		    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size.");
		    return(0);
	    }
	    break;

	case ST_SIZE_DWORD:
	    switch(encoding)
	    {
		case ST_ENCODING_SIGN2:
		    copy_buf = st_sl_copy_buf;
		    break;
		case ST_ENCODING_UNSIGNED:
		    copy_buf = st_udw_copy_buf;
		    break;
		default:
		    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size.");
		    return(0);
	    }
	    break;

	case ST_SIZE_FLOAT:
	    copy_buf = st_f32_copy_buf;
	    /* Hack hack... Encoding should be FLOAT, not the size */
	    size = 4;
	    break;

	case ST_SIZE_DOUBLE:
	    copy_buf = st_f64_copy_buf;
	    /* Hack hack... Encoding should be FLOAT, not the size */
	    size = 8;
	    break;

	default:
	    st_fail_errno(ft,ST_EFMT,"Do not support this data size for this handler");
	    return (0);
    }

    
    len = MIN(n,(ft->file.count-ft->file.pos)/size);
    if (len)
    {
	copy_buf(p + done, ft->file.buf + ft->file.pos, len, ft->swap);
        ft->file.pos += (len*size);
	done += len;
    }

    while (done < n)
    {
	/* See if there is not enough data in buffer for any more reads
	 * or if there is no data in the buffer at all.
	 * If not then shift any remaining data down to the beginning
	 * and attempt to fill up the rest of the buffer.  
	 */
	if (!ft->file.eof && (ft->file.count == 0 ||
		              ft->file.pos >= (ft->file.count-size+1)))
	{
	    for (i = 0; i < (ft->file.count-ft->file.pos); i++)
		ft->file.buf[i] = ft->file.buf[ft->file.pos+i];

	    i = ft->file.count-ft->file.pos;
	    ft->file.pos = 0;

	    ft->file.count = fread(ft->file.buf+i, 1, ft->file.size-i, ft->fp) ;
	    if (ft->file.count == 0)
	    {
		ft->file.eof = 1;
	    }
	    ft->file.count += i;
	}

        len = MIN(n - done,(ft->file.count-ft->file.pos)/size);
        if (len)
        {
	    copy_buf(p + done, ft->file.buf + ft->file.pos, len, ft->swap);
            ft->file.pos += (len*size);
	    done += len;
        }
	if (ft->file.eof)
	    break;
    }
    return done;
}

LONG st_rawread(ft, buf, nsamp) 
ft_t ft;
LONG *buf, nsamp;
{
    return st_readbuf(buf, nsamp, ft->info.size, ft->info.encoding, ft);
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
		st_fail_errno(ft,errno,"Error writing data to file");
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
				p[0] = st_swapw(buf[0] >> 16); 
				p[1] = st_swapw(buf[1] >> 16); 
				p[2] = st_swapw(buf[2] >> 16); 
				p[3] = st_swapw(buf[3] >> 16); 
				p += 4; buf += 4;
			}
			q += 4;
#	endif
			while (p<q) {
				*p++ = st_swapw((*buf++) >> 16); 
			}
		} else {
#	ifdef MAXWSPEED
			q -= 4;
			while (p<q) {
				p[0] = buf[0] >> 16; 
				p[1] = buf[1] >> 16; 
				p[2] = buf[2] >> 16; 
				p[3] = buf[3] >> 16; 
				p += 4; buf += 4;
			}
			q += 4;
#	endif
			while (p<q) {
				*p++ = (*buf++) >> 16; 
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
					datum = 
					    ST_SAMPLE_TO_SIGNED_BYTE(*buf++);
					blockputc(ft, datum);
					done++;
				}
				return done;
			case ST_ENCODING_UNSIGNED:
				while(done < nsamp) {
					int datum;
					/* scale signed up to long's range */
					datum = 
					    ST_SAMPLE_TO_UNSIGNED_BYTE(*buf++);
					/* Convert to unsigned */
					blockputc(ft, datum);
					done++;
				}
				return done;
			case ST_ENCODING_ULAW:
				while(done < nsamp) {
					short datum;
					/* scale signed up to long's range */
					datum = 
					    ST_SAMPLE_TO_ULAW_BYTE(*buf++);
					blockputc(ft, datum);
					done++;
				}
				return done;
			case ST_ENCODING_ALAW:
				while(done < nsamp) {
					int datum;
					/* scale signed up to long's range */
					datum =
					    ST_SAMPLE_TO_ALAW_BYTE(*buf++);
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
					u_int16_t s;
					/* scale signed up to long's range */
					s = ST_SAMPLE_TO_UNSIGNED_WORD(*buf++);
					/* Convert to unsigned */
					blockw(&s, sizeof(u_int16_t),ft);
					done++;
				}
				return done;
			case ST_ENCODING_ULAW:
				st_fail_errno(ft,ST_EFMT,"No U-Law support for shorts");
				return 0;
			case ST_ENCODING_ALAW:
				st_fail_errno(ft,ST_EFMT,"No A-Law support for shorts");
				return 0;
		    }
		    break;
		case ST_SIZE_DWORD:
		    switch(ft->info.encoding)
		    {
			case ST_ENCODING_SIGN2:
				while(done < nsamp) {
					/* scale signed up to long's range */
					blockw(buf, sizeof(u_int32_t), ft);
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
	/* Possible overflow */
	st_fail_errno(ft,ST_EFMT,"Sorry, don't have code to write %s, %s",
		st_encodings_str[(unsigned char)ft->info.encoding],
     		st_sizes_str[(unsigned char)ft->info.size]);
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

