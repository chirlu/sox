/*
 * Export module for AIFF format, implemented in aiff.c.
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

static const char *aiffnames[] = {
  "aiff",
  "aif",
  NULL
};

static sox_format_handler_t sox_aiff_format = {
  aiffnames,
  SOX_FILE_LOOPS | SOX_FILE_BIG_END,
  sox_aiffstartread,
  sox_aiffread,
  sox_aiffstopread,
  sox_aiffstartwrite,
  sox_aiffwrite,
  sox_aiffstopwrite,
  sox_aiffseek
};

const sox_format_handler_t *sox_aiff_format_fn(void);

const sox_format_handler_t *sox_aiff_format_fn(void)
{
    return &sox_aiff_format;
}
