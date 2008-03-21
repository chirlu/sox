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
#include <string.h>

void *lsx_realloc(void *ptr, size_t newsize);
#define lsx_malloc(size) lsx_realloc(NULL, (size))
#define lsx_calloc(n,s) ((n)*(s)? memset(lsx_malloc((n)*(s)),0,(n)*(s)) : NULL)
#define lsx_strdup(s) ((s)? strcpy((char *)lsx_malloc(strlen(s) + 1), s) : NULL)

#endif
