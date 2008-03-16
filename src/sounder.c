/*
 * Sounder format handler          (c) 2008 robs@users.sourceforge.net
 * Based on description in soundr3b.zip.
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

#include "sox_i.h"

static int start_read(sox_format_t * ft)
{
  uint16_t type, rate;

  if (sox_readw(ft, &type) || sox_readw(ft, &rate) || sox_skipbytes(ft, 4))
    return SOX_EOF;
  if (type) {
    sox_fail_errno(ft, SOX_EHDR, "invalid Sounder header");
    return SOX_EOF;
  }
  return sox_check_read_params(ft, 1, (sox_rate_t)rate, SOX_ENCODING_UNSIGNED, 8, (off_t)0);
}

static int write_header(sox_format_t * ft)
{
  return sox_writew(ft, 0)   /* sample type */
      || sox_writew(ft, min(65535, (unsigned)(ft->signal.rate + .5)))
      || sox_writew(ft, 10)  /* speaker driver volume */
      || sox_writew(ft, 4)?  /* speaker driver DC shift */
      SOX_EOF : SOX_SUCCESS;
}

SOX_FORMAT_HANDLER(sounder)
{
  static char const * const names[] = {"sndr", NULL};
  static unsigned const write_encodings[] = {SOX_ENCODING_UNSIGNED, 8, 0, 0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "8-bit linear audio as used by Aaron Wallace's `Sounder' of 1991",
    names, SOX_FILE_LIT_END | SOX_FILE_MONO,
    start_read, sox_rawread, NULL,
    write_header, sox_rawwrite, NULL,
    sox_rawseek, write_encodings, NULL
  };
  return &handler;
}
