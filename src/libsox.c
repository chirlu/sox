/* Implements the public API for libSoX general functions
 * All public functions & data are prefixed with sox_ .
 *
 * (c) 2006-8 Chris Bagwell and SoX contributors
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

const char *sox_version(void)
{
  static char versionstr[20];

  sprintf(versionstr, "%d.%d.%d",
          (SOX_LIB_VERSION_CODE & 0xff0000) >> 16,
          (SOX_LIB_VERSION_CODE & 0x00ff00) >> 8,
          (SOX_LIB_VERSION_CODE & 0x0000ff));
  return(versionstr);
}

/* Default routine to output messages; can be overridden */
static void output_message(
    unsigned level, const char *filename, const char *fmt, va_list ap)
{
  if (sox_globals.verbosity >= level) {
    sox_output_message(stderr, filename, fmt, ap);
    fprintf(stderr, "\n");
  }
}

sox_globals_t sox_globals = {
  2,
  output_message,
  sox_false,
  8192,
  0,
  NULL, NULL, NULL};

void sox_output_message(FILE *file, const char *filename, const char *fmt, va_list ap)
{
  char const * slash_pos = LAST_SLASH(filename);
  char const * base_name = slash_pos? slash_pos + 1 : filename;
  char const * dot_pos   = strrchr(base_name, '.');
  fprintf(file, "%.*s: ", dot_pos? (int)(dot_pos - base_name) : -1, base_name);
  vfprintf(file, fmt, ap);
}

#undef lsx_fail
#undef lsx_warn
#undef lsx_report
#undef lsx_debug
#undef lsx_debug_more
#undef lsx_debug_most

#define SOX_MESSAGE_FUNCTION(name,level) \
void name(char const * fmt, ...) { \
  va_list ap; \
  va_start(ap, fmt); \
  if (sox_globals.output_message_handler) \
    (*sox_globals.output_message_handler)(level,sox_globals.subsystem,fmt,ap); \
  va_end(ap); \
}

SOX_MESSAGE_FUNCTION(lsx_fail  , 1)
SOX_MESSAGE_FUNCTION(lsx_warn  , 2)
SOX_MESSAGE_FUNCTION(lsx_report, 3)
SOX_MESSAGE_FUNCTION(lsx_debug , 4)
SOX_MESSAGE_FUNCTION(lsx_debug_more , 5)
SOX_MESSAGE_FUNCTION(lsx_debug_most , 6)

#undef SOX_MESSAGE_FUNCTION

