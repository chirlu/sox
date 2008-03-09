/*
 * Copyright 1991, 1992, 1993 Guido van Rossum And Sundry Contributors.
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Guido van Rossum And Sundry Contributors are not responsible for
 * the consequences of using this software.
 *
 * October 7, 1998 - cbagwell@sprynet.com
 *   G.723 was using incorrect # of bits.  Corrected to 3 and 5 bits.
 *
 * libSoX Sun format with header (SunOS 4.1; see /usr/demo/SOUND).
 * NeXT uses this format also, but has more format codes defined.
 * DEC uses a slight variation and swaps bytes.
 * We support only the common formats, plus
 * CCITT G.721 (32 kbit/s) and G.723 (24/40 kbit/s),
 * courtesy of Sun's public domain implementation.
 * Output is always in big-endian (Sun/NeXT) order.
 */

#include "sox_i.h"
#include "g72x.h"
#include <string.h>

/* Magic numbers used in Sun and NeXT audio files */
#define SUN_MAGIC     0x2e736e64  /* Really '.snd' */
#define SUN_INV_MAGIC 0x646e732e  /* '.snd' reversed bytes */
#define DEC_MAGIC     0x2e736400  /* Really '\0ds.' (for DEC) */
#define DEC_INV_MAGIC 0x0064732e  /* '\0ds.' reversed bytes */
#define SUN_HDRSIZE   24          /* Size of minimal header */
#define SUN_UNSPEC    ~0u         /* Unspecified data size (this is legal) */

typedef enum {
  Unspecified, Mulaw_8, Linear_8, Linear_16, Linear_24, Linear_32, Float,
  Double, Indirect, Nested, Dsp_core, Dsp_data_8, Dsp_data_16, Dsp_data_24,
  Dsp_data_32, Unknown, Display, Mulaw_squelch, Emphasized, Compressed,
  Compressed_emphasized, Dsp_commands, Dsp_commands_samples, Adpcm_g721,
  Adpcm_g722, Adpcm_g723_3, Adpcm_g723_5, Alaw_8, Unknown_other} sun_encoding_t;
static char const * const str[] = {
  "Unspecified", "8-bit mu-law", "8-bit signed linear", "16-bit signed linear",
  "24-bit signed linear", "32-bit signed linear", "Floating-point",
  "Double precision float", "Fragmented sampled data", "Unknown", "DSP program",
  "8-bit fixed-point", "16-bit fixed-point", "24-bit fixed-point",
  "32-bit fixed-point", "Unknown", "Non-audio data", "Mu-law squelch",
  "16-bit linear with emphasis", "16-bit linear with compression",
  "16-bit linear with emphasis and compression", "Music Kit DSP commands",
  "Music Kit DSP samples", "4-bit G.721 ADPCM", "G.722 ADPCM",
  "3-bit G.723 ADPCM", "5-bit G.723 ADPCM", "8-bit a-law", "Unknown"};

static sun_encoding_t sun_enc(unsigned size, sox_encoding_t sox_enc)
{
  sun_encoding_t result = Unspecified;
  if      (sox_enc == SOX_ENCODING_ULAW  && size ==  8) result = Mulaw_8;
  else if (sox_enc == SOX_ENCODING_ALAW  && size ==  8) result = Alaw_8;
  else if (sox_enc == SOX_ENCODING_SIGN2 && size ==  8) result = Linear_8;
  else if (sox_enc == SOX_ENCODING_SIGN2 && size == 16) result = Linear_16;
  else if (sox_enc == SOX_ENCODING_SIGN2 && size == 24) result = Linear_24;
  else if (sox_enc == SOX_ENCODING_SIGN2 && size == 32) result = Linear_32;
  else if (sox_enc == SOX_ENCODING_FLOAT && size == 32) result = Float;
  else if (sox_enc == SOX_ENCODING_FLOAT && size == 64) result = Double;
  return result;
}

static int sox_enc(
    uint32_t sun_encoding, sox_encoding_t * encoding, unsigned * size)
{
  switch (sun_encoding) {
    case Mulaw_8     : *encoding = SOX_ENCODING_ULAW ; *size =  8; break;
    case Alaw_8      : *encoding = SOX_ENCODING_ALAW ; *size =  8; break;
    case Linear_8    : *encoding = SOX_ENCODING_SIGN2; *size =  8; break;
    case Linear_16   : *encoding = SOX_ENCODING_SIGN2; *size = 16; break;
    case Linear_24   : *encoding = SOX_ENCODING_SIGN2; *size = 24; break;
    case Linear_32   : *encoding = SOX_ENCODING_SIGN2; *size = 32; break;
    case Float       : *encoding = SOX_ENCODING_FLOAT; *size = 32; break;
    case Double      : *encoding = SOX_ENCODING_FLOAT; *size = 64; break;
    /* Sun encodings that SoX can read, but not write: */
    case Adpcm_g721  : *encoding = SOX_ENCODING_G721 ; *size =  4; break;
    case Adpcm_g723_3: *encoding = SOX_ENCODING_G723 ; *size =  3; break;
    case Adpcm_g723_5: *encoding = SOX_ENCODING_G723 ; *size =  5; break;

    default: sox_debug("encoding: 0x%x", sun_encoding); return SOX_EOF;
  }
  return SOX_SUCCESS;
}

typedef struct {        /* For G72x decoding: */
  struct g72x_state state;
  int (*dec_routine)(int i, int out_coding, struct g72x_state *state_ptr);
  unsigned int in_buffer;
  int in_bits;
} priv_t;

/*
 * Unpack input codes and pass them back as bytes.
 * Returns 1 if there is residual input, returns -1 if eof, else returns 0.
 * (Adapted from Sun's decode.c.)
 */
static int unpack_input(sox_format_t * ft, unsigned char *code)
{
  priv_t * p = (priv_t * ) ft->priv;
  unsigned char           in_byte;

  if (p->in_bits < (int)ft->encoding.bits_per_sample) {
    if (sox_readb(ft, &in_byte) == SOX_EOF) {
      *code = 0;
      return -1;
    }
    p->in_buffer |= (in_byte << p->in_bits);
    p->in_bits += 8;
  }
  *code = p->in_buffer & ((1 << ft->encoding.bits_per_sample) - 1);
  p->in_buffer >>= ft->encoding.bits_per_sample;
  p->in_bits -= ft->encoding.bits_per_sample;
  return p->in_bits > 0;
}

static sox_size_t dec_read(sox_format_t *ft, sox_sample_t *buf, sox_size_t samp)
{
  priv_t * p = (priv_t *)ft->priv;
  unsigned char code;
  sox_size_t done;

  for (done = 0; samp > 0 && unpack_input(ft, &code) >= 0; ++done, --samp)
    *buf++ = SOX_SIGNED_16BIT_TO_SAMPLE(
        (*p->dec_routine)(code, AUDIO_ENCODING_LINEAR, &p->state),);
  return done;
}

static int startread(sox_format_t * ft)
{
  priv_t * p = (priv_t * ) ft->priv;
  uint32_t magic;        /* These 6 uint32_t variables represent a Sun sound */
  uint32_t hdr_size;     /* header on disk.  The numbers are written as */
  uint32_t data_size;    /* big-endians.  Any extra bytes (totalling */
  uint32_t sun_encoding; /* hdr_size - 24) are an "info" field of */
  uint32_t sample_rate;  /* unspecified nature, usually a string.  By */
  uint32_t channels;     /* convention the header size is a multiple of 4. */
  unsigned bits_per_sample;
  sox_encoding_t sox_encoding;

  sox_readdw(ft, &magic);
  if (magic == DEC_INV_MAGIC) {
    sox_debug("Found inverted DEC magic word.");
    /* Inverted headers are not standard.  Code was probably
     * left over from pre-standardize period of testing for
     * endianess.  Its not hurting though.
     */
    ft->encoding.reverse_bytes = SOX_IS_BIGENDIAN;
  }
  else if (magic == SUN_INV_MAGIC) {
    sox_debug("Found inverted Sun/NeXT magic word.");
    ft->encoding.reverse_bytes = SOX_IS_BIGENDIAN;
  }
  else if (magic == SUN_MAGIC) {
    sox_debug("Found Sun/NeXT magic word");
    ft->encoding.reverse_bytes = SOX_IS_LITTLEENDIAN;
  }
  else if (magic == DEC_MAGIC) {
    sox_debug("Found DEC magic word");
    ft->encoding.reverse_bytes = SOX_IS_LITTLEENDIAN;
  }
  else {
    sox_fail_errno(ft,SOX_EHDR,"Did not detect valid Sun/NeXT/DEC magic number in header.");
    return SOX_EOF;
  }

  sox_readdw(ft, &hdr_size);
  if (hdr_size < SUN_HDRSIZE) {
    sox_fail_errno(ft, SOX_EHDR, "Sun/NeXT header size too small.");
    return SOX_EOF;
  }
  sox_readdw(ft, &data_size);     /* Can be SUN_UNSPEC */
  sox_readdw(ft, &sun_encoding);
  sox_readdw(ft, &sample_rate);
  sox_readdw(ft, &channels);

  if (sox_enc(sun_encoding, &sox_encoding, &bits_per_sample) == SOX_EOF) {
    int n = min(sun_encoding, Unknown_other);
    sox_fail_errno(ft, SOX_EFMT, "unsupported encoding `%s' (%u)", str[n], sun_encoding);
    return SOX_EOF;
  }
  switch (sun_encoding) {
    case Adpcm_g721  : p->dec_routine = g721_decoder   ; break;
    case Adpcm_g723_3: p->dec_routine = g723_24_decoder; break;
    case Adpcm_g723_5: p->dec_routine = g723_40_decoder; break;
  }
  if (p->dec_routine) {
    g72x_init_state(&p->state);
    ft->handler.seek = NULL;
    ft->handler.read = dec_read;
  }

  hdr_size -= SUN_HDRSIZE; /* # bytes already read */
  if (hdr_size > 0) {
    char * buf = xcalloc(1, hdr_size + 1); /* +1 ensures null-terminated */
    if (sox_readbuf(ft, buf, hdr_size) != hdr_size) {
      sox_fail_errno(ft, SOX_EOF, "Unexpected EOF in Sun/NeXT header info.");
      free(buf);
      return SOX_EOF;
    }
    append_comments(&ft->comments, buf);
    free(buf);
  }
  if (data_size == SUN_UNSPEC)
    data_size = 0;  /* SoX uses 0 for unspecified */
  return sox_check_read_params( ft, channels, (sox_rate_t)sample_rate,
      sox_encoding, bits_per_sample, div_bits(data_size, bits_per_sample));
}

static int write_header(sox_format_t * ft)
{
  char   *comment = cat_comments(ft->comments);
  size_t len      = strlen(comment) + 1;     /* Write out null-terminated */
  size_t info_len = max(4, (len + 3) & ~3u); /* Minimum & multiple of 4 bytes */
  size_t size     = ft->olength? ft->olength : ft->length;
  sox_bool error  = sox_false
  ||sox_writedw(ft, SUN_MAGIC)
  ||sox_writedw(ft, SUN_HDRSIZE + info_len)
  ||sox_writedw(ft, size? size*(ft->encoding.bits_per_sample >> 3) : SUN_UNSPEC)
  ||sox_writedw(ft, sun_enc(ft->encoding.bits_per_sample,ft->encoding.encoding))
  ||sox_writedw(ft, (unsigned)(ft->signal.rate + .5))
  ||sox_writedw(ft, ft->signal.channels)
  ||sox_writebuf(ft, comment, len) != len
  ||sox_padbytes(ft, info_len - len);
  free(comment);
  return error? SOX_EOF: SOX_SUCCESS;
}

SOX_FORMAT_HANDLER(au)
{
  static char const * const names[] = {"au", "snd", NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_ULAW, 8, 0,
    SOX_ENCODING_ALAW, 8, 0,
    SOX_ENCODING_SIGN2, 8, 16, 24, 32, 0,
    SOX_ENCODING_FLOAT, 32, 64, 0,
    0};
  static sox_format_handler_t const handler = {
    SOX_LIB_VERSION_CODE,
    "PCM file format used widely on Sun systems",
    names, SOX_FILE_BIG_END | SOX_FILE_REWIND,
    startread, sox_rawread, NULL,
    write_header, sox_rawwrite, NULL,
    sox_rawseek, write_encodings, NULL
  };
  return &handler;
}
