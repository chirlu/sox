/*
 * June 30, 1992
 * Copyright 1992 Leigh Smith And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Leigh Smith And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools SampleVision file format driver.
 * Output is always in little-endian (80x86/VAX) order.
 * 
 * Derived from: Sound Tools skeleton handler file.
 *
 * Add: Loop point verbose info.  It's a start, anyway.
 */

#include "st.h"
#include <string.h>

#define NAMELEN    30		/* Size of Samplevision name */
#define COMMENTLEN 60		/* Size of Samplevision comment, not shared */
#define MIDI_UNITY 60		/* MIDI note number to play sample at unity */

/* The header preceeding the sample data */
struct smpheader {
	char Id[18];		/* File identifier */
	char version[4];	/* File version */
	char comments[COMMENTLEN];	/* User comments */
	char name[NAMELEN + 1];	/* Sample Name, left justified */
};
#define HEADERSIZE (sizeof(struct smpheader) - 1)	/* -1 for name's \0 */

/* Samplevision loop definition structure */
struct loop {
	ULONG start; /* Sample count into sample data, not byte count */
	ULONG end;   /* end point */
	char type;	     /* 0 = loop off, 1 = forward, 2 = forw/back */
	short count;	     /* No of times to loop */
};

/* Samplevision marker definition structure */
struct marker {
	char name[10];		/* Ascii Marker name */
	ULONG position;	/* Sample Number, not byte number */
};

/* The trailer following the sample data */
struct smptrailer {
	struct loop loops[8];		/* loops */
	struct marker markers[8];	/* markers */
	char MIDInote;			/* for unity pitch playback */
	ULONG rate;			/* in hertz */
	ULONG SMPTEoffset;		/* in subframes - huh? */
	ULONG CycleSize;		/* sample count in one cycle of the */
					/* sampled sound -1 if unknown */
};

/* Private data for SMP file */
typedef struct smpstuff {
  ULONG NoOfSamps;		/* Sample data count in words */
  /* comment memory resides in private data because it's small */
  char comment[COMMENTLEN + NAMELEN + 3];
} *smp_t;

char *SVmagic = "SOUND SAMPLE DATA ", *SVvers = "2.1 ";

/*
 * Read the SampleVision trailer structure.
 * Returns 1 if everything was read ok, 0 if there was an error.
 */
static int readtrailer(ft, trailer)
ft_t ft;
struct smptrailer *trailer;
{
	int i;

	rlshort(ft);			/* read reserved word */
	for(i = 0; i < 8; i++) {	/* read the 8 loops */
		trailer->loops[i].start = rllong(ft);
		ft->loops[i].start = trailer->loops[i].start;
		trailer->loops[i].end = rllong(ft);
		ft->loops[i].length = 
			trailer->loops[i].end - trailer->loops[i].start;
		trailer->loops[i].type = getc(ft->fp);
		ft->loops[i].type = trailer->loops[8].type;
		trailer->loops[i].count = rlshort(ft);
		ft->loops[8].count = trailer->loops[8].count;
	}
	for(i = 0; i < 8; i++) {	/* read the 8 markers */
		if (fread(trailer->markers[i].name, 1, 10, ft->fp) != 10)
			return(0);
		trailer->markers[i].position = rllong(ft);
	}
	trailer->MIDInote = getc(ft->fp);
	trailer->rate = rllong(ft);
	trailer->SMPTEoffset = rllong(ft);
	trailer->CycleSize = rllong(ft);
	return(1);
}

/*
 * set the trailer data - loops and markers, to reasonably benign values
 */
void settrailer(ft, trailer, rate)
ft_t ft;
struct smptrailer *trailer;
unsigned int rate;
{
	int i;

	for(i = 0; i < 8; i++) {	/* copy the 8 loops */
	    if (ft->loops[i].type != 0) {
		trailer->loops[i].start = ft->loops[i].start;
		/* to mark it as not set */
		trailer->loops[i].end = ft->loops[i].start + ft->loops[i].length;
		trailer->loops[i].type = ft->loops[i].type;
		trailer->loops[i].count = ft->loops[i].count;	
	    } else {
		/* set first loop start as FFFFFFFF */
		trailer->loops[i].start = ~0;	
		/* to mark it as not set */
		trailer->loops[i].end = 0;	
		trailer->loops[i].type = 0;
		trailer->loops[i].count = 0;
	    }
	}
	for(i = 0; i < 8; i++) {	/* write the 8 markers */
		strcpy(trailer->markers[i].name, "          ");
		trailer->markers[i].position = ~0;
	}
	trailer->MIDInote = MIDI_UNITY;		/* Unity play back */
	trailer->rate = rate;
	trailer->SMPTEoffset = 0;
	trailer->CycleSize = -1;
}

/*
 * Write the SampleVision trailer structure.
 * Returns 1 if everything was written ok, 0 if there was an error.
 */
static int writetrailer(ft, trailer)
ft_t ft;
struct smptrailer *trailer;
{
	int i;

	wlshort(ft, 0);			/* write the reserved word */
	for(i = 0; i < 8; i++) {	/* write the 8 loops */
		wllong(ft, trailer->loops[i].start);
		wllong(ft, trailer->loops[i].end);
		putc(trailer->loops[i].type, ft->fp);
		wlshort(ft, trailer->loops[i].count);
	}
	for(i = 0; i < 8; i++) {	/* write the 8 markers */
		if (fwrite(trailer->markers[i].name, 1, 10, ft->fp) != 10)
			return(0);
		wllong(ft, trailer->markers[i].position);
	}
	putc(trailer->MIDInote, ft->fp);
	wllong(ft, trailer->rate);
	wllong(ft, trailer->SMPTEoffset);
	wllong(ft, trailer->CycleSize);
	return(1);
}

/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *	Find out sampling rate, 
 *	size and style of samples, 
 *	mono/stereo/quad.
 */
void smpstartread(ft) 
ft_t ft;
{
	smp_t smp = (smp_t) ft->priv;
	int littlendian = 0, i;
	int namelen, commentlen;
	LONG samplestart;
	char *endptr;
	struct smpheader header;
	struct smptrailer trailer;

	/* If you need to seek around the input file. */
	if (! ft->seekable)
		fail("SMP input file must be a file, not a pipe");

	/* Read SampleVision header */
	if (fread((char *) &header, 1, HEADERSIZE, ft->fp) != HEADERSIZE)
		fail("unexpected EOF in SMP header");
	if (strncmp(header.Id, SVmagic, 17) != 0)
		fail("SMP header does not begin with magic word %s\n", SVmagic);
	if (strncmp(header.version, SVvers, 4) != 0)
		fail("SMP header is not version %s\n", SVvers);

	/* Format the sample name and comments to a single comment */
	/* string. We decrement the counters till we encounter non */
        /* padding space chars, so the *lengths* are low by one */
        for (namelen = NAMELEN-1;
            namelen >= 0 && header.name[namelen] == ' '; namelen--)
	  ;
        for (commentlen = COMMENTLEN-1;
            commentlen >= 0 && header.comments[commentlen] == ' '; commentlen--)
	  ;
	sprintf(smp->comment, "%.*s: %.*s", namelen+1, header.name,
		commentlen+1, header.comments);
	ft->comment = smp->comment;

	report("SampleVision file name and comments: %s", ft->comment);
	/* Extract out the sample size (always intel format) */
	smp->NoOfSamps = rllong(ft);
	/* mark the start of the sample data */
	samplestart = ftell(ft->fp);

	/* seek from the current position (the start of sample data) by */
	/* NoOfSamps * 2 */
	if (fseek(ft->fp, smp->NoOfSamps * 2L, 1) == -1)
		fail("SMP unable to seek to trailer");
	if (!readtrailer(ft, &trailer))
		fail("unexpected EOF in SMP trailer");

	/* seek back to the beginning of the data */
	if (fseek(ft->fp, samplestart, 0) == -1) 
		fail("SMP unable to seek back to start of sample data");

	ft->info.rate = (int) trailer.rate;
	ft->info.size = WORD;
	ft->info.style = SIGN2;
	ft->info.channels = 1;

	endptr = (char *) &littlendian;
	*endptr = 1;
	if (littlendian != 1)
		ft->swap = 1;
	
	if (verbose) {
		fprintf(stderr, "SampleVision trailer:\n");
		for(i = 0; i < 8; i++) if (1 || trailer.loops[i].count) {
#ifdef __alpha__
			fprintf(stderr, "Loop %d: start: %6d", i, trailer.loops[i].start);
			fprintf(stderr, " end:   %6d", trailer.loops[i].end);
#else
			fprintf(stderr, "Loop %d: start: %6ld", i, trailer.loops[i].start);
			fprintf(stderr, " end:   %6ld", trailer.loops[i].end);
#endif
			fprintf(stderr, " count: %6d", trailer.loops[i].count);
			fprintf(stderr, " type:  ");
			switch(trailer.loops[i].type) {
				case 0: fprintf(stderr, "off\n"); break;
				case 1: fprintf(stderr, "forward\n"); break;
				case 2: fprintf(stderr, "forward/backward\n"); break;
			}
		}
		fprintf(stderr, "MIDI Note number: %d\n\n", trailer.MIDInote);
	}
	ft->instr.nloops = 0;
	for(i = 0; i < 8; i++) 
		if (trailer.loops[i].type) 
			ft->instr.nloops++;
	for(i = 0; i < ft->instr.nloops; i++) {
		ft->loops[i].type = trailer.loops[i].type;
		ft->loops[i].count = trailer.loops[i].count;
		ft->loops[i].start = trailer.loops[i].start;
		ft->loops[i].length = trailer.loops[i].end 
			- trailer.loops[i].start;
	}
	ft->instr.MIDIlow = ft->instr.MIDIhi =
		ft->instr.MIDInote = trailer.MIDInote;
	if (ft->instr.nloops > 0)
		ft->instr.loopmode = LOOP_8;
	else
		ft->instr.loopmode = LOOP_NONE;
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */
LONG smpread(ft, buf, len) 
ft_t ft;
LONG *buf, len;
{
	smp_t smp = (smp_t) ft->priv;
	LONG datum;
	int done = 0;
	
	for(; done < len && smp->NoOfSamps; done++, smp->NoOfSamps--) {
		datum = rshort(ft);
		/* scale signed up to long's range */
		*buf++ = LEFT(datum, 16);
	}
	return done;
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
void smpstopread(ft) 
ft_t ft;
{
}

void smpstartwrite(ft) 
ft_t ft;
{
	smp_t smp = (smp_t) ft->priv;
	struct smpheader header;

	/* If you have to seek around the output file */
	if (! ft->seekable)
		fail("Output .smp file must be a file, not a pipe");

	/* If your format specifies any of the following info. */
	ft->info.size = WORD;
	ft->info.style = SIGN2;
	ft->info.channels = 1;

	strcpy(header.Id, SVmagic);
	strcpy(header.version, SVvers);
	sprintf(header.comments, "%-*s", COMMENTLEN, "Converted using Sox.");
	sprintf(header.name, "%-*.*s", NAMELEN, NAMELEN, ft->comment);

	/* Write file header */
	if(fwrite(&header, 1, HEADERSIZE, ft->fp) != HEADERSIZE)
		fail("SMP: Can't write header completely");
	wllong(ft, 0);	/* write as zero length for now, update later */
	smp->NoOfSamps = 0;
}

void smpwrite(ft, buf, len) 
ft_t ft;
LONG *buf, len;
{
	smp_t smp = (smp_t) ft->priv;
	register int datum;

	while(len--) {
		datum = (int) RIGHT(*buf++, 16);
		wlshort(ft, datum);
		smp->NoOfSamps++;
	}
	/* If you cannot write out all of the supplied samples, */
	/*	fail("SMP: Can't write all samples to %s", ft->filename); */
}

void smpstopwrite(ft) 
ft_t ft;
{
	smp_t smp = (smp_t) ft->priv;
	struct smptrailer trailer;

	/* Assign the trailer data */
	settrailer(ft, &trailer, ft->info.rate);
	writetrailer(ft, &trailer);
	if (fseek(ft->fp, 112, 0) == -1)
		fail("SMP unable to seek back to save size");
	wllong(ft, smp->NoOfSamps);
}
