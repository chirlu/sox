/*
 * Originally created: July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"
#include <string.h>

/* File format handlers. */

#ifdef HAVE_LIBLTDL
/* FIXME: Use a vector, not a fixed-size array */
  #define MAX_FORMATS 256
  unsigned sox_formats = 0;
  sox_format_tab_t sox_format_fns[MAX_FORMATS];
#else
  #define FORMAT(f) extern sox_format_handler_t const * sox_##f##_format_fn(void);
  #include "formats.h"
  #undef FORMAT
  sox_format_tab_t sox_format_fns[] = {
  #define FORMAT(f) {0, sox_##f##_format_fn},
  #include "formats.h"
  #undef FORMAT
  };
  unsigned sox_formats = array_length(sox_format_fns);
#endif 

/* Find a named format in the formats library */
sox_format_handler_t const * sox_find_format(char const * name, sox_bool no_dev)
{
  sox_size_t f, n;

  if (name) for (f = 0; f < sox_formats; ++f) {
    sox_format_handler_t const * fh = sox_format_fns[f].fn();

    if (!(no_dev && (fh->flags & SOX_FILE_DEVICE)))
      for (n = 0; fh->names[n]; ++n)
        if (!strcasecmp(fh->names[n], name))
          return fh;                 /* Found it. */
  }
  return NULL;
}
