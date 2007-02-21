/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * libSoX null file format driver.
 * Written by Carsten Borchardt 
 * The author is not responsible for the consequences 
 * of using this software
 */

#include "sox_i.h"
#include <string.h>

static int sox_nulstartread(ft_t ft) 
{
  /* If format parameters are not given, set somewhat arbitrary
   * (but commonly used) defaults: */
  if (ft->signal.rate     == 0) ft->signal.rate     = 44100;
  if (ft->signal.channels == 0) ft->signal.channels = 2;
  if (ft->signal.size     ==-1) ft->signal.size     = SOX_SIZE_16BIT;
  if (ft->signal.encoding == SOX_ENCODING_UNKNOWN) ft->signal.encoding = SOX_ENCODING_SIGN2;

  return SOX_SUCCESS;
}

static sox_size_t sox_nulread(ft_t ft UNUSED, sox_sample_t *buf, sox_size_t len) 
{
  /* Reading from null generates silence i.e. (sox_sample_t)0. */
  memset(buf, 0, sizeof(sox_sample_t) * len);
  return len; /* Return number of samples "read". */
}

static sox_size_t sox_nulwrite(ft_t ft UNUSED, const sox_sample_t *buf UNUSED, sox_size_t len) 
{
  /* Writing to null just discards the samples */
  return len; /* Return number of samples "written". */
}

static const char *nulnames[] = {
  "null",
  "nul",  /* For backwards compatibility with scripts that used -t nul. */
  NULL,
};

static sox_format_t sox_nul_format = {
  nulnames,
  NULL,
  SOX_FILE_DEVICE | SOX_FILE_PHONY | SOX_FILE_NOSTDIO,
  sox_nulstartread,
  sox_nulread,
  sox_format_nothing,
  sox_format_nothing,
  sox_nulwrite,
  sox_format_nothing,
  sox_format_nothing_seek
};

const sox_format_t *sox_nul_format_fn(void)
{
    return &sox_nul_format;
}
