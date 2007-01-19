/*
 * Sound Tools raw file formats
 *
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "st_i.h"
#include "g711.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define ST_ULAW_BYTE_TO_SAMPLE(d,clips)   ST_SIGNED_WORD_TO_SAMPLE(st_ulaw2linear16(d),clips)
#define ST_ALAW_BYTE_TO_SAMPLE(d,clips)   ST_SIGNED_WORD_TO_SAMPLE(st_alaw2linear16(d),clips)
#define ST_SAMPLE_TO_ULAW_BYTE(d,c) st_14linear2ulaw(ST_SAMPLE_TO_SIGNED_WORD(d,c) >> 2)
#define ST_SAMPLE_TO_ALAW_BYTE(d,c) st_13linear2alaw(ST_SAMPLE_TO_SIGNED_WORD(d,c) >> 3)

int st_rawseek(ft_t ft, st_size_t offset)
{
    st_size_t new_offset, channel_block, alignment;

    switch(ft->signal.size) {
        case ST_SIZE_BYTE:
        case ST_SIZE_16BIT:
        case ST_SIZE_24BIT:
        case ST_SIZE_32BIT:
        case ST_SIZE_64BIT:
            break;
        default:
            st_fail_errno(ft,ST_ENOTSUP,"Can't seek this data size");
            return ft->st_errno;
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

    ft->st_errno = st_seeki(ft, new_offset, SEEK_SET);

    return ft->st_errno;
}

/* Works nicely for starting read and write; st_rawstart{read,write}
   are #defined in st_i.h */
int st_rawstart(ft_t ft, st_bool default_rate, st_bool default_channels, st_encoding_t encoding, signed char size, st_option_t rev_bits)
{
  if (default_rate && ft->signal.rate == 0) {
    st_warn("'%s': sample rate not specified; trying 8kHz", ft->filename);
    ft->signal.rate = 8000;
  }

  if (default_channels && ft->signal.channels == 0) {
    st_warn("'%s': # channels not specified; trying mono", ft->filename);
    ft->signal.channels = 1;
  }

  if (encoding != ST_ENCODING_UNKNOWN) {
    if (ft->mode == 'r' &&
        ft->signal.encoding != ST_ENCODING_UNKNOWN &&
        ft->signal.encoding != encoding)
      st_report("'%s': Format options overriding file-type encoding", ft->filename);
    else ft->signal.encoding = encoding;
  }

  if (size != -1) {
    if (ft->mode == 'r' &&
        ft->signal.size != -1 && ft->signal.size != size)
      st_report("'%s': Format options overriding file-type sample-size", ft->filename);
    else ft->signal.size = size;
  }

  if (rev_bits != ST_OPTION_DEFAULT) {
    if (ft->mode == 'r' &&
        ft->signal.reverse_bits != ST_OPTION_DEFAULT &&
        ft->signal.reverse_bits != rev_bits)
      st_report("'%s': Format options overriding file-type bit-order", ft->filename);
    else ft->signal.reverse_bits = rev_bits;
  }

  ft->eof = 0;
  return ST_SUCCESS;
}

#define READ_FUNC(size, sign, ctype, uctype, cast) \
  static st_size_t st_ ## sign ## size ## _read_buf( \
      ft_t ft, st_sample_t *buf, st_size_t len) \
  { \
    st_size_t n; \
    for (n = 0; n < len; n++) { \
      ctype datum; \
      int ret = st_read ## size(ft, (uctype *)&datum); \
      if (ret != ST_SUCCESS) \
        break; \
      *buf++ = ST_ ## cast ## _TO_SAMPLE(datum, ft->clips); \
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
  static st_size_t st_ ## sign ## size ## _write_buf( \
      ft_t ft, st_sample_t *buf, st_size_t len) \
  { \
    st_size_t n; \
    for (n = 0; n < len; n++) { \
      int ret = st_write ## size(ft, ST_SAMPLE_TO_ ## cast(*buf++, ft->clips)); \
      if (ret != ST_SUCCESS) \
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

typedef st_size_t (ft_io_fun)(ft_t ft, st_sample_t *buf, st_size_t len);

static ft_io_fun *check_format(ft_t ft, st_bool write)
{
    switch (ft->signal.size) {
    case ST_SIZE_BYTE:
      switch (ft->signal.encoding) {
      case ST_ENCODING_SIGN2:
        return write ? st_sb_write_buf : st_sb_read_buf;
      case ST_ENCODING_UNSIGNED:
        return write ? st_ub_write_buf : st_ub_read_buf;
      case ST_ENCODING_ULAW:
        return write ? st_ulawb_write_buf : st_ulawb_read_buf;
      case ST_ENCODING_ALAW:
        return write ? st_alawb_write_buf : st_alawb_read_buf;
      default:
        break;
      }
      break;
      
    case ST_SIZE_16BIT: 
      switch (ft->signal.encoding) {
      case ST_ENCODING_SIGN2:
        return write ? st_sw_write_buf : st_sw_read_buf;
      case ST_ENCODING_UNSIGNED:
        return write ? st_uw_write_buf : st_uw_read_buf;
      default:
        break;
      }
      break;

    case ST_SIZE_24BIT:
      switch (ft->signal.encoding) {
      case ST_ENCODING_SIGN2:
        return write ? st_s3_write_buf : st_s3_read_buf;
      case ST_ENCODING_UNSIGNED:
        return write ? st_u3_write_buf: st_u3_read_buf;
      default:
        break;
      }
      break;
      
    case ST_SIZE_32BIT:
      switch (ft->signal.encoding) {
      case ST_ENCODING_SIGN2:
        return write ? st_sdw_write_buf : st_sdw_read_buf;
      case ST_ENCODING_UNSIGNED:
        return write ? st_udw_write_buf : st_udw_read_buf;
      case ST_ENCODING_FLOAT:
        return write ? st_suf_write_buf : st_suf_read_buf;
      default:
        break;
      }
      break;
      
    case ST_SIZE_64BIT:
      switch (ft->signal.encoding) {
      case ST_ENCODING_FLOAT:
        return write ? st_sudf_write_buf : st_sudf_read_buf;
      default:
        break;
      }
      break;

    default:
      st_fail_errno(ft,ST_EFMT,"this handler does not support this data size");
      return NULL;
    }

    st_fail_errno(ft,ST_EFMT,"this encoding is not supported for this data size");
    return NULL;
}

/* Read a stream of some type into SoX's internal buffer format. */
st_size_t st_rawread(ft_t ft, st_sample_t *buf, st_size_t nsamp)
{
    ft_io_fun * read_buf = check_format(ft, st_false);

    if (read_buf && nsamp)
      return read_buf(ft, buf, nsamp);

    return 0;
}

static void writeflush(ft_t ft)
{
  ft->eof = ST_EOF;
}


/* Writes SoX's internal buffer format to buffer of various data types. */
st_size_t st_rawwrite(ft_t ft, const st_sample_t *buf, st_size_t nsamp)
{
    ft_io_fun *write_buf = check_format(ft, st_true);

    if (write_buf && nsamp)
      return write_buf(ft, (st_sample_t *)buf, nsamp);

    return 0;
}

int st_rawstopwrite(ft_t ft)
{
        writeflush(ft);
        return ST_SUCCESS;
}

static int raw_start(ft_t ft) {
  return st_rawstart(ft,st_false,st_false,ST_ENCODING_UNKNOWN,-1,ST_OPTION_DEFAULT);
}
st_format_t const * st_raw_format_fn(void) {
  static char const * names[] = {"raw", NULL};
  static st_format_t driver = {
    names, NULL, ST_FILE_SEEK,
    raw_start, st_rawread , st_rawstopread,
    raw_start, st_rawwrite, st_rawstopwrite,
    st_rawseek
  };
  return &driver;
}

#define RAW_FORMAT(id,alt1,alt2,size,rev_bits,encoding) \
static int id##_start(ft_t ft) { \
  return st_rawstart(ft,st_true,st_true,ST_ENCODING_##encoding,ST_SIZE_##size,ST_OPTION_##rev_bits); \
} \
st_format_t const * st_##id##_format_fn(void) { \
  static char const * names[] = {#id, alt1, alt2, NULL}; \
  static st_format_t driver = { \
    names, NULL, 0, \
    id##_start, st_rawread , st_rawstopread, \
    id##_start, st_rawwrite, st_rawstopwrite, \
    st_format_nothing_seek \
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
