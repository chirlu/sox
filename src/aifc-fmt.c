/*
 * Export module for AIFF-C format, implemented in aiff.c.
 *
 * Copyright 1991-2007 Guido van Rossum And Sundry Contributors
 * 
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Guido van Rossum And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

#include "sox_i.h"
#include "aiff.h"

static const char *aifcnames[] = {
  "aifc",
  "aiffc",
  NULL
};

static sox_format_handler_t sox_aifc_format = {
  aifcnames,
  SOX_FILE_LOOPS | SOX_FILE_BIG_END,
  sox_aiffstartread,
  sox_aiffread,
  sox_aiffstopread,
  sox_aifcstartwrite,
  sox_aiffwrite,
  sox_aifcstopwrite,
  sox_aiffseek
};

const sox_format_handler_t *sox_aifc_format_fn(void);

const sox_format_handler_t *sox_aifc_format_fn(void)
{
    return &sox_aifc_format;
}
