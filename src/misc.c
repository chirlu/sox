/*
 * libSoX miscellaneous file-handling functions.
 */

/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
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

const char * const sox_sizes_str[] = {
        "NONSENSE!",
        "1 byte",
        "2 bytes",
        "3 bytes",
        "4 bytes",
        "NONSENSE",
        "NONSENSE",
        "NONSENSE",
        "8 bytes"
};

const char * const sox_size_bits_str[] = {
        "NONSENSE!",
        "8-bit",
        "16-bit",
        "24-bit",
        "32-bit",
        "NONSENSE",
        "NONSENSE",
        "NONSENSE",
        "64-bit"
};

const char * const sox_encodings_str[] = {
        "NONSENSE!",

        "u-law",
        "A-law",
        "G72x-ADPCM",
        "MS-ADPCM",
        "IMA-ADPCM",
        "OKI-ADPCM",

        "",   /* FIXME, see sox.h */

        "unsigned",
        "signed (2's complement)",
        "floating point",
        "GSM",
        "MPEG audio (layer I, II or III)",
        "Vorbis",
        "FLAC",
        "AMR-WB",
        "AMR-NB",
};

assert_static(array_length(sox_encodings_str) == SOX_ENCODINGS,
    SIZE_MISMATCH_BETWEEN_sox_encodings_t_AND_sox_encodings_str);

const char sox_readerr[] = "Premature EOF while reading sample file.";
const char sox_writerr[] = "Error writing sample file.  You are probably out of disk space.";

/* Lookup table to reverse the bit order of a byte. ie MSB become LSB */
uint8_t const cswap[256] = {
  0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0,
  0x30, 0xB0, 0x70, 0xF0, 0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 
  0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 0x04, 0x84, 0x44, 0xC4, 
  0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
  0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC,
  0x3C, 0xBC, 0x7C, 0xFC, 0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 
  0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 0x0A, 0x8A, 0x4A, 0xCA, 
  0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
  0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6,
  0x36, 0xB6, 0x76, 0xF6, 0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 
  0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE, 0x01, 0x81, 0x41, 0xC1, 
  0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
  0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9,
  0x39, 0xB9, 0x79, 0xF9, 0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 
  0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5, 0x0D, 0x8D, 0x4D, 0xCD, 
  0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3,
  0x33, 0xB3, 0x73, 0xF3, 0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 
  0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB, 0x07, 0x87, 0x47, 0xC7, 
  0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
  0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF,
  0x3F, 0xBF, 0x7F, 0xFF
};

/* Utilities */

/* Read in a buffer of data of length len bytes.
 * Returns number of bytes read.
 */
size_t sox_readbuf(ft_t ft, void *buf, sox_size_t len)
{
    return fread(buf, 1, len, ft->fp);
}

/* Skip input without seeking. */
int sox_skipbytes(ft_t ft, sox_size_t n)
{
  unsigned char trash;

  while (n--)
    if (sox_readb(ft, &trash) == SOX_EOF)
      return (SOX_EOF);
  
  return (SOX_SUCCESS);
}

/* Pad output. */
int sox_padbytes(ft_t ft, sox_size_t n)
{
  while (n--)
    if (sox_writeb(ft, '\0') == SOX_EOF)
      return (SOX_EOF);

  return (SOX_SUCCESS);
}

/* Write a buffer of data of length bytes.
 * Returns number of bytes written.
 */

size_t sox_writebuf(ft_t ft, void const *buf, sox_size_t len)
{
    return fwrite(buf, 1, len, ft->fp);
}

sox_size_t sox_filelength(ft_t ft)
{
  struct stat st;

  fstat(fileno(ft->fp), &st);

  return (sox_size_t)st.st_size;
}

int sox_flush(ft_t ft)
{
  return fflush(ft->fp);
}

sox_ssize_t sox_tell(ft_t ft)
{
  return (sox_ssize_t)ftello(ft->fp);
}

int sox_eof(ft_t ft)
{
  return feof(ft->fp);
}

int sox_error(ft_t ft)
{
  return ferror(ft->fp);
}

void sox_rewind(ft_t ft)
{
  rewind(ft->fp);
}

void sox_clearerr(ft_t ft)
{
  clearerr(ft->fp);
}

/* Read and write known datatypes in "machine format".  Swap if indicated.
 * They all return SOX_EOF on error and SOX_SUCCESS on success.
 */
/* Read n-char string (and possibly null-terminating).
 * Stop reading and null-terminate string if either a 0 or \n is reached.
 */
int sox_reads(ft_t ft, char *c, sox_size_t len)
{
    char *sc;
    char in;

    sc = c;
    do
    {
        if (sox_readbuf(ft, &in, 1) != 1)
        {
            *sc = 0;
                sox_fail_errno(ft,errno,sox_readerr);
            return (SOX_EOF);
        }
        if (in == 0 || in == '\n')
            break;

        *sc = in;
        sc++;
    } while (sc - c < (ptrdiff_t)len);
    *sc = 0;
    return(SOX_SUCCESS);
}

/* Write null-terminated string (without \0). */
int sox_writes(ft_t ft, char const * c)
{
        if (sox_writebuf(ft, c, strlen(c)) != strlen(c))
        {
                sox_fail_errno(ft,errno,sox_writerr);
                return(SOX_EOF);
        }
        return(SOX_SUCCESS);
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

void put16_le(unsigned char **p, unsigned val)
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

void put16_be(unsigned char **p, int val)
{
  *(*p)++ = (val >> 8) & 0xff;
  *(*p)++ = val & 0xff;
}

/* generic swap routine. Swap l and place in to f (datatype length = n) */
static void sox_swap(char *l, char *f, int n)
{
    register int i;

    for (i= 0; i< n; i++)
        f[i]= l[n-i-1];
}

/* return swapped 32-bit float */
float sox_swapf(float f)
{
    union {
        uint32_t dw;
        float f;
    } u;

    u.f= f;
    u.dw= (u.dw>>24) | ((u.dw>>8)&0xff00) | ((u.dw<<8)&0xff0000) | (u.dw<<24);
    return u.f;
}

uint32_t sox_swap3(uint24_t udw)
{
    return ((udw >> 16) & 0xff) | (udw & 0xff00) | ((udw << 16) & 0xff0000);
}

double sox_swapdf(double df)
{
    double sdf;
    sox_swap((char *)&df, (char *)&sdf, sizeof(double));
    return (sdf);
}


/* dummy format routines for do-nothing functions */
int sox_format_nothing(ft_t ft UNUSED) { return(SOX_SUCCESS); }
sox_size_t sox_format_nothing_read(ft_t ft UNUSED, sox_ssample_t *buf UNUSED, sox_size_t len UNUSED) { return(0); }
sox_size_t sox_format_nothing_write(ft_t ft UNUSED, const sox_ssample_t *buf UNUSED, sox_size_t len UNUSED) { return(0); }
int sox_format_nothing_seek(ft_t ft UNUSED, sox_size_t offset UNUSED) { sox_fail_errno(ft, SOX_ENOTSUP, "operation not supported"); return(SOX_EOF); }

/* dummy effect routine for do-nothing functions */
int sox_effect_nothing(eff_t effp UNUSED)
{
  return SOX_SUCCESS;
}

int sox_effect_nothing_flow(eff_t effp UNUSED, const sox_ssample_t *ibuf UNUSED, sox_ssample_t *obuf UNUSED, sox_size_t *isamp, sox_size_t *osamp)
{
  /* Pass through samples verbatim */
  *isamp = *osamp = min(*isamp, *osamp);
  memcpy(obuf, ibuf, *isamp * sizeof(sox_ssample_t));
  return SOX_SUCCESS;
}

int sox_effect_nothing_drain(eff_t effp UNUSED, sox_ssample_t *obuf UNUSED, sox_size_t *osamp)
{
  /* Inform no more samples to drain */
  *osamp = 0;
  return SOX_EOF;
}

int sox_effect_nothing_getopts(eff_t effp, int n, char **argv UNUSED)
{
  if (n) {
    sox_fail(effp->h->usage);
    return (SOX_EOF);
  }
  return (SOX_SUCCESS);
}


/* here for linear interp.  might be useful for other things */
sox_ssample_t sox_gcd(sox_ssample_t a, sox_ssample_t b)
{
  if (b == 0)
    return a;
  else
    return sox_gcd(b, a % b);
}

sox_ssample_t sox_lcm(sox_ssample_t a, sox_ssample_t b)
{
  /* parenthesize this way to avoid sox_sample_t overflow in product term */
  return a * (b / sox_gcd(a, b));
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
  while (--n && *s1 && (toupper(*s1) == toupper(*s2)))
    s1++, s2++;
  return toupper(*s1) - toupper(*s2);
}
#endif

sox_bool strcaseends(char const * str, char const * end)
{
  size_t str_len = strlen(str), end_len = strlen(end);
  return str_len >= end_len && !strcasecmp(str + str_len - end_len, end);
}

#ifndef HAVE_STRDUP
/*
 * Portable strdup() function
 */
char *strdup(const char *s)
{
    return strcpy((char *)xmalloc(strlen(s) + 1), s);
}
#endif



void sox_generate_wave_table(
    sox_wave_t wave_type,
    sox_data_t data_type,
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
      case SOX_WAVE_SINE:
      d = (sin((double)point / table_size * 2 * M_PI) + 1) / 2;
      break;

      case SOX_WAVE_TRIANGLE:
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
      case SOX_FLOAT:
        {
          float *fp = (float *)table;
          *fp++ = (float)d;
          table = fp;
          continue;
        }
      case SOX_DOUBLE:
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
      case SOX_SHORT:
        {
          short *sp = table;
          *sp++ = (short)d;
          table = sp;
          continue;
        }
      case SOX_INT:
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



const char *sox_version(void)
{
    static char versionstr[20];

    sprintf(versionstr, "%d.%d.%d",
            (SOX_LIB_VERSION_CODE & 0xff0000) >> 16,
            (SOX_LIB_VERSION_CODE & 0x00ff00) >> 8,
            (SOX_LIB_VERSION_CODE & 0x0000ff));
    return(versionstr);
}

/* Implements traditional fseek() behavior.  Meant to abstract out
 * file operations so that they could one day also work on memory
 * buffers.
 *
 * N.B. Can only seek forwards on non-seekable streams!
 */
int sox_seeki(ft_t ft, sox_ssize_t offset, int whence)
{
    if (ft->seekable == 0) {
        /* If a stream peel off chars else EPERM */
        if (whence == SEEK_CUR) {
            while (offset > 0 && !feof(ft->fp)) {
                getc(ft->fp);
                offset--;
            }
            if (offset)
                sox_fail_errno(ft,SOX_EOF, "offset past EOF");
            else
                ft->sox_errno = SOX_SUCCESS;
        } else
            sox_fail_errno(ft,SOX_EPERM, "file not seekable");
    } else {
        if (fseeko(ft->fp, (off_t)offset, whence) == -1)
            sox_fail_errno(ft,errno,strerror(errno));
        else
            ft->sox_errno = SOX_SUCCESS;
    }
    return ft->sox_errno;
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

enum_item const sox_wave_enum[] = {
  ENUM_ITEM(SOX_WAVE_,SINE)
  ENUM_ITEM(SOX_WAVE_,TRIANGLE)
  {0, 0}};

sox_bool is_uri(char const * text)
{
  if (!isalpha(*text))
    return sox_false;
  ++text;
  do {
    if (!isalnum(*text) && !strchr("+-.", *text))
      return sox_false;
    ++text;
  } while (*text && *text != ':');
  return *text == ':';
}

FILE * xfopen(char const * identifier, char const * mode) 
{ 
  if (is_uri(identifier)) {
    FILE * f; 
    char const * const command_format = "wget -q -O- \"%s\"";
    char * command = xmalloc(strlen(command_format) + strlen(identifier)); 
    sprintf(command, command_format, identifier); 
    f = popen(command, "r"); 
    free(command); 
    return f;
  }
  return fopen(identifier, mode);
} 

/* PRC detection code is here rather than in prc.c because the
   latter is a plug-in, and prc_checkheader is called from auto.c too */

/* File header. The first 4 words are fixed; the rest of the header
   could theoretically be different, and this is the first place to
   check with apparently invalid files.

   N.B. All offsets are from start of file. */
const char prc_header[41] = {
  /* Header section */
  '\x37','\x00','\x00','\x10', /* 0x00: File type (UID 1) */
  '\x6d','\x00','\x00','\x10', /* 0x04: File kind (UID 2) */
  '\x7e','\x00','\x00','\x10', /* 0x08: Application ID (UID 3) */
  '\xcf','\xac','\x08','\x55', /* 0x0c: Checksum of UIDs 1-3 */
  '\x14','\x00','\x00','\x00', /* 0x10: File offset of Section Table Section */
  /* Section Table Section: a BListL, i.e. a list of longs preceded by
     length byte.
     The longs are in (ID, offset) pairs, each pair identifying a
     section. */
  '\x04',                      /* 0x14: List has 4 bytes, i.e. 2 pairs */
  '\x52','\x00','\x00','\x10', /* 0x15: ID: Record Section */
  '\x34','\x00','\x00','\x00', /* 0x19: Offset to Record Section */
  '\x89','\x00','\x00','\x10', /* 0x1d: ID: Application ID Section */
  '\x25','\x00','\x00','\x00', /* 0x21: Offset to Application ID Section */
  '\x7e','\x00','\x00','\x10', /* 0x25: Application ID Section:
                                  Record.app identifier */
  /* Next comes the string, which can be either case. */
};

/* Format of the Record Section (offset 0x34):

00 L Uncompressed data length
04 ID a1 01 00 10 for ADPCM, 00 00 00 00 for A-law
08 W number of times sound will be repeated (0 = played once)
0a B Volume setting (01-05)
0b B Always 00 (?)
0c L Time between repeats in usec
10 LListB (i.e. long giving number of bytes followed by bytes) Sound Data
*/

int prc_checkheader(ft_t ft, char *head)
{
  sox_readbuf(ft, head, sizeof(prc_header));
  return memcmp(head, prc_header, sizeof(prc_header)) == 0;
}
