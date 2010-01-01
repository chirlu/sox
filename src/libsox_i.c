/* libSoX internal functions that apply to both formats and effects
 * All public functions & data are prefixed with lsx_ .
 *
 * Copyright (c) 2008 robs@users.sourceforge.net
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

#ifdef HAVE_IO_H
  #include <io.h>
#endif

#ifdef HAVE_UNISTD_H
  #include <unistd.h>
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
  #define MKTEMP_X _O_BINARY|_O_TEMPORARY
#else
  #define MKTEMP_X 0
#endif

#ifndef HAVE_MKSTEMP
  #include <fcntl.h>
  #include <sys/types.h>
  #include <sys/stat.h>
  #define mkstemp(t) open(mktemp(t), MKTEMP_X|O_RDWR|O_TRUNC|O_CREAT, S_IREAD|S_IWRITE)
  #define FAKE_MKSTEMP "fake "
#else
  #define FAKE_MKSTEMP
#endif

FILE * lsx_tmpfile(void)
{
  if (sox_globals.tmp_path) {
    /* Emulate tmpfile (delete on close); tmp dir is given tmp_path: */
    char const * const end = "/libSoX.tmp.XXXXXX";
    char * name = lsx_malloc(strlen(sox_globals.tmp_path) + strlen(end) + 1);
    int fildes;
    strcpy(name, sox_globals.tmp_path);
    strcat(name, end);
    fildes = mkstemp(name);
#ifdef HAVE_UNISTD_H
    lsx_debug(FAKE_MKSTEMP "mkstemp, name=%s (unlinked)", name);
    unlink(name);
#else
    lsx_debug(FAKE_MKSTEMP "mkstemp, name=%s (O_TEMPORARY)", name);
#endif
    free(name);
    return fildes == -1? NULL : fdopen(fildes, "w+");
  }

  /* Use standard tmpfile (delete on close); tmp dir is undefined: */
  lsx_debug("tmpfile()");
  return tmpfile();
}
