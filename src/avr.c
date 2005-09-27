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

#include "st_i.h"

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


int st_avrstartread(ft_t ft) 
{
  avr_t avr = (avr_t)ft->priv;
  int rc;

  /* AVR is a Big Endian format.  Swap whats read in on Little */
  /* Endian machines.                                          */
  if (ST_IS_LITTLEENDIAN)
  {
          ft->swap = ft->swap ? 0 : 1;
  }

  st_reads(ft, avr->magic, 4);

  if (strncmp (avr->magic, AVR_MAGIC, 4)) {
    st_fail_errno(ft,ST_EHDR,"AVR: unknown header");
    return(ST_EOF);
  }

  st_readbuf(ft, avr->name, 1, sizeof(avr->name));

  st_readw (ft, &(avr->mono));
  if (avr->mono) {
    ft->info.channels = 2;
  }
  else {
    ft->info.channels = 1;
  }

  st_readw (ft, &(avr->rez));
  if (avr->rez == 8) {
    ft->info.size = ST_SIZE_BYTE;
  }
  else if (avr->rez == 16) {
    ft->info.size = ST_SIZE_WORD;
  }
  else {
    st_fail_errno(ft,ST_EFMT,"AVR: unsupported sample resolution");
    return(ST_EOF);
  }

  st_readw (ft, &(avr->sign));
  if (avr->sign) {
    ft->info.encoding = ST_ENCODING_SIGN2;
  }
  else {
    ft->info.encoding = ST_ENCODING_UNSIGNED;
  }

  st_readw (ft, &(avr->loop));

  st_readw (ft, &(avr->midi));

  st_readdw (ft, &(avr->rate));
  /*
   * No support for AVRs created by ST-Replay,
   * Replay Proffesional and PRO-Series 12.
   *
   * Just masking the upper byte out.
   */
  ft->info.rate = (avr->rate & 0x00ffffff);

  st_readdw (ft, &(avr->size));

  st_readdw (ft, &(avr->lbeg));

  st_readdw (ft, &(avr->lend));

  st_readw (ft, &(avr->res1));

  st_readw (ft, &(avr->res2));

  st_readw (ft, &(avr->res3));

  st_readbuf(ft, avr->ext, 1, sizeof(avr->ext));

  st_readbuf(ft, avr->user, 1, sizeof(avr->user));

  rc = st_rawstartread (ft);
  if (rc)
      return rc;

  return(ST_SUCCESS);
}

int st_avrstartwrite(ft_t ft) 
{
  avr_t avr = (avr_t)ft->priv;
  int rc;

  /* AVR is a Big Endian format.  Swap whats read in on Little */
  /* Endian machines.                                          */
  if (ST_IS_LITTLEENDIAN)
  {
          ft->swap = ft->swap ? 0 : 1;
  }

  if (!ft->seekable) {
    st_fail_errno(ft,ST_EOF,"AVR: file is not seekable");
    return(ST_EOF);
  }

  rc = st_rawstartwrite (ft);
  if (rc)
      return rc;

  /* magic */
  st_writes(ft, AVR_MAGIC);

  /* name */
  st_writeb(ft, 0);
  st_writeb(ft, 0);
  st_writeb(ft, 0);
  st_writeb(ft, 0);
  st_writeb(ft, 0);
  st_writeb(ft, 0);
  st_writeb(ft, 0);
  st_writeb(ft, 0);

  /* mono */
  if (ft->info.channels == 1) {
    st_writew (ft, 0);
  }
  else if (ft->info.channels == 2) {
    st_writew (ft, 0xffff);
  }
  else {
    st_fail_errno(ft,ST_EFMT,"AVR: number of channels not supported");
    return(0);
  }

  /* rez */
  if (ft->info.size == ST_SIZE_BYTE) {
    st_writew (ft, 8);
  }
  else if (ft->info.size == ST_SIZE_WORD) {
    st_writew (ft, 16);
  }
  else {
    st_fail_errno(ft,ST_EFMT,"AVR: unsupported sample resolution");
    return(ST_EOF);
  }

  /* sign */
  if (ft->info.encoding == ST_ENCODING_SIGN2) {
    st_writew (ft, 0xffff);
  }
  else if (ft->info.encoding == ST_ENCODING_UNSIGNED) {
    st_writew (ft, 0);
  }
  else {
    st_fail_errno(ft,ST_EFMT,"AVR: unsupported encoding");
    return(ST_EOF);
  }

  /* loop */
  st_writew (ft, 0xffff);

  /* midi */
  st_writew (ft, 0xffff);

  /* rate */
  st_writedw (ft, ft->info.rate);

  /* size */
  /* Don't know the size yet. */
  st_writedw (ft, 0);

  /* lbeg */
  st_writedw (ft, 0);

  /* lend */
  /* Don't know the size yet, so we can't set lend, either. */
  st_writedw (ft, 0);

  /* res1 */
  st_writew (ft, 0);

  /* res2 */
  st_writew (ft, 0);

  /* res3 */
  st_writew (ft, 0);

  /* ext */
  st_writebuf(ft, (void *)"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 1, sizeof(avr->ext));

  /* user */
  st_writebuf(ft, 
           (void *)"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
           "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
           "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
           "\0\0\0\0", 1, sizeof (avr->user));

  return(ST_SUCCESS);
}

st_ssize_t st_avrwrite(ft_t ft, st_sample_t *buf, st_ssize_t nsamp) 
{
  avr_t avr = (avr_t)ft->priv;

  avr->size += nsamp;

  return (st_rawwrite (ft, buf, nsamp));
}

int st_avrstopwrite(ft_t ft) 
{
  avr_t avr = (avr_t)ft->priv;
  int rc;

  int size = avr->size / ft->info.channels;

  rc = st_rawstopwrite(ft);
  if (rc)
      return rc;

  /* Fix size */
  st_seeki(ft, 26L, SEEK_SET);
  st_writedw (ft, size);

  /* Fix lend */
  st_seeki(ft, 34L, SEEK_SET);
  st_writedw (ft, size);

  return(ST_SUCCESS);
}
