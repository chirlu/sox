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
#include "sgetopt.h"
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

sox_bool lsx_strends(char const * str, char const * end)
{
  size_t str_len = strlen(str), end_len = strlen(end);
  return str_len >= end_len && !strcmp(str + str_len - end_len, end);
}

char const * lsx_find_file_extension(char const * pathname)
{
  /* First, chop off any path portions of filename.  This
   * prevents the next search from considering that part. */
  char const * result = LAST_SLASH(pathname);
  if (!result)
    result = pathname;

  /* Now look for an filename extension */
  result = strrchr(result, '.');
  if (result)
    ++result;
  return result;
}

lsx_enum_item const * lsx_find_enum_text(char const * text, lsx_enum_item const * enum_items, unsigned flags)
{
  lsx_enum_item const * result = NULL; /* Assume not found */
  sox_bool sensitive = !!(flags & LSX_FET_CASE);

  while (enum_items->text) {
    if ((!sensitive && !strcasecmp(text, enum_items->text)) ||
        ( sensitive && !    strcmp(text, enum_items->text)))
      return enum_items;    /* Found exact match */
    if ((!sensitive && !strncasecmp(text, enum_items->text, strlen(text))) ||
        ( sensitive && !    strncmp(text, enum_items->text, strlen(text)))) {
      if (result != NULL && result->value != enum_items->value)
        return NULL;        /* Found ambiguity */
      result = enum_items;  /* Found sub-string match */
    }
    ++enum_items;
  }
  return result;
}

lsx_enum_item const * lsx_find_enum_value(unsigned value, lsx_enum_item const * enum_items)
{
  for (;enum_items->text; ++enum_items)
    if (value == enum_items->value)
      return enum_items;
  return NULL;
}

int lsx_enum_option(int c, lsx_enum_item const * items)
{
  lsx_enum_item const * p = lsx_find_enum_text(lsx_optarg, items, sox_false);
  if (p == NULL) {
    size_t len = 1;
    char * set = lsx_malloc(len);
    *set = 0;
    for (p = items; p->text; ++p) {
      set = lsx_realloc(set, len += 2 + strlen(p->text));
      strcat(set, ", "); strcat(set, p->text);
    }
    lsx_fail("-%c: `%s' is not one of: %s.", c, lsx_optarg, set + 2);
    free(set);
    return INT_MAX;
  }
  return p->value;
}

char const * lsx_sigfigs3(double number)
{
  static char const symbols[] = "\0kMGTPEZY";
  static char string[16][10];   /* FIXME: not thread-safe */
  static unsigned n;            /* ditto */
  unsigned a, b, c;
  sprintf(string[n = (n+1) & 15], "%#.3g", number);
  switch (sscanf(string[n], "%u.%ue%u", &a, &b, &c)) {
    case 2: if (b) return string[n]; /* Can fall through */
    case 1: c = 2; break;
    case 3: a = 100*a + b; break;
  }
  if (c < array_length(symbols) * 3 - 3) switch (c%3) {
    case 0: sprintf(string[n], "%u.%02u%c", a/100,a%100, symbols[c/3]); break;
    case 1: sprintf(string[n], "%u.%u%c"  , a/10 ,a%10 , symbols[c/3]); break;
    case 2: sprintf(string[n], "%u%c"     , a          , symbols[c/3]); break;
  }
  return string[n];
}

char const * lsx_sigfigs3p(double percentage)
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

