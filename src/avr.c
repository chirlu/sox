/*

    AVR file format driver for SOX
    Copyright (C) 1999 Jan Paul Schmidt <jps@fundament.org>

    This source code is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This source code is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <string.h>

#include "st.h"

#define AVR_MAGIC "2BIT"

/* Taken from the Audio File Formats FAQ */

typedef struct avrstuff {
  char magic [4];      /* 2BIT */
  char name [8];       /* null-padded sample name */
  unsigned short mono; /* 0 = mono, 0xffff = stereo */
  unsigned short rez;  /* 8 = 8 bit, 16 = 16 bit */
  unsigned short sign; /* 0 = unsigned, 0xffff = signed */
  unsigned short loop; /* 0 = no loop, 0xffff = looping sample */
  unsigned short midi; /* 0xffff = no MIDI note assigned,
			  0xffXX = single key note assignment
			  0xLLHH = key split, low/hi note */
  ULONG int rate;	       /* sample frequency in hertz */
  ULONG size;	       /* sample length in bytes or words (see rez) */
  ULONG lbeg;	       /* offset to start of loop in bytes or words.
			  set to zero if unused. */
  ULONG lend;	       /* offset to end of loop in bytes or words.
			  set to sample length if unused. */
  unsigned short res1; /* Reserved, MIDI keyboard split */
  unsigned short res2; /* Reserved, sample compression */
  unsigned short res3; /* Reserved */
  char ext[20];	       /* Additional filename space, used
			  if (name[7] != 0) */
  char user[64];       /* User defined. Typically ASCII message. */
} *avr_t;



/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *	Find out sampling rate, 
 *	size and style of samples, 
 *	mono/stereo/quad.
 */

void
avrstartread(ft) 
ft_t ft;
{
  avr_t	avr = (avr_t)ft->priv;
  int littlendian = 1;
  char *endptr;

  /* AVR is a Big Endian format.  Swap whats read in on Little */
  /* Endian machines.					       */
  endptr = (char *) &littlendian;
  if (*endptr)
  {
	  ft->swap = ft->swap ? 0 : 1;
  }

  rawstartread (ft);

  fread (avr->magic, 1, sizeof (avr->magic), ft->fp);

  if (strncmp (avr->magic, AVR_MAGIC, 4)) {
    fail ("AVR: unknown header");
  }

  fread (avr->name, 1, sizeof (avr->name), ft->fp);

  avr->mono = rshort (ft);
  if (avr->mono) {
    ft->info.channels = 2;
  }
  else {
    ft->info.channels = 1;
  }

  avr->rez = rshort (ft);
  if (avr->rez == 8) {
    ft->info.size = BYTE;
  }
  else if (avr->rez == 16) {
    ft->info.size = WORD;
  }
  else {
    fail ("AVR: unsupported sample resolution");
  }

  avr->sign = rshort (ft);
  if (avr->sign) {
    ft->info.style = SIGN2;
  }
  else {
    ft->info.style = UNSIGNED;
  }

  avr->loop = rshort (ft);

  avr->midi = rshort (ft);

  avr->rate = rlong (ft);
  /*
   * No support for AVRs created by ST-Replay,
   * Replay Proffesional and PRO-Series 12.
   *
   * Just masking the upper byte out.
   */
  ft->info.rate = (avr->rate & 0x00ffffff);

  avr->size = rlong (ft);

  avr->lbeg = rlong (ft);

  avr->lend = rlong (ft);

  avr->res1 = rshort (ft);

  avr->res2 = rshort (ft);

  avr->res3 = rshort (ft);

  fread (avr->ext, 1, sizeof (avr->ext), ft->fp);

  fread (avr->user, 1, sizeof (avr->user), ft->fp);
}



void
avrstartwrite(ft) 
ft_t ft;
{
  avr_t	avr = (avr_t)ft->priv;
  int littlendian = 1;
  char *endptr;

  /* AVR is a Big Endian format.  Swap whats read in on Little */
  /* Endian machines.					       */
  endptr = (char *) &littlendian;
  if (*endptr)
  {
	  ft->swap = ft->swap ? 0 : 1;
  }

  if (!ft->seekable) {
    fail ("AVR: file is not seekable");
  }

  rawstartwrite (ft);

  /* magic */
  fwrite (AVR_MAGIC, 1, sizeof (avr->magic), ft->fp);

  /* name */
  fwrite ("\0\0\0\0\0\0\0\0", 1, sizeof (avr->name), ft->fp);

  /* mono */
  if (ft->info.channels == 1) {
    wshort (ft, 0);
  }
  else if (ft->info.channels == 2) {
    wshort (ft, 0xffff);
  }
  else {
    fail ("AVR: number of channels not supported");
  }

  /* rez */
  if (ft->info.size == BYTE) {
    wshort (ft, 8);
  }
  else if (ft->info.size == WORD) {
    wshort (ft, 16);
  }
  else {
    fail ("AVR: unsupported sample resolution");
  }

  /* sign */
  if (ft->info.style == SIGN2) {
    wshort (ft, 0xffff);
  }
  else if (ft->info.style == UNSIGNED) {
    wshort (ft, 0);
  }
  else {
    fail ("AVR: unsupported style");
  }

  /* loop */
  wshort (ft, 0xffff);

  /* midi */
  wshort (ft, 0xffff);

  /* rate */
  wlong (ft, ft->info.rate);

  /* size */
  /* Don't know the size yet. */
  wlong (ft, 0);

  /* lbeg */
  wlong (ft, 0);

  /* lend */
  /* Don't know the size yet, so we can't set lend, either. */
  wlong (ft, 0);

  /* res1 */
  wshort (ft, 0);

  /* res2 */
  wshort (ft, 0);

  /* res3 */
  wshort (ft, 0);

  /* ext */
  fwrite ("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 1, sizeof (avr->ext),
          ft->fp);

  /* user */
  fwrite ("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
          "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
          "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
          "\0\0\0\0", 1, sizeof (avr->user), ft->fp);
}



void
avrwrite(ft, buf, nsamp) 
ft_t ft;
LONG *buf, nsamp;
{
  avr_t	avr = (avr_t)ft->priv;

  avr->size += nsamp;

  rawwrite (ft, buf, nsamp);
}



void
avrstopwrite(ft) 
ft_t ft;
{
  avr_t	avr = (avr_t)ft->priv;

  int size = avr->size / ft->info.channels;

  /* Fix size */
  fseek (ft->fp, 26L, SEEK_SET);
  wlong (ft, size);

  /* Fix lend */
  fseek (ft->fp, 34L, SEEK_SET);
  wlong (ft, size);
}
