/*
 * September 25, 1991
 * Copyright 1991 Guido van Rossum And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Guido van Rossum And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools SGI/Amiga AIFF format.
 * Used by SGI on 4D/35 and Indigo.
 * This is a subformat of the EA-IFF-85 format.
 * This is related to the IFF format used by the Amiga.
 * But, apparently, not the same.
 *
 * Jan 93: new version from Guido Van Rossum that 
 * correctly skips unwanted sections.
 *
 * Jan 94: add loop & marker support
 * Jul 97: added comments I/O by Leigh Smith
 * Nov 97: added verbose chunk comments
 *
 * June 1, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *   Fixed compile warnings reported by Kjetil Torgrim Homme
 *   <kjetilho@ifi.uio.no>
 *
 * Sept 9, 1998 - fixed loop markers.
 *
 * Feb. 9, 1999 - Small fix to work with invalid headers that include
 *   a INST block with markers that equal 0.  It should ingore those.
 *   Also fix endian problems when ran on Intel machines.  The check
 *   for endianness was being performed AFTER reading the header instead
 *   of before reading it.
 *
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef unix
#include <unistd.h>	/* For SEEK_* defines if not found in stdio */
#endif

#include "st.h"

/* Private data used by writer */
struct aiffpriv {
	ULONG nsamples;	/* number of 1-channel samples read or written */
};

double read_ieee_extended();
LONG rawread(P3(ft_t, LONG *, LONG));
void aiffwriteheader(P2(ft_t, LONG));
void rawwrite(P3(ft_t, LONG *, LONG));
void write_ieee_extended(P2(ft_t, double));
double ConvertFromIeeeExtended();
void ConvertToIeeeExtended(P2(double, char *));
void textChunk(P3(char **text, char *chunkDescription, ft_t ft));
void reportInstrument(P1(ft_t ft));

void aiffstartread(ft) 
ft_t ft;
{
	struct aiffpriv *p = (struct aiffpriv *) ft->priv;
	char buf[5];
	ULONG totalsize;
	LONG chunksize;
	int channels = 0;
	ULONG frames;
	int bits = 0;
	double rate = 0.0;
	ULONG offset = 0;
	ULONG blocksize = 0;
	int littlendian;
	char *endptr;
	int foundcomm = 0, foundmark = 0, foundinstr = 0;
	struct mark {
		int id, position;
		char name[40]; 
	} marks[32];
	int i, j;
	LONG nmarks = 0;
	LONG sustainLoopBegin = 0, sustainLoopEnd = 0,
	     releaseLoopBegin = 0, releaseLoopEnd = 0;
	LONG seekto = 0L, ssndsize = 0L;
	char *author;
	char *copyright;
	char *nametext;


	/* AIFF is in Big Endian format.  Swap whats read in on Little */
	/* Endian machines.                                            */
	littlendian = 1;
	endptr = (char *) &littlendian;
	if (*endptr)
	{
	    ft->swap = ft->swap ? 0 : 1;
	}

	/* FORM chunk */
	if (fread(buf, 1, 4, ft->fp) != 4 || strncmp(buf, "FORM", 4) != 0)
		fail("AIFF header does not begin with magic word 'FORM'");
	totalsize = rblong(ft);
	if (fread(buf, 1, 4, ft->fp) != 4 || strncmp(buf, "AIFF", 4) != 0)
		fail("AIFF 'FORM' chunk does not specify 'AIFF' as type");

	
	/* Skip everything but the COMM chunk and the SSND chunk */
	/* The SSND chunk must be the last in the file */
	while (1) {
		if (fread(buf, 1, 4, ft->fp) != 4)
			if (ssndsize > 0)
				break;
			else
				fail("Missing SSND chunk in AIFF file");

		if (strncmp(buf, "COMM", 4) == 0) {
			/* COMM chunk */
			chunksize = rblong(ft);
			if (chunksize != 18)
				fail("AIFF COMM chunk has bad size");
			channels = rbshort(ft);
			frames = rblong(ft);
			bits = rbshort(ft);
			rate = read_ieee_extended(ft);
			foundcomm = 1;
		}
		else if (strncmp(buf, "SSND", 4) == 0) {
			/* SSND chunk */
			chunksize = rblong(ft);
			offset = rblong(ft);
			blocksize = rblong(ft);
			chunksize -= 8;
			ssndsize = chunksize;
			/* if can't seek, just do sound now */
			if (!ft->seekable)
				break;
			/* else, seek to end of sound and hunt for more */
			seekto = ftell(ft->fp);
			fseek(ft->fp, chunksize, SEEK_CUR); 
		}
		else if (strncmp(buf, "MARK", 4) == 0) {
			/* MARK chunk */
			chunksize = rblong(ft);
			nmarks = rbshort(ft);
			chunksize -= 2;
			for(i = 0; i < nmarks; i++) {
				int len;

				marks[i].id = rbshort(ft);
				marks[i].position = rblong(ft);
				chunksize -= 6;
				len = getc(ft->fp);
				chunksize -= len + 1;
				for(j = 0; j < len ; j++) 
					marks[i].name[j] = getc(ft->fp);
				marks[i].name[j] = 0;
				if ((len & 1) == 0) {
					chunksize--;
					getc(ft->fp);
				}
			}
			/* HA HA!  Sound Designer (and others) makes */
			/* bogus files. It spits out bogus chunksize */
			/* for MARK field */
			while(chunksize-- > 0)
				getc(ft->fp);
			foundmark = 1;
		}
		else if (strncmp(buf, "INST", 4) == 0) {
			/* INST chunk */
			chunksize = rblong(ft);
			ft->instr.MIDInote = getc(ft->fp);
			getc(ft->fp);				/* detune */
			ft->instr.MIDIlow = getc(ft->fp);
			ft->instr.MIDIhi = getc(ft->fp);
			getc(ft->fp);			/* low velocity */
			getc(ft->fp);			/* hi  velocity */
			rbshort(ft);				/* gain */
			ft->loops[0].type = rbshort(ft); /* sustain loop */
			sustainLoopBegin = rbshort(ft);	 /* begin marker */
			sustainLoopEnd = rbshort(ft);    /* end marker */
			ft->loops[1].type = rbshort(ft); /* release loop */
			releaseLoopBegin = rbshort(ft);  /* begin marker */
			releaseLoopEnd = rbshort(ft);    /* end marker */

			/* At least one known program generates an INST */
			/* block with everything zeroed out (meaning    */
			/* no Loops used).  In this case it should just */
			/* be ingorned.				        */
			if (sustainLoopBegin == 0 && releaseLoopBegin == 0)
				foundinstr = 0;
			else
				foundinstr = 1;
		}
		else if (strncmp(buf, "APPL", 4) == 0) {
			chunksize = rblong(ft);
			while(chunksize-- > 0)
				getc(ft->fp);
		}
		else if (strncmp(buf, "ALCH", 4) == 0) {
			/* I think this is bogus and gets grabbed by APPL */
			/* INST chunk */
			rblong(ft);		/* ENVS - jeez! */
			chunksize = rblong(ft);
			while(chunksize-- > 0)
				getc(ft->fp);
		}
		else if (strncmp(buf, "ANNO", 4) == 0) {
			/* Old form of comment chunk */
			chunksize = rblong(ft);
			/* allocate enough memory to hold the comment */
			ft->comment = (char *) malloc((size_t) chunksize);
			if (ft->comment == NULL)
			  fail("AIFF: Couldn't allocate ANNO header");
			if (fread(ft->comment, 1, chunksize, ft->fp) != chunksize)
			  fail("AIFF: Unexpected EOF in ANNO header");
		}
		else if (strncmp(buf, "AUTH", 4) == 0) {
		  /* Author chunk */
		  textChunk(&author, "Author:", ft);
		  free(author);
		}
		else if (strncmp(buf, "NAME", 4) == 0) {
		  /* Name chunk */
		  textChunk(&nametext, "Name:", ft);
		  free(nametext);
		}
		else if (strncmp(buf, "(c) ", 4) == 0) {
		  /* Copyright chunk */
		  textChunk(&copyright, "Copyright:", ft);
		  free(copyright);
		}
		else {
			buf[4] = 0;
			/* bogus file, probably from the Mac */
			if ((buf[0] < 'A' || buf[0] > 'Z') ||
			    (buf[1] < 'A' || buf[1] > 'Z') ||
			    (buf[2] < 'A' || buf[2] > 'Z') ||
			    (buf[3] < 'A' || buf[3] > 'Z'))
				break;
			if (feof(ft->fp))
				break;
			report("AIFFstartread: ignoring '%s' chunk\n", buf);
			chunksize = rblong(ft);
			if (feof(ft->fp))
				break;
			/* Skip the chunk using getc() so we may read
			   from a pipe */
			while (chunksize-- > 0) {
				if (getc(ft->fp) == EOF)
					break;
			}
		}
		if (feof(ft->fp))
			break;
	}

	/* 
	 * if a pipe, we lose all chunks after sound.  
	 * Like, say, instrument loops. 
	 */
	if (ft->seekable)
		if (seekto > 0)
			fseek(ft->fp, seekto, SEEK_SET);
		else
			fail("AIFF: no sound data on input file");

	/* SSND chunk just read */
	if (blocksize != 0)
		fail("AIFF header specifies nonzero blocksize?!?!");
	while ((LONG) (--offset) >= 0) {
		if (getc(ft->fp) == EOF)
			fail("unexpected EOF while skipping AIFF offset");
	}

	if (foundcomm) {
		ft->info.channels = channels;
		ft->info.rate = rate;
		ft->info.style = SIGN2;
		switch (bits) {
		case 8:
			ft->info.size = BYTE;
			break;
		case 16:
			ft->info.size = WORD;
			break;
		default:
			fail("unsupported sample size in AIFF header: %d", bits);
			/*NOTREACHED*/
		}
	} else  {
		if ((ft->info.channels == -1)
			|| (ft->info.rate == -1)
			|| (ft->info.style == -1)
			|| (ft->info.size == -1)) {
		  report("You must specify # channels, sample rate, signed/unsigned,\n");
		  report("and 8/16 on the command line.");
		  fail("Bogus AIFF file: no COMM section.");
		}

	}

	p->nsamples = ssndsize / ft->info.size;	/* leave out channels */

	if (foundmark && !foundinstr)
		fail("Bogus AIFF file: MARKers but no INSTrument.");
	if (!foundmark && foundinstr)
		fail("Bogus AIFF file: INSTrument but no MARKers.");
	if (foundmark && foundinstr) {
		int i;
		int slbIndex = 0, sleIndex = 0;
		int rlbIndex = 0, rleIndex = 0;

		/* find our loop markers and save their marker indexes */
		for(i = 0; i < nmarks; i++) { 
		  if(marks[i].id == sustainLoopBegin)
		    slbIndex = i;
		  if(marks[i].id == sustainLoopEnd)
		    sleIndex = i;
		  if(marks[i].id == releaseLoopBegin)
		    rlbIndex = i;
		  if(marks[i].id == releaseLoopEnd)
		    rleIndex = i;
		}

		ft->instr.nloops = 0;
		if (ft->loops[0].type != 0) {
			ft->loops[0].start = marks[slbIndex].position;
			ft->loops[0].length = 
			    marks[sleIndex].position - marks[slbIndex].position;
			/* really the loop count should be infinite */
			ft->loops[0].count = 1;	
			ft->instr.loopmode = LOOP_SUSTAIN_DECAY | ft->loops[0].type;
			ft->instr.nloops++;
		}
		if (ft->loops[1].type != 0) {
			ft->loops[1].start = marks[rlbIndex].position;
			ft->loops[1].length = 
			    marks[rleIndex].position - marks[rlbIndex].position;
			/* really the loop count should be infinite */
			ft->loops[1].count = 1;
			ft->instr.loopmode = LOOP_SUSTAIN_DECAY | ft->loops[1].type;
			ft->instr.nloops++;
		} 
	}
	if (verbose)
	  reportInstrument(ft);
}

/* print out the MIDI key allocations, loop points, directions etc */
void reportInstrument(ft)
ft_t ft;
{
  int loopNum;

  if(ft->instr.nloops > 0)
    fprintf(stderr, "AIFF Loop markers:\n");
  for(loopNum  = 0; loopNum < ft->instr.nloops; loopNum++) {
    if (ft->loops[loopNum].count) {
      fprintf(stderr, "Loop %d: start: %6d", loopNum, ft->loops[loopNum].start);
      fprintf(stderr, " end:   %6d", 
	      ft->loops[loopNum].start + ft->loops[loopNum].length);
      fprintf(stderr, " count: %6d", ft->loops[loopNum].count);
      fprintf(stderr, " type:  ");
      switch(ft->loops[loopNum].type & ~LOOP_SUSTAIN_DECAY) {
      case 0: fprintf(stderr, "off\n"); break;
      case 1: fprintf(stderr, "forward\n"); break;
      case 2: fprintf(stderr, "forward/backward\n"); break;
      }
    }
  }
  fprintf(stderr, "Unity MIDI Note: %d\n", ft->instr.MIDInote);
  fprintf(stderr, "Low   MIDI Note: %d\n", ft->instr.MIDIlow);
  fprintf(stderr, "High  MIDI Note: %d\n", ft->instr.MIDIhi);
}

/* Process a text chunk, allocate memory, display it if verbose and return */
void textChunk(text, chunkDescription, ft) 
char **text;
char *chunkDescription;
ft_t ft;
{
  LONG chunksize = rblong(ft);
  /* allocate enough memory to hold the text including a terminating \0 */
  *text = (char *) malloc((size_t) chunksize + 1);
  if (*text == NULL)
    fail("AIFF: Couldn't allocate %s header", chunkDescription);
  if (fread(*text, 1, chunksize, ft->fp) != chunksize)
    fail("AIFF: Unexpected EOF in %s header", chunkDescription);
  *(*text + chunksize) = '\0';
  if(verbose) {
    printf("%-10s   \"%s\"\n", chunkDescription, *text);
  }
}

LONG aiffread(ft, buf, len)
ft_t ft;
LONG *buf, len;
{
	struct aiffpriv *p = (struct aiffpriv *) ft->priv;

	/* just read what's left of SSND chunk */
	if (len > p->nsamples)
		len = p->nsamples;
	rawread(ft, buf, len);
	p->nsamples -= len;
	return len;
}

void aiffstopread(ft) 
ft_t ft;
{
	char buf[5];
	ULONG chunksize;

	if (!ft->seekable)
	    while (! feof(ft->fp)) {
		if (fread(buf, 1, 4, ft->fp) != 4)
			return;

		chunksize = rblong(ft);
		if (feof(ft->fp))
			return;
		buf[4] = '\0';
		warn("Ignoring AIFF tail chunk: '%s', %d bytes long\n", 
			buf, chunksize);
		if (! strcmp(buf, "MARK") || ! strcmp(buf, "INST"))
			warn("	You're stripping MIDI/loop info!\n");
		while ((LONG) (--chunksize) >= 0) 
			if (getc(ft->fp) == EOF)
				return;
	}
	return;
}

/* When writing, the header is supposed to contain the number of
   samples and data bytes written.
   Since we don't know how many samples there are until we're done,
   we first write the header with an very large number,
   and at the end we rewind the file and write the header again
   with the right number.  This only works if the file is seekable;
   if it is not, the very large size remains in the header.
   Strictly spoken this is not legal, but the playaiff utility
   will still be able to play the resulting file. */

void aiffstartwrite(ft)
ft_t ft;
{
	struct aiffpriv *p = (struct aiffpriv *) ft->priv;
	int littlendian;
	char *endptr;

	/* AIFF is in Big Endian format.  Swap whats read in on Little */
	/* Endian machines.                                            */
	littlendian = 1;
	endptr = (char *) &littlendian;
	if (*endptr)
	{
	    ft->swap = ft->swap ? 0 : 1;
	}

	p->nsamples = 0;
	if (ft->info.style == ULAW && ft->info.size == BYTE) {
		report("expanding 8-bit u-law to 16 bits");
		ft->info.size = WORD;
	}
	ft->info.style = SIGN2; /* We have a fixed style */
	/* Compute the "very large number" so that a maximum number
	   of samples can be transmitted through a pipe without the
	   risk of causing overflow when calculating the number of bytes.
	   At 48 kHz, 16 bits stereo, this gives ~3 hours of music.
	   Sorry, the AIFF format does not provide for an "infinite"
	   number of samples. */
	aiffwriteheader(ft, 0x7f000000L / (ft->info.size*ft->info.channels));
}

void aiffwrite(ft, buf, len)
ft_t ft;
LONG *buf, len;
{
	struct aiffpriv *p = (struct aiffpriv *) ft->priv;
	p->nsamples += len;
	rawwrite(ft, buf, len);
}

void
aiffstopwrite(ft)
ft_t ft;
{
	struct aiffpriv *p = (struct aiffpriv *) ft->priv;
	if (!ft->seekable)
		return;
	if (fseek(ft->fp, 0L, SEEK_SET) != 0)
		fail("can't rewind output file to rewrite AIFF header");
	aiffwriteheader(ft, p->nsamples / ft->info.channels);
}

void aiffwriteheader(ft, nframes)
ft_t ft;
LONG nframes;
{
	int hsize =
		8 /*COMM hdr*/ + 18 /*COMM chunk*/ +
		8 /*SSND hdr*/ + 12 /*SSND chunk*/;
	int bits = 0;
	int i;

	hsize += 8 + 2 + 16*ft->instr.nloops;	/* MARK chunk */
	hsize += 20;				/* INST chunk */

	if (ft->info.style == SIGN2 && ft->info.size == BYTE)
		bits = 8;
	else if (ft->info.style == SIGN2 && ft->info.size == WORD)
		bits = 16;
	else
		fail("unsupported output style/size for AIFF header");

	fputs("FORM", ft->fp); /* IFF header */
	wblong(ft, hsize + nframes * ft->info.size * ft->info.channels); /* file size */
	fputs("AIFF", ft->fp); /* File type */

	/* ANNO chunk -- holds comments text, however this is */
	/* discouraged by Apple in preference to a COMT comments */
	/* chunk, which holds a timestamp and marker id */
	fputs("ANNO", ft->fp);
	wblong(ft, (LONG) strlen(ft->comment)); /* ANNO chunk size, the No of chars */
	fputs(ft->comment, ft->fp);

	/* COMM chunk -- describes encoding (and #frames) */
	fputs("COMM", ft->fp);
	wblong(ft, (LONG) 18); /* COMM chunk size */
	wbshort(ft, ft->info.channels); /* nchannels */
	wblong(ft, nframes); /* number of frames */
	wbshort(ft, bits); /* sample width, in bits */
	write_ieee_extended(ft, (double)ft->info.rate);

	/* MARK chunk -- set markers */
	if (ft->instr.nloops) {
		fputs("MARK", ft->fp);
		if (ft->instr.nloops > 2)
			ft->instr.nloops = 2;
		wblong(ft, 2 + 16*ft->instr.nloops);
		wbshort(ft, ft->instr.nloops);

		for(i = 0; i < ft->instr.nloops; i++) {
			wbshort(ft, i + 1);
			wblong(ft, ft->loops[i].start);
			fputc(0, ft->fp);
			fputc(0, ft->fp);
			wbshort(ft, i*2 + 1);
			wblong(ft, ft->loops[i].start + ft->loops[i].length);
			fputc(0, ft->fp);
			fputc(0, ft->fp);
			}

		fputs("INST", ft->fp);
		wblong(ft, 20);
		/* random MIDI shit that we default on */
		fputc(ft->instr.MIDInote, ft->fp);
		fputc(0, ft->fp);			/* detune */
		fputc(ft->instr.MIDIlow, ft->fp);
		fputc(ft->instr.MIDIhi, ft->fp);
		fputc(1, ft->fp);			/* low velocity */
		fputc(127, ft->fp);			/* hi  velocity */
		wbshort(ft, 0);				/* gain */

		/* sustain loop */
		wbshort(ft, ft->loops[0].type);
		wbshort(ft, 1);				/* marker 1 */
		wbshort(ft, 3);				/* marker 3 */
		/* release loop, if there */
		if (ft->instr.nloops == 2) {
			wbshort(ft, ft->loops[1].type);
			wbshort(ft, 2);			/* marker 2 */
			wbshort(ft, 4);			/* marker 4 */
		} else {
			wbshort(ft, 0);			/* no release loop */
			wbshort(ft, 0);
			wbshort(ft, 0);
		}
	}

	/* SSND chunk -- describes data */
	fputs("SSND", ft->fp);
	/* chunk size */
	wblong(ft, 8 + nframes * ft->info.channels * ft->info.size); 
	wblong(ft, (LONG) 0); /* offset */
	wblong(ft, (LONG) 0); /* block size */
}

double read_ieee_extended(ft)
ft_t ft;
{
	char buf[10];
	if (fread(buf, 1, 10, ft->fp) != 10)
		fail("EOF while reading IEEE extended number");
	return ConvertFromIeeeExtended(buf);
}

void write_ieee_extended(ft, x)
ft_t ft;
double x;
{
	char buf[10];
	ConvertToIeeeExtended(x, buf);
	/*
	report("converted %g to %o %o %o %o %o %o %o %o %o %o",
		x,
		buf[0], buf[1], buf[2], buf[3], buf[4],
		buf[5], buf[6], buf[7], buf[8], buf[9]);
	*/
	(void) fwrite(buf, 1, 10, ft->fp);
}


/*
 * C O N V E R T   T O   I E E E   E X T E N D E D
 */

/* Copyright (C) 1988-1991 Apple Computer, Inc.
 * All rights reserved.
 *
 * Machine-independent I/O routines for IEEE floating-point numbers.
 *
 * NaN's and infinities are converted to HUGE_VAL or HUGE, which
 * happens to be infinity on IEEE machines.  Unfortunately, it is
 * impossible to preserve NaN's in a machine-independent way.
 * Infinities are, however, preserved on IEEE machines.
 *
 * These routines have been tested on the following machines:
 *    Apple Macintosh, MPW 3.1 C compiler
 *    Apple Macintosh, THINK C compiler
 *    Silicon Graphics IRIS, MIPS compiler
 *    Cray X/MP and Y/MP
 *    Digital Equipment VAX
 *
 *
 * Implemented by Malcolm Slaney and Ken Turkowski.
 *
 * Malcolm Slaney contributions during 1988-1990 include big- and little-
 * endian file I/O, conversion to and from Motorola's extended 80-bit
 * floating-point format, and conversions to and from IEEE single-
 * precision floating-point format.
 *
 * In 1991, Ken Turkowski implemented the conversions to and from
 * IEEE double-precision format, added more precision to the extended
 * conversions, and accommodated conversions involving +/- infinity,
 * NaN's, and denormalized numbers.
 */

#ifndef HUGE_VAL
# define HUGE_VAL HUGE
#endif /*HUGE_VAL*/

# define FloatToUnsigned(f)      ((ULONG)(((LONG)(f - 2147483648.0)) + 2147483647L) + 1)

void ConvertToIeeeExtended(num, bytes)
double num;
char *bytes;
{
    int    sign;
    int expon;
    double fMant, fsMant;
    ULONG hiMant, loMant;

    if (num < 0) {
        sign = 0x8000;
        num *= -1;
    } else {
        sign = 0;
    }

    if (num == 0) {
        expon = 0; hiMant = 0; loMant = 0;
    }
    else {
        fMant = frexp(num, &expon);
        if ((expon > 16384) || !(fMant < 1)) {    /* Infinity or NaN */
            expon = sign|0x7FFF; hiMant = 0; loMant = 0; /* infinity */
        }
        else {    /* Finite */
            expon += 16382;
            if (expon < 0) {    /* denormalized */
                fMant = ldexp(fMant, expon);
                expon = 0;
            }
            expon |= sign;
            fMant = ldexp(fMant, 32);          
            fsMant = floor(fMant); 
            hiMant = FloatToUnsigned(fsMant);
            fMant = ldexp(fMant - fsMant, 32); 
            fsMant = floor(fMant); 
            loMant = FloatToUnsigned(fsMant);
        }
    }
    
    bytes[0] = expon >> 8;
    bytes[1] = expon;
    bytes[2] = hiMant >> 24;
    bytes[3] = hiMant >> 16;
    bytes[4] = hiMant >> 8;
    bytes[5] = hiMant;
    bytes[6] = loMant >> 24;
    bytes[7] = loMant >> 16;
    bytes[8] = loMant >> 8;
    bytes[9] = loMant;
}


/*
 * C O N V E R T   F R O M   I E E E   E X T E N D E D  
 */

/* 
 * Copyright (C) 1988-1991 Apple Computer, Inc.
 * All rights reserved.
 *
 * Machine-independent I/O routines for IEEE floating-point numbers.
 *
 * NaN's and infinities are converted to HUGE_VAL or HUGE, which
 * happens to be infinity on IEEE machines.  Unfortunately, it is
 * impossible to preserve NaN's in a machine-independent way.
 * Infinities are, however, preserved on IEEE machines.
 *
 * These routines have been tested on the following machines:
 *    Apple Macintosh, MPW 3.1 C compiler
 *    Apple Macintosh, THINK C compiler
 *    Silicon Graphics IRIS, MIPS compiler
 *    Cray X/MP and Y/MP
 *    Digital Equipment VAX
 *
 *
 * Implemented by Malcolm Slaney and Ken Turkowski.
 *
 * Malcolm Slaney contributions during 1988-1990 include big- and little-
 * endian file I/O, conversion to and from Motorola's extended 80-bit
 * floating-point format, and conversions to and from IEEE single-
 * precision floating-point format.
 *
 * In 1991, Ken Turkowski implemented the conversions to and from
 * IEEE double-precision format, added more precision to the extended
 * conversions, and accommodated conversions involving +/- infinity,
 * NaN's, and denormalized numbers.
 */

#ifndef HUGE_VAL
# define HUGE_VAL HUGE
#endif /*HUGE_VAL*/

# define UnsignedToFloat(u)         (((double)((LONG)(u - 2147483647L - 1))) + 2147483648.0)

/****************************************************************
 * Extended precision IEEE floating-point conversion routine.
 ****************************************************************/

double ConvertFromIeeeExtended(bytes)
unsigned char *bytes;	/* LCN */
{
    double    f;
    int    expon;
    ULONG hiMant, loMant;
    
    expon = ((bytes[0] & 0x7F) << 8) | (bytes[1] & 0xFF);
    hiMant    =    ((ULONG)(bytes[2] & 0xFF) << 24)
            |    ((ULONG)(bytes[3] & 0xFF) << 16)
            |    ((ULONG)(bytes[4] & 0xFF) << 8)
            |    ((ULONG)(bytes[5] & 0xFF));
    loMant    =    ((ULONG)(bytes[6] & 0xFF) << 24)
            |    ((ULONG)(bytes[7] & 0xFF) << 16)
            |    ((ULONG)(bytes[8] & 0xFF) << 8)
            |    ((ULONG)(bytes[9] & 0xFF));

    if (expon == 0 && hiMant == 0 && loMant == 0) {
        f = 0;
    }
    else {
        if (expon == 0x7FFF) {    /* Infinity or NaN */
            f = HUGE_VAL;
        }
        else {
            expon -= 16383;
            f  = ldexp(UnsignedToFloat(hiMant), expon-=31);
            f += ldexp(UnsignedToFloat(loMant), expon-=32);
        }
    }

    if (bytes[0] & 0x80)
        return -f;
    else
        return f;
}

