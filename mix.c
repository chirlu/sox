/*
 * Mix - the audiofile mixing program of SOX
 *
 * This is the main function for the command line sox program.
 *
 * January 26, 2001
 * Copyright 2001 ben last, Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * ben last, Lance Norskog And Sundry Contributors are not responsible
 * for the consequences of using this software.
 *
 * Change History:
 *
 * January 26, 2001 - ben last (ben@benlast.com)
 *   Derived mix.c from sox.c and then rewrote it for
 *   mix functionality.
 */ 

/* FIXME: Quickly ported from 12.16 to 12.17.2... Make sure all st_*
 * functions are checking return values!
 */

#include "st.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>		/* for malloc() */
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <errno.h>
#include <sys/types.h>		/* for fstat() */
#include <sys/stat.h>		/* for fstat() */

#ifdef HAVE_UNISTD_H
#include <unistd.h>		/* for unlink() */
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#ifndef HAVE_GETOPT
int getopt(int,char **,char *);
extern char *optarg;
extern int optind;
#endif
#endif

#ifdef VMS
#include <perror.h>
#define LASTCHAR        ']'
#else
#define LASTCHAR        '/'
#endif

/*
 * SOX mix main program.
 */

static int dovolume = 0;	/* User wants volume change */
static double volume = 1.0;	/* Linear volume change */
static int clipped = 0;		/* Volume change clipping errors */
static int writing = 0;		/* are we writing to a file? */

void init();
void doopts(int, char **);
void usage(char *);
int filetype(int);
void process();
void statistics();
LONG volumechange();
void checkeffect(eff_t);
int flow_effect(int);
int drain_effect(int);

struct st_soundstream informat, mixformat, outformat;

static ft_t ft;

char *ifile, *mfile, *ofile, *itype, *mtype, *otype;
extern char *optarg;
extern int optind;

int main(argc, argv)
int argc;
char **argv;
{
	myname = argv[0];
	init();
	
	ifile = mfile = ofile = NULL;

	/* Get input format options */
	ft = &informat;
	doopts(argc, argv);
	/* Get input file */
	if (optind >= argc)
		usage("No input file?");
	
	ifile = argv[optind];
	if (! strcmp(ifile, "-"))
		ft->fp = stdin;
	else if ((ft->fp = fopen(ifile, READBINARY)) == NULL)
		st_fail("Can't open input file '%s': %s", 
			ifile, strerror(errno));
	ft->filename = ifile;
	optind++;

	/* Get mix format options */
	ft = &mixformat;
	doopts(argc, argv);
	/* Get mix file */
	if (optind >= argc)
		usage("No mix file?");
	
	mfile = argv[optind];
	if (! strcmp(mfile, "-"))
		ft->fp = stdin;
	else if ((ft->fp = fopen(mfile, READBINARY)) == NULL)
		st_fail("Can't open mix file '%s': %s", 
			mfile, strerror(errno));
	ft->filename = mfile;
	optind++;

	/* Get output format options */
	ft = &outformat;
	doopts(argc, argv);
	writing = 1;
	if (writing) {
	    /* Get output file */
	    if (optind >= argc)
		usage("No output file?");
	    ofile = argv[optind];
	    ft->filename = ofile;
	    /*
	     * There are two choices here:
	     *	1) stomp the old file - normal shell "> file" behavior
	     *	2) fail if the old file already exists - csh mode
	     */
	    if (! strcmp(ofile, "-"))
	    {
		ft->fp = stdout;

		/* stdout tends to be line-buffered.  Override this */
		/* to be Full Buffering. */
		if (setvbuf (ft->fp,NULL,_IOFBF,sizeof(char)*BUFSIZ))
		    st_fail("Can't set write buffer");
	    }
	    else {

		ft->fp = fopen(ofile, WRITEBINARY);

		if (ft->fp == NULL)
		    st_fail("Can't open output file '%s': %s", 
			 ofile, strerror(errno));

		/* stdout tends to be line-buffered.  Override this */
		/* to be Full Buffering. */
		if (setvbuf (ft->fp,NULL,_IOFBF,sizeof(char)*BUFSIZ))
		    st_fail("Can't set write buffer");

	    } /* end of else != stdout */
	    
	    /* Move past filename */
	    optind++;
	} /* end if writing */

	/* Check global arguments */
	if (volume <= 0.0)
		st_fail("Volume must be greater than 0.0");
	
#if	defined(DUMB_FILESYSETM)
	informat.seekable  = 0;
	mixformat.seekable  = 0;
	outformat.seekable = 0;
#else
	informat.seekable  = (filetype(fileno(informat.fp)) == S_IFREG);
	mixformat.seekable  = (filetype(fileno(mixformat.fp)) == S_IFREG);
	outformat.seekable = (filetype(fileno(outformat.fp)) == S_IFREG); 
#endif

	/* If file types have not been set with -t, set from file names. */
	if (! informat.filetype) {
		if ((informat.filetype = strrchr(ifile, LASTCHAR)) != NULL)
			informat.filetype++;
		else
			informat.filetype = ifile;
		if ((informat.filetype = strrchr(informat.filetype, '.')) != NULL)
			informat.filetype++;
		else /* Default to "auto" */
			informat.filetype = "auto";
	}
	if (! mixformat.filetype) {
		if ((mixformat.filetype = strrchr(mfile, LASTCHAR)) != NULL)
			mixformat.filetype++;
		else
			mixformat.filetype = mfile;
		if ((mixformat.filetype = strrchr(mixformat.filetype, '.')) != NULL)
			mixformat.filetype++;
		else /* Default to "auto" */
			mixformat.filetype = "auto";
	}
	if (writing && ! outformat.filetype) {
		if ((outformat.filetype = strrchr(ofile, LASTCHAR)) != NULL)
			outformat.filetype++;
		else
			outformat.filetype = ofile;
		if ((outformat.filetype = strrchr(outformat.filetype, '.')) != NULL)
			outformat.filetype++;
	}
	/* Default the input and mix comments to filenames. 
	 * The output comment will be assigned when the informat 
	 * structure is copied to the outformat. 
	 */
	informat.comment = informat.filename;
	mixformat.comment = mixformat.filename;

	process();
	statistics();
	return(0);
}

#ifdef HAVE_GETOPT_H
char *getoptstr = "+r:v:t:c:phsuUAagbwlfdDxV";
#else
char *getoptstr = "r:v:t:c:phsuUAagbwlfdDxV";
#endif

void doopts(int argc, char **argv)
{
	int c;
	char *str;

	while ((c = getopt(argc, argv, getoptstr)) != -1) {
		switch(c) {
		case 'h':
			usage((char *)0);
			/* no return from above */

		case 't':
			if (! ft) usage("-t");
			ft->filetype = optarg;
			if (ft->filetype[0] == '.')
				ft->filetype++;
			break;

		case 'r':
			if (! ft) usage("-r");
			str = optarg;
#ifdef __alpha__
			if ((! sscanf(str, "%u", &ft->info.rate)) ||
					(ft->info.rate <= 0))
#else
			if ((! sscanf(str, "%lu", &ft->info.rate)) ||
					(ft->info.rate <= 0))
#endif
				st_fail("-r must be given a positive integer");
			break;
		case 'v':
			if (! ft) usage("-v");
			str = optarg;
			if ((! sscanf(str, "%lf", &volume)) ||
					(volume <= 0))
				st_fail("Volume value '%s' is not a number",
					optarg);
			dovolume = 1;
			break;

		case 'c':
			if (! ft) usage("-c");
			str = optarg;
			if (! sscanf(str, "%d", &ft->info.channels))
				st_fail("-c must be given a number");
			break;
		case 'b':
			if (! ft) usage("-b");
			ft->info.size = ST_SIZE_BYTE;
			break;
		case 'w':
			if (! ft) usage("-w");
			ft->info.size = ST_SIZE_WORD;
			break;
		case 'l':
			if (! ft) usage("-l");
			ft->info.size = ST_SIZE_DWORD;
			break;
		case 'f':
			if (! ft) usage("-f");
			ft->info.size = ST_SIZE_FLOAT;
			break;
		case 'd':
			if (! ft) usage("-d");
			ft->info.size = ST_SIZE_DOUBLE;
			break;
		case 'D':
			if (! ft) usage("-D");
			ft->info.size = ST_SIZE_IEEE;
			break;

		case 's':
			if (! ft) usage("-s");
			ft->info.encoding = ST_ENCODING_SIGN2;
			break;
		case 'u':
			if (! ft) usage("-u");
			ft->info.encoding = ST_ENCODING_UNSIGNED;
			break;
		case 'U':
			if (! ft) usage("-U");
			ft->info.encoding = ST_ENCODING_ULAW;
			break;
		case 'A':
			if (! ft) usage("-A");
			ft->info.encoding = ST_ENCODING_ALAW;
			break;
		case 'a':
			if (! ft) usage("-a");
			ft->info.encoding = ST_ENCODING_ADPCM;
			break;
		case 'g':
			if (! ft) usage("-g");
			ft->info.encoding = ST_ENCODING_GSM;
			break;
		
		case 'x':
			if (! ft) usage("-x");
			ft->swap = 1;
			break;
		
		case 'V':
			verbose = 1;
			break;
		}
	}
}

void init() {

	/* init files */
	informat.info.rate      = mixformat.info.rate = outformat.info.rate  = 0;
	informat.info.size      = mixformat.info.size = outformat.info.size  = -1;
	informat.info.encoding  = mixformat.info.encoding = outformat.info.encoding = -1;
	informat.info.channels  = mixformat.info.channels = outformat.info.channels = -1;
	informat.comment   = mixformat.comment = outformat.comment = NULL;
	informat.swap      = mixformat.swap = 0;
	informat.filetype  = mixformat.filetype = outformat.filetype  = (char *) 0;
	informat.fp        = stdin;
	mixformat.fp       = NULL;
	outformat.fp       = stdout;
	informat.filename  = "input";
	mixformat.filename  = "mix";
	outformat.filename = "output";
}

/* 
 * Process input file -> mix with mixfile -> output file
 *	one buffer at a time
 */

void process() {
    LONG result, i, *ibuf, *mbuf, *obuf, ilen=0, mlen=0, olen=0;

    st_gettype(&informat);
    st_gettype(&mixformat);
    if (writing)
	st_gettype(&outformat);
    
    /* Read and write starters can change their formats. */
    (* informat.h->startread)(&informat);
    st_checkformat(&informat);
    
    (* mixformat.h->startread)(&mixformat);
    st_checkformat(&mixformat);
    
    if (dovolume)
	st_report("Volume factor: %f\n", volume);
    
    st_report("Input file: using sample rate %lu\n\tsize %s, encoding %s, %d %s",
	   informat.info.rate, st_sizes_str[informat.info.size], 
	   st_encodings_str[informat.info.encoding], informat.info.channels, 
	   (informat.info.channels > 1) ? "channels" : "channel");
    if (informat.comment)
	st_report("Input file: comment \"%s\"\n", informat.comment);
	
    st_report("Mix file: using sample rate %lu\n\tsize %s, encoding %s, %d %s",
	   mixformat.info.rate, st_sizes_str[mixformat.info.size], 
	   st_encodings_str[mixformat.info.encoding], mixformat.info.channels, 
	   (mixformat.info.channels > 1) ? "channels" : "channel");
    if (mixformat.comment)
	st_report("Mix file: comment \"%s\"\n", mixformat.comment);

/*
	Expect the formats of the input and mix files to be compatible.
	Although it's true that I could fix it up on the fly, it's easier
	for me to tell the user to use sox to fix it first and it's also
	more reliable; better to use the code that Chris, Lance & Co have
	written well than for me to rewrite the same stuff badly!

	Sample rates are a cause for failure.
*/
    if(informat.info.rate != mixformat.info.rate)
        st_fail("fail: Input and mix files have different sample rates.\nUse sox to resample one of them.\n");

    /* need to check EFF_REPORT */
    if (writing) {
	st_copyformat(&informat, &outformat);
	(* outformat.h->startwrite)(&outformat);
	st_checkformat(&outformat);
	st_report("Output file: using sample rate %lu\n\tsize %s, encoding %s, %d %s",
	       outformat.info.rate, st_sizes_str[outformat.info.size], 
	       st_encodings_str[outformat.info.encoding], outformat.info.channels, 
	       (outformat.info.channels > 1) ? "channels" : "channel");
	if (outformat.comment)
	    st_report("Output file: comment \"%s\"\n", outformat.comment);
    }

    /* Allocate buffers */
    ibuf = (LONG *) malloc(BUFSIZ * sizeof(LONG));
    mbuf = (LONG *) malloc(BUFSIZ * sizeof(LONG));
    obuf = (LONG *) malloc(BUFSIZ * sizeof(LONG));
    if((!ibuf) || (!mbuf) || (!obuf))
    {
	fprintf(stderr, "Can't allocate memory for buffer (%s)\n", 
		strerror(errno));
	return;
    }

    /* Read initial chunks of input data. */
    /* Do the input file first */
    ilen = (*informat.h->read)(&informat, ibuf, (LONG) BUFSIZ);
    /* Change the volume of this data if needed. */
    if(dovolume && ilen)
	for (i = 0; i < ilen; i++)
	    ibuf[i] = volumechange(ibuf[i]);

    /* Now do the mixfile */
    mlen = (*mixformat.h->read)(&mixformat, mbuf, (LONG) BUFSIZ);
    /* Change the volume of this data if needed. */
    if(dovolume && mlen)
	for (i = 0; i < mlen; i++)
	    mbuf[i] = volumechange(mbuf[i]);

    /* mix until both input files are done */
    while (ilen || mlen) {
	/* Do the mixing from ibuf and mbuf into obuf. */
	olen = (ilen > mlen) ? ilen : mlen;
	for (i=0; i<olen; i++)
	{
	    /* initial crude mix that might lose a bit of accuracy */
	    result = (i<ilen) ? (ibuf[i]/2) : 0;
	    if (i<mlen) result += mbuf[i]/2;
	    obuf[i] = result;
	}

	if (writing && olen)
	    (* outformat.h->write)(&outformat, obuf, (LONG) olen);

	/* Read another chunk of input data. */
	ilen = (*informat.h->read)(&informat, 
		ibuf, (LONG) BUFSIZ);

	/* Change volume of these samples if needed. */
	if(dovolume && ilen)
	    for (i = 0; i < ilen; i++)
		ibuf[i] = volumechange(ibuf[i]);

	/* Read another chunk of mix data. */
	mlen = (*mixformat.h->read)(&mixformat, 
		mbuf, (LONG) BUFSIZ);

	/* Change volume of these samples if needed. */
	if(dovolume && mlen)
	    for (i = 0; i < mlen; i++)
		mbuf[i] = volumechange(mbuf[i]);
    }

    (* informat.h->stopread)(&informat);
    fclose(informat.fp);

    (* mixformat.h->stopread)(&mixformat);
    fclose(mixformat.fp);

    if (writing)
        (* outformat.h->stopwrite)(&outformat);
    if (writing)
        fclose(outformat.fp);
}

/* Guido Van Rossum fix */
void statistics() {
	if (dovolume && clipped > 0)
		st_report("Volume change clipped %d samples", clipped);
}

LONG volumechange(y)
LONG y;
{
	double y1;

	y1 = y * volume;
	if (y1 < -2147483647.0) {
		y1 = -2147483647.0;
		clipped++;
	}
	else if (y1 > 2147483647.0) {
		y1 = 2147483647.0;
		clipped++;
	}

	return y1;
}

int filetype(fd)
int fd;
{
	struct stat st;

	fstat(fd, &st);

	return st.st_mode & S_IFMT;
}

char *usagestr = 
"[ gopts ] [ fopts ] ifile [ fopts ] mixfile [ fopts ] ofile";

void usage(opt)
char *opt;
{
    int i;
    
	fprintf(stderr, "%s: ", myname);
	if (verbose || !opt)
		fprintf(stderr, "%s\n\n", st_version());
	fprintf(stderr, "Usage: %s\n\n", usagestr);
	if (opt)
		fprintf(stderr, "Failed at: %s\n", opt);
	else {
	    fprintf(stderr,"gopts: -e -h -p -v volume -V\n\n");
	    fprintf(stderr,"fopts: -r rate -c channels -s/-u/-U/-A/-a/-g -b/-w/-l/-f/-d/-D -x\n\n");
	    fprintf(stderr, "Supported file s: ");
	    for (i = 0; st_formats[i].names != NULL; i++) {
		/* only print the first name */
		fprintf(stderr, "%s ", st_formats[i].names[0]);
	    }
	    fputc('\n', stderr);
	}
	exit(1);
}


/* called from util.c:fail */
void cleanup() {
	/* Close the input file and outputfile before exiting*/
	if (informat.fp)
		fclose(informat.fp);
	if (mixformat.fp)
		fclose(mixformat.fp);
	if (outformat.fp) {
		fclose(outformat.fp);
		/* remove the output file because we failed, if it's ours. */
		/* Don't if its not a regular file. */
		if (filetype(fileno(outformat.fp)) == S_IFREG)
		    REMOVE(outformat.filename);
	}
}
