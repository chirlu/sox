/* libSoX internal functions that apply to both formats and effects
 * All public functions & data are prefixed with lsx_ .
 *
 * Copyright 1998-2008 Chris Bagwell and SoX Contributors
 * Copyright 1991 Lance Norskog And Sundry Contributors
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"
#include <ctype.h>

#ifndef HAVE_STRCASECMP
int lsx_strcasecmp(const char * s1, const char * s2)
{
  while (*s1 && (toupper(*s1) == toupper(*s2)))
    s1++, s2++;
  return toupper(*s1) - toupper(*s2);
}

int lsx_strncasecmp(char const * s1, char const * s2, size_t n)
{
  while (--n && *s1 && (toupper(*s1) == toupper(*s2)))
    s1++, s2++;
  return toupper(*s1) - toupper(*s2);
}
#endif

