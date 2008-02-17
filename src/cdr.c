/*
 * CD Digital Audio format handler: pads to integer number of CDDA sectors
 *
 * David Elliott, Sony Microsystems -  July 5, 1991
 *
 * Copyright 1991 David Elliott And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * David Elliott And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

#include "sox_i.h"

#define SECTOR_SIZE   (2352 / 2)
#define samples       (*(sox_size_t *)ft->priv)

static int start(sox_format_t * ft) 
{
  ft->signal.rate = 44100;
  ft->signal.size = SOX_SIZE_16BIT;
  ft->signal.encoding = SOX_ENCODING_SIGN2;
  ft->signal.channels = 2;

  if (ft->mode == 'r' && ft->seekable) /* Need length for seeking */
    ft->length = sox_filelength(ft)/SOX_SIZE_16BIT;
  
  return SOX_SUCCESS;
}

static sox_size_t write(
    sox_format_t * ft, const sox_sample_t *buf, sox_size_t len) 
{
  samples += len;
  return sox_rawwrite(ft, buf, len);
}

static int stopwrite(sox_format_t * ft) 
{
  sox_size_t i = samples % SECTOR_SIZE;

  if (i) while (i++ < SECTOR_SIZE)     /* Pad with silence to multiple */
    sox_writew(ft, 0);                 /* of SECTOR_SIZE samples. */

  return SOX_SUCCESS;
}

const sox_format_handler_t *sox_cdr_format_fn(void);
const sox_format_handler_t *sox_cdr_format_fn(void)
{
  static const char * names[] = {"cdda", "cdr", NULL};

  static sox_format_handler_t handler = {
    names, SOX_FILE_BIG_END,
    start, sox_rawread, NULL,
    start, write, stopwrite,
    sox_rawseek
  };

  return &handler;
}
