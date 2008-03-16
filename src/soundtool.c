/*
 * Sounder format handler          (c) 2008 robs@users.sourceforge.net
 * Based on description in sndtl26.zip.
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

static char const ID1[6] = "SOUND\x1a";
#define text_field_len 96  /* Includes null-terminator */

static int start_read(sox_format_t * ft)
{
  char id1[sizeof(ID1)], comments[text_field_len + 1];
  uint32_t nsamples;
  uint16_t rate;

  if (sox_readchars(ft, id1, sizeof(ID1)) ||
      sox_skipbytes(ft, 10) || sox_readdw(ft, &nsamples) ||
      sox_readw(ft, &rate) || sox_skipbytes(ft, 6) ||
      sox_readchars(ft, comments, text_field_len))
    return SOX_EOF;
  if (memcmp(ID1, id1, sizeof(id1))) {
    sox_fail_errno(ft, SOX_EHDR, "soundtool: can't find SoundTool identifier");
    return SOX_EOF;
  }
  comments[text_field_len] = '\0'; /* Be defensive against incorrect files */
  append_comments(&ft->comments, comments);
  return sox_check_read_params(ft, 1, (sox_rate_t)rate, SOX_ENCODING_UNSIGNED, 8, (off_t)0);
}

static int write_header(sox_format_t * ft)
{
  char * comment = cat_comments(ft->comments);
  char text_buf[text_field_len];
  sox_size_t length = ft->olength? ft->olength:ft->length;

  memset(text_buf, 0, sizeof(text_buf));
  strncpy(text_buf, comment, text_field_len - 1);
  free(comment);
  return sox_writechars(ft, ID1, sizeof(ID1))
      || sox_writew  (ft, 0)      /* GSound: not used */
      || sox_writedw (ft, length) /* length of complete sample */
      || sox_writedw (ft, 0)      /* first byte to play from sample */
      || sox_writedw (ft, length) /* first byte NOT to play from sample */
      || sox_writew  (ft, min(65535, (unsigned)(ft->signal.rate + .5)))
      || sox_writew  (ft, 0)      /* sample size/type */
      || sox_writew  (ft, 10)     /* speaker driver volume */
      || sox_writew  (ft, 4)      /* speaker driver DC shift */
      || sox_writechars(ft, text_buf, sizeof(text_buf))?  SOX_EOF:SOX_SUCCESS;
}

SOX_FORMAT_HANDLER(soundtool)
{
  static char const * const names[] = {"sndt", NULL};
  static unsigned const write_encodings[] = {SOX_ENCODING_UNSIGNED, 8, 0, 0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "8-bit linear audio as used by Martin Hepperle's `SoundTool' of 1991/2",
    names, SOX_FILE_LIT_END | SOX_FILE_MONO | SOX_FILE_REWIND,
    start_read, sox_rawread, NULL,
    write_header, sox_rawwrite, NULL,
    sox_rawseek, write_encodings, NULL
  };
  return &handler;
}
