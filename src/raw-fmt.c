/*
 * File formats: raw         (c) 2007-8 SoX contributors
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
 
static int raw_start(sox_format_t * ft) {
  return lsx_rawstart(ft, sox_false, sox_false, sox_true, SOX_ENCODING_UNKNOWN, 0);
}

SOX_FORMAT_HANDLER(raw)
{
  static char const * const names[] = {"raw", NULL};
  static unsigned const encodings[] = {
    SOX_ENCODING_SIGN2, 32, 24, 16, 8, 0,
    SOX_ENCODING_UNSIGNED, 32, 24, 16, 8, 0,
    SOX_ENCODING_ULAW, 8, 0,
    SOX_ENCODING_ALAW, 8, 0,
    SOX_ENCODING_FLOAT, 64, 32, 0,
    0};
  static sox_format_handler_t const handler = {
    SOX_LIB_VERSION_CODE,
    "Raw PCM, mu-law, or A-law",
    names, 0,
    raw_start, lsx_rawread , NULL,
    raw_start, lsx_rawwrite, NULL,
    lsx_rawseek, encodings, NULL
  };
  return &handler;
}
