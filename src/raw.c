/*
 * libSoX raw file formats
 *
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"
#include "g711.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define SOX_ULAW_BYTE_TO_SAMPLE(d,clips)   SOX_SIGNED_WORD_TO_SAMPLE(sox_ulaw2linear16(d),clips)
#define SOX_ALAW_BYTE_TO_SAMPLE(d,clips)   SOX_SIGNED_WORD_TO_SAMPLE(sox_alaw2linear16(d),clips)
#define SOX_SAMPLE_TO_ULAW_BYTE(d,c) sox_14linear2ulaw(SOX_SAMPLE_TO_SIGNED_WORD(d,c) >> 2)
#define SOX_SAMPLE_TO_ALAW_BYTE(d,c) sox_13linear2alaw(SOX_SAMPLE_TO_SIGNED_WORD(d,c) >> 3)

int sox_rawseek(ft_t ft, sox_size_t offset)
{
    sox_size_t new_offset, channel_block, alignment;

    switch(ft->signal.size) {
        case SOX_SIZE_BYTE:
        case SOX_SIZE_16BIT:
        case SOX_SIZE_24BIT:
        case SOX_SIZE_32BIT:
        case SOX_SIZE_64BIT:
            break;
        default:
            sox_fail_errno(ft,SOX_ENOTSUP,"Can't seek this data size");
            return ft->sox_errno;
    }

    new_offset = offset * ft->signal.size;
    /* Make sure request aligns to a channel block (ie left+right) */
    channel_block = ft->signal.channels * ft->signal.size;
    alignment = new_offset % channel_block;
    /* Most common mistaken is to compute something like
     * "skip everthing upto and including this sample" so
     * advance to next sample block in this case.
     */
    if (alignment != 0)
        new_offset += (channel_block - alignment);

    ft->sox_errno = sox_seeki(ft, new_offset, SEEK_SET);

    return ft->sox_errno;
}

/* Works nicely for starting read and write; sox_rawstart{read,write}
   are #defined in sox_i.h */
int sox_rawstart(ft_t ft, sox_bool default_rate, sox_bool default_channels, sox_encoding_t encoding, int size, sox_option_t rev_bits)
{
  if (default_rate && ft->signal.rate == 0) {
    sox_warn("'%s': sample rate not specified; trying 8kHz", ft->filename);
    ft->signal.rate = 8000;
  }

  if (default_channels && ft->signal.channels == 0) {
    sox_warn("'%s': # channels not specified; trying mono", ft->filename);
    ft->signal.channels = 1;
  }

  if (encoding != SOX_ENCODING_UNKNOWN) {
    if (ft->mode == 'r' &&
        ft->signal.encoding != SOX_ENCODING_UNKNOWN &&
        ft->signal.encoding != encoding)
      sox_report("'%s': Format options overriding file-type encoding", ft->filename);
    else ft->signal.encoding = encoding;
  }

  if (size != -1) {
    if (ft->mode == 'r' &&
        ft->signal.size != -1 && ft->signal.size != size)
      sox_report("'%s': Format options overriding file-type sample-size", ft->filename);
    else ft->signal.size = size;
  }

  if (rev_bits != SOX_OPTION_DEFAULT) {
    if (ft->mode == 'r' &&
        ft->signal.reverse_bits != SOX_OPTION_DEFAULT &&
        ft->signal.reverse_bits != rev_bits)
      sox_report("'%s': Format options overriding file-type bit-order", ft->filename);
    else ft->signal.reverse_bits = rev_bits;
  }

  return SOX_SUCCESS;
}

/* Dummy macros */
#define sox_swapb(d) (d)
#define idcast(x, y) (x)

#define TWIDDLE_BYTE_READ(ub, type) \
  if (ft->signal.reverse_bits) \
    ub = cswap[ub]; \
  if (ft->signal.reverse_nibbles) \
    ub = ((ub & 15) << 4) | (ub >> 4);

#define TWIDDLE_BYTE_WRITE(ub, type) \
  if (ft->signal.reverse_nibbles) \
    ub = ((ub & 15) << 4) | (ub >> 4); \
  if (ft->signal.reverse_bits) \
    ub = cswap[ub];

#define TWIDDLE_WORD(uw, type) \
  if (ft->signal.reverse_bytes ^ SOX_IS_BIGENDIAN) \
    uw = sox_swap ## type(uw);

/* N.B. This macro doesn't work for types, like 3-byte types, which
   don't fill the type used to represent them. */
#define READ_FUNC(type, size, sign, ctype, uctype, outtype, cast, suffix, twiddle) \
  sox_size_t sox_read_ ## sign ## type ## suffix( \
      ft_t ft, outtype *buf, sox_size_t len) \
  { \
    sox_size_t n = 0, nread; \
    ctype *data = xmalloc(sizeof(ctype) * len); \
    if ((nread = sox_readbuf(ft, data, len * size)) != len * size) \
      sox_fail_errno(ft, errno, sox_readerr); \
    nread /= size; \
    for (; n < nread; n++) { \
      twiddle(((uctype *)data)[n], type); \
      *buf++ = cast(data[n], ft->clips); \
    } \
    free(data); \
    return nread; \
  }

/* This (slower) macro works for 3-byte types. */
#define READ_FUNC2(type, size, sign, ctype, uctype, outtype, cast, suffix, twiddle) \
  sox_size_t sox_read_ ## sign ## type ## suffix( \
      ft_t ft, outtype *buf, sox_size_t len) \
  { \
    sox_size_t n; \
    for (n = 0; n < len; n++) { \
      ctype datum = 0; \
      if (sox_readbuf(ft, &datum, size) != size) { \
        sox_fail_errno(ft, errno, sox_readerr); \
        break; \
      } \
      twiddle(datum, type); \
      *buf++ = cast(datum, ft->clips); \
    } \
    return n; \
  }

READ_FUNC(b, 1, u, uint8_t, uint8_t, uint8_t, idcast, _buf, TWIDDLE_BYTE_READ)
READ_FUNC(b, 1, s, int8_t, uint8_t, int8_t, idcast, _buf, TWIDDLE_BYTE_READ)
READ_FUNC(b, 1, ulaw, uint8_t, uint8_t, uint8_t, idcast, _buf, TWIDDLE_BYTE_READ)
READ_FUNC(b, 1, alaw, uint8_t, uint8_t, uint8_t, idcast, _buf, TWIDDLE_BYTE_READ)
READ_FUNC(w, 2, u, uint16_t, uint16_t, uint16_t, idcast, _buf, TWIDDLE_WORD)
READ_FUNC(w, 2, s, int16_t, uint16_t, int16_t, idcast, _buf, TWIDDLE_WORD)
READ_FUNC2(3, 3, u, uint24_t, uint24_t, uint24_t, idcast, _buf, TWIDDLE_WORD)
READ_FUNC2(3, 3, s, int24_t, uint24_t, int24_t, idcast, _buf, TWIDDLE_WORD)
READ_FUNC(dw, 4, u, uint32_t, uint32_t, uint32_t, idcast, _buf, TWIDDLE_WORD)
READ_FUNC(dw, 4, s, int32_t, uint32_t, int32_t, idcast, _buf, TWIDDLE_WORD)
READ_FUNC(f, sizeof(float), su, float, float, float, idcast, _buf, TWIDDLE_WORD)
READ_FUNC(df, sizeof(double), su, double, double, double, idcast, _buf, TWIDDLE_WORD)

static READ_FUNC(b, 1, u, uint8_t, uint8_t, sox_sample_t, SOX_UNSIGNED_BYTE_TO_SAMPLE, _samples, TWIDDLE_BYTE_READ)
static READ_FUNC(b, 1, s, int8_t, uint8_t, sox_sample_t, SOX_SIGNED_BYTE_TO_SAMPLE, _samples, TWIDDLE_BYTE_READ)
static READ_FUNC(b, 1, ulaw, uint8_t, uint8_t, sox_sample_t, SOX_ULAW_BYTE_TO_SAMPLE, _samples, TWIDDLE_BYTE_READ)
static READ_FUNC(b, 1, alaw, uint8_t, uint8_t, sox_sample_t, SOX_ALAW_BYTE_TO_SAMPLE, _samples, TWIDDLE_BYTE_READ)
static READ_FUNC(w, 2, u, uint16_t, uint16_t, sox_sample_t, SOX_UNSIGNED_WORD_TO_SAMPLE, _samples, TWIDDLE_WORD)
static READ_FUNC(w, 2, s, int16_t, uint16_t, sox_sample_t, SOX_SIGNED_WORD_TO_SAMPLE, _samples, TWIDDLE_WORD)
static READ_FUNC2(3, 3, u, uint24_t, uint24_t, sox_sample_t, SOX_UNSIGNED_24BIT_TO_SAMPLE, _samples, TWIDDLE_WORD)
static READ_FUNC2(3, 3, s, int24_t, uint24_t, sox_sample_t, SOX_SIGNED_24BIT_TO_SAMPLE, _samples, TWIDDLE_WORD)
static READ_FUNC(dw, 4, u, uint32_t, uint32_t, sox_sample_t, SOX_UNSIGNED_DWORD_TO_SAMPLE, _samples, TWIDDLE_WORD)
static READ_FUNC(dw, 4, s, int32_t, uint32_t, sox_sample_t, SOX_SIGNED_DWORD_TO_SAMPLE, _samples, TWIDDLE_WORD)
static READ_FUNC(f, sizeof(float), su, float, float, sox_sample_t, SOX_FLOAT_DWORD_TO_SAMPLE, _samples, TWIDDLE_WORD)
static READ_FUNC(df, sizeof(double), su, double, double, sox_sample_t, SOX_FLOAT_DDWORD_TO_SAMPLE, _samples, TWIDDLE_WORD)

/* N.B. This macro doesn't work for types, like 3-byte types, which
   don't fill the type used to represent them. */
#define WRITE_FUNC(type, size, sign, ctype, uctype, outtype, cast, suffix, twiddle) \
  sox_size_t sox_write_ ## sign ## type ## suffix( \
      ft_t ft, outtype *buf, sox_size_t len) \
  { \
    sox_size_t n = 0, nwritten; \
    ctype *data = xmalloc(sizeof(ctype) * len); \
    for (; n < len; n++) { \
      data[n] = cast(*buf++, ft->clips); \
      twiddle(((uctype *)data)[n], type); \
    } \
    if ((nwritten = sox_writebuf(ft, data, len * size)) != len * size) \
      sox_fail_errno(ft, errno, sox_readerr); \
    free(data); \
    return nwritten / size; \
  }

/* This (slower) macro works for 3-byte types. */
#define WRITE_FUNC2(type, size, sign, ctype, uctype, intype, cast, suffix, twiddle) \
  sox_size_t sox_write_ ## sign ## type ## suffix( \
      ft_t ft, intype *buf, sox_size_t len) \
  { \
    sox_size_t n; \
    for (n = 0; n < len; n++) { \
      ctype datum = cast(*buf++, ft->clips); \
      twiddle(datum, type); \
      if (sox_writebuf(ft, &datum, size) != size) { \
        sox_fail_errno(ft, errno, sox_readerr); \
        break; \
      } \
    } \
    return n; \
  }

WRITE_FUNC(b, 1, u, uint8_t, uint8_t, uint8_t, idcast, _buf, TWIDDLE_BYTE_WRITE)
WRITE_FUNC(b, 1, s, int8_t, uint8_t, int8_t, idcast, _buf, TWIDDLE_BYTE_WRITE)
WRITE_FUNC(b, 1, ulaw, uint8_t, uint8_t, uint8_t, idcast, _buf, TWIDDLE_BYTE_WRITE)
WRITE_FUNC(b, 1, alaw, uint8_t, uint8_t, uint8_t, idcast, _buf, TWIDDLE_BYTE_WRITE)
WRITE_FUNC(w, 2, u, uint16_t, uint16_t, uint16_t, idcast, _buf, TWIDDLE_WORD)
WRITE_FUNC(w, 2, s, int16_t, uint16_t, int16_t, idcast, _buf, TWIDDLE_WORD)
WRITE_FUNC2(3, 3, u, uint24_t, uint24_t, uint24_t, idcast, _buf, TWIDDLE_WORD)
WRITE_FUNC2(3, 3, s, int24_t, uint24_t, int24_t, idcast, _buf, TWIDDLE_WORD)
WRITE_FUNC(dw, 4, u, uint32_t, uint32_t, uint32_t, idcast, _buf, TWIDDLE_WORD)
WRITE_FUNC(dw, 4, s, int32_t, uint32_t, int32_t, idcast, _buf, TWIDDLE_WORD)
WRITE_FUNC(f, sizeof(float), su, float, float, float, idcast, _buf, TWIDDLE_WORD)
WRITE_FUNC(df, sizeof(double), su, double, double, double, idcast, _buf, TWIDDLE_WORD)

static WRITE_FUNC(b, 1, u, uint8_t, uint8_t, sox_sample_t, SOX_SAMPLE_TO_UNSIGNED_BYTE, _samples, TWIDDLE_BYTE_WRITE)
static WRITE_FUNC(b, 1, s, int8_t, uint8_t, sox_sample_t, SOX_SAMPLE_TO_SIGNED_BYTE, _samples, TWIDDLE_BYTE_WRITE)
static WRITE_FUNC(b, 1, ulaw, uint8_t, uint8_t, sox_sample_t, SOX_SAMPLE_TO_ULAW_BYTE, _samples, TWIDDLE_BYTE_WRITE)
static WRITE_FUNC(b, 1, alaw, uint8_t, uint8_t, sox_sample_t, SOX_SAMPLE_TO_ALAW_BYTE, _samples, TWIDDLE_BYTE_WRITE)
static WRITE_FUNC(w, 2, u, uint16_t, uint16_t, sox_sample_t, SOX_SAMPLE_TO_UNSIGNED_WORD, _samples, TWIDDLE_WORD)
static WRITE_FUNC(w, 2, s, int16_t, uint16_t, sox_sample_t, SOX_SAMPLE_TO_SIGNED_WORD, _samples, TWIDDLE_WORD)
static WRITE_FUNC2(3, 3, u, uint24_t, uint24_t, sox_sample_t, SOX_SAMPLE_TO_UNSIGNED_24BIT, _samples, TWIDDLE_WORD)
static WRITE_FUNC2(3, 3, s, int24_t, uint24_t, sox_sample_t, SOX_SAMPLE_TO_SIGNED_24BIT, _samples, TWIDDLE_WORD)
static WRITE_FUNC(dw, 4, u, uint32_t, uint32_t, sox_sample_t, SOX_SAMPLE_TO_UNSIGNED_DWORD, _samples, TWIDDLE_WORD)
static WRITE_FUNC(dw, 4, s, int32_t, uint32_t, sox_sample_t, SOX_SAMPLE_TO_SIGNED_DWORD, _samples, TWIDDLE_WORD)
static WRITE_FUNC(f, sizeof(float), su, float, float, sox_sample_t, SOX_SAMPLE_TO_FLOAT_DWORD, _samples, TWIDDLE_WORD)
static WRITE_FUNC(df, sizeof(double), su, double, double, sox_sample_t, SOX_SAMPLE_TO_FLOAT_DDWORD, _samples, TWIDDLE_WORD)

#define WRITE1_FUNC(type, sign, ctype) \
  int sox_write ## type(ft_t ft, ctype datum) \
  { \
    return sox_write_ ## sign ## type ## _buf(ft, &datum, 1) == 1 ? SOX_SUCCESS : SOX_EOF; \
  }

WRITE1_FUNC(b, u, uint8_t)
WRITE1_FUNC(w, u, uint16_t)
WRITE1_FUNC(3, u, uint24_t)
WRITE1_FUNC(dw, u, uint32_t)
WRITE1_FUNC(f, su, float)
WRITE1_FUNC(df, su, double)

typedef sox_size_t (ft_io_fun)(ft_t ft, sox_sample_t *buf, sox_size_t len);

static ft_io_fun *check_format(ft_t ft, sox_bool write)
{
    switch (ft->signal.size) {
    case SOX_SIZE_BYTE:
      switch (ft->signal.encoding) {
      case SOX_ENCODING_SIGN2:
        return write ? sox_write_sb_samples : sox_read_sb_samples;
      case SOX_ENCODING_UNSIGNED:
        return write ? sox_write_ub_samples : sox_read_ub_samples;
      case SOX_ENCODING_ULAW:
        return write ? sox_write_ulawb_samples : sox_read_ulawb_samples;
      case SOX_ENCODING_ALAW:
        return write ? sox_write_alawb_samples : sox_read_alawb_samples;
      default:
        break;
      }
      break;
      
    case SOX_SIZE_16BIT: 
      switch (ft->signal.encoding) {
      case SOX_ENCODING_SIGN2:
        return write ? sox_write_sw_samples : sox_read_sw_samples;
      case SOX_ENCODING_UNSIGNED:
        return write ? sox_write_uw_samples : sox_read_uw_samples;
      default:
        break;
      }
      break;

    case SOX_SIZE_24BIT:
      switch (ft->signal.encoding) {
      case SOX_ENCODING_SIGN2:
        return write ? sox_write_s3_samples : sox_read_s3_samples;
      case SOX_ENCODING_UNSIGNED:
        return write ? sox_write_u3_samples: sox_read_u3_samples;
      default:
        break;
      }
      break;
      
    case SOX_SIZE_32BIT:
      switch (ft->signal.encoding) {
      case SOX_ENCODING_SIGN2:
        return write ? sox_write_sdw_samples : sox_read_sdw_samples;
      case SOX_ENCODING_UNSIGNED:
        return write ? sox_write_udw_samples : sox_read_udw_samples;
      case SOX_ENCODING_FLOAT:
        return write ? sox_write_suf_samples : sox_read_suf_samples;
      default:
        break;
      }
      break;
      
    case SOX_SIZE_64BIT:
      switch (ft->signal.encoding) {
      case SOX_ENCODING_FLOAT:
        return write ? sox_write_sudf_samples : sox_read_sudf_samples;
      default:
        break;
      }
      break;

    default:
      sox_fail_errno(ft,SOX_EFMT,"this handler does not support this data size");
      return NULL;
    }

    sox_fail_errno(ft,SOX_EFMT,"this encoding is not supported for this data size");
    return NULL;
}

/* Read a stream of some type into SoX's internal buffer format. */
sox_size_t sox_rawread(ft_t ft, sox_sample_t *buf, sox_size_t nsamp)
{
    ft_io_fun * read_buf = check_format(ft, sox_false);

    if (read_buf && nsamp)
      return read_buf(ft, buf, nsamp);

    return 0;
}

/* Writes SoX's internal buffer format to buffer of various data types. */
sox_size_t sox_rawwrite(ft_t ft, const sox_sample_t *buf, sox_size_t nsamp)
{
    ft_io_fun *write_buf = check_format(ft, sox_true);

    if (write_buf && nsamp)
      return write_buf(ft, (sox_sample_t *)buf, nsamp);

    return 0;
}

static int raw_start(ft_t ft) {
  return sox_rawstart(ft,sox_false,sox_false,SOX_ENCODING_UNKNOWN,-1,SOX_OPTION_DEFAULT);
}
sox_format_t const * sox_raw_format_fn(void) {
  static char const * names[] = {"raw", NULL};
  static sox_format_t driver = {
    names, NULL, SOX_FILE_SEEK,
    raw_start, sox_rawread , sox_format_nothing,
    raw_start, sox_rawwrite, sox_format_nothing,
    sox_rawseek
  };
  return &driver;
}

#define RAW_FORMAT(id,alt1,alt2,size,rev_bits,encoding) \
static int id##_start(ft_t ft) { \
  return sox_rawstart(ft,sox_true,sox_true,SOX_ENCODING_##encoding,SOX_SIZE_##size,SOX_OPTION_##rev_bits); \
} \
sox_format_t const * sox_##id##_format_fn(void) { \
  static char const * names[] = {#id, alt1, alt2, NULL}; \
  static sox_format_t driver = { \
    names, NULL, 0, \
    id##_start, sox_rawread , sox_format_nothing, \
    id##_start, sox_rawwrite, sox_format_nothing, \
    sox_format_nothing_seek \
  }; \
  return &driver; \
}

RAW_FORMAT(sb,NULL ,NULL  ,BYTE , DEFAULT,SIGN2)
RAW_FORMAT(sl,NULL ,NULL  ,32BIT, DEFAULT,SIGN2)
RAW_FORMAT(s3,NULL ,NULL  ,24BIT, DEFAULT,SIGN2)
RAW_FORMAT(sw,NULL ,NULL  ,16BIT, DEFAULT,SIGN2)
                   
RAW_FORMAT(ub,"sou","fssd",BYTE , DEFAULT,UNSIGNED)
RAW_FORMAT(uw,NULL ,NULL  ,16BIT, DEFAULT,UNSIGNED)
RAW_FORMAT(u3,NULL ,NULL  ,24BIT, DEFAULT,UNSIGNED)
RAW_FORMAT(u4,NULL ,NULL  ,32BIT, DEFAULT,UNSIGNED)
                   
RAW_FORMAT(al,NULL ,NULL  ,BYTE ,NO     ,ALAW)
RAW_FORMAT(ul,NULL ,NULL  ,BYTE ,NO     ,ULAW)
RAW_FORMAT(la,NULL ,NULL  ,BYTE ,YES    ,ALAW)
RAW_FORMAT(lu,NULL ,NULL  ,BYTE ,YES    ,ULAW)
