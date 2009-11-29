/* File format: AMR   (c) 2007 robs@users.sourceforge.net
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <string.h>
#include <math.h>

typedef struct {
  void * state;
  unsigned mode;
  short pcm[AMR_FRAME];
  size_t pcm_index;
  LSX_DLENTRIES_TO_PTRS(AMR_FUNC_ENTRIES, amr_dl);
} priv_t;

static size_t decode_1_frame(sox_format_t * ft)
{
  priv_t * p = (priv_t *)ft->priv;
  size_t n_1;
  uint8_t coded[AMR_CODED_MAX];

  if (lsx_readbuf(ft, &coded[0], (size_t)1) != 1)
    return AMR_FRAME;
  n_1 = block_size[(coded[0] >> 3) & 0x0F] - 1;
  if (lsx_readbuf(ft, &coded[1], n_1) != n_1)
    return AMR_FRAME;
  p->D_IF_decode(p->state, coded, p->pcm, 0);
  return 0;
}

static int startread(sox_format_t * ft)
{
  priv_t * p = (priv_t *)ft->priv;
  char buffer[sizeof(magic) - 1];
  int open_library_result;

  if (lsx_readchars(ft, buffer, sizeof(buffer)))
    return SOX_EOF;
  if (memcmp(buffer, magic, sizeof(buffer))) {
    lsx_fail_errno(ft, SOX_EHDR, "invalid magic number");
    return SOX_EOF;
  }

  LSX_DLLIBRARY_OPEN(
      p,
      amr_dl,
      AMR_FUNC_ENTRIES,
      AMR_DESC,
      amr_library_names,
      open_library_result);
  if (open_library_result)
    return SOX_EOF;

  p->pcm_index = AMR_FRAME;
  p->state = p->D_IF_init();

  ft->signal.rate = AMR_RATE;
  ft->encoding.encoding = AMR_ENCODING;
  ft->signal.channels = 1;
  return SOX_SUCCESS;
}

static size_t read_samples(sox_format_t * ft, sox_sample_t * buf, size_t len)
{
  priv_t * p = (priv_t *)ft->priv;
  size_t done;

  for (done = 0; done < len; done++) {
    if (p->pcm_index >= AMR_FRAME)
      p->pcm_index = decode_1_frame(ft);
    if (p->pcm_index >= AMR_FRAME)
      break;
    *buf++ = SOX_SIGNED_16BIT_TO_SAMPLE(p->pcm[p->pcm_index++], ft->clips);
  }
  return done;
}

static int stopread(sox_format_t * ft)
{
  priv_t * p = (priv_t *)ft->priv;
  p->D_IF_exit(p->state);
  LSX_DLLIBRARY_CLOSE(p, amr_dl);
  return SOX_SUCCESS;
}

static int startwrite(sox_format_t * ft)
{
#ifdef DISABLE_AMR_WB_ENCODE
  lsx_fail_errno(ft, SOX_EOF, "SoX was compiled without AMR-WB encoding support.");
  return SOX_EOF;
#else
  priv_t * p = (priv_t *)ft->priv;
  int open_library_result;

  if (ft->encoding.compression != HUGE_VAL) {
    p->mode = (unsigned)ft->encoding.compression;
    if (p->mode != ft->encoding.compression || p->mode > AMR_MODE_MAX) {
      lsx_fail_errno(ft, SOX_EINVAL, "compression level must be a whole number from 0 to %i", AMR_MODE_MAX);
      return SOX_EOF;
    }
  }
  else p->mode = 0;

  LSX_DLLIBRARY_OPEN(
      p,
      amr_dl,
      AMR_FUNC_ENTRIES,
      AMR_DESC,
      amr_library_names,
      open_library_result);
  if (open_library_result)
    return SOX_EOF;

#define IGNORE_WARNING \
  p->state = p->E_IF_init();
#include "ignore-warning.h"

  lsx_writes(ft, magic);
  p->pcm_index = 0;
  return SOX_SUCCESS;
#endif
}

#ifndef DISABLE_AMR_WB_ENCODE
static sox_bool encode_1_frame(sox_format_t * ft)
{
  priv_t * p = (priv_t *)ft->priv;
  uint8_t coded[AMR_CODED_MAX];
#define IGNORE_WARNING \
  int n = p->E_IF_encode(p->state, p->mode, p->pcm, coded, 1);
#include "ignore-warning.h"
  sox_bool result = lsx_writebuf(ft, coded, (size_t) (size_t) (unsigned)n) == (unsigned)n;
  if (!result)
    lsx_fail_errno(ft, errno, "write error");
  return result;
}
#endif

#ifdef DISABLE_AMR_WB_ENCODE
#define write_samples NULL
#else
static size_t write_samples(sox_format_t * ft, const sox_sample_t * buf, size_t len)
{
  priv_t * p = (priv_t *)ft->priv;
  size_t done;

  for (done = 0; done < len; ++done) {
    SOX_SAMPLE_LOCALS;
    p->pcm[p->pcm_index++] = SOX_SAMPLE_TO_SIGNED_16BIT(*buf++, ft->clips);
    if (p->pcm_index == AMR_FRAME) {
      p->pcm_index = 0;
      if (!encode_1_frame(ft))
        return 0;
    }
  }
  return done;
}
#endif

#ifdef DISABLE_AMR_WB_ENCODE
#define stopwrite NULL
#else
static int stopwrite(sox_format_t * ft)
{
  priv_t * p = (priv_t *)ft->priv;
  int result = SOX_SUCCESS;

  if (p->pcm_index) {
    do {
      p->pcm[p->pcm_index++] = 0;
    } while (p->pcm_index < AMR_FRAME);
    if (!encode_1_frame(ft))
      result = SOX_EOF;
  }
  p->E_IF_exit(p->state);
  return result;
}
#endif

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
    startread, read_samples, stopread,
    startwrite, write_samples, stopwrite,
    NULL, write_encodings, write_rates, sizeof(priv_t)
  };
  return &handler;
}
