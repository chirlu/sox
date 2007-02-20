/*

    AVR file format driver for SoX
    Copyright (C) 1999 Jan Paul Schmidt <jps@fundament.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
 
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 */

#include <stdio.h>
#include <string.h>

#include "sox_i.h"

#define AVR_MAGIC "2BIT"

/* Taken from the Audio File Formats FAQ */

typedef struct avrstuff {
  char magic [5];      /* 2BIT */
  char name [8];       /* null-padded sample name */
  unsigned short mono; /* 0 = mono, 0xffff = stereo */
  unsigned short rez;  /* 8 = 8 bit, 16 = 16 bit */
  unsigned short sign; /* 0 = unsigned, 0xffff = signed */
  unsigned short loop; /* 0 = no loop, 0xffff = looping sample */
  unsigned short midi; /* 0xffff = no MIDI note assigned,
                          0xffXX = single key note assignment
                          0xLLHH = key split, low/hi note */
  uint32_t rate;       /* sample frequency in hertz */
  uint32_t size;       /* sample length in bytes or words (see rez) */
  uint32_t lbeg;       /* offset to start of loop in bytes or words.
                          set to zero if unused. */
  uint32_t lend;       /* offset to end of loop in bytes or words.
                          set to sample length if unused. */
  unsigned short res1; /* Reserved, MIDI keyboard split */
  unsigned short res2; /* Reserved, sample compression */
  unsigned short res3; /* Reserved */
  char ext[20];        /* Additional filename space, used
                          if (name[7] != 0) */
  char user[64];       /* User defined. Typically ASCII message. */
} *avr_t;



/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *      Find out sampling rate, 
 *      size and encoding of samples, 
 *      mono/stereo/quad.
 */


static int sox_avrstartread(ft_t ft) 
{
  avr_t avr = (avr_t)ft->priv;
  int rc;

  sox_reads(ft, avr->magic, 4);

  if (strncmp (avr->magic, AVR_MAGIC, 4)) {
    sox_fail_errno(ft,SOX_EHDR,"AVR: unknown header");
    return(SOX_EOF);
  }

  sox_readbuf(ft, avr->name, 1, sizeof(avr->name));

  sox_readw (ft, &(avr->mono));
  if (avr->mono) {
    ft->signal.channels = 2;
  }
  else {
    ft->signal.channels = 1;
  }

  sox_readw (ft, &(avr->rez));
  if (avr->rez == 8) {
    ft->signal.size = SOX_SIZE_BYTE;
  }
  else if (avr->rez == 16) {
    ft->signal.size = SOX_SIZE_16BIT;
  }
  else {
    sox_fail_errno(ft,SOX_EFMT,"AVR: unsupported sample resolution");
    return(SOX_EOF);
  }

  sox_readw (ft, &(avr->sign));
  if (avr->sign) {
    ft->signal.encoding = SOX_ENCODING_SIGN2;
  }
  else {
    ft->signal.encoding = SOX_ENCODING_UNSIGNED;
  }

  sox_readw (ft, &(avr->loop));

  sox_readw (ft, &(avr->midi));

  sox_readdw (ft, &(avr->rate));
  /*
   * No support for AVRs created by ST-Replay,
   * Replay Proffesional and PRO-Series 12.
   *
   * Just masking the upper byte out.
   */
  ft->signal.rate = (avr->rate & 0x00ffffff);

  sox_readdw (ft, &(avr->size));

  sox_readdw (ft, &(avr->lbeg));

  sox_readdw (ft, &(avr->lend));

  sox_readw (ft, &(avr->res1));

  sox_readw (ft, &(avr->res2));

  sox_readw (ft, &(avr->res3));

  sox_readbuf(ft, avr->ext, 1, sizeof(avr->ext));

  sox_readbuf(ft, avr->user, 1, sizeof(avr->user));

  rc = sox_rawstartread (ft);
  if (rc)
      return rc;

  return(SOX_SUCCESS);
}

static int sox_avrstartwrite(ft_t ft) 
{
  avr_t avr = (avr_t)ft->priv;
  int rc;

  if (!ft->seekable) {
    sox_fail_errno(ft,SOX_EOF,"AVR: file is not seekable");
    return(SOX_EOF);
  }

  rc = sox_rawstartwrite (ft);
  if (rc)
      return rc;

  /* magic */
  sox_writes(ft, AVR_MAGIC);

  /* name */
  sox_writeb(ft, 0);
  sox_writeb(ft, 0);
  sox_writeb(ft, 0);
  sox_writeb(ft, 0);
  sox_writeb(ft, 0);
  sox_writeb(ft, 0);
  sox_writeb(ft, 0);
  sox_writeb(ft, 0);

  /* mono */
  if (ft->signal.channels == 1) {
    sox_writew (ft, 0);
  }
  else if (ft->signal.channels == 2) {
    sox_writew (ft, 0xffff);
  }
  else {
    sox_fail_errno(ft,SOX_EFMT,"AVR: number of channels not supported");
    return(0);
  }

  /* rez */
  if (ft->signal.size == SOX_SIZE_BYTE) {
    sox_writew (ft, 8);
  }
  else if (ft->signal.size == SOX_SIZE_16BIT) {
    sox_writew (ft, 16);
  }
  else {
    sox_fail_errno(ft,SOX_EFMT,"AVR: unsupported sample resolution");
    return(SOX_EOF);
  }

  /* sign */
  if (ft->signal.encoding == SOX_ENCODING_SIGN2) {
    sox_writew (ft, 0xffff);
  }
  else if (ft->signal.encoding == SOX_ENCODING_UNSIGNED) {
    sox_writew (ft, 0);
  }
  else {
    sox_fail_errno(ft,SOX_EFMT,"AVR: unsupported encoding");
    return(SOX_EOF);
  }

  /* loop */
  sox_writew (ft, 0xffff);

  /* midi */
  sox_writew (ft, 0xffff);

  /* rate */
  sox_writedw (ft, ft->signal.rate);

  /* size */
  /* Don't know the size yet. */
  sox_writedw (ft, 0);

  /* lbeg */
  sox_writedw (ft, 0);

  /* lend */
  /* Don't know the size yet, so we can't set lend, either. */
  sox_writedw (ft, 0);

  /* res1 */
  sox_writew (ft, 0);

  /* res2 */
  sox_writew (ft, 0);

  /* res3 */
  sox_writew (ft, 0);

  /* ext */
  sox_writebuf(ft, (void *)"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 1, sizeof(avr->ext));

  /* user */
  sox_writebuf(ft, 
           (void *)"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
           "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
           "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
           "\0\0\0\0", 1, sizeof (avr->user));

  return(SOX_SUCCESS);
}

static sox_size_t sox_avrwrite(ft_t ft, const sox_sample_t *buf, sox_size_t nsamp) 
{
  avr_t avr = (avr_t)ft->priv;

  avr->size += nsamp;

  return (sox_rawwrite (ft, buf, nsamp));
}

static int sox_avrstopwrite(ft_t ft) 
{
  avr_t avr = (avr_t)ft->priv;
  int rc;

  int size = avr->size / ft->signal.channels;

  rc = sox_rawstopwrite(ft);
  if (rc)
      return rc;

  /* Fix size */
  sox_seeki(ft, 26, SEEK_SET);
  sox_writedw (ft, size);

  /* Fix lend */
  sox_seeki(ft, 34, SEEK_SET);
  sox_writedw (ft, size);

  return(SOX_SUCCESS);
}

static const char *avrnames[] = {
  "avr",
  NULL
};

static sox_format_t sox_avr_format = {
  avrnames,
  NULL,
  SOX_FILE_BIG_END,
  sox_avrstartread,
  sox_rawread,
  sox_format_nothing,
  sox_avrstartwrite,
  sox_avrwrite,
  sox_avrstopwrite,
  sox_format_nothing_seek
};

const sox_format_t *sox_avr_format_fn(void)
{
    return &sox_avr_format;
}
