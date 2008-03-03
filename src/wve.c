/*
 * File format: Psion wve   (c) 2008 robs@users.sourceforge.net
 *
 * See http://filext.com/file-extension/WVE
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
#include <string.h>

static char const ID1[18] = "ALawSoundFile**\0\017\020";
static char const ID2[] = {0,0,0,1,0,0,0,0,0,0}; /* pad & repeat info: ignore */

static int start_read(sox_format_t * ft)
{
  char buf[sizeof(ID1)];
  uint32_t num_samples;

  if (sox_readbuf(ft, buf, sizeof(buf)) != sizeof(buf) ||
      sox_readdw(ft, &num_samples) || sox_skipbytes(ft, sizeof(ID2)))
    return SOX_EOF;
  if (memcmp(ID1, buf, sizeof(buf))) {
    sox_fail_errno(ft,SOX_EHDR,"wve: can't find Psion identifier");
    return SOX_EOF;
  }
  return sox_check_read_params(ft, 1, 8000., SOX_ENCODING_ALAW, 8, (off_t)num_samples);
}

static int write_header(sox_format_t * ft)
{
  return sox_writebuf(ft, ID1, sizeof(ID1)) != sizeof(ID1)
      || sox_writedw(ft, ft->olength? ft->olength:ft->length)
      || sox_writebuf(ft, ID2, sizeof(ID2)) != sizeof(ID2)? SOX_EOF:SOX_SUCCESS;
}

SOX_FORMAT_HANDLER(wve)
{
  static char const * const names[] = {"wve", NULL};
  static sox_rate_t   const write_rates[] = {8000, 0};
  static unsigned     const write_encodings[] = {SOX_ENCODING_ALAW, 8, 0, 0};
  static sox_format_handler_t const handler = {
    names, SOX_FILE_BIG_END | SOX_FILE_MONO | SOX_FILE_REWIND,
    start_read, sox_rawread, NULL,
    write_header, sox_rawwrite, NULL,
    sox_rawseek, write_encodings, write_rates
  };
  return &handler;
}
