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

#ifndef XMALLOC_H

#include <stddef.h>

#define xmalloc(size) xrealloc(NULL, (size))

void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t newsize);
char *xstrdup(const char *s);

#endif
