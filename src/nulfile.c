/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools null file format driver.
 * Written by Carsten Borchardt 
 * The author is not responsible for the consequences 
 * of using this software
 */

#include "st_i.h"
#include <string.h>

static int st_nulstartread(ft_t ft) 
{
  /* If format parameters are not given, set somewhat arbitrary
   * (but commonly used) defaults: */
  if (ft->signal.rate     == 0) ft->signal.rate     = 44100;
  if (ft->signal.channels == 0) ft->signal.channels = 2;
  if (ft->signal.size     ==-1) ft->signal.size     = ST_SIZE_16BIT;
  if (ft->signal.encoding == ST_ENCODING_UNKNOWN) ft->signal.encoding = ST_ENCODING_SIGN2;

  return ST_SUCCESS;
}

static st_size_t st_nulread(ft_t ft UNUSED, st_sample_t *buf, st_size_t len) 
{
  /* Reading from null generates silence i.e. (st_sample_t)0. */
  memset(buf, 0, sizeof(st_sample_t) * len);
  return len; /* Return number of samples "read". */
}

static st_size_t st_nulwrite(ft_t ft UNUSED, const st_sample_t *buf UNUSED, st_size_t len) 
{
  /* Writing to null just discards the samples */
  return len; /* Return number of samples "written". */
}

static const char *nulnames[] = {
  "null",
  "nul",  /* For backwards compatibility with scripts that used -t nul. */
  NULL,
};

static st_format_t st_nul_format = {
  nulnames,
  NULL,
  ST_FILE_DEVICE | ST_FILE_PHONY | ST_FILE_NOSTDIO,
  st_nulstartread,
  st_nulread,
  st_format_nothing,
  st_format_nothing,
  st_nulwrite,
  st_format_nothing,
  st_format_nothing_seek
};

const st_format_t *st_nul_format_fn(void)
{
    return &st_nul_format;
}
