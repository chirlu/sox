/*
 * File format: null   (c) 2006-7 SoX contributers
 * Based on an original idea by Carsten Borchardt
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

static int startread(sox_format_t * ft)
{
  /* If format parameters are not given, set somewhat arbitrary
   * (but commonly used) defaults: */
  if (!ft->signal.rate) {
    ft->signal.rate = 44100;
    sox_report("sample rate not specified; using %i", ft->signal.rate);
  }
  if (ft->signal.size <= 0) {
    ft->signal.size = SOX_SIZE_16BIT;
    sox_report("precision not specified; using %s", sox_size_bits_str[ft->signal.size]);
  }
  if (ft->signal.encoding == SOX_ENCODING_UNKNOWN) {
    ft->signal.encoding = SOX_ENCODING_SIGN2;
    sox_report("encoding not specified; using %s", sox_encodings_str[ft->signal.encoding]);
  }
  return SOX_SUCCESS;
}

static sox_size_t read(sox_format_t * ft UNUSED, sox_ssample_t *buf, sox_size_t len)
{
  /* Reading from null generates silence i.e. (sox_sample_t)0. */
  memset(buf, 0, sizeof(sox_ssample_t) * len);
  return len; /* Return number of samples "read". */
}

static sox_size_t write(sox_format_t * ft UNUSED, const sox_ssample_t *buf UNUSED, sox_size_t len)
{
  /* Writing to null just discards the samples */
  return len; /* Return number of samples "written". */
}

const sox_format_handler_t *sox_nul_format_fn(void);

const sox_format_handler_t *sox_nul_format_fn(void)
{
  static const char *names[] = { "null", "nul"/* with -t; deprecated*/, NULL};
  static sox_format_handler_t handler = {
    names, SOX_FILE_DEVICE | SOX_FILE_PHONY | SOX_FILE_NOSTDIO,
    startread, read, 0, 0, write, 0, 0
  };
  return &handler;
}
