/*
 * Sound Tools raw format file.
 *
 * Includes .ub, .uw, .sb, .sw, and .ul formats at end
 *
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

/*
 * Notes: most of the headerless formats set their handlers to raw
 * in their startread/write routines.
 *
 */

#include "st_i.h"
#include "g711.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* Lookup table to reverse the bit order of a byte. ie MSB become LSB */
unsigned char cswap[256] = {
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

#define ST_ULAW_BYTE_TO_SAMPLE(d,clips)   ST_SIGNED_WORD_TO_SAMPLE(st_ulaw2linear16(d),clips)
#define ST_ALAW_BYTE_TO_SAMPLE(d,clips)   ST_SIGNED_WORD_TO_SAMPLE(st_alaw2linear16(d),clips)
#define ST_SAMPLE_TO_ULAW_BYTE(d,c) st_14linear2ulaw(ST_SAMPLE_TO_SIGNED_WORD(d,c) >> 2)
#define ST_SAMPLE_TO_ALAW_BYTE(d,c) st_13linear2alaw(ST_SAMPLE_TO_SIGNED_WORD(d,c) >> 3)

/* Some hardware sends MSB last. These account for that */
#define ST_INVERT_ULAW_BYTE_TO_SAMPLE(d,clips) \
    ST_SIGNED_WORD_TO_SAMPLE(st_ulaw2linear16(cswap[d]),clips)
#define ST_INVERT_ALAW_BYTE_TO_SAMPLE(d,clips) \
    ST_SIGNED_WORD_TO_SAMPLE(st_alaw2linear16(cswap[d]),clips)
#define ST_SAMPLE_TO_INVERT_ULAW_BYTE(d,c) \
    cswap[st_14linear2ulaw(ST_SAMPLE_TO_SIGNED_WORD(d,c) >> 2)]
#define ST_SAMPLE_TO_INVERT_ALAW_BYTE(d,c) \
    cswap[st_13linear2alaw(ST_SAMPLE_TO_SIGNED_WORD(d,c) >> 3)]

static void rawdefaults(ft_t ft);

int st_rawseek(ft_t ft, st_size_t offset)
{
    st_size_t new_offset, channel_block, alignment;

    switch(ft->signal.size) {
        case ST_SIZE_BYTE:
        case ST_SIZE_WORD:
        case ST_SIZE_24BIT:
        case ST_SIZE_DWORD:
        case ST_SIZE_DDWORD:
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
int st_rawstart(ft_t ft)
{
    ft->eof = 0;

    return ST_SUCCESS;
}

#define READ_FUNC(size, sign, ctype, uctype, cast) \
  static st_size_t st_ ## sign ## size ## _read_buf(st_sample_t *buf, ft_t ft, st_size_t len, st_size_t *clippedCount UNUSED) \
  { \
    st_size_t n; \
    for (n = 0; n < len; n++) { \
      ctype datum; \
      int ret = st_read ## size(ft, (uctype *)&datum); \
      if (ret != ST_SUCCESS) \
        break; \
      *buf++ = ST_ ## cast ## _TO_SAMPLE(datum, *clippedCount); \
    } \
    return n; \
  }

READ_FUNC(b, u, uint8_t, uint8_t, UNSIGNED_BYTE)
READ_FUNC(b, s, int8_t, uint8_t, SIGNED_BYTE)
READ_FUNC(b, ulaw, uint8_t, uint8_t, ULAW_BYTE)
READ_FUNC(b, alaw, uint8_t, uint8_t, ALAW_BYTE)
READ_FUNC(b, inv_ulaw, uint8_t, uint8_t, INVERT_ULAW_BYTE)
READ_FUNC(b, inv_alaw, uint8_t, uint8_t, INVERT_ALAW_BYTE)
READ_FUNC(w, u, uint16_t, uint16_t, UNSIGNED_WORD)
READ_FUNC(w, s, int16_t, uint16_t, SIGNED_WORD)
READ_FUNC(3, u, uint24_t, uint24_t, UNSIGNED_24BIT)
READ_FUNC(3, s, int24_t, uint24_t, SIGNED_24BIT)
READ_FUNC(dw, u, uint32_t, uint32_t, UNSIGNED_DWORD)
READ_FUNC(dw, , int32_t, uint32_t, SIGNED_DWORD)
READ_FUNC(f, , float, float, FLOAT_DWORD)
READ_FUNC(df, , double, double, FLOAT_DDWORD)

#define WRITE_FUNC(size, sign, cast) \
  static st_size_t st_ ## sign ## size ## _write_buf(st_sample_t *buf, ft_t ft, st_size_t len, st_size_t *clippedCount UNUSED) \
  { \
    st_size_t n; \
    for (n = 0; n < len; n++) { \
      int ret = st_write ## size(ft, ST_SAMPLE_TO_ ## cast(*buf++, *clippedCount)); \
      if (ret != ST_SUCCESS) \
        break; \
    } \
    return n; \
  }

WRITE_FUNC(b, u, UNSIGNED_BYTE)
WRITE_FUNC(b, s, SIGNED_BYTE)
WRITE_FUNC(b, ulaw, ULAW_BYTE)
WRITE_FUNC(b, alaw, ALAW_BYTE)
WRITE_FUNC(b, inv_ulaw, INVERT_ULAW_BYTE)
WRITE_FUNC(b, inv_alaw, INVERT_ALAW_BYTE)
WRITE_FUNC(w, u, UNSIGNED_WORD)
WRITE_FUNC(w, s, SIGNED_WORD)
WRITE_FUNC(3, u, UNSIGNED_24BIT)
WRITE_FUNC(3, s, SIGNED_24BIT)
WRITE_FUNC(dw, u, UNSIGNED_DWORD)
WRITE_FUNC(dw, , SIGNED_DWORD)
WRITE_FUNC(f, , FLOAT_DWORD)
WRITE_FUNC(df, , FLOAT_DDWORD)

typedef st_size_t (ft_io_fun)(st_sample_t *buf, ft_t ft, st_size_t len, st_size_t *clippedCount);

static ft_io_fun *check_format(ft_t ft, bool write)
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
      case ST_ENCODING_INV_ULAW:
        return write ? st_inv_ulawb_write_buf : st_inv_ulawb_read_buf;
      case ST_ENCODING_INV_ALAW:
        return write ? st_inv_alawb_write_buf : st_inv_alawb_read_buf;
      default:
        break;
      }
      break;
      
    case ST_SIZE_WORD: 
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
      
    case ST_SIZE_DWORD:
      switch (ft->signal.encoding) {
      case ST_ENCODING_SIGN2:
        return write ? st_dw_write_buf : st_dw_read_buf;
      case ST_ENCODING_UNSIGNED:
        return write ? st_udw_write_buf : st_udw_read_buf;
      case ST_ENCODING_FLOAT:
        return write ? st_f_write_buf : st_f_read_buf;
      default:
        break;
      }
      break;
      
    case ST_SIZE_DDWORD:
      switch (ft->signal.encoding) {
      case ST_ENCODING_FLOAT:
        return write ? st_df_write_buf : st_df_read_buf;
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
    st_size_t (*read_buf)(st_sample_t *, ft_t, st_size_t, st_size_t *) = check_format(ft, false);

    if (read_buf && nsamp)
      return read_buf(buf, ft, nsamp, &ft->clippedCount);

    return 0;
}

static void writeflush(ft_t ft)
{
  ft->eof = ST_EOF;
}


/* Writes SoX's internal buffer format to buffer of various data types. */
st_size_t st_rawwrite(ft_t ft, const st_sample_t *buf, st_size_t nsamp)
{
    ft_io_fun *write_buf = check_format(ft, true);

    if (write_buf && nsamp)
      return write_buf((st_sample_t *)buf, ft, nsamp, &ft->clippedCount);

    return 0;
}

int st_rawstopwrite(ft_t ft)
{
        writeflush(ft);
        return ST_SUCCESS;
}

/*
* Set parameters to the fixed parameters known for this format,
* and change format to raw format.
*/

#define STARTREAD(NAME,SIZE,STYLE) \
static int NAME(ft_t ft) \
{ \
        ft->signal.size = SIZE; \
        ft->signal.encoding = STYLE; \
        rawdefaults(ft); \
        return st_rawstartread(ft); \
}

#define STARTWRITE(NAME,SIZE,STYLE)\
static int NAME(ft_t ft) \
{ \
        ft->signal.size = SIZE; \
        ft->signal.encoding = STYLE; \
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

STARTREAD(st_u3startread,ST_SIZE_24BIT,ST_ENCODING_UNSIGNED)
STARTWRITE(st_u3startwrite,ST_SIZE_24BIT,ST_ENCODING_UNSIGNED)

STARTREAD(st_s3startread,ST_SIZE_24BIT,ST_ENCODING_SIGN2)
STARTWRITE(st_s3startwrite,ST_SIZE_24BIT,ST_ENCODING_SIGN2)

STARTREAD(st_u4startread,ST_SIZE_DWORD,ST_ENCODING_UNSIGNED)
STARTWRITE(st_u4startwrite,ST_SIZE_DWORD,ST_ENCODING_UNSIGNED)

STARTREAD(st_slstartread,ST_SIZE_DWORD,ST_ENCODING_SIGN2)
STARTWRITE(st_slstartwrite,ST_SIZE_DWORD,ST_ENCODING_SIGN2)

STARTREAD(st_ulstartread,ST_SIZE_BYTE,ST_ENCODING_ULAW)
STARTWRITE(st_ulstartwrite,ST_SIZE_BYTE,ST_ENCODING_ULAW)

STARTREAD(st_alstartread,ST_SIZE_BYTE,ST_ENCODING_ALAW)
STARTWRITE(st_alstartwrite,ST_SIZE_BYTE,ST_ENCODING_ALAW)

STARTREAD(st_lustartread,ST_SIZE_BYTE,ST_ENCODING_INV_ULAW)
STARTWRITE(st_lustartwrite,ST_SIZE_BYTE,ST_ENCODING_INV_ULAW)

STARTREAD(st_lastartread,ST_SIZE_BYTE,ST_ENCODING_INV_ALAW)
STARTWRITE(st_lastartwrite,ST_SIZE_BYTE,ST_ENCODING_INV_ALAW)

void rawdefaults(ft_t ft)
{
        if (ft->signal.rate == 0)
                ft->signal.rate = 8000;
        if (ft->signal.channels == 0)
                ft->signal.channels = 1;
}

static const char *rawnames[] = {
  "raw",
  NULL
};

static st_format_t st_raw_format = {
  rawnames,
  NULL,
  ST_FILE_STEREO | ST_FILE_SEEK,
  st_rawstartread,
  st_rawread,
  st_rawstopread,
  st_rawstartwrite,
  st_rawwrite,
  st_rawstopwrite,
  st_rawseek
};

const st_format_t *st_raw_format_fn(void)
{
    return &st_raw_format;
}

/* a-law byte raw */
static const char *alnames[] = {
  "al",
  NULL
};

static st_format_t st_al_format = {
  alnames,
  NULL,
  ST_FILE_STEREO,
  st_alstartread,
  st_rawread,
  st_rawstopread,
  st_alstartwrite,
  st_rawwrite,
  st_rawstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_al_format_fn(void)
{
    return &st_al_format;
}

/* inverse a-law byte raw */
static const char *lanames[] = {
  "la",
  NULL
};

static st_format_t st_la_format = {
  lanames,
  NULL,
  ST_FILE_STEREO,
  st_lastartread,
  st_rawread,
  st_rawstopread,
  st_lastartwrite,
  st_rawwrite,
  st_rawstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_la_format_fn(void)
{
    return &st_la_format;
}

/* inverse u-law byte raw */
static const char *lunames[] = {
  "lu",
  NULL
};

static st_format_t st_lu_format = {
  lunames,
  NULL,
  ST_FILE_STEREO,
  st_lustartread,
  st_rawread,
  st_rawstopread,
  st_lustartwrite,
  st_rawwrite,
  st_rawstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_lu_format_fn(void)
{
    return &st_lu_format;
}

static const char *sbnames[] = {
  "sb",
  NULL
};

static st_format_t st_sb_format = {
  sbnames,
  NULL,
  ST_FILE_STEREO,
  st_sbstartread,
  st_rawread,
  st_rawstopread,
  st_sbstartwrite,
  st_rawwrite,
  st_rawstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_sb_format_fn(void)
{
    return &st_sb_format;
}



/* Unsigned 4 byte raw; used for testing only; not documented in the man page */

static const char *u4names[] = {
  "u4",
  NULL,
};

static st_format_t st_u4_format = {
  u4names,
  NULL,
  ST_FILE_STEREO,
  st_u4startread,
  st_rawread,
  st_rawstopread,
  st_u4startwrite,
  st_rawwrite,
  st_rawstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_u4_format_fn(void)
{
    return &st_u4_format;
}



static const char *slnames[] = {
  "sl",
  NULL,
};

static st_format_t st_sl_format = {
  slnames,
  NULL,
  ST_FILE_STEREO,
  st_slstartread,
  st_rawread,
  st_rawstopread,
  st_slstartwrite,
  st_rawwrite,
  st_rawstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_sl_format_fn(void)
{
    return &st_sl_format;
}

static const char *swnames[] = {
  "sw",
  NULL
};

static st_format_t st_sw_format = {
  swnames,
  NULL,
  ST_FILE_STEREO,
  st_swstartread,
  st_rawread,
  st_rawstopread,
  st_swstartwrite,
  st_rawwrite,
  st_rawstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_sw_format_fn(void)
{
    return &st_sw_format;
}



/* Signed 3 byte raw; used for testing only; not documented in the man page */

static const char *s3names[] = {
  "s3",
  NULL
};

static st_format_t st_s3_format = {
  s3names,
  NULL,
  ST_FILE_STEREO,
  st_s3startread,
  st_rawread,
  st_rawstopread,
  st_s3startwrite,
  st_rawwrite,
  st_rawstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_s3_format_fn(void)
{
    return &st_s3_format;
}



static const char *ubnames[] = {
  "ub",
  "sou",
  "fssd",
  NULL
};

static st_format_t st_ub_format = {
  ubnames,
  NULL,
  ST_FILE_STEREO,
  st_ubstartread,
  st_rawread,
  st_rawstopread,
  st_ubstartwrite,
  st_rawwrite,
  st_rawstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_ub_format_fn(void)
{
    return &st_ub_format;
}

static const char *ulnames[] = {
  "ul",
  NULL
};

static st_format_t st_ul_format = {
  ulnames,
  NULL,
  ST_FILE_STEREO,
  st_ulstartread,
  st_rawread,
  st_rawstopread,
  st_ulstartwrite,
  st_rawwrite,
  st_rawstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_ul_format_fn(void)
{
    return &st_ul_format;
}

static const char *uwnames[] = {
  "uw",
  NULL
};

static st_format_t st_uw_format = {
  uwnames,
  NULL,
  ST_FILE_STEREO,
  st_uwstartread,
  st_rawread,
  st_rawstopread,
  st_uwstartwrite,
  st_rawwrite,
  st_rawstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_uw_format_fn(void)
{
    return &st_uw_format;
}



/* Unsigned 3 byte raw; used for testing only; not documented in the man page */

static const char *u3names[] = {
  "u3",
  NULL
};

static st_format_t st_u3_format = {
  u3names,
  NULL,
  ST_FILE_STEREO,
  st_u3startread,
  st_rawread,
  st_rawstopread,
  st_u3startwrite,
  st_rawwrite,
  st_rawstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_u3_format_fn(void)
{
    return &st_u3_format;
}
