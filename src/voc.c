/*
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 *
 * September 8, 1993
 * Copyright 1993 T. Allen Grider - for changes to support block type 9
 * and word sized samples.  Same caveats and disclaimer as above.
 *
 * February 22, 1996
 * by Chris Bagwell (cbagwell@sprynet.com)
 * Added support for block type 8 (extended) which allows for 8-bit stereo 
 * files.  Added support for saving stereo files and 16-bit files.
 * Added VOC format info from audio format FAQ so I don't have to keep
 * looking around for it.
 */

/*
 * Sound Tools Sound Blaster VOC handler sources.
 */

/*------------------------------------------------------------------------
The following is taken from the Audio File Formats FAQ dated 2-Jan-1995
and submitted by Guido van Rossum <guido@cwi.nl>.
--------------------------------------------------------------------------
Creative Voice (VOC) file format
--------------------------------

From: galt@dsd.es.com

(byte numbers are hex!)

    HEADER (bytes 00-19)
    Series of DATA BLOCKS (bytes 1A+) [Must end w/ Terminator Block]

- ---------------------------------------------------------------

HEADER:
-------
     byte #     Description
     ------     ------------------------------------------
     00-12      "Creative Voice File"
     13         1A (eof to abort printing of file)
     14-15      Offset of first datablock in .voc file (std 1A 00
                in Intel Notation)
     16-17      Version number (minor,major) (VOC-HDR puts 0A 01)
     18-19      2's Comp of Ver. # + 1234h (VOC-HDR puts 29 11)

- ---------------------------------------------------------------

DATA BLOCK:
-----------

   Data Block:  TYPE(1-byte), SIZE(3-bytes), INFO(0+ bytes)
   NOTE: Terminator Block is an exception -- it has only the TYPE byte.

      TYPE   Description     Size (3-byte int)   Info
      ----   -----------     -----------------   -----------------------
      00     Terminator      (NONE)              (NONE)
      01     Sound data      2+length of data    *
      02     Sound continue  length of data      Voice Data
      03     Silence         3                   **
      04     Marker          2                   Marker# (2 bytes)
      05     ASCII           length of string    null terminated string
      06     Repeat          2                   Count# (2 bytes)
      07     End repeat      0                   (NONE)
      08     Extended        4                   ***

      *Sound Info Format:       **Silence Info Format:
       ---------------------      ----------------------------
       00   Sample Rate           00-01  Length of silence - 1
       01   Compression Type      02     Sample Rate
       02+  Voice Data

    ***Extended Info Format:
       ---------------------
       00-01  Time Constant: Mono: 65536 - (256000000/sample_rate)
                             Stereo: 65536 - (25600000/(2*sample_rate))
       02     Pack
       03     Mode: 0 = mono
                    1 = stereo


  Marker#           -- Driver keeps the most recent marker in a status byte
  Count#            -- Number of repetitions + 1
                         Count# may be 1 to FFFE for 0 - FFFD repetitions
                         or FFFF for endless repetitions
  Sample Rate       -- SR byte = 256-(1000000/sample_rate)
  Length of silence -- in units of sampling cycle
  Compression Type  -- of voice data
                         8-bits    = 0
                         4-bits    = 1
                         2.6-bits  = 2
                         2-bits    = 3
                         Multi DAC = 3+(# of channels) [interesting--
                                       this isn't in the developer's manual]

Detailed description of new data blocks (VOC files version 1.20 and above):

        (Source is fax from Barry Boone at Creative Labs, 405/742-6622)

BLOCK 8 - digitized sound attribute extension, must preceed block 1.
          Used to define stereo, 8 bit audio
        BYTE bBlockID;       // = 8
        BYTE nBlockLen[3];   // 3 byte length
        WORD wTimeConstant;  // time constant = same as block 1
        BYTE bPackMethod;    // same as in block 1
        BYTE bVoiceMode;     // 0-mono, 1-stereo

        Data is stored left, right

BLOCK 9 - data block that supersedes blocks 1 and 8.  
          Used for stereo, 16 bit.

        BYTE bBlockID;          // = 9
        BYTE nBlockLen[3];      // length 12 plus length of sound
        DWORD dwSamplesPerSec;  // samples per second, not time const.
        BYTE bBitsPerSample;    // e.g., 8 or 16
        BYTE bChannels;         // 1 for mono, 2 for stereo
        WORD wFormat;           // see below
        BYTE reserved[4];       // pad to make block w/o data 
                                // have a size of 16 bytes

        Valid values of wFormat are:

                0x0000  8-bit unsigned PCM
                0x0001  Creative 8-bit to 4-bit ADPCM
                0x0002  Creative 8-bit to 3-bit ADPCM
                0x0003  Creative 8-bit to 2-bit ADPCM
                0x0004  16-bit signed PCM
                0x0006  CCITT a-Law
                0x0007  CCITT u-Law
                0x02000 Creative 16-bit to 4-bit ADPCM

        Data is stored left, right

------------------------------------------------------------------------*/

#include "st.h"
#include <string.h>

/* Private data for VOC file */
typedef struct vocstuff {
	LONG	rest;			/* bytes remaining in current block */
	LONG	rate;			/* rate code (byte) of this chunk */
	int		silent;			/* sound or silence? */
	LONG	srate;			/* rate code (byte) of silence */
	LONG	blockseek;		/* start of current output block */
	LONG	samples;		/* number of samples output */
	int		size;			/* word length of data */
	int		channels;		/* number of sound channels */
	int     extended;       /* Has an extended block been read? */
} *vs_t;

#define	VOC_TERM	0
#define	VOC_DATA	1
#define	VOC_CONT	2
#define	VOC_SILENCE	3
#define	VOC_MARKER	4
#define	VOC_TEXT	5
#define	VOC_LOOP	6
#define	VOC_LOOPEND	7
#define VOC_EXTENDED    8
#define VOC_DATA_16	9

#define	min(a, b)	(((a) < (b)) ? (a) : (b))

void getblock();
void blockstart(P1(ft_t));
void blockstop(P1(ft_t));

void vocstartread(ft) 
ft_t ft;
{
	char header[20];
	vs_t v = (vs_t) ft->priv;
	int sbseek;

	if (! ft->seekable)
		fail("VOC input file must be a file, not a pipe");
	if (fread(header, 1, 20, ft->fp) != 20)
		fail("unexpected EOF in VOC header");
	if (strncmp(header, "Creative Voice File\032", 19))
		fail("VOC file header incorrect");

	sbseek = rlshort(ft);
	fseek(ft->fp, sbseek, 0);

	v->rate = -1;
	v->rest = 0;
	v->extended = 0;
	getblock(ft);
	if (v->rate == -1)
		fail("Input .voc file had no sound!");

	ft->info.size = v->size;
	ft->info.style = UNSIGNED;
	if (v->size == WORD)
	    ft->info.style = SIGN2;
	if (ft->info.channels == -1)
		ft->info.channels = v->channels;
}

LONG vocread(ft, buf, len) 
ft_t ft;
LONG *buf, len;
{
	vs_t v = (vs_t) ft->priv;
	int done = 0;
	
	if (v->rest == 0)
		getblock(ft);
	if (v->rest == 0)
		return 0;

	if (v->silent) {
		/* Fill in silence */
		for(;v->rest && (done < len); v->rest--, done++)
			*buf++ = 0x80000000L;
	} else {
		for(;v->rest && (done < len); v->rest--, done++) {
			LONG l1, l2;
			switch(v->size)
			{
			    case BYTE:
				if ((l1 = getc(ft->fp)) == EOF) {
				    fail("VOC input: short file"); /* */
				    v->rest = 0;
				    return 0;
				}
				l1 ^= 0x80;	/* convert to signed */
				*buf++ = LEFT(l1, 24);
				break;
			    case WORD:
				l1 = getc(ft->fp);
				l2 = getc(ft->fp);
				if (l1 == EOF || l2 == EOF)
				{
				    fail("VOC input: short file");
				    v->rest = 0;
				    return 0;
				}
				l1 = (l2 << 8) | l1; /* already sign2 */
				*buf++ = LEFT(l1, 16);
				v->rest--;
				break;
			}	
		}
	}
	return done;
}

/* nothing to do */
void vocstopread(ft) 
ft_t ft;
{
}

/* When saving samples in VOC format the following outline is followed:
 * If an 8-bit mono sample then use a VOC_DATA header.
 * If an 8-bit stereo sample then use a VOC_EXTENDED header followed
 * by a VOC_DATA header.
 * If a 16-bit sample (either stereo or mono) then save with a 
 * VOC_DATA_16 header.
 *
 * This approach will cause the output to be an its most basic format
 * which will work with the oldest software (eg. an 8-bit mono sample
 * will be able to be played with a really old SB VOC player.)
 */
void vocstartwrite(ft) 
ft_t ft;
{
	vs_t v = (vs_t) ft->priv;

	if (! ft->seekable)
		fail("Output .voc file must be a file, not a pipe");

	v->samples = 0;

	/* File format name and a ^Z (aborts printing under DOS) */
	(void) fwrite("Creative Voice File\032\032", 1, 20, ft->fp);
	wlshort(ft, 26);		/* size of header */
	wlshort(ft, 0x10a);             /* major/minor version number */
	wlshort(ft, 0x1129);		/* checksum of version number */

	if (ft->info.size == BYTE)
	  ft->info.style = UNSIGNED;
	else
	  ft->info.style = SIGN2;
	if (ft->info.channels == -1)
		ft->info.channels = 1;
}

void vocwrite(ft, buf, len) 
ft_t ft;
LONG *buf, len;
{
	vs_t v = (vs_t) ft->priv;
	unsigned char uc;
	int sw;
	
	if (v->samples == 0) {
	  /* No silence packing yet. */
	  v->silent = 0;
	  blockstart(ft);
	}
	v->samples += len;
	while(len--) {
	  if (ft->info.size == BYTE) {
	    uc = RIGHT(*buf++, 24);
	    uc ^= 0x80;
	    putc(uc, ft->fp);
	  } else {
		sw = (int) RIGHT(*buf++, 16);
	    wlshort(ft,sw);
          }
	}
}

void vocstopwrite(ft) 
ft_t ft;
{
	blockstop(ft);
}

/* Voc-file handlers */

/* Read next block header, save info, leave position at start of data */
void
getblock(ft)
ft_t ft;
{
	vs_t v = (vs_t) ft->priv;
	unsigned char uc, block;
	ULONG sblen;
	LONG new_rate;
	int i;

	v->silent = 0;
	while (v->rest == 0) {
		if (feof(ft->fp))
			return;
		block = getc(ft->fp);
		if (block == VOC_TERM)
			return;
		if (feof(ft->fp))
			return;
		uc = getc(ft->fp);
		sblen = uc;
		uc = getc(ft->fp);
		sblen |= ((LONG) uc) << 8;
		uc = getc(ft->fp);
		sblen |= ((LONG) uc) << 16;
		switch(block) {
		case VOC_DATA: 
		        uc = getc(ft->fp);
			/* When DATA block preceeded by an EXTENDED     */
			/* block, the DATA blocks rate value is invalid */
		        if (!v->extended) {
			  if (uc == 0)
			    fail("File %s: Sample rate is zero?");
			  if ((v->rate != -1) && (uc != v->rate))
			    fail("File %s: sample rate codes differ: %d != %d",
				 ft->filename,v->rate, uc);
			  v->rate = uc;
			  ft->info.rate = 1000000.0/(256 - v->rate);
			  v->channels = 1;
			}
			uc = getc(ft->fp);
			if (uc != 0)
			  fail("File %s: only interpret 8-bit data!",
			       ft->filename);
			v->extended = 0;
			v->rest = sblen - 2;
			v->size = BYTE;
			return;
		case VOC_DATA_16:
			new_rate = rllong(ft);
			if (new_rate == 0)
			    fail("File %s: Sample rate is zero?",ft->filename);
			if ((v->rate != -1) && (new_rate != v->rate))
			    fail("File %s: sample rate codes differ: %d != %d",
				ft->filename, v->rate, new_rate);
			v->rate = new_rate;
			ft->info.rate = new_rate;
			uc = getc(ft->fp);
			switch (uc)
			{
			    case 8:	v->size = BYTE; break;
			    case 16:	v->size = WORD; break;
			    default:	fail("Don't understand size %d", uc);
			}
			v->channels = getc(ft->fp);
			getc(ft->fp);	/* unknown1 */
			getc(ft->fp);	/* notused */
			getc(ft->fp);	/* notused */
			getc(ft->fp);	/* notused */
			getc(ft->fp);	/* notused */
			getc(ft->fp);	/* notused */
			v->rest = sblen - 12;
			return;
		case VOC_CONT: 
			v->rest = sblen;
			return;
		case VOC_SILENCE: 
			{
			unsigned short period;

			period = rlshort(ft);
			uc = getc(ft->fp);
			if (uc == 0)
				fail("File %s: Silence sample rate is zero");
			/* 
			 * Some silence-packed files have gratuitously
			 * different sample rate codes in silence.
			 * Adjust period.
			 */
			if ((v->rate != -1) && (uc != v->rate))
				period = (period * (256 - uc))/(256 - v->rate);
			else
				v->rate = uc;
			v->rest = period;
			v->silent = 1;
			return;
			}
		case VOC_MARKER:
			uc = getc(ft->fp);
			uc = getc(ft->fp);
			/* Falling! Falling! */
		case VOC_TEXT:
			{
			int i;
			/* Could add to comment in SF? */
			for(i = 0; i < sblen; i++)
				getc(ft->fp);
			}
			continue;	/* get next block */
		case VOC_LOOP:
		case VOC_LOOPEND:
			report("File %s: skipping repeat loop");
			for(i = 0; i < sblen; i++)
				getc(ft->fp);
			break;
		case VOC_EXTENDED:
			/* An Extended block is followed by a data block */
			/* Set this byte so we know to use the rate      */
			/* value from the extended block and not the     */
			/* data block.					 */
			v->extended = 1;
			new_rate = rlshort(ft);
			if (new_rate == 0)
			   fail("File %s: Sample rate is zero?");
			if ((v->rate != -1) && (new_rate != v->rate))
			   fail("File %s: sample rate codes differ: %d != %d",
					ft->filename, v->rate, new_rate);
			v->rate = new_rate;
			uc = getc(ft->fp);
			if (uc != 0)
				fail("File %s: only interpret 8-bit data!",
					ft->filename);
			uc = getc(ft->fp);
			if (uc)
				ft->info.channels = 2;  /* Stereo */
			/* Needed number of channels before finishing
			   compute for rate */
			ft->info.rate = (256000000L/(65536L - v->rate))/ft->info.channels;
			/* An extended block must be followed by a data */
			/* block to be valid so loop back to top so it  */
			/* can be grabed.				*/
			continue;
		default:
			report("File %s: skipping unknown block code %d",
				ft->filename, block);
			for(i = 0; i < sblen; i++)
				getc(ft->fp);
		}
	}
}

/* Start an output block. */
void blockstart(ft)
ft_t ft;
{
	vs_t v = (vs_t) ft->priv;

	v->blockseek = ftell(ft->fp);
	if (v->silent) {
		putc(VOC_SILENCE, ft->fp);	/* Silence block code */
		putc(0, ft->fp);		/* Period length */
		putc(0, ft->fp);		/* Period length */
		putc((int) v->rate, ft->fp);		/* Rate code */
	} else {
	  if (ft->info.size == BYTE) {
	    /* 8-bit sample section.  By always setting the correct     */
	    /* rate value in the DATA block (even when its preceeded    */
	    /* by an EXTENDED block) old software can still play stereo */
	    /* files in mono by just skipping over the EXTENDED block.  */
	    /* Prehaps the rate should be doubled though to make up for */
	    /* double amount of samples for a given time????            */
	    if (ft->info.channels > 1) {
	      putc(VOC_EXTENDED, ft->fp);	/* Voice Extended block code */
	      putc(4, ft->fp);                /* block length = 4 */
	      putc(0, ft->fp);                /* block length = 4 */
	      putc(0, ft->fp);                /* block length = 4 */
		  v->rate = 65536L - (256000000.0/(2*(float)ft->info.rate));
	      wlshort(ft,v->rate);	/* Rate code */
	      putc(0, ft->fp);                /* File is not packed */
	      putc(1, ft->fp);                /* samples are in stereo */
	    }
	    putc(VOC_DATA, ft->fp);		/* Voice Data block code */
	    putc(0, ft->fp);		/* block length (for now) */
	    putc(0, ft->fp);		/* block length (for now) */
	    putc(0, ft->fp);		/* block length (for now) */
	    v->rate = 256 - (1000000.0/(float)ft->info.rate);
	    putc((int) v->rate, ft->fp);/* Rate code */
	    putc(0, ft->fp);		/* 8-bit raw data */
	} else {
	    putc(VOC_DATA_16, ft->fp);		/* Voice Data block code */
	    putc(0, ft->fp);		/* block length (for now) */
	    putc(0, ft->fp);		/* block length (for now) */
	    putc(0, ft->fp);		/* block length (for now) */
	    v->rate = ft->info.rate;
	    wllong(ft, v->rate);	/* Rate code */
	    putc(16, ft->fp);		/* Sample Size */
	    putc(ft->info.channels, ft->fp);	/* Sample Size */
	    putc(0, ft->fp);		/* Unknown */
	    putc(0, ft->fp);		/* Unused */
	    putc(0, ft->fp);		/* Unused */
	    putc(0, ft->fp);		/* Unused */
	    putc(0, ft->fp);		/* Unused */
	    putc(0, ft->fp);		/* Unused */
	  }
	}
}

/* End the current data or silence block. */
void blockstop(ft) 
ft_t ft;
{
	vs_t v = (vs_t) ft->priv;
	LONG datum;

	putc(0, ft->fp);			/* End of file block code */
	fseek(ft->fp, v->blockseek, 0);		/* seek back to block length */
	fseek(ft->fp, 1, 1);			/* seek forward one */
	if (v->silent) {
		datum = (v->samples) & 0xff;
		putc((int)datum, ft->fp);       /* low byte of length */
		datum = (v->samples >> 8) & 0xff;
		putc((int)datum, ft->fp);  /* high byte of length */
	} else {
	  if (ft->info.size == BYTE) {
	    if (ft->info.channels > 1) {
	      fseek(ft->fp, 8, 1); /* forward 7 + 1 for new block header */
	    }
	  }
	        v->samples += 2;		/* adjustment: SBDK pp. 3-5 */
		datum = (v->samples) & 0xff;
		putc((int)datum, ft->fp);       /* low byte of length */
		datum = (v->samples >> 8) & 0xff;
		putc((int)datum, ft->fp);  /* middle byte of length */
		datum = (v->samples >> 16) & 0xff;
		putc((int)datum, ft->fp); /* high byte of length */
	}
}

