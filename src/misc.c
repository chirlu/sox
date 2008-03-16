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

const char * const sox_encodings_str[] = {
  "?",
  "Signed Integer PCM",
  "Unsigned Integer PCM",
  "Floating Point PCM",
  "Floating Point (text) PCM",
  "FLAC",
  "HCOM",
  "", /* Lossless above, lossy below */
  "u-law",
  "A-law",
  "G.721 ADPCM",
  "G.723 ADPCM",
  "CL ADPCM (from 8-bit)",
  "CL ADPCM (from 16-bit)",
  "MS ADPCM",
  "IMA ADPCM",
  "OKI ADPCM",
  "GSM",
  "MPEG audio (layer I, II or III)",
  "Vorbis",
  "AMR-WB",
  "AMR-NB",
  "CVSD",
  "LPC10",
};

const char * const sox_encodings_short_str[] = {
  "n/a",
  "Signed PCM",
  "Unsigned PCM",
  "F.P. PCM",
  "F.P. PCM",
  "FLAC",
  "HCOM",
  "", /* Lossless above, lossy below */
  "u-law",
  "A-law",
  "G.721 ADPCM",
  "G.723 ADPCM",
  "CL ADPCM (8)",
  "CL ADPCM (16)",
  "MS ADPCM",
  "IMA ADPCM",
  "OKI ADPCM",
  "GSM",
  "MPEG audio",
  "Vorbis",
  "AMR-WB",
  "AMR-NB",
  "CVSD",
  "LPC10",
};

assert_static(array_length(sox_encodings_str) == SOX_ENCODINGS,
    SIZE_MISMATCH_BETWEEN_sox_encodings_t_AND_sox_encodings_str);

void sox_init_encodinginfo(sox_encodinginfo_t * e)
{
  e->reverse_bytes = SOX_OPTION_DEFAULT;
  e->reverse_nibbles = SOX_OPTION_DEFAULT;
  e->reverse_bits = SOX_OPTION_DEFAULT;
  e->compression = HUGE_VAL;
}

unsigned sox_precision(sox_encoding_t encoding, unsigned bits_per_sample)
{
  switch (encoding) {
    case SOX_ENCODING_HCOM:       return !(bits_per_sample & 7) && (bits_per_sample >> 3) - 1 < 1? bits_per_sample: 0;
    case SOX_ENCODING_FLAC:       return !(bits_per_sample & 7) && (bits_per_sample >> 3) - 1 < 3? bits_per_sample: 0;
    case SOX_ENCODING_SIGN2:      return bits_per_sample <= 32? bits_per_sample : 0;
    case SOX_ENCODING_UNSIGNED:   return !(bits_per_sample & 7) && (bits_per_sample >> 3) - 1 < 4? bits_per_sample: 0;

    case SOX_ENCODING_ALAW:       return bits_per_sample == 8? 13: 0;
    case SOX_ENCODING_ULAW:       return bits_per_sample == 8? 14: 0;

    case SOX_ENCODING_CL_ADPCM:   return bits_per_sample? 8: 0;
    case SOX_ENCODING_CL_ADPCM16: return bits_per_sample == 4? 13: 0;
    case SOX_ENCODING_MS_ADPCM:   return bits_per_sample == 4? 14: 0;
    case SOX_ENCODING_IMA_ADPCM:  return bits_per_sample == 4? 13: 0;
    case SOX_ENCODING_OKI_ADPCM:  return bits_per_sample == 4? 12: 0;
    case SOX_ENCODING_G721:       return bits_per_sample == 4? 12: 0;
    case SOX_ENCODING_G723:       return bits_per_sample == 3? 8:
                                         bits_per_sample == 5? 14: 0;
    case SOX_ENCODING_CVSD:       return bits_per_sample == 1? 16: 0;

    case SOX_ENCODING_GSM:
    case SOX_ENCODING_MP3:
    case SOX_ENCODING_VORBIS:
    case SOX_ENCODING_AMR_WB:
    case SOX_ENCODING_AMR_NB:
    case SOX_ENCODING_LPC10:      return !bits_per_sample? 16: 0;

    case SOX_ENCODING_FLOAT:      return bits_per_sample == 32 ? 24: bits_per_sample == 64 ? 53: 0;
    case SOX_ENCODING_FLOAT_TEXT: return !bits_per_sample? 53: 0;

    case SOX_ENCODINGS:
    case SOX_ENCODING_LOSSLESS:
    case SOX_ENCODING_UNKNOWN:    break;
  }
  return 0;
}

int sox_check_read_params(sox_format_t * ft, unsigned channels,
    sox_rate_t rate, sox_encoding_t encoding, unsigned bits_per_sample, off_t length)
{
  ft->length = length;

  if (ft->seekable)
    ft->data_start = sox_tell(ft);

  if (channels && ft->signal.channels && ft->signal.channels != channels)
    sox_warn("`%s': overriding number of channels", ft->filename);
  else ft->signal.channels = channels;

  if (rate && ft->signal.rate && ft->signal.rate != rate)
    sox_warn("`%s': overriding sample rate", ft->filename);
  else ft->signal.rate = rate;

  if (encoding && ft->encoding.encoding && ft->encoding.encoding != encoding)
    sox_warn("`%s': overriding encoding type", ft->filename);
  else ft->encoding.encoding = encoding;

  if (bits_per_sample && ft->encoding.bits_per_sample && ft->encoding.bits_per_sample != bits_per_sample)
    sox_warn("`%s': overriding encoding size", ft->filename);
  ft->encoding.bits_per_sample = bits_per_sample;

  if (ft->encoding.bits_per_sample && sox_filelength(ft)) {
    off_t calculated_length = div_bits(sox_filelength(ft) - ft->data_start, ft->encoding.bits_per_sample);
    if (!ft->length)
      ft->length = calculated_length;
    else if (length != calculated_length)
      sox_warn("`%s': file header gives the total number of samples as %u but file length indicates the number is in fact %u", ft->filename, (unsigned)length, (unsigned)calculated_length); /* FIXME: casts */
  }

  if (sox_precision(ft->encoding.encoding, ft->encoding.bits_per_sample))
    return SOX_SUCCESS;
  sox_fail_errno(ft, EINVAL, "invalid format for this file type");
  return SOX_EOF;
}

sox_sample_t sox_sample_max(sox_encodinginfo_t const * encoding)
{
  unsigned precision = encoding->encoding == SOX_ENCODING_FLOAT?
    SOX_SAMPLE_PRECISION : sox_precision(encoding->encoding, encoding->bits_per_sample);
  unsigned shift = SOX_SAMPLE_PRECISION - min(precision, SOX_SAMPLE_PRECISION);
  return (SOX_SAMPLE_MAX >> shift) << shift;
}

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
size_t sox_readbuf(sox_format_t * ft, void *buf, sox_size_t len)
{
  size_t ret = fread(buf, 1, len, ft->fp);
  if (ret != len && ferror(ft->fp))
    sox_fail_errno(ft, errno, "sox_readbuf");
  ft->tell += ret;
  return ret;
}

/* Skip input without seeking. */
int sox_skipbytes(sox_format_t * ft, sox_size_t n)
{
  unsigned char trash;

  while (n--)
    if (sox_readb(ft, &trash) == SOX_EOF)
      return (SOX_EOF);
  
  return (SOX_SUCCESS);
}

/* Pad output. */
int sox_padbytes(sox_format_t * ft, sox_size_t n)
{
  while (n--)
    if (sox_writeb(ft, '\0') == SOX_EOF)
      return (SOX_EOF);

  return (SOX_SUCCESS);
}

/* Write a buffer of data of length bytes.
 * Returns number of bytes written.
 */

size_t sox_writebuf(sox_format_t * ft, void const * buf, sox_size_t len)
{
  size_t ret = fwrite(buf, 1, len, ft->fp);
  if (ret != len) {
    sox_fail_errno(ft, errno, "error writing output file");
    clearerr(ft->fp); /* Allows us to seek back to write header */
  }
  ft->tell += ret;
  return ret;
}

sox_size_t sox_filelength(sox_format_t * ft)
{
  struct stat st;
  int ret = fstat(fileno(ft->fp), &st);

  return ret? 0 : (sox_size_t)st.st_size;
}

int sox_flush(sox_format_t * ft)
{
  return fflush(ft->fp);
}

sox_ssize_t sox_tell(sox_format_t * ft)
{
  return ft->seekable? (sox_ssize_t)ftello(ft->fp) : ft->tell;
}

int sox_eof(sox_format_t * ft)
{
  return feof(ft->fp);
}

int sox_error(sox_format_t * ft)
{
  return ferror(ft->fp);
}

void sox_rewind(sox_format_t * ft)
{
  rewind(ft->fp);
}

void sox_clearerr(sox_format_t * ft)
{
  clearerr(ft->fp);
}

/* Read and write known datatypes in "machine format".  Swap if indicated.
 * They all return SOX_EOF on error and SOX_SUCCESS on success.
 */
/* Read n-char string (and possibly null-terminating).
 * Stop reading and null-terminate string if either a 0 or \n is reached.
 */
int sox_reads(sox_format_t * ft, char *c, sox_size_t len)
{
    char *sc;
    char in;

    sc = c;
    do
    {
        if (sox_readbuf(ft, &in, 1) != 1)
        {
            *sc = 0;
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
int sox_writes(sox_format_t * ft, char const * c)
{
        if (sox_writebuf(ft, c, strlen(c)) != strlen(c))
                return(SOX_EOF);
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
void sox_swapf(float * f)
{
    union {
        uint32_t dw;
        float f;
    } u;

    u.f= *f;
    u.dw= (u.dw>>24) | ((u.dw>>8)&0xff00) | ((u.dw<<8)&0xff0000) | (u.dw<<24);
    *f = u.f;
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

/* here for linear interp.  might be useful for other things */
sox_sample_t sox_gcd(sox_sample_t a, sox_sample_t b)
{
  if (b == 0)
    return a;
  else
    return sox_gcd(b, a % b);
}

sox_sample_t sox_lcm(sox_sample_t a, sox_sample_t b)
{
  /* parenthesize this way to avoid sox_sample_t overflow in product term */
  return a * (b / sox_gcd(a, b));
}

char const * find_file_extension(char const * pathname)
{
  /* First, chop off any path portions of filename.  This
   * prevents the next search from considering that part. */
  char const * result = LAST_SLASH(pathname);
  if (!result)
    result = pathname;

  /* Now look for an filename extension */
  result = strrchr(result, '.');
  if (result)
    ++result;
  return result;
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

sox_bool strends(char const * str, char const * end)
{
  size_t str_len = strlen(str), end_len = strlen(end);
  return str_len >= end_len && !strcmp(str + str_len - end_len, end);
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
int sox_seeki(sox_format_t * ft, sox_ssize_t offset, int whence)
{
    if (ft->seekable == 0) {
        /* If a stream peel off chars else EPERM */
        if (whence == SEEK_CUR) {
            while (offset > 0 && !feof(ft->fp)) {
                getc(ft->fp);
                offset--;
                ++ft->tell;
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

int sox_offset_seek(sox_format_t * ft, off_t byte_offset, sox_size_t to_sample)
{
  double wide_sample = to_sample - (to_sample % ft->signal.channels);
  double to_d = wide_sample * ft->encoding.bits_per_sample / 8;
  off_t to = to_d;
  return (to != to_d)? SOX_EOF : sox_seeki(ft, byte_offset + to, SEEK_SET);
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

enum_item const * find_enum_value(unsigned value, enum_item const * enum_items)
{
  for (;enum_items->text; ++enum_items)
    if (value == enum_items->value)
      return enum_items;
  return NULL;
}

enum_item const sox_wave_enum[] = {
  ENUM_ITEM(SOX_WAVE_,SINE)
  ENUM_ITEM(SOX_WAVE_,TRIANGLE)
  {0, 0}};

sox_bool is_uri(char const * text)
{
  if (!isalpha((int)*text))
    return sox_false;
  ++text;
  do {
    if (!isalnum((int)*text) && !strchr("+-.", *text))
      return sox_false;
    ++text;
  } while (*text && *text != ':');
  return *text == ':';
}

FILE * xfopen(char const * identifier, char const * mode) 
{ 
  if (is_uri(identifier)) {
    FILE * f = NULL;
#ifdef HAVE_POPEN
    char const * const command_format = "wget --no-check-certificate -q -O- \"%s\"";
    char * command = xmalloc(strlen(command_format) + strlen(identifier)); 
    sprintf(command, command_format, identifier); 
    f = popen(command, "r"); 
    free(command);
#endif 
    return f;
  }
  return fopen(identifier, mode);
} 
