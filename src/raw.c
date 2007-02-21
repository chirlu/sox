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
int sox_rawstart(ft_t ft, sox_bool default_rate, sox_bool default_channels, sox_encoding_t encoding, signed char size, sox_option_t rev_bits)
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

  ft->eof = 0;
  return SOX_SUCCESS;
}

#define READ_FUNC(size, sign, ctype, uctype, cast) \
  static sox_size_t sox_ ## sign ## size ## _read_buf( \
      ft_t ft, sox_sample_t *buf, sox_size_t len) \
  { \
    sox_size_t n; \
    for (n = 0; n < len; n++) { \
      ctype datum; \
      int ret = sox_read ## size(ft, (uctype *)&datum); \
      if (ret != SOX_SUCCESS) \
        break; \
      *buf++ = SOX_ ## cast ## _TO_SAMPLE(datum, ft->clips); \
    } \
    return n; \
  }

READ_FUNC(b, u, uint8_t, uint8_t, UNSIGNED_BYTE)
READ_FUNC(b, s, int8_t, uint8_t, SIGNED_BYTE)
READ_FUNC(b, ulaw, uint8_t, uint8_t, ULAW_BYTE)
READ_FUNC(b, alaw, uint8_t, uint8_t, ALAW_BYTE)
READ_FUNC(w, u, uint16_t, uint16_t, UNSIGNED_WORD)
READ_FUNC(w, s, int16_t, uint16_t, SIGNED_WORD)
READ_FUNC(3, u, uint24_t, uint24_t, UNSIGNED_24BIT)
READ_FUNC(3, s, int24_t, uint24_t, SIGNED_24BIT)
READ_FUNC(dw, u, uint32_t, uint32_t, UNSIGNED_DWORD)
READ_FUNC(dw, s, int32_t, uint32_t, SIGNED_DWORD)
READ_FUNC(f, su, float, float, FLOAT_DWORD)
READ_FUNC(df, su, double, double, FLOAT_DDWORD)

#define WRITE_FUNC(size, sign, cast) \
  static sox_size_t sox_ ## sign ## size ## _write_buf( \
      ft_t ft, sox_sample_t *buf, sox_size_t len) \
  { \
    sox_size_t n; \
    for (n = 0; n < len; n++) { \
      int ret = sox_write ## size(ft, SOX_SAMPLE_TO_ ## cast(*buf++, ft->clips)); \
      if (ret != SOX_SUCCESS) \
        break; \
    } \
    return n; \
  }

WRITE_FUNC(b, u, UNSIGNED_BYTE)
WRITE_FUNC(b, s, SIGNED_BYTE)
WRITE_FUNC(b, ulaw, ULAW_BYTE)
WRITE_FUNC(b, alaw, ALAW_BYTE)
WRITE_FUNC(w, u, UNSIGNED_WORD)
WRITE_FUNC(w, s, SIGNED_WORD)
WRITE_FUNC(3, u, UNSIGNED_24BIT)
WRITE_FUNC(3, s, SIGNED_24BIT)
WRITE_FUNC(dw, u, UNSIGNED_DWORD)
WRITE_FUNC(dw, s, SIGNED_DWORD)
WRITE_FUNC(f, su, FLOAT_DWORD)
WRITE_FUNC(df, su, FLOAT_DDWORD)

typedef sox_size_t (ft_io_fun)(ft_t ft, sox_sample_t *buf, sox_size_t len);

static ft_io_fun *check_format(ft_t ft, sox_bool write)
{
    switch (ft->signal.size) {
    case SOX_SIZE_BYTE:
      switch (ft->signal.encoding) {
      case SOX_ENCODING_SIGN2:
        return write ? sox_sb_write_buf : sox_sb_read_buf;
      case SOX_ENCODING_UNSIGNED:
        return write ? sox_ub_write_buf : sox_ub_read_buf;
      case SOX_ENCODING_ULAW:
        return write ? sox_ulawb_write_buf : sox_ulawb_read_buf;
      case SOX_ENCODING_ALAW:
        return write ? sox_alawb_write_buf : sox_alawb_read_buf;
      default:
        break;
      }
      break;
      
    case SOX_SIZE_16BIT: 
      switch (ft->signal.encoding) {
      case SOX_ENCODING_SIGN2:
        return write ? sox_sw_write_buf : sox_sw_read_buf;
      case SOX_ENCODING_UNSIGNED:
        return write ? sox_uw_write_buf : sox_uw_read_buf;
      default:
        break;
      }
      break;

    case SOX_SIZE_24BIT:
      switch (ft->signal.encoding) {
      case SOX_ENCODING_SIGN2:
        return write ? sox_s3_write_buf : sox_s3_read_buf;
      case SOX_ENCODING_UNSIGNED:
        return write ? sox_u3_write_buf: sox_u3_read_buf;
      default:
        break;
      }
      break;
      
    case SOX_SIZE_32BIT:
      switch (ft->signal.encoding) {
      case SOX_ENCODING_SIGN2:
        return write ? sox_sdw_write_buf : sox_sdw_read_buf;
      case SOX_ENCODING_UNSIGNED:
        return write ? sox_udw_write_buf : sox_udw_read_buf;
      case SOX_ENCODING_FLOAT:
        return write ? sox_suf_write_buf : sox_suf_read_buf;
      default:
        break;
      }
      break;
      
    case SOX_SIZE_64BIT:
      switch (ft->signal.encoding) {
      case SOX_ENCODING_FLOAT:
        return write ? sox_sudf_write_buf : sox_sudf_read_buf;
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

static void writeflush(ft_t ft)
{
  ft->eof = SOX_EOF;
}


/* Writes SoX's internal buffer format to buffer of various data types. */
sox_size_t sox_rawwrite(ft_t ft, const sox_sample_t *buf, sox_size_t nsamp)
{
    ft_io_fun *write_buf = check_format(ft, sox_true);

    if (write_buf && nsamp)
      return write_buf(ft, (sox_sample_t *)buf, nsamp);

    return 0;
}

int sox_rawstopwrite(ft_t ft)
{
        writeflush(ft);
        return SOX_SUCCESS;
}

static int raw_start(ft_t ft) {
  return sox_rawstart(ft,sox_false,sox_false,SOX_ENCODING_UNKNOWN,-1,SOX_OPTION_DEFAULT);
}
sox_format_t const * sox_raw_format_fn(void) {
  static char const * names[] = {"raw", NULL};
  static sox_format_t driver = {
    names, NULL, SOX_FILE_SEEK,
    raw_start, sox_rawread , sox_rawstopread,
    raw_start, sox_rawwrite, sox_rawstopwrite,
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
    id##_start, sox_rawread , sox_rawstopread, \
    id##_start, sox_rawwrite, sox_rawstopwrite, \
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
