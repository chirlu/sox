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
#include <stddef.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
/* for fstat */
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif

const char * const st_sizes_str[] = {
        "NONSENSE!",
        "bytes",
        "shorts",
        "24 bits",
        "longs",
        "NONSENSE",
        "NONSENSE",
        "NONSENSE",
        "long longs"
};

const char * const st_size_bits_str[] = {
        "NONSENSE!",
        "8-bits",
        "16-bits",
        "24-bits",
        "32-bits",
        "NONSENSE",
        "NONSENSE",
        "NONSENSE",
        "64-bits"
};

const char * const st_encodings_str[] = {
        "NONSENSE!",
        "unsigned",
        "signed (2's complement)",
        "u-law",
        "a-law",
        "floating point",
        "ADPCM",
        "IMA-ADPCM",
        "GSM",
        "inversed u-law",
        "inversed A-law",
        "MPEG audio (layer I, II or III)",
        "Vorbis",
        "FLAC",
        "OKI-ADPCM"
};

assert_static(array_length(st_encodings_str) == ST_ENCODINGS,
    SIZE_MISMATCH_BETWEEN_st_encodings_t_AND_st_encodings_str);

static const char readerr[] = "Premature EOF while reading sample file.";
static const char writerr[] = "Error writing sample file.  You are probably out of disk space.";

/* Utilities */

/* Read in a buffer of data of length len and each element is size bytes.
 * Returns number of elements read, not bytes read.
 */
size_t st_readbuf(ft_t ft, void *buf, size_t size, st_size_t len)
{
    return fread(buf, size, len, ft->fp);
}

/* Skip input without seeking. */
int st_skipbytes(ft_t ft, st_size_t n)
{
  unsigned char trash;

  while (n--)
    if (st_readb(ft, &trash) == ST_EOF)
      return (ST_EOF);
  
  return (ST_SUCCESS);
}

/* Pad output. */
int st_padbytes(ft_t ft, st_size_t n)
{
  while (n--)
    if (st_writeb(ft, '\0') == ST_EOF)
      return (ST_EOF);

  return (ST_SUCCESS);
}

/* Write a buffer of data of length len and each element is size bytes.
 * Returns number of elements writen, not bytes written.
 */

size_t st_writebuf(ft_t ft, void const *buf, size_t size, st_size_t len)
{
    return fwrite(buf, size, len, ft->fp);
}

st_size_t st_filelength(ft_t ft)
{
  struct stat st;

  fstat(fileno(ft->fp), &st);

  return (st_size_t)st.st_size;
}

int st_flush(ft_t ft)
{
  return fflush(ft->fp);
}

st_size_t st_tell(ft_t ft)
{
  return (st_size_t)ftello(ft->fp);
}

int st_eof(ft_t ft)
{
  return feof(ft->fp);
}

int st_error(ft_t ft)
{
  return ferror(ft->fp);
}

void st_rewind(ft_t ft)
{
  rewind(ft->fp);
}

void st_clearerr(ft_t ft)
{
  clearerr(ft->fp);
}

/* Read and write known datatypes in "machine format".  Swap if indicated.
 * They all return ST_EOF on error and ST_SUCCESS on success.
 */
/* Read n-char string (and possibly null-terminating).
 * Stop reading and null-terminate string if either a 0 or \n is reached.
 */
int st_reads(ft_t ft, char *c, st_size_t len)
{
    char *sc;
    char in;

    sc = c;
    do
    {
        if (st_readbuf(ft, &in, 1, 1) != 1)
        {
            *sc = 0;
                st_fail_errno(ft,errno,readerr);
            return (ST_EOF);
        }
        if (in == 0 || in == '\n')
            break;

        *sc = in;
        sc++;
    } while (sc - c < (ptrdiff_t)len);
    *sc = 0;
    return(ST_SUCCESS);
}

/* Write null-terminated string (without \0). */
int st_writes(ft_t ft, char *c)
{
        if (st_writebuf(ft, c, 1, strlen(c)) != strlen(c))
        {
                st_fail_errno(ft,errno,writerr);
                return(ST_EOF);
        }
        return(ST_SUCCESS);
}

/* Read byte. */
int st_readb(ft_t ft, uint8_t *ub)
{
        if (st_readbuf(ft, ub, 1, 1) != 1)
        {
            st_fail_errno(ft,errno,readerr);
            return(ST_EOF);
        }
        return ST_SUCCESS;
}

/* Write byte. */
int st_writeb(ft_t ft, uint8_t ub)
{
        if (st_writebuf(ft, &ub, 1, 1) != 1)
        {
                st_fail_errno(ft,errno,writerr);
                return(ST_EOF);
        }
        return(ST_SUCCESS);
}

/* Read word. */
int st_readw(ft_t ft, uint16_t *uw)
{
        if (st_readbuf(ft, uw, 2, 1) != 1)
        {
            st_fail_errno(ft,errno,readerr);
            return (ST_EOF);
        }
        if (ft->info.swap)
                *uw = st_swapw(*uw);
        return ST_SUCCESS;
}

/* Write word. */
int st_writew(ft_t ft, uint16_t uw)
{
        if (ft->info.swap)
                uw = st_swapw(uw);
        if (st_writebuf(ft, &uw, 2, 1) != 1)
        {
                st_fail_errno(ft,errno,writerr);
                return (ST_EOF);
        }
        return(ST_SUCCESS);
}

/* Read three bytes. */
int st_read3(ft_t ft, uint24_t *u3)
{
        if (st_readbuf(ft, u3, 3, 1) != 1)
        {
            st_fail_errno(ft,errno,readerr);
            return (ST_EOF);
        }
        if (ft->info.swap)
                *u3 = st_swap24(*u3);
        return ST_SUCCESS;
}

/* Write three bytes. */
int st_write3(ft_t ft, uint24_t u3)
{
        if (ft->info.swap)
                u3 = st_swap24(u3);
        if (st_writebuf(ft, &u3, 2, 1) != 1)
        {
                st_fail_errno(ft,errno,writerr);
                return (ST_EOF);
        }
        return(ST_SUCCESS);
}

/* Read double word. */
int st_readdw(ft_t ft, uint32_t *udw)
{
        if (st_readbuf(ft, udw, 4, 1) != 1)
        {
            st_fail_errno(ft,errno,readerr);
            return (ST_EOF);
        }
        if (ft->info.swap)
                *udw = st_swapdw(*udw);
        return ST_SUCCESS;
}

/* Write double word. */
int st_writedw(ft_t ft, uint32_t udw)
{
        if (ft->info.swap)
                udw = st_swapdw(udw);
        if (st_writebuf(ft, &udw, 4, 1) != 1)
        {
                st_fail_errno(ft,errno,writerr);
                return (ST_EOF);
        }
        return(ST_SUCCESS);
}

/* Read float. */
int st_readf(ft_t ft, float *f)
{
        if (st_readbuf(ft, f, sizeof(float), 1) != 1)
        {
            st_fail_errno(ft,errno,readerr);
            return(ST_EOF);
        }
        if (ft->info.swap)
                *f = st_swapf(*f);
        return ST_SUCCESS;
}

/* Write float. */
int st_writef(ft_t ft, float f)
{
        float t = f;

        if (ft->info.swap)
                t = st_swapf(t);
        if (st_writebuf(ft, &t, sizeof(float), 1) != 1)
        {
                st_fail_errno(ft,errno,writerr);
                return (ST_EOF);
        }
        return (ST_SUCCESS);
}

/* Read double. */
int st_readdf(ft_t ft, double *d)
{
        if (st_readbuf(ft, d, sizeof(double), 1) != 1)
        {
            st_fail_errno(ft,errno,readerr);
            return(ST_EOF);
        }
        if (ft->info.swap)
                *d = st_swapd(*d);
        return ST_SUCCESS;
}

/* Write double. */
int st_writedf(ft_t ft, double d)
{
        if (ft->info.swap)
                d = st_swapd(d);
        if (st_writebuf(ft, &d, sizeof(double), 1) != 1)
        {
                st_fail_errno(ft,errno,writerr);
                return (ST_EOF);
        }
        return (ST_SUCCESS);
}


uint32_t get32_le(unsigned char **p)
{
        uint32_t val = (((*p)[3]) << 24) | (((*p)[2]) << 16) | 
                (((*p)[1]) << 8) | (**p);
        (*p) += 4;
        return val;
}

uint16_t get16_le(unsigned char **p)
{
        unsigned val = (((*p)[1]) << 8) | (**p);
        (*p) += 2;
        return val;
}

void put32_le(unsigned char **p, uint32_t val)
{
        *(*p)++ = val & 0xff;
        *(*p)++ = (val >> 8) & 0xff;
        *(*p)++ = (val >> 16) & 0xff;
        *(*p)++ = (val >> 24) & 0xff;
}

void put16_le(unsigned char **p, int16_t val)
{
        *(*p)++ = val & 0xff;
        *(*p)++ = (val >> 8) & 0xff;
}

void put32_be(unsigned char **p, int32_t val)
{
  *(*p)++ = (val >> 24) & 0xff;
  *(*p)++ = (val >> 16) & 0xff;
  *(*p)++ = (val >> 8) & 0xff;
  *(*p)++ = val & 0xff;
}

void put16_be(unsigned char **p, short val)
{
  *(*p)++ = (val >> 8) & 0xff;
  *(*p)++ = val & 0xff;
}

/* generic swap routine. Swap l and place in to f (datatype length = n) */
static void st_swapb(char *l, char *f, int n)
{
    register int i;

    for (i= 0; i< n; i++)
        f[i]= l[n-i-1];
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

uint32_t st_swap24(uint24_t udw)
{
    return ((udw >> 16) & 0xff) | (udw & 0xff00) | ((udw << 16) & 0xff0000L);
}

double st_swapd(double df)
{
    double sdf;
    st_swapb((char *)&df, (char *)&sdf, sizeof(double));
    return (sdf);
}


/* dummy format routines for do-nothing functions */
int st_format_nothing(ft_t ft UNUSED) { return(ST_SUCCESS); }
st_size_t st_format_nothing_read_io(ft_t ft UNUSED, st_sample_t *buf UNUSED, st_size_t len UNUSED) { return(0); }
st_size_t st_format_nothing_write_io(ft_t ft UNUSED, const st_sample_t *buf UNUSED, st_size_t len UNUSED) { return(0); }
int st_format_nothing_seek(ft_t ft UNUSED, st_size_t offset UNUSED) { st_fail_errno(ft, ST_ENOTSUP, "operation not supported"); return(ST_EOF); }

/* dummy effect routine for do-nothing functions */
int st_effect_nothing(eff_t effp UNUSED) { return(ST_SUCCESS); }
int st_effect_nothing_drain(eff_t effp UNUSED, st_sample_t *obuf UNUSED, st_size_t *osamp)
  { /* Inform no more samples to drain */ *osamp = 0; return(ST_EOF); }

int st_effect_nothing_getopts(eff_t effp, int n, char **argv UNUSED)
{
     if (n) {
          st_fail(effp->h->usage);
          return (ST_EOF);
     }
     return (ST_SUCCESS);
}


/* here for linear interp.  might be useful for other things */
REGPARM(2) st_sample_t st_gcd(st_sample_t a, st_sample_t b)
{
        if (b == 0)
                return a;
        else
                return st_gcd(b, a % b);
}

REGPARM(2) st_sample_t st_lcm(st_sample_t a, st_sample_t b)
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
  while (*s1 && (toupper(*s1) == toupper(*s2)))
    s1++, s2++;
  return toupper(*s1) - toupper(*s2);
}

int strncasecmp(char const *s1, char const * s2, size_t n)
{
  while (n-- && *s1 && (toupper(*s1) == toupper(*s2)))
    s1++, s2++;
  return toupper(*s1) - toupper(*s2);
}
#endif

#ifndef HAVE_STRDUP
/*
 * Portable strdup() function
 */
char *strdup(const char *s)
{
    return strcpy((char *)xmalloc(strlen(s) + 1), s);
}
#endif

/* Util to set initial seed so that we are a little less non-random */
void st_initrand(void) {
    time_t t;

    time(&t);
    srand(t);
}



void st_generate_wave_table(
    st_wave_t wave_type,
    st_data_t data_type,
    void *table,
    uint32_t table_size,
    double min,
    double max,
    double phase)
{
  uint32_t t;
  uint32_t phase_offset = phase / M_PI / 2 * table_size + 0.5;

  for (t = 0; t < table_size; t++)
  {
    uint32_t point = (t + phase_offset) % table_size;
    double d;
    switch (wave_type)
    {
      case ST_WAVE_SINE:
      d = (sin((double)point / table_size * 2 * M_PI) + 1) / 2;
      break;

      case ST_WAVE_TRIANGLE:
      d = (double)point * 2 / table_size;
      switch (4 * point / table_size)
      {
        case 0:         d = d + 0.5; break;
        case 1: case 2: d = 1.5 - d; break;
        case 3:         d = d - 1.5; break;
      }
      break;

      default: /* Oops! FIXME */
        d = 0.0; /* Make sure we have a value */
      break;
    }
    d  = d * (max - min) + min;
    switch (data_type)
    {
      case ST_FLOAT:
        {
          float *fp = (float *)table;
          *fp++ = (float)d;
          table = fp;
          continue;
        }
      case ST_DOUBLE:
        {
          double *dp = (double *)table;
          *dp++ = d;
          table = dp;
          continue;
        }
      default: break;
    }
    d += d < 0? -0.5 : +0.5;
    switch (data_type)
    {
      case ST_SHORT:
        {
          short *sp = table;
          *sp++ = (short)d;
          table = sp;
          continue;
        }
      case ST_INT:
        {
          int *ip = table;
          *ip++ = (int)d;
          table = ip;
          continue;
        }
      default: break;
    }
  }
}



const char *st_version(void)
{
  return PACKAGE_VERSION;
}

/* Implements traditional fseek() behavior.  Meant to abstract out
 * file operations so that they could one day also work on memory
 * buffers.
 *
 * N.B. Can only seek forwards!
 */
int st_seeki(ft_t ft, st_size_t offset, int whence)
{
    if (ft->seekable == 0) {
        /* If a stream peel off chars else EPERM */
        if (whence == SEEK_CUR) {
            while (offset > 0 && !feof(ft->fp)) {
                getc(ft->fp);
                offset--;
            }
            if (offset)
                st_fail_errno(ft,ST_EOF, "offset past EOF");
            else
                ft->st_errno = ST_SUCCESS;
        } else
            st_fail_errno(ft,ST_EPERM, "file not seekable");
    } else {
        if (fseeko(ft->fp, offset, whence) == -1)
            st_fail_errno(ft,errno,strerror(errno));
        else
            ft->st_errno = ST_SUCCESS;
    }

    /* Empty the st file buffer */
    if (ft->st_errno == ST_SUCCESS)
        ft->eof = 0;

    return ft->st_errno;
}

enum_item const * find_enum_text(char const * text, enum_item const * enum_items)
{
  enum_item const * result = NULL; /* Assume not found */

  while (enum_items->text)
  {
    if (strncasecmp(text, enum_items->text, strlen(text)) == 0)
    {
      if (result != NULL && result->value != enum_items->value)
        return NULL;        /* Found ambiguity */
      result = enum_items;  /* Found match */
    }
    ++enum_items;
  }
  return result;
}

enum_item const st_wave_enum[] = {
  ENUM_ITEM(ST_WAVE_,SINE)
  ENUM_ITEM(ST_WAVE_,TRIANGLE)
  {0, 0}};

