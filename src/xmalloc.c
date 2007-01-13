/* Memory allocation functions
   Copyright (c) 2005-2006 Reuben Thomas.
   All rights reserved.

   This file is part of SoX.

   SoX is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2, or (at your option) any later
   version.

   SoX is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with SoX; see the file COPYING.  If not, write to the Free
   Software Foundation, Fifth Floor, 51 Franklin Street, Boston, MA
   02111-1301, USA.  */

#include "st_i.h"

#include <stdlib.h>
#include <string.h>

#include "xmalloc.h"


/*
 * Resize an allocated memory area; abort if not possible.
 */
void *xrealloc(void *ptr, size_t newsize)
{
  /* Behaviour in this case is unspecified for malloc */
  if (ptr && newsize == 0)
    return NULL;

  if ((ptr = realloc(ptr, newsize)) == NULL) {
    st_fail("out of memory");
    exit(2);
  }

  return ptr;
}

/*
 * Perform a calloc; abort if not possible.
 */
void *xcalloc(size_t nmemb, size_t size)
{
  void *ptr = calloc(nmemb, size);

  if (ptr == NULL) {
    st_fail("out of memory");
    exit(2);
  }

  return ptr;
}

/*
 * Perform a strdup; abort if not possible.
 */
char *xstrdup(const char *s)
{
  char * t;

  if (s == NULL)
    return NULL;

  t = strdup(s);
  if (t == NULL) {
    st_fail("out of memory");
    exit(2);
  }

  return t;
}
