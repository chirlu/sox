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
 * Nov 25, 1999 - internal functions made static
 *
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>	/* For SEEK_* defines if not found in stdio */
#endif

#include "st.h"

/* Private data used by writer */
struct aiffpriv {
	ULONG nsamples;	/* number of 1-channel samples read or written */
};

/* forward declarations */
static double read_ieee_extended(P1(ft_t));
static int aiffwriteheader(P2(ft_t, LONG));
static void write_ieee_extended(P2(ft_t, double));
static double ConvertFromIeeeExtended(P1(unsigned char*));
static void ConvertToIeeeExtended(P2(double, char *));
static int textChunk(P3(char **text, char *chunkDescription, ft_t ft));
static void reportInstrument(P1(ft_t ft));

int st_aiffstartread(ft) 
ft_t ft;
{
	struct aiffpriv *p = (struct aiffpriv *) ft->priv;
	char buf[5];
	ULONG totalsize;
	LONG chunksize;
	unsigned short channels = 0;
	ULONG frames;
	unsigned short bits = 0;
	double rate = 0.0;
	ULONG offset = 0;
	ULONG blocksize = 0;
	int littlendian = 1;
	char *endptr;
	int foundcomm = 0, foundmark = 0, foundinstr = 0;
	struct mark {
		unsigned short id;
		ULONG position;
		char name[40]; 
	} marks[32];
	unsigned short looptype;
	int i, j;
	unsigned short nmarks = 0;
	unsigned short sustainLoopBegin = 0, sustainLoopEnd = 0,
	     	       releaseLoopBegin = 0, releaseLoopEnd = 0;
	LONG seekto = 0L, ssndsize = 0L;
	char *author;
	char *copyright;
	char *nametext;

	ULONG trash;

	int rc;

	/* Needed because of st_rawread() */
	rc = st_rawstartread(ft);
	if (rc)
	    return rc;

	/* AIFF is in Big Endian format.  Swap whats read in on Little */
	/* Endian machines.                                            */
	endptr = (char *) &littlendian;
	if (*endptr)
	{
	    ft->swap = ft->swap ? 0 : 1;
	}

	/* FORM chunk */
	if (st_reads(ft, buf, 4) == ST_EOF || strncmp(buf, "FORM", 4) != 0)
	{
		st_fail("AIFF header does not begin with magic word 'FORM'");
		return(ST_EOF);
	}
	st_readdw(ft, &totalsize);
	if (st_reads(ft, buf, 4) == ST_EOF || strncmp(buf, "AIFF", 4) != 0)
	{
		st_fail("AIFF 'FORM' chunk does not specify 'AIFF' as type");
		return(ST_EOF);
	}

	
	/* Skip everything but the COMM chunk and the SSND chunk */
	/* The SSND chunk must be the last in the file */
	while (1) {
		if (st_reads(ft, buf, 4) == ST_EOF)
		{
			if (ssndsize > 0)
				break;
			else
			{
				st_fail("Missing SSND chunk in AIFF file");
				return(ST_EOF);
			}
		}
		if (strncmp(buf, "COMM", 4) == 0) {
			/* COMM chunk */
			st_readdw(ft, &chunksize);
			if (chunksize != 18)
			{
				st_fail("AIFF COMM chunk has bad size");
				return(ST_EOF);
			}
			st_readw(ft, &channels);
			st_readdw(ft, &frames);
			st_readw(ft, &bits);
			rate = read_ieee_extended(ft);
			foundcomm = 1;
		}
		else if (strncmp(buf, "SSND", 4) == 0) {
			/* SSND chunk */
			st_readdw(ft, &chunksize);
			st_readdw(ft, &offset);
			st_readdw(ft, &blocksize);
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
			st_readdw(ft, &chunksize);
			st_readw(ft, &nmarks);

			/* Some programs like to always have a MARK chunk
			 * but will set number of marks to 0 and force
			 * software to detect and ignore it.
			 */
			if (nmarks == 0)
			    foundmark = 0;
			else
			    foundmark = 1;

			chunksize -= 2;
			for(i = 0; i < nmarks; i++) {
				unsigned char len;

				st_readw(ft, &(marks[i].id));
				st_readdw(ft, &(marks[i].position));
				chunksize -= 6;
				st_readb(ft, &len);
				chunksize -= len + 1;
				for(j = 0; j < len ; j++) 
				    st_readb(ft, &(marks[i].name[j]));
				marks[i].name[j] = 0;
				if ((len & 1) == 0) {
					chunksize--;
					st_readb(ft, (unsigned char *)&trash);
				}
			}
			/* HA HA!  Sound Designer (and others) makes */
			/* bogus files. It spits out bogus chunksize */
			/* for MARK field */
			while(chunksize-- > 0)
			    st_readb(ft, (unsigned char *)&trash);
		}
		else if (strncmp(buf, "INST", 4) == 0) {
			/* INST chunk */
			st_readdw(ft, &chunksize);
			st_readb(ft, &(ft->instr.MIDInote));
			st_readb(ft, (unsigned char *)&trash);
			st_readb(ft, &(ft->instr.MIDIlow));
			st_readb(ft, &(ft->instr.MIDIhi));
			/* Low  velocity */
			st_readb(ft, (unsigned char *)&trash);
			/* Hi  velocity */
			st_readb(ft, (unsigned char *)&trash);
			st_readw(ft, (unsigned short *)&trash);	/* gain */
			st_readw(ft, &looptype); /* sustain loop */
			ft->loops[0].type = looptype;
			st_readw(ft, &sustainLoopBegin); /* begin marker */
			st_readw(ft, &sustainLoopEnd);    /* end marker */
			st_readw(ft, &looptype); /* release loop */
			ft->loops[1].type = looptype;
			st_readw(ft, &releaseLoopBegin);  /* begin marker */
			st_readw(ft, &releaseLoopEnd);    /* end marker */

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
			st_readdw(ft, &chunksize);
			while(chunksize-- > 0)
			    st_readb(ft, (unsigned char *)&trash);
		}
		else if (strncmp(buf, "ALCH", 4) == 0) {
			/* I think this is bogus and gets grabbed by APPL */
			/* INST chunk */
			st_readdw(ft, &trash);		/* ENVS - jeez! */
			st_readdw(ft, &chunksize);
			while(chunksize-- > 0)
			    st_readb(ft, (unsigned char *)&trash);
		}
		else if (strncmp(buf, "ANNO", 4) == 0) {
			rc = textChunk(&(ft->comment), "Annotation:", ft);
			if (rc)
			{
				/* Fail already called in function */
			  return(ST_EOF);
			}
		}
		else if (strncmp(buf, "AUTH", 4) == 0) {
		  /* Author chunk */
		  rc = textChunk(&author, "Author:", ft);
		  if (rc)
		  {
		      /* Fail already called in function */
		      return(ST_EOF);
		  }
		  free(author);
		}
		else if (strncmp(buf, "NAME", 4) == 0) {
		  /* Name chunk */
		  rc = textChunk(&nametext, "Name:", ft);
		  if (rc)
		  {
		      /* Fail already called in function */
		      return(ST_EOF);
		  }
		  free(nametext);
		}
		else if (strncmp(buf, "(c) ", 4) == 0) {
		  /* Copyright chunk */
		  rc = textChunk(&copyright, "Copyright:", ft);
		  if (rc)
		  {
		      /* Fail already called in function */
		      return(ST_EOF);
		  }
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
			st_report("AIFFstartread: ignoring '%s' chunk\n", buf);
			st_readdw(ft, &chunksize);
			if (feof(ft->fp))
				break;
			/* Skip the chunk using st_readb() so we may read
			   from a pipe */
			while (chunksize-- > 0) {
			    if (st_readb(ft, (unsigned char *)&trash) == ST_EOF)
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
	{
		if (seekto > 0)
			fseek(ft->fp, seekto, SEEK_SET);
		else
		{
			st_fail("AIFF: no sound data on input file");
			return(ST_EOF);
		}
	}
	/* SSND chunk just read */
	if (blocksize != 0)
	{
		st_fail("AIFF header specifies nonzero blocksize?!?!");
		return(ST_EOF);
	}
	while ((LONG) (--offset) >= 0) {
		if (st_readb(ft, (unsigned char *)&trash) == ST_EOF)
		{
			st_fail("unexpected EOF while skipping AIFF offset");
			return(ST_EOF);
		}
	}

	if (foundcomm) {
		ft->info.channels = channels;
		ft->info.rate = rate;
		ft->info.encoding = ST_ENCODING_SIGN2;
		switch (bits) {
		case 8:
			ft->info.size = ST_SIZE_BYTE;
			break;
		case 16:
			ft->info.size = ST_SIZE_WORD;
			break;
		default:
			st_fail("unsupported sample size in AIFF header: %d", bits);
			return(ST_EOF);
			/*NOTREACHED*/
		}
	} else  {
		if ((ft->info.channels == -1)
			|| (ft->info.rate == -1)
			|| (ft->info.encoding == -1)
			|| (ft->info.size == -1)) {
		  st_report("You must specify # channels, sample rate, signed/unsigned,\n");
		  st_report("and 8/16 on the command line.");
		  st_fail("Bogus AIFF file: no COMM section.");
		  return(ST_EOF);
		}

	}

	p->nsamples = ssndsize / ft->info.size;	/* leave out channels */

	if (foundmark && !foundinstr)
	{
		st_fail("Bogus AIFF file: MARKers but no INSTrument.");
		return(ST_EOF);
	}
	if (!foundmark && foundinstr)
	{
		st_fail("Bogus AIFF file: INSTrument but no MARKers.");
		return(ST_EOF);
	}
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
			ft->instr.loopmode = ST_LOOP_SUSTAIN_DECAY | ft->loops[0].type;
			ft->instr.nloops++;
		}
		if (ft->loops[1].type != 0) {
			ft->loops[1].start = marks[rlbIndex].position;
			ft->loops[1].length = 
			    marks[rleIndex].position - marks[rlbIndex].position;
			/* really the loop count should be infinite */
			ft->loops[1].count = 1;
			ft->instr.loopmode = ST_LOOP_SUSTAIN_DECAY | ft->loops[1].type;
			ft->instr.nloops++;
		} 
	}
	if (verbose)
	  reportInstrument(ft);

	return(ST_SUCCESS);
}

/* print out the MIDI key allocations, loop points, directions etc */
static void reportInstrument(ft)
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
      switch(ft->loops[loopNum].type & ~ST_LOOP_SUSTAIN_DECAY) {
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
static int textChunk(text, chunkDescription, ft) 
char **text;
char *chunkDescription;
ft_t ft;
{
  LONG chunksize;
  st_readdw(ft, &chunksize);
  /* allocate enough memory to hold the text including a terminating \0 */
  *text = (char *) malloc((size_t) chunksize + 1);
  if (*text == NULL)
  {
    st_fail("AIFF: Couldn't allocate %s header", chunkDescription);
    return(ST_EOF);
  }
  if (fread(*text, 1, chunksize, ft->fp) != chunksize)
  {
    st_fail("AIFF: Unexpected EOF in %s header", chunkDescription);
    return(ST_EOF);
  }
  *(*text + chunksize) = '\0';
	if (chunksize % 2)
	{
		/* Read past pad byte */
		char c;
		if (fread(&c, 1, 1, ft->fp) != 1)
		{
			st_fail("AIFF: Unexpected EOF in %s header", chunkDescription);
			return(ST_EOF);
		}
	}
  if(verbose) {
    printf("%-10s   \"%s\"\n", chunkDescription, *text);
  }
  return(ST_SUCCESS);
}

LONG st_aiffread(ft, buf, len)
ft_t ft;
LONG *buf, len;
{
	struct aiffpriv *p = (struct aiffpriv *) ft->priv;

	/* just read what's left of SSND chunk */
	if (len > p->nsamples)
		len = p->nsamples;
	st_rawread(ft, buf, len);
	p->nsamples -= len;
	return len;
}

int st_aiffstopread(ft) 
ft_t ft;
{
	char buf[5];
	ULONG chunksize;
	ULONG trash;

	if (!ft->seekable)
	{
	    while (! feof(ft->fp)) 
	    {
		if (fread(buf, 1, 4, ft->fp) != 4)
			break;

		st_readdw(ft, &chunksize);
		if (feof(ft->fp))
			break;
		buf[4] = '\0';
		st_warn("Ignoring AIFF tail chunk: '%s', %d bytes long\n", 
			buf, chunksize);
		if (! strcmp(buf, "MARK") || ! strcmp(buf, "INST"))
			st_warn("	You're stripping MIDI/loop info!\n");
		while ((LONG) (--chunksize) >= 0) 
		{
			if (st_readb(ft, (unsigned char *)&trash) == ST_EOF)
				break;
		}
	    }
	}

	/* Needed because of st_rawwrite() */
	return st_rawstopread(ft);
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

int st_aiffstartwrite(ft)
ft_t ft;
{
	struct aiffpriv *p = (struct aiffpriv *) ft->priv;
	int littlendian;
	char *endptr;
	int rc;

	/* Needed because st_rawwrite() */
	rc = st_rawstartwrite(ft);
	if (rc)
	    return rc;

	/* AIFF is in Big Endian format.  Swap whats read in on Little */
	/* Endian machines.                                            */
	littlendian = 1;
	endptr = (char *) &littlendian;
	if (*endptr)
	{
	    ft->swap = ft->swap ? 0 : 1;
	}

	p->nsamples = 0;
	if ((ft->info.encoding == ST_ENCODING_ULAW ||
	     ft->info.encoding == ST_ENCODING_ALAW) && 
	    ft->info.size == ST_SIZE_BYTE) {
		st_report("expanding 8-bit u-law to 16 bits");
		ft->info.size = ST_SIZE_WORD;
	}
	ft->info.encoding = ST_ENCODING_SIGN2; /* We have a fixed encoding */

	/* Compute the "very large number" so that a maximum number
	   of samples can be transmitted through a pipe without the
	   risk of causing overflow when calculating the number of bytes.
	   At 48 kHz, 16 bits stereo, this gives ~3 hours of music.
	   Sorry, the AIFF format does not provide for an "infinite"
	   number of samples. */
	return(aiffwriteheader(ft, 0x7f000000L / (ft->info.size*ft->info.channels)));
}

LONG st_aiffwrite(ft, buf, len)
ft_t ft;
LONG *buf, len;
{
	struct aiffpriv *p = (struct aiffpriv *) ft->priv;
	p->nsamples += len;
	st_rawwrite(ft, buf, len);
	return(len);
}

int st_aiffstopwrite(ft)
ft_t ft;
{
	struct aiffpriv *p = (struct aiffpriv *) ft->priv;
	int rc;

	/* Needed because of st_rawwrite().  Call now to flush
	 * buffer now before seeking around below.
	 */
	rc = st_rawstopwrite(ft);
	if (rc)
	    return rc;

	if (!ft->seekable)
	{
	    st_fail("Non-seekable file.");
	    return(ST_EOF);
	}
	if (fseek(ft->fp, 0L, SEEK_SET) != 0)
	{
		st_fail("can't rewind output file to rewrite AIFF header");
		return(ST_EOF);
	}
	return(aiffwriteheader(ft, p->nsamples / ft->info.channels));
}

static int aiffwriteheader(ft, nframes)
ft_t ft;
LONG nframes;
{
	int hsize =
		8 /*COMM hdr*/ + 18 /*COMM chunk*/ +
		8 /*SSND hdr*/ + 12 /*SSND chunk*/;
	int bits = 0;
	int i;
	int comment_size;

	hsize += 8 + 2 + 16*ft->instr.nloops;	/* MARK chunk */
	hsize += 20;				/* INST chunk */

	if (ft->info.encoding == ST_ENCODING_SIGN2 && 
	    ft->info.size == ST_SIZE_BYTE)
		bits = 8;
	else if (ft->info.encoding == ST_ENCODING_SIGN2 && 
		 ft->info.size == ST_SIZE_WORD)
		bits = 16;
	else
	{
		st_fail("unsupported output encoding/size for AIFF header");
		return(ST_EOF);
	}

	st_writes(ft, "FORM"); /* IFF header */
	st_writedw(ft, hsize + nframes * ft->info.size * ft->info.channels); /* file size */
	st_writes(ft, "AIFF"); /* File type */

	/* ANNO chunk -- holds comments text, however this is */
	/* discouraged by Apple in preference to a COMT comments */
	/* chunk, which holds a timestamp and marker id */
	if (ft->comment)
	{
	  st_writes(ft, "ANNO");
	  /* Must put an even number of characters out.  True 68k processors
	   * OS's seem to require this 
	   */
	  comment_size = strlen(ft->comment);
	  st_writedw(ft, (LONG)(((comment_size % 2) == 0) ? comment_size : comment_size + 1)); /* ANNO chunk size, the No of chars */
	  st_writes(ft, ft->comment);
	  if (comment_size % 2 == 1)
		st_writes(ft, " ");
	}

	/* COMM chunk -- describes encoding (and #frames) */
	st_writes(ft, "COMM");
	st_writedw(ft, (LONG) 18); /* COMM chunk size */
	st_writew(ft, ft->info.channels); /* nchannels */
	st_writedw(ft, nframes); /* number of frames */
	st_writew(ft, bits); /* sample width, in bits */
	write_ieee_extended(ft, (double)ft->info.rate);

	/* MARK chunk -- set markers */
	if (ft->instr.nloops) {
		st_writes(ft, "MARK");
		if (ft->instr.nloops > 2)
			ft->instr.nloops = 2;
		st_writedw(ft, 2 + 16*ft->instr.nloops);
		st_writew(ft, ft->instr.nloops);

		for(i = 0; i < ft->instr.nloops; i++) {
			st_writew(ft, i + 1);
			st_writedw(ft, ft->loops[i].start);
			st_writeb(ft, 0);
			st_writeb(ft, 0);
			st_writew(ft, i*2 + 1);
			st_writedw(ft, ft->loops[i].start + ft->loops[i].length);
			st_writeb(ft, 0);
			st_writeb(ft, 0);
			}

		st_writes(ft, "INST");
		st_writedw(ft, 20);
		/* random MIDI shit that we default on */
		st_writeb(ft, ft->instr.MIDInote);
		st_writeb(ft, 0);			/* detune */
		st_writeb(ft, ft->instr.MIDIlow);
		st_writeb(ft, ft->instr.MIDIhi);
		st_writeb(ft, 1);			/* low velocity */
		st_writeb(ft, 127);			/* hi  velocity */
		st_writew(ft, 0);				/* gain */

		/* sustain loop */
		st_writew(ft, ft->loops[0].type);
		st_writew(ft, 1);				/* marker 1 */
		st_writew(ft, 3);				/* marker 3 */
		/* release loop, if there */
		if (ft->instr.nloops == 2) {
			st_writew(ft, ft->loops[1].type);
			st_writew(ft, 2);			/* marker 2 */
			st_writew(ft, 4);			/* marker 4 */
		} else {
			st_writew(ft, 0);			/* no release loop */
			st_writew(ft, 0);
			st_writew(ft, 0);
		}
	}

	/* SSND chunk -- describes data */
	st_writes(ft, "SSND");
	/* chunk size */
	st_writedw(ft, 8 + nframes * ft->info.channels * ft->info.size); 
	st_writedw(ft, (LONG) 0); /* offset */
	st_writedw(ft, (LONG) 0); /* block size */
	return(ST_SUCCESS);
}

static double read_ieee_extended(ft)
ft_t ft;
{
	char buf[10];
	if (fread(buf, 1, 10, ft->fp) != 10)
	{
		st_fail("EOF while reading IEEE extended number");
		return(ST_EOF);
	}
	return ConvertFromIeeeExtended(buf);
}

static void write_ieee_extended(ft, x)
ft_t ft;
double x;
{
	char buf[10];
	ConvertToIeeeExtended(x, buf);
	/*
	st_report("converted %g to %o %o %o %o %o %o %o %o %o %o",
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

static void ConvertToIeeeExtended(num, bytes)
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

static double ConvertFromIeeeExtended(bytes)
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

