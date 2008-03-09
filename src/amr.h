/*
 * File format: AMR   (c) 2007 robs@users.sourceforge.net
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, write to the Free Software Foundation,
 * Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301, USA.
 */

#include <string.h>
#include <math.h>

typedef struct amr
{
  void * state;
  unsigned mode;
  short pcm[AMR_FRAME];
  sox_size_t pcm_index;
} * amr_t;

assert_static(sizeof(struct amr) <= SOX_MAX_FILE_PRIVSIZE, AMR_PRIV_TOO_BIG);

static sox_size_t decode_1_frame(sox_format_t * ft)
{
  amr_t amr = (amr_t) ft->priv;
  size_t block_size_1;
  UWord8 coded[AMR_CODED_MAX];

  if (fread(coded, sizeof(coded[0]), 1, ft->fp) != 1)
    return AMR_FRAME;
  block_size_1 = block_size[(coded[0] >> 3) & 0x0F] - 1;
  if (fread(&coded[1], sizeof(coded[1]), block_size_1, ft->fp) != block_size_1)
    return AMR_FRAME;
  D_IF_decode(amr->state, coded, amr->pcm, 0);
  return 0;
}

static sox_bool encode_1_frame(sox_format_t * ft)
{
  amr_t amr = (amr_t) ft->priv;
  UWord8 coded[AMR_CODED_MAX];
#include "amr1.h"
  sox_bool result = fwrite(coded, (unsigned)n, 1, ft->fp) == 1;
  if (!result)
    sox_fail_errno(ft, errno, "write error");
  return result;
}

static int startread(sox_format_t * ft)
{
  amr_t amr = (amr_t) ft->priv;
  char buffer[sizeof(magic) - 1];

  amr->pcm_index = AMR_FRAME;
  amr->state = D_IF_init();

  if (fread(buffer, sizeof(buffer), 1, ft->fp) != 1) {
    sox_fail_errno(ft, errno, "read error");
    return SOX_EOF;
  }
  if (memcmp(buffer, magic, sizeof(buffer))) {
    sox_fail_errno(ft, SOX_EHDR, "invalid magic number");
    return SOX_EOF;
  }
  ft->signal.rate = AMR_RATE;
  ft->encoding.encoding = AMR_ENCODING;
  ft->signal.channels = 1;
  return SOX_SUCCESS;
}

static sox_size_t read(sox_format_t * ft, sox_sample_t * buf, sox_size_t len)
{
  amr_t amr = (amr_t) ft->priv;
  sox_size_t done;

  for (done = 0; done < len; done++) {
    if (amr->pcm_index >= AMR_FRAME)
      amr->pcm_index = decode_1_frame(ft);
    if (amr->pcm_index >= AMR_FRAME)
      break;
    *buf++ = SOX_SIGNED_16BIT_TO_SAMPLE(amr->pcm[amr->pcm_index++], ft->clips);
  }
  return done;
}

static int stopread(sox_format_t * ft)
{
  amr_t amr = (amr_t) ft->priv;
  D_IF_exit(amr->state);
  return SOX_SUCCESS;
}

static int startwrite(sox_format_t * ft)
{
  amr_t amr = (amr_t) ft->priv;

  if (ft->encoding.compression != HUGE_VAL) {
    amr->mode = ft->encoding.compression;
    if (amr->mode != ft->encoding.compression || amr->mode > AMR_MODE_MAX) {
      sox_fail_errno(ft, SOX_EINVAL, "compression level must be a whole number from 0 to %i", AMR_MODE_MAX);
      return SOX_EOF;
    }
  }
  else amr->mode = 0;

#include "amr2.h"
  sox_writes(ft, magic);
  amr->pcm_index = 0;
  return SOX_SUCCESS;
}

static sox_size_t write(sox_format_t * ft, const sox_sample_t * buf, sox_size_t len)
{
  amr_t amr = (amr_t) ft->priv;
  sox_size_t done;

  for (done = 0; done < len; ++done) {
    amr->pcm[amr->pcm_index++] = SOX_SAMPLE_TO_SIGNED_16BIT(*buf++, ft->clips);
    if (amr->pcm_index == AMR_FRAME) {
      amr->pcm_index = 0;
      if (!encode_1_frame(ft))
        return 0;
    }
  }
  return done;
}

static int stopwrite(sox_format_t * ft)
{
  amr_t amr = (amr_t) ft->priv;
  int result = SOX_SUCCESS;

  if (amr->pcm_index) {
    do {
      amr->pcm[amr->pcm_index++] = 0;
    } while (amr->pcm_index < AMR_FRAME);
    if (!encode_1_frame(ft))
      result = SOX_EOF;
  }
  E_IF_exit(amr->state);
  return result;
}

sox_format_handler_t const * AMR_FORMAT_FN(void);
sox_format_handler_t const * AMR_FORMAT_FN(void)
{
  static char const * const names[] = {AMR_NAMES, NULL};
  static sox_rate_t   const write_rates[] = {AMR_RATE, 0};
  static unsigned const write_encodings[] = {AMR_ENCODING, 0, 0};
  static sox_format_handler_t handler = {
    SOX_LIB_VERSION_CODE,
    "3GPP Adaptive Multi Rate lossy speech compressor",
    names, SOX_FILE_MONO,
    startread, read, stopread,
    startwrite, write, stopwrite,
    NULL, write_encodings, write_rates
  };
  return &handler;
}
