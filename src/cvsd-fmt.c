/*
 * File format: CVSD (see cvsd.c)        (c) 2007-8 SoX contributors
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

#include "cvsd.h"

SOX_FORMAT_HANDLER(cvsd)
{
  static char const * const names[] = {"cvsd", "cvs", NULL};
  static unsigned const write_encodings[] = {SOX_ENCODING_CVSD, 1, 0, 0};
  static sox_format_handler_t const handler = {
    "Headerless Continuously Variable Slope Delta modulation",
    names, SOX_FILE_MONO,
    sox_cvsdstartread, sox_cvsdread, sox_cvsdstopread,
    sox_cvsdstartwrite, sox_cvsdwrite, sox_cvsdstopwrite,
    sox_rawseek, write_encodings, NULL
  };
  return &handler;
}
