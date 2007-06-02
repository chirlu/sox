/*
 * libSoX raw file format
 *
 * Copyright 1991-2007 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"
 
static int raw_start(sox_format_t * ft) {
  return sox_rawstart(ft,sox_false,sox_false,SOX_ENCODING_UNKNOWN,-1);
}

const sox_format_handler_t *sox_raw_format_fn(void);

const sox_format_handler_t *sox_raw_format_fn(void)
{
  static char const * names[] = {"raw", NULL};
  static sox_format_handler_t handler = {
    names, SOX_FILE_SEEK,
    raw_start, sox_rawread , sox_format_nothing,
    raw_start, sox_rawwrite, sox_format_nothing,
    sox_rawseek
  };
  return &handler;
}
