/*
 * libSoX raw I/O
 *
 * Copyright 1991-2007 Lance Norskog And Sundry Contributors
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

#define SOX_ULAW_BYTE_TO_SAMPLE(d,clips)   SOX_SIGNED_16BIT_TO_SAMPLE(sox_ulaw2linear16(d),clips)
#define SOX_ALAW_BYTE_TO_SAMPLE(d,clips)   SOX_SIGNED_16BIT_TO_SAMPLE(sox_alaw2linear16(d),clips)
#define SOX_SAMPLE_TO_ULAW_BYTE(d,c) sox_14linear2ulaw(SOX_SAMPLE_TO_SIGNED_16BIT(d,c) >> 2)
#define SOX_SAMPLE_TO_ALAW_BYTE(d,c) sox_13linear2alaw(SOX_SAMPLE_TO_SIGNED_16BIT(d,c) >> 3)

int sox_rawseek(sox_format_t * ft, sox_size_t offset)
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

    ft->sox_errno = sox_seeki(ft, (sox_ssize_t)new_offset, SEEK_SET);

    return ft->sox_errno;
}

/* Works nicely for starting read and write; sox_rawstart{read,write}
   are #defined in sox_i.h */
int sox_rawstart(sox_format_t * ft, sox_bool default_rate, sox_bool default_channels, sox_encoding_t encoding, int size)
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

  return SOX_SUCCESS;
}

#define READ_SAMPLES_FUNC(type, size, sign, ctype, uctype, cast) \
  sox_size_t sox_read_ ## sign ## type ## _samples( \
      sox_format_t * ft, sox_sample_t *buf, sox_size_t len) \
  { \
    sox_size_t n, nread; \
    ctype *data = xmalloc(sizeof(ctype) * len); \
    if ((nread = sox_read_ ## type ## _buf(ft, (uctype *)data, len)) != len) \
      sox_fail_errno(ft, errno, sox_readerr); \
    for (n = 0; n < nread; n++) \
      *buf++ = cast(data[n], ft->clips); \
    free(data); \
    return nread; \
  }

static READ_SAMPLES_FUNC(b, 1, u, uint8_t, uint8_t, SOX_UNSIGNED_8BIT_TO_SAMPLE)
static READ_SAMPLES_FUNC(b, 1, s, int8_t, uint8_t, SOX_SIGNED_8BIT_TO_SAMPLE)
static READ_SAMPLES_FUNC(b, 1, ulaw, uint8_t, uint8_t, SOX_ULAW_BYTE_TO_SAMPLE)
static READ_SAMPLES_FUNC(b, 1, alaw, uint8_t, uint8_t, SOX_ALAW_BYTE_TO_SAMPLE)
static READ_SAMPLES_FUNC(w, 2, u, uint16_t, uint16_t, SOX_UNSIGNED_16BIT_TO_SAMPLE)
static READ_SAMPLES_FUNC(w, 2, s, int16_t, uint16_t, SOX_SIGNED_16BIT_TO_SAMPLE)
static READ_SAMPLES_FUNC(3, 3, u, uint24_t, uint24_t, SOX_UNSIGNED_24BIT_TO_SAMPLE)
static READ_SAMPLES_FUNC(3, 3, s, int24_t, uint24_t, SOX_SIGNED_24BIT_TO_SAMPLE)
static READ_SAMPLES_FUNC(dw, 4, u, uint32_t, uint32_t, SOX_UNSIGNED_32BIT_TO_SAMPLE)
static READ_SAMPLES_FUNC(dw, 4, s, int32_t, uint32_t, SOX_SIGNED_32BIT_TO_SAMPLE)
static READ_SAMPLES_FUNC(f, sizeof(float), su, float, float, SOX_FLOAT_32BIT_TO_SAMPLE)
static READ_SAMPLES_FUNC(df, sizeof(double), su, double, double, SOX_FLOAT_64BIT_TO_SAMPLE)

#define WRITE_SAMPLES_FUNC(type, size, sign, ctype, uctype, cast) \
  sox_size_t sox_write_ ## sign ## type ## _samples( \
      sox_format_t * ft, sox_sample_t *buf, sox_size_t len) \
  { \
    sox_size_t n, nwritten; \
    ctype *data = xmalloc(sizeof(ctype) * len); \
    for (n = 0; n < len; n++) \
      data[n] = cast(buf[n], ft->clips); \
    if ((nwritten = sox_write_ ## type ## _buf(ft, (uctype *)data, len)) != len) \
      sox_fail_errno(ft, errno, sox_writerr); \
    free(data); \
    return nwritten; \
  }

static WRITE_SAMPLES_FUNC(b, 1, u, uint8_t, uint8_t, SOX_SAMPLE_TO_UNSIGNED_8BIT)
static WRITE_SAMPLES_FUNC(b, 1, s, int8_t, uint8_t, SOX_SAMPLE_TO_SIGNED_8BIT)
static WRITE_SAMPLES_FUNC(b, 1, ulaw, uint8_t, uint8_t, SOX_SAMPLE_TO_ULAW_BYTE)
static WRITE_SAMPLES_FUNC(b, 1, alaw, uint8_t, uint8_t, SOX_SAMPLE_TO_ALAW_BYTE)
static WRITE_SAMPLES_FUNC(w, 2, u, uint16_t, uint16_t, SOX_SAMPLE_TO_UNSIGNED_16BIT)
static WRITE_SAMPLES_FUNC(w, 2, s, int16_t, uint16_t, SOX_SAMPLE_TO_SIGNED_16BIT)
static WRITE_SAMPLES_FUNC(3, 3, u, uint24_t, uint24_t, SOX_SAMPLE_TO_UNSIGNED_24BIT)
static WRITE_SAMPLES_FUNC(3, 3, s, int24_t, uint24_t, SOX_SAMPLE_TO_SIGNED_24BIT)
static WRITE_SAMPLES_FUNC(dw, 4, u, uint32_t, uint32_t, SOX_SAMPLE_TO_UNSIGNED_32BIT)
static WRITE_SAMPLES_FUNC(dw, 4, s, int32_t, uint32_t, SOX_SAMPLE_TO_SIGNED_32BIT)
static WRITE_SAMPLES_FUNC(f, sizeof(float), su, float, float, SOX_SAMPLE_TO_FLOAT_32BIT)
static WRITE_SAMPLES_FUNC(df, sizeof(double), su, double, double, SOX_SAMPLE_TO_FLOAT_64BIT)

typedef sox_size_t (ft_io_fun)(sox_format_t * ft, sox_sample_t *buf, sox_size_t len);

static ft_io_fun *check_format(sox_format_t * ft, sox_bool write)
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
sox_size_t sox_rawread(sox_format_t * ft, sox_sample_t *buf, sox_size_t nsamp)
{
    ft_io_fun * read_buf = check_format(ft, sox_false);

    if (read_buf && nsamp)
      return read_buf(ft, buf, nsamp);

    return 0;
}

/* Writes SoX's internal buffer format to buffer of various data types. */
sox_size_t sox_rawwrite(sox_format_t * ft, const sox_sample_t *buf, sox_size_t nsamp)
{
    ft_io_fun *write_buf = check_format(ft, sox_true);

    if (write_buf && nsamp)
      return write_buf(ft, (sox_sample_t *)buf, nsamp);

    return 0;
}
