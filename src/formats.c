/*
 * Originally created: July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"

/* File format handlers. */

#ifdef HAVE_LTDL_H
/* FIXME: Use a vector, not a fixed-size array */
  #define MAX_FORMATS 256
  unsigned sox_formats = 0;
  sox_format_tab_t sox_format_fns[MAX_FORMATS];
#else
  #define FORMAT(f) extern sox_format_t const * sox_##f##_format_fn(void);
  #include "formats.h"
  #undef FORMAT
  sox_format_tab_t sox_format_fns[] = {
  #define FORMAT(f) {0, sox_##f##_format_fn},
  #include "formats.h"
  #undef FORMAT
  };
  unsigned sox_formats = array_length(sox_format_fns);
#endif 
