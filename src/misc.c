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

#include "st_i.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
/* for fstat */
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif

const char *st_sizes_str[] = {
        "NONSENSE!",
        "bytes",
        "shorts",
        "NONSENSE",
        "longs",
        "NONSENSE",
        "NONSENSE",
        "NONSENSE",
        "long longs"
};

const char *st_encodings_str[] = {
        "NONSENSE!",
        "unsigned",
        "signed (2's complement)",
        "u-law",
        "a-law",
	"floating point",
        "adpcm",
        "ima_adpcm",
        "gsm",
	"inversed u-law",
	"inversed A-law"
};

static const char readerr[] = "Premature EOF while reading sample file.";
static const char writerr[] = "Error writing sample file.  You are probably out of disk space.";

/* Utilities */

/* Read in a buffer of data of length len and each element is size bytes.
 * Returns number of elements read, not bytes read.
 */

st_ssize_t st_read(ft_t ft, void *buf, size_t size, st_ssize_t len)
{
    return fread(buf, size, len, ft->fp);
}

/* Write a buffer of data of length len and each element is size bytes.
 * Returns number of elements writen, not bytes writen.
 */

st_ssize_t st_write(ft_t ft, void *buf, size_t size, st_ssize_t len)
{
    return fwrite(buf, size, len, ft->fp);
}

/* Read and write known datatypes in "machine format".  Swap if indicated.
 * They all return ST_EOF on error and ST_SUCCESS on success.
 */
/* Read n-char string (and possibly null-terminating).
 * Stop reading and null-terminate string if either a 0 or \n is reached.
 */
int st_reads(ft_t ft, char *c, st_ssize_t len)
{
    char *sc;
    char in;

    sc = c;
    do
    {
        if (fread(&in, 1, 1, ft->fp) != 1)
        {
            *sc = 0;
                st_fail_errno(ft,errno,readerr);
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
int st_writes(ft_t ft, char *c)
{
        if (fwrite(c, 1, strlen(c), ft->fp) != strlen(c))
        {
                st_fail_errno(ft,errno,writerr);
                return(ST_EOF);
        }
        return(ST_SUCCESS);
}

/* Read byte. */
int st_readb(ft_t ft, uint8_t *ub)
{
        if (fread(ub, 1, 1, ft->fp) != 1)
        {
                st_fail_errno(ft,errno,readerr);
            return(ST_EOF);
        }
        return ST_SUCCESS;
}

/* Write byte. */
int st_writeb(ft_t ft, uint8_t ub)
{
        if (fwrite(&ub, 1, 1, ft->fp) != 1)
        {
                st_fail_errno(ft,errno,writerr);
                return(ST_EOF);
        }
        return(ST_SUCCESS);
}

/* Read word. */
int st_readw(ft_t ft, uint16_t *uw)
{
        if (fread(uw, 2, 1, ft->fp) != 1)
        {
                st_fail_errno(ft,errno,readerr);
            return (ST_EOF);
        }
        if (ft->swap)
                *uw = st_swapw(*uw);
        return ST_SUCCESS;
}

/* Write word. */
int st_writew(ft_t ft, uint16_t uw)
{
        if (ft->swap)
                uw = st_swapw(uw);
        if (fwrite(&uw, 2, 1, ft->fp) != 1)
        {
                st_fail_errno(ft,errno,writerr);
                return (ST_EOF);
        }
        return(ST_SUCCESS);
}

/* Read double word. */
int st_readdw(ft_t ft, uint32_t *udw)
{
        if (fread(udw, 4, 1, ft->fp) != 1)
        {
                st_fail_errno(ft,errno,readerr);
            return (ST_EOF);
        }
        if (ft->swap)
                *udw = st_swapdw(*udw);
        return ST_SUCCESS;
}

/* Write double word. */
int st_writedw(ft_t ft, uint32_t udw)
{
        if (ft->swap)
                udw = st_swapdw(udw);
        if (fwrite(&udw, 4, 1, ft->fp) != 1)
        {
                st_fail_errno(ft,errno,writerr);
                return (ST_EOF);
        }
        return(ST_SUCCESS);
}

/* Read float. */
int st_readf(ft_t ft, float *f)
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
int st_writef(ft_t ft, float f)
{
        float t = f;

        if (ft->swap)
                t = st_swapf(t);
        if (fwrite(&t, sizeof(float), 1, ft->fp) != 1)
        {
                st_fail_errno(ft,errno,writerr);
                return (ST_EOF);
        }
        return (ST_SUCCESS);
}

/* Read double. */
int st_readdf(ft_t ft, double *d)
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
int st_writedf(ft_t ft, double d)
{
        if (ft->swap)
                d = st_swapd(d);
        if (fwrite(&d, sizeof(double), 1, ft->fp) != 1)
        {
                st_fail_errno(ft,errno,writerr);
                return (ST_EOF);
        }
        return (ST_SUCCESS);
}

/* generic swap routine. Swap l and place in to f (datatype length = n) */
void st_swapb(char *l, char *f, int n)
{
    register int i;

    for (i= 0; i< n; i++)
        f[i]= l[n-i-1];
}


/* Byte swappers, use libc optimized macro's if possible */
#ifndef HAVE_BYTESWAP_H

uint16_t st_swapw(uint16_t uw)
{
    return ((uw >> 8) | (uw << 8)) & 0xffff;
}

uint32_t st_swapdw(uint32_t udw)
{
    return (udw >> 24) | ((udw >> 8) & 0xff00) | ((udw << 8) & 0xff0000L) | (udw << 24);
}

/* return swapped 32-bit float */
float st_swapf(float f)
{
    union {
        uint32_t dw;
        float f;
    } u;

    u.f= f;
    u.dw= (u.dw>>24) | ((u.dw>>8)&0xff00) | ((u.dw<<8)&0xff0000L) | (u.dw<<24);
    return u.f;
}

#endif

double st_swapd(double df)
{
    double sdf;
    st_swapb((char *)&df, (char *)&sdf, sizeof(double));
    return (sdf);
}


/* dummy format routines for do-nothing functions */
int st_format_nothing(ft_t ft) { return(ST_SUCCESS); }
st_ssize_t st_format_nothing_io(ft_t ft, st_sample_t *buf, st_ssize_t len) { return(0); }
int st_format_nothing_seek(ft_t ft, st_size_t offset) { return(ST_SUCCESS); }

/* dummy effect routine for do-nothing functions */
int st_effect_nothing(eff_t effp) { return(ST_SUCCESS); }
int st_effect_nothing_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp) { *osamp = 0; return(ST_SUCCESS); }

/* here for linear interp.  might be useful for other things */
st_sample_t st_gcd(st_sample_t a, st_sample_t b)
{
        if (b == 0)
                return a;
        else
                return st_gcd(b, a % b);
}

st_sample_t st_lcm(st_sample_t a, st_sample_t b)
{
    /* parenthesize this way to avoid st_sample_t overflow in product term */
    return a * (b / st_gcd(a, b));
}

#ifndef HAVE_STRCASECMP
/*
 * Portable strcasecmp() function
 */
int strcasecmp(const char *s1, const char *s2)
{
    for (; toupper(*s1) == toupper(*s2); ++s1, ++s2)
    {
	if (*s1 == '\0')
	    return(0);
    }
    return ((*(unsigned char *)s1 < *(unsigned char *)s2) ? -1 : +1);
}
#endif

#ifndef HAVE_RAND
/*
 * Portable random generator as defined by ANSI C Standard.
 * Don't ask me why not all C libraries include it.
 */

static long rand_seed = 1;

int rand(void) {
    rand_seed = (rand_seed * 1103515245L) + 12345L;
    return ((unsigned int)(rand_seed/65536L) % 32768L);
}

void srand(unsigned int seed)
{
    rand_seed = seed;
}
#endif

/* Util to set initial seed so that we are a little less non-random */
void st_initrand(void) {
    time_t t;

    time(&t);
    srand(t);
}

st_sample_t st_clip24(st_sample_t l)
{
    if (l >= ((st_sample_t)1 << 23))
        return ((st_sample_t)1 << 23) - 1;
    else if (l <= -((st_sample_t)1 << 23))
        return -((st_sample_t)1 << 23) + 1;
    else
        return l;
}

/* This was very painful.  We need a sine library. */

void st_sine(int *buf, st_ssize_t len, int max, int depth)
{
    st_ssize_t i;
    int offset;
    double val;

    offset = max - depth;
    for (i = 0; i < len; i++) {
        val = sin((double)i/(double)len * 2.0 * M_PI);
        buf[i] = (int) ((1.0 + val) * depth / 2.0);
    }
}

void st_triangle(int *buf, st_ssize_t len, int max, int depth)
{
    st_ssize_t i;
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

const char *st_version()
{
    static char versionstr[20];

    sprintf(versionstr, "Version %d.%d.%d",
            (ST_LIB_VERSION_CODE & 0xff0000) >> 16,
            (ST_LIB_VERSION_CODE & 0x00ff00) >> 8,
            (ST_LIB_VERSION_CODE & 0x0000ff));
    return(versionstr);
}


#ifndef HAVE_STRERROR
/* strerror function */
char *strerror(int errcode)
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

/* Sets file offset
 * offset in bytes
 */
int st_seek(ft_t ft, st_size_t offset, int whence)
{
    if( ft->seekable == 0 ){
        /*
         * If a stream peel off chars else
         * EPERM        "Operation not permitted"
         */
        if(whence == SEEK_CUR ){
            if( offset < 0 ){
                st_fail_errno(ft,ST_EINVAL,"Can't seek backwards in pipe");
            } else {
                while ( offset > 0 && !feof(ft->fp) )
                {
                    getc(ft->fp);
                    offset--;
                }
                if(offset)
                    st_fail_errno(ft,ST_EOF,"offset past eof");
                else
                    ft->st_errno = ST_SUCCESS;
            }
        } else {
            st_fail_errno(ft,ST_EPERM,"File not seekable");
        }
    } else {
        if( fseek(ft->fp,offset,whence) == -1 )
            st_fail_errno(ft,errno,strerror(errno));
        else
            ft->st_errno = ST_SUCCESS;
    }

    /* Empty the st file buffer */
    if( ft->st_errno == ST_SUCCESS ){
        ft->file.count = 0;
        ft->file.pos = 0;
        ft->file.eof = 0;
    }

    return(ft->st_errno);
}

st_size_t st_filelength(ft_t ft)
{

    struct stat st;

    fstat(fileno(ft->fp), &st);

    return (st_size_t)st.st_size;
}
