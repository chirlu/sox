/* libSoX file format: SoX temporary   (c) 2008 robs@users.sourceforge.net
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

#include "sox_i.h"
#include <string.h>

static char const magic[4] = ".SoX";
#define FIXED_HDR     (4 + 4 + 8 + 8 + 4)

static int startread(sox_format_t * ft)
{
  char     magic_[sizeof(magic)];
  uint32_t hdr_size;
  uint64_t data_size;
  double   rate;
  uint32_t channels;

  if (lsx_readchars(ft, magic_, sizeof(magic)))
    return SOX_EOF;

  if (memcmp(magic, magic_, sizeof(magic))) {
    lsx_fail_errno(ft, SOX_EHDR, "can't find sox file format identifier");
    return SOX_EOF;
  }
  if (lsx_readdw(ft, &hdr_size) ||
      lsx_readqw(ft, &data_size) ||
      lsx_readdf(ft, &rate) ||
      lsx_readdw(ft, &channels))
    return SOX_EOF;

  if (hdr_size < FIXED_HDR) {
    lsx_fail_errno(ft, SOX_EHDR, "header size %u is too small", hdr_size);
    return SOX_EOF;
  }

  if (hdr_size > FIXED_HDR) {
    size_t info_size = hdr_size - FIXED_HDR;
    char * buf = lsx_calloc(1, info_size + 1); /* +1 ensures null-terminated */
    if (lsx_readchars(ft, buf, info_size) != SOX_SUCCESS) {
      free(buf);
      return SOX_EOF;
    }
    sox_append_comments(&ft->oob.comments, buf);
    free(buf);
  }
  return lsx_check_read_params(
      ft, channels, rate, SOX_ENCODING_SIGN2, 32, (off_t)data_size);
}

static int write_header(sox_format_t * ft)
{
  char * comment  = lsx_cat_comments(ft->oob.comments);
  size_t len      = strlen(comment) + 1;     /* Write out null-terminated */
  size_t info_len = max(4, (len + 3) & ~3u); /* Minimum & multiple of 4 bytes */
  uint64_t size   = ft->olength? ft->olength : ft->signal.length;
  sox_bool error  = sox_false
  ||lsx_writechars(ft, magic, sizeof(magic))
  ||lsx_writedw(ft, FIXED_HDR + (unsigned)info_len)
  ||lsx_writeqw(ft, size)
  ||lsx_writedf(ft, ft->signal.rate)
  ||lsx_writedw(ft, ft->signal.channels)
  ||lsx_writechars(ft, comment, len)
  ||lsx_padbytes(ft, info_len - len);
  free(comment);
  return error? SOX_EOF: SOX_SUCCESS;
}

SOX_FORMAT_HANDLER(sox)
{
  static char const * const names[] = {"sox", NULL};
  static unsigned const write_encodings[] = {SOX_ENCODING_SIGN2, 32, 0, 0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "SoX temporary", names, SOX_FILE_REWIND, startread, lsx_rawread, NULL,
    write_header, lsx_rawwrite, NULL, lsx_rawseek, write_encodings, NULL, 0
  };
  return &handler;
}
