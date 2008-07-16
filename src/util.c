/* General purpose, i.e. non SoX specific, utility functions.
 * Copyright (c) 2007-8 robs@users.sourceforge.net
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
#include <ctype.h>
#include <stdio.h>

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

enum_item const * find_enum_text(char const * text, enum_item const * enum_items)
{
  enum_item const * result = NULL; /* Assume not found */

  while (enum_items->text) {
    if (strcasecmp(text, enum_items->text) == 0)
      return enum_items;    /* Found exact match */
    if (strncasecmp(text, enum_items->text, strlen(text)) == 0) {
      if (result != NULL && result->value != enum_items->value)
        return NULL;        /* Found ambiguity */
      result = enum_items;  /* Found sub-string match */
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

char const * sigfigs3(size_t number)
{
  static char string[16][10];
  static unsigned n;
  unsigned a, b, c = 2;
  sprintf(string[n = (n+1) & 15], "%#.3g", (double)number);
  if (sscanf(string[n], "%u.%ue%u", &a, &b, &c) == 3)
    a = 100*a + b;
  switch (c%3) {
    case 0: sprintf(string[n], "%u.%02u%c", a/100,a%100, " kMGTPE"[c/3]); break;
    case 1: sprintf(string[n], "%u.%u%c"  , a/10 ,a%10 , " kMGTPE"[c/3]); break;
    case 2: sprintf(string[n], "%u%c"     , a          , " kMGTPE"[c/3]); break;
  }
  return string[n];
}

char const * sigfigs3p(double percentage)
{
  static char string[16][10];
  static unsigned n;
  sprintf(string[n = (n+1) & 15], "%.1f%%", percentage);
  if (strlen(string[n]) < 5)
    sprintf(string[n], "%.2f%%", percentage);
  else if (strlen(string[n]) > 5)
    sprintf(string[n], "%.0f%%", percentage);
  return string[n];
}

