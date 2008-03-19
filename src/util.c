/*
 * General purpose, i.e. non SoX specific, utility functions
 *
 * (c) 2006-8 Chris Bagwell and SoX contributors
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

#include "soxconfig.h"
#include "util.h"
#include <string.h>

#ifndef HAVE_STRCASECMP
int strcasecmp(const char * s1, const char * s2)
{
  while (*s1 && (toupper(*s1) == toupper(*s2)))
    s1++, s2++;
  return toupper(*s1) - toupper(*s2);
}

int strncasecmp(char const * s1, char const * s2, size_t n)
{
  while (--n && *s1 && (toupper(*s1) == toupper(*s2)))
    s1++, s2++;
  return toupper(*s1) - toupper(*s2);
}
#endif

#ifndef HAVE_STRDUP
char * strdup(char const * s)
{
  return strcpy((char *)xmalloc(strlen(s) + 1), s);
}
#endif

int strcaseends(char const * str, char const * end)
{
  size_t str_len = strlen(str), end_len = strlen(end);
  return str_len >= end_len && !strcasecmp(str + str_len - end_len, end);
}

int strends(char const * str, char const * end)
{
  size_t str_len = strlen(str), end_len = strlen(end);
  return str_len >= end_len && !strcmp(str + str_len - end_len, end);
}

enum_item const * find_enum_text(char const * text, enum_item const * enum_items)
{
  enum_item const * result = NULL; /* Assume not found */

  while (enum_items->text) {
    if (strncasecmp(text, enum_items->text, strlen(text)) == 0) {
      if (result != NULL && result->value != enum_items->value)
        return NULL;        /* Found ambiguity */
      result = enum_items;  /* Found match */
    }
    ++enum_items;
  }
  return result;
}

enum_item const * find_enum_value(unsigned value, enum_item const * enum_items)
{
  for (;enum_items->text; ++enum_items)
    if (value == enum_items->value)
      return enum_items;
  return NULL;
}
