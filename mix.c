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

static void init();
static void doopts(int, char **);
static void usage(char *);
static int filetype(int);
static void process();
static void statistics();
static LONG volumechange();

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
#if	defined(DUMB_FILESYSTEM)
	ft->seekable = 0;
#else
	ft->seekable = (filetype(fileno(informat.fp)) == S_IFREG);
#endif

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
#if	defined(DUMB_FILESYSTEM)
	ft->seekable = 0;
#else
	ft->seekable = (filetype(fileno(informat.fp)) == S_IFREG);
#endif

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

	    /* Move past filename */
	    optind++;

	    /* Hold off on opening file until the very last minute.
	     * This allows us to verify header in input files are
	     * what they should be and parse effect command lines.
	     * That way if anything is found invalid, we will abort
	     * without truncating any existing file that has the same
	     * output filename.
	     */

	} /* end if writing */

	/* Check global arguments */

	/* negative volume is phase-reversal */
	if (dovolume && volume < 0.0)
	    st_report("Volume adjustment is negative.  This will result in a phase change\n");
	
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
char *getoptstr = "+r:v:t:c:hsuUAagbwlfdDxV";
#else
char *getoptstr = "r:v:t:c:hsuUAagbwlfdDxV";
#endif

static void doopts(int argc, char **argv)
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
		case 'i':
			if (! ft) usage("-i");
			ft->info.encoding = ST_ENCODING_IMA_ADPCM;
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

static void init() {

    /* init files */
    st_initformat(&informat);
    st_initformat(&mixformat);
    st_initformat(&outformat);

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

static void process() {
    LONG result, i, *ibuf, *mbuf, *obuf, ilen=0, mlen=0, olen=0;

    if ( st_gettype(&informat) )
	st_fail("bad input format");
    
    if ( st_gettype(&mixformat) )
	st_fail("bad input format");

    if (writing)
	if (st_gettype(&outformat))
	    st_fail("bad output format");
    
    /* Read and write starters can change their formats. */
    if ((* informat.h->startread)(&informat) == ST_EOF)
    {
	st_fail(informat.st_errstr);
    }

    if (st_checkformat(&informat))
	st_fail("bad input format");
    
    if ((* mixformat.h->startread)(&mixformat) == ST_EOF)
    {
	st_fail(mixformat.st_errstr);
    }

    if (st_checkformat(&mixformat))
	st_fail("bad input format");
    
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

    /* Expect the formats to have the same sample rate.  Although its
     * possible to auto-convert the sample rate of one file to match
     * the other using the rate effect, its easier to tell the user
     * to figure it out and run sox on the input file seperately to
     * correct it before hand.
     */
    if(informat.info.rate != mixformat.info.rate)
        st_fail("fail: Input and mix files have different sample rates.\nUse sox to resample one of them.\n");

    /* need to check EFF_REPORT */
    if (writing) {
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
	    {
	        st_fail("Can't set write buffer");
	    }
	 }
         else {

	     ft->fp = fopen(ofile, WRITEBINARY);

	     if (ft->fp == NULL)
	         st_fail("Can't open output file '%s': %s", 
		      ofile, strerror(errno));

	     /* stdout tends to be line-buffered.  Override this */
	     /* to be Full Buffering. */
	     if (setvbuf (ft->fp,NULL,_IOFBF,sizeof(char)*BUFSIZ))
	     {
	         st_fail("Can't set write buffer");
	     }

        } /* end of else != stdout */
#if	defined(DUMB_FILESYSTEM)
	outformat.seekable = 0;
#else
	outformat.seekable  = (filetype(fileno(outformat.fp)) == S_IFREG);
#endif

	/* FIXME: We are defaulting to using the first file's format
	 * for the output file. Should compare the two inputs and
	 * warn user that the output file will use the first input
	 * file's values.
	 */
	st_copyformat(&informat, &outformat);
	if ((* outformat.h->startwrite)(&outformat) == ST_EOF)
	{
	    st_fail(outformat.st_errstr);
	}

	if (st_checkformat(&outformat))
	    st_fail("bad output format");

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
    if(dovolume)
	clipped += volumechange(ibuf, ilen, volume);

    /* Now do the mixfile */
    mlen = (*mixformat.h->read)(&mixformat, mbuf, (LONG) BUFSIZ);
    /* Change the volume of this data if needed. */
    if(dovolume)
	clipped += volumechange(mbuf, mlen, volume);

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
	if(dovolume)
	    clipped += volumechange(ibuf, ilen, volume);

	/* Read another chunk of mix data. */
	mlen = (*mixformat.h->read)(&mixformat, 
		mbuf, (LONG) BUFSIZ);

	/* Change volume of these samples if needed. */
	if(dovolume)
	    clipped += volumechange(ibuf, ilen, volume);
    }

    /* If closing fails then just warn user instead of exiting.
     * This is because we are basically done with the files anyways.
     */
    if ((* informat.h->stopread)(&informat) == ST_EOF)
	st_warn(informat.st_errstr);
    fclose(informat.fp);

    if ((* mixformat.h->stopread)(&mixformat) == ST_EOF)
	st_warn(mixformat.st_errstr);
    fclose(mixformat.fp);

    if (writing)
        if ((* outformat.h->stopwrite)(&outformat) == ST_EOF)
	    st_warn(outformat.st_errstr);
    if (writing)
        fclose(outformat.fp);
}

/* Guido Van Rossum fix */
static void statistics() {
	if (dovolume && clipped > 0)
		st_report("Volume change clipped %d samples", clipped);
}

static LONG volumechange(buf, ct, vol)
LONG *buf;
LONG ct;
double vol;
{
	double y;
	LONG *p,*top;
	LONG clips=0;

	p = buf;
	top = buf+ct;
	while (p < top) {
	    y = vol * *p;
	    if (y < -2147483647.0) {
		y = -2147483647.0;
		clips++;
	    }
	    else if (y > 2147483647.0) {
		y = 2147483647.0;
		clips++;
	    }
	    *p++ = y + 0.5;
	}
	return clips;
}

static int filetype(fd)
int fd;
{
	struct stat st;

	fstat(fd, &st);

	return st.st_mode & S_IFMT;
}

static char *usagestr = 
"[ gopts ] [ fopts ] ifile [ fopts ] mixfile [ fopts ] ofile";

static void usage(opt)
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
