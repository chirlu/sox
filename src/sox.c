/*
 * Sox - The Swiss Army Knife of Audio Manipulation.
 *
 * This is the main function for the command line sox program.
 *
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 *
 * Change History:
 *
 * Nov. 1, 1999 - Stan Brooks (stabro@megsinet.com)
 *   Added -X input option, to extract range of samples.
 *   Moved volume and dovolume from static variables into ft->info...
 *   TODO: This should allow a way (kluge) of specifying multiple
 *   inputs to be mixed together.
 *
 * June 1, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *   Added patch to get volume working again.  Based on patch sent from
 *   Matija Nalis <mnalis@public.srce.hr>.
 *   Added command line switches to force format to ADPCM or GSM.
 *
 * September 12, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *   Reworked code that handled effects.  Wasn't correctly draining
 *   stereo effects and a few other problems.
 *   Made command usage (-h) show supported effects and file formats.
 *   (this is partially from a patch by Leigh Smith
 *    leigh@psychokiller.dialix.oz.au).
 *
 */ 

#include "st.h"
#include "version.h"
#include "patchlvl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>		/* for malloc() */
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
int getopt(P3(int,char **,char *));
extern char *optarg;
extern int optind;
#endif
#endif

#if defined(VMS)
#define LASTCHAR	']'
#elif defined(DOS) || defined(WIN32)
#define LASTCHAR	'\\'
#else
#define LASTCHAR        '/'
#endif

/*
 * SOX main program.
 *
 * Rewrite for new nicer option syntax.  July 13, 1991.
 * Rewrite for separate effects library.  Sep. 15, 1991.
 * Incorporate Jimen Ching's fixes for real library operation: Aug 3, 1994.
 * Rewrite for multiple effects: Aug 24, 1994.
 */

static int clipped = 0;		/* Volume change clipping errors */
static int writing = 0;	/* are we writing to a file? */
static int soxpreview = 0;	/* preview mode */


static LONG ibufl[BUFSIZ/2];	/* Left/right interleave buffers */
static LONG ibufr[BUFSIZ/2];	
static LONG obufl[BUFSIZ/2];
static LONG obufr[BUFSIZ/2];

/* local forward declarations */
static void init(P0);
static void doopts(P2(int, char **));
static void usage(P1(char *))NORET;
static int filetype(P1(int));
static void process(P0);
static void statistics(P0);
static LONG volumechange(P3(LONG *buf, LONG ct, double vol));
static void checkeffect(P1(eff_t));
static int flow_effect(P1(int));
static int drain_effect(P1(int));

static struct st_soundstream informat, outformat;

static ft_t ft;

#define MAXEFF 4
static struct st_effect eff;
static struct st_effect efftab[MAXEFF];	/* table of left/mono channel effects */
static struct st_effect efftabR[MAXEFF];	/* table of right channel effects */
				/* efftab[0] is the input stream */
static int neffects;			/* # of effects */
static char *ifile, *ofile;

int main(argc, argv)
int argc;
char **argv;
{
	myname = argv[0];
	init();
	
	ifile = ofile = NULL;

	/* Get input format options */
	ft = &informat;
	clipped = 0;
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
	ft->seekable  = (filetype(fileno(informat.fp)) == S_IFREG);
#endif

	optind++;

	/* If more arguments are left then look for -e to see if */
	/* no output file is used, then just do an effect */
	if (optind < argc && strcmp(argv[optind], "-e"))
	    writing = 1;
	else if (optind < argc) {
	    writing = 0;
	    optind++;  /* Move passed -e */
	}
	else
	    writing = 1;  /* No arguments left but let next check fail */
	    
	/* Get output format options */
	ft = &outformat;
	doopts(argc, argv);

	/* Find and save output filename, if writing to file */
	if (writing) {
	    /* Get output file */
	    if (optind >= argc)
		usage("No output file?");
	    ofile = argv[optind];
	    ft->filename = ofile;

	    /* Move passed filename */
	    optind++;

	    /* Hold off on opening file until the very last minute.
	     * This allows us to verify header in input files are
	     * what they should be and parse effect command lines.
	     * That way if anything is found invalid, we will abort
	     * without truncating any existing file that has the same
	     * output filename.
	     */
	}

	/* Get effect name */
	if (optind < argc) {
		eff.name = argv[optind];
		optind++;
		st_geteffect(&eff);
		(* eff.h->getopts)(&eff, argc - optind, &argv[optind]);
	} else {
		eff.name = "null";
		st_geteffect(&eff);
	}

	/* Check global arguments */
	if (informat.info.dovol && informat.info.vol == 0.0)
		st_fail("Volume must be non-zero"); /* negative volume is phase-reversal */
	
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
	if (writing && ! outformat.filetype) {
		if ((outformat.filetype = strrchr(ofile, LASTCHAR)) != NULL)
			outformat.filetype++;
		else
			outformat.filetype = ofile;
		if ((outformat.filetype = strrchr(outformat.filetype, '.')) != NULL)
			outformat.filetype++;
	}
	/* Default the input comment to the filename. 
	 * The output comment will be assigned when the informat 
	 * structure is copied to the outformat. 
	 */
	informat.comment = informat.filename;

	process();
	statistics();
	return(0);
}

#ifdef HAVE_GETOPT_H
static char *getoptstr = "+r:v:t:c:B:X:phsuUAaigbwlfdDxV";
#else
static char *getoptstr = "r:v:t:c:B:X:phsuUAaigbwlfdDxV";
#endif

static void doopts(argc, argv)
int argc;
char **argv;
{
	int c;
	char *str;

	while ((c = getopt(argc, argv, getoptstr)) != -1) {
		switch(c) {
		case 'p':
			soxpreview++;
			break;

		case 'h':
			usage((char *)0);
			/* no return from above */

		case 't':
			if (! ft) usage("-t");
			ft->filetype = optarg;
			if (ft->filetype[0] == '.')
				ft->filetype++;
			break;

		case 'X':  /* extract a subinterval of samples as input */
			if (! ft) usage("-X");
			str = optarg;
			/* FIXME: allow "A-B" "-B" "A-" "A,B" also maybe lists of ranges */
			if ( 2 != sscanf(str, "%lu-%lu", &ft->info.x0, &ft->info.x1))
				st_fail("eXtract range '%s' is not valid", optarg);
			if (ft->info.x1==0) ft->info.x1 -= 1; /* MAXULONG */
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
			if (!ft || ft->info.dovol) usage("-v");
			str = optarg;
			if (! sscanf(str, "%lf", &ft->info.vol)) /* neg volume is ok */
				st_fail("Volume value '%s' is not a number", optarg);
			ft->info.dovol = 1;
			break;

		case 'c':
			if (! ft) usage("-c");
			str = optarg;
			if (! sscanf(str, "%d", &ft->info.channels))
				st_fail("-c must be given a number");
			break;
		case 'B':
			if (! ft) usage("-B");
			str = optarg;
			if (! sscanf(str, "%hu", &ft->info.bs) || ft->info.bs<=0) /* blocksize */
				st_fail("-B must be given a positive number");
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

	/* SJB: fix the -X extract interval for # channels...
	 * this is not perfect re cases where channels=-1,
	 * and also indicates that info.x0, info.x1, info.x
	 * SHOULD be (long long) instead of (ULONG), for large files
	 */
	if (ft->info.channels > 1) {
		if (ft->info.x0 != 0)
			ft->info.x0 *= ft->info.channels;
		if (ft->info.x1 != MAXULONG && ft->info.x1 < MAXULONG/ft->info.channels)
			ft->info.x1 *= ft->info.channels;
	}

}

static void init(P0) {

	/* init files */
	informat.info.rate      = outformat.info.rate  = 0;
	informat.info.size      = outformat.info.size  = -1;
	informat.info.encoding  = outformat.info.encoding = -1;
	informat.info.channels  = outformat.info.channels = -1;
	informat.info.bs        = outformat.info.bs    = 0;
	informat.info.dovol     = outformat.info.dovol = 0;
	informat.info.vol       = outformat.info.vol   = 1.0;
	informat.info.x         = outformat.info.x  = 0;
	informat.info.x0        = outformat.info.x0 = 0;
	informat.info.x1        = outformat.info.x1 = -1;
	informat.comment   = outformat.comment = NULL;
	informat.swap      = 0;
	informat.filetype  = outformat.filetype  = (char *) 0;
	informat.fp        = stdin;
	outformat.fp       = stdout;
	informat.filename  = "input";
	outformat.filename = "output";
}

/* 
 * Process input file -> effect table -> output file
 *	one buffer at a time
 */

static void process(P0) {
    LONG i;
    int e, f, havedata;

    st_gettype(&informat);
    if (writing)
	st_gettype(&outformat);
    
    /* Read and write starters can change their formats. */
    if ((* informat.h->startread)(&informat) == ST_EOF)
    {
        st_fail(informat.st_errstr);
    }
    st_checkformat(&informat);
    
    if (informat.info.dovol)
	st_report("Volume factor: %f\n", informat.info.vol);
    if (informat.info.x0 || informat.info.x1 != MAXULONG)
	st_report("Extract samples %lu <= x < %lu\n", informat.info.x0, informat.info.x0);
    
    st_report("Input file: using sample rate %lu\n\tsize %s, encoding %s, %d %s",
	   informat.info.rate, st_sizes_str[informat.info.size], 
	   st_encodings_str[informat.info.encoding], informat.info.channels, 
	   (informat.info.channels > 1) ? "channels" : "channel");
    if (informat.comment)
	st_report("Input file: comment \"%s\"\n", informat.comment);
	
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
	outformat.seekable  = (filetype(fileno(informat.fp)) == S_IFREG);
#endif

	st_copyformat(&informat, &outformat);
	if ((* outformat.h->startwrite)(&outformat) == ST_EOF)
	{
	    st_fail(outformat.st_errstr);
	}
	st_checkformat(&outformat);
	st_cmpformats(&informat, &outformat);
	st_report("Output file: using sample rate %lu\n\tsize %s, encoding %s, %d %s",
	       outformat.info.rate, st_sizes_str[outformat.info.size], 
	       st_encodings_str[outformat.info.encoding], outformat.info.channels, 
	       (outformat.info.channels > 1) ? "channels" : "channel");
	if (outformat.comment)
	    st_report("Output file: comment \"%s\"\n", outformat.comment);
    }

    /* Very Important: 
     * Effect fabrication and start is called AFTER files open.
     * Effect may write out data beforehand, and
     * some formats don't know their sample rate until now.
     */
	
    /* inform effect about signal information */
    eff.ininfo = informat.info;
    eff.outinfo = outformat.info;
    for(i = 0; i < 8; i++) {
	memcpy(&eff.loops[i], &informat.loops[i], sizeof(struct st_loopinfo));
    }
    eff.instr = informat.instr;

    /* build efftab */
    checkeffect(&eff);

    /* Start all effects */
    for(e = 1; e < neffects; e++) {
	(* efftab[e].h->start)(&efftab[e]);
	if (efftabR[e].name) 
	    (* efftabR[e].h->start)(&efftabR[e]);
    }

    /* Reserve an output buffer for all effects */
    for(e = 0; e < neffects; e++) {
	efftab[e].obuf = (LONG *) malloc(BUFSIZ * sizeof(LONG));
	if (efftabR[e].name) 
	    efftabR[e].obuf = (LONG *) malloc(BUFSIZ * sizeof(LONG));
    }


    /* Run input data through effects and get more until olen == 0 */
    informat.st_errno = 0;
    outformat.st_errno = 0;
    while (informat.info.x < informat.info.x1) {
	ULONG ct, r;

	/* Read (extracted) chunk of input data. */
	ct = (informat.info.x < informat.info.x0)?
	  (informat.info.x0 - informat.info.x) : (informat.info.x1 - informat.info.x);
	if (ct > BUFSIZ) ct = BUFSIZ;
	r = (*informat.h->read)(&informat, efftab[0].obuf, ct);
	if (!r) break;
	informat.info.x += r;
	if (informat.info.x <= informat.info.x0) continue;
	efftab[0].olen = r;
	efftab[0].odone = 0;

	/* Change the volume of this initial input data if needed. */
	if (informat.info.dovol)
	    clipped += volumechange(efftab[0].obuf, efftab[0].olen, informat.info.vol);

	/* mark chain as empty */
	for(e = 1; e < neffects; e++)
	    efftab[e].odone = efftab[e].olen = 0;

	do {
	    ULONG w;

	    /* run entire chain BACKWARDS: pull, don't push.*/
	    /* this is because buffering system isn't a nice queueing system */
	    for(e = neffects - 1; e > 0; e--) 
		if (flow_effect(e))
		    break;

	    /* If outputing and output data was generated then write it */
	    if (writing&&(efftab[neffects-1].olen>efftab[neffects-1].odone)) {
		w = (* outformat.h->write)(&outformat, efftab[neffects-1].obuf, 
				           (LONG) efftab[neffects-1].olen);
	        efftab[neffects-1].odone = efftab[neffects-1].olen;
	    }

	    if (outformat.st_errno)
		st_fail(outformat.st_errstr);

	    /* if stuff still in pipeline, set up to flow effects again */
	    havedata = 0;
	    for(e = 0; e < neffects - 1; e++)
		if (efftab[e].odone < efftab[e].olen) {
		    havedata = 1;
		    break;
		}
	} while (havedata);

    }

    if (informat.st_errno)
	st_fail(informat.st_errstr);

    /* Drain the effects out first to last, 
     * pushing residue through subsequent effects */
    /* oh, what a tangled web we weave */
    for(f = 1; f < neffects; f++)
    {
	while (1) {

	    if (drain_effect(f) == 0)
		break;		/* out of while (1) */
	
	    if (writing&&efftab[neffects-1].olen > 0)
		(* outformat.h->write)(&outformat, efftab[neffects-1].obuf,
				       (LONG) efftab[neffects-1].olen);

	    if (efftab[f].olen != BUFSIZ)
		break;
	}
    }
	

    /* Very Important: 
     * Effect stop is called BEFORE files close.
     * Effect may write out more data after. 
     */
    
    for (e = 1; e < neffects; e++) {
	(* efftab[e].h->stop)(&efftab[e]);
	if (efftabR[e].name)
	    (* efftabR[e].h->stop)(&efftabR[e]);
    }

    if ((* informat.h->stopread)(&informat) == ST_EOF)
	st_fail(informat.st_errstr);
    fclose(informat.fp);

    if (writing)
    {
        if ((* outformat.h->stopwrite)(&outformat) == ST_EOF)
	    st_fail(outformat.st_errstr);
    }
    if (writing)
        fclose(outformat.fp);
}

static int flow_effect(e)
int e;
{
    LONG i, done, idone, odone, idonel, odonel, idoner, odoner;
    LONG *ibuf, *obuf;

    /* I have no input data ? */
    if (efftab[e-1].odone == efftab[e-1].olen)
	return 0;

    if (! efftabR[e].name) {
	/* No stereo data, or effect can handle stereo data so
	 * run effect over entire buffer.
	 */
	idone = efftab[e-1].olen - efftab[e-1].odone;
	odone = BUFSIZ;
	(* efftab[e].h->flow)(&efftab[e], 
			      &efftab[e-1].obuf[efftab[e-1].odone], 
			      efftab[e].obuf, &idone, &odone);
	efftab[e-1].odone += idone;
	efftab[e].odone = 0;
	efftab[e].olen = odone;
	done = idone + odone;
    } else {
	
	/* Put stereo data in two seperate buffers and run effect
	 * on each of them.
	 */
	idone = efftab[e-1].olen - efftab[e-1].odone;
	odone = BUFSIZ;
	ibuf = &efftab[e-1].obuf[efftab[e-1].odone];
	for(i = 0; i < idone; i += 2) {
	    ibufl[i/2] = *ibuf++;
	    ibufr[i/2] = *ibuf++;
	}
	
	/* left */
	idonel = (idone + 1)/2;		/* odd-length logic */
	odonel = odone/2;
	(* efftab[e].h->flow)(&efftab[e], ibufl, obufl, &idonel, &odonel);
	
	/* right */
	idoner = idone/2;		/* odd-length logic */
	odoner = odone/2;
	(* efftabR[e].h->flow)(&efftabR[e], ibufr, obufr, &idoner, &odoner);

	obuf = efftab[e].obuf;
	 /* This loop implies left and right effect will always output
	  * the same amount of data.
	  */
	for(i = 0; i < odoner; i++) {
	    *obuf++ = obufl[i];
	    *obuf++ = obufr[i];
	}
	efftab[e-1].odone += idonel + idoner;
	efftab[e].odone = 0;
	efftab[e].olen = odonel + odoner;
	done = idonel + idoner + odonel + odoner;
    } 
    if (done == 0) 
	st_fail("Effect took & gave no samples!");
    return 1;
}

static int drain_effect(e)
int e;
{
    LONG i, olen, olenl, olenr;
    LONG *obuf;

    if (! efftabR[e].name) {
	efftab[e].olen = BUFSIZ;
	(* efftab[e].h->drain)(&efftab[e],efftab[e].obuf,
			       &efftab[e].olen);
    }
    else {
	olen = BUFSIZ;
		
	/* left */
	olenl = olen/2;
	(* efftab[e].h->drain)(&efftab[e], obufl, &olenl);
	
	/* right */
	olenr = olen/2;
	(* efftab[e].h->drain)(&efftabR[e], obufr, &olenr);
	
	obuf = efftab[e].obuf;
	/* This loop implies left and right effect will always output
	 * the same amount of data.
	 */
	for(i = 0; i < olenr; i++) {
	    *obuf++ = obufl[i];
	    *obuf++ = obufr[i];
	}
	efftab[e].olen = olenl + olenr;
    }
    return(efftab[e].olen);
}

#define setin(eff, effname) \
	{eff.name = effname; \
	eff.ininfo.rate = informat.info.rate; \
	eff.ininfo.channels = informat.info.channels; \
	eff.outinfo.rate = informat.info.rate; \
	eff.outinfo.channels = informat.info.channels;}

#define setout(eff, effname) \
	{eff.name = effname; \
	eff.ininfo.rate = outformat.info.rate; \
	eff.ininfo.channels = outformat.info.channels; \
	eff.outinfo.rate = outformat.info.rate; \
	eff.outinfo.channels = outformat.info.channels;}

/*
 * If no effect given, decide what it should be.
 * Smart ruleset for multiple effects in sequence.
 * 	Puts user-specified effect in right place.
 */
static void
checkeffect(effp)
eff_t effp;
{
	int i, j;
	int needchan = 0, needrate = 0;

	/* if given effect does these, we don't need to add them */
	needrate = (informat.info.rate != outformat.info.rate) &&
		! (effp->h->flags & ST_EFF_RATE);
	needchan = (informat.info.channels != outformat.info.channels) &&
		! (effp->h->flags & ST_EFF_MCHAN);

	neffects = 1;
	/* effect #0 is the input stream */
	/* inform all effects about all relevant changes */
	for(i = 0; i < MAXEFF; i++) {
		efftab[i].name = efftabR[i].name = (char *) 0;
		/* inform effect about signal information */
		efftab[i].ininfo = informat.info;
		efftabR[i].ininfo = informat.info;
		efftab[i].outinfo = outformat.info;
		efftabR[i].outinfo = outformat.info;
		for(j = 0; j < 8; j++) {
			memcpy(&efftab[i].loops[j], 
				&informat.loops[j], sizeof(struct st_loopinfo));
			memcpy(&efftabR[i].loops[j], 
				&informat.loops[j], sizeof(struct st_loopinfo));
		}
		efftab[i].instr = informat.instr;
		efftabR[i].instr = informat.instr;
	}

	/* If not writing output, then just add the user specified effect.
	 * This is to avoid channel and rate averaging since you don't have
	 * a real output format.
	 */
	if (! writing) {
		neffects = 2;
		efftab[1].name = effp->name;
		if ((informat.info.channels == 2) &&
		   (! (effp->h->flags & ST_EFF_MCHAN)))
			efftabR[1].name = effp->name;
	}
	else if (soxpreview) {
	    /* to go faster, i suppose rate could come first if downsampling */
	    if (needchan && (informat.info.channels > outformat.info.channels))
		{
	        if (needrate) {
		    neffects = 4;
		    efftab[1].name = "avg";
		    efftab[2].name = "rate";
		    setout(efftab[3], effp->name);
		} else {
		    neffects = 3;
		    efftab[1].name = "avg";
		    setout(efftab[2], effp->name);
		}
	    } else if (needchan && 
		    (informat.info.channels < outformat.info.channels)) {
	        if (needrate) {
		    neffects = 4;
		    efftab[1].name = effp->name;
		    efftab[1].outinfo.rate = informat.info.rate;
		    efftab[1].outinfo.channels = informat.info.channels;
		    efftab[2].name = "rate";
		    efftab[3].name = "avg";
		} else {
		    neffects = 3;
		    efftab[1].name = effp->name;
		    efftab[1].outinfo.channels = informat.info.channels;
		    efftab[2].name = "avg";
		}
	    } else {
	        if (needrate) {
		    neffects = 3;
		    efftab[1].name = effp->name;
		    efftab[1].outinfo.rate = informat.info.rate;
		    efftab[2].name = "rate";
		    if (informat.info.channels == 2)
			    efftabR[2].name = "rate";
		} else {
		    neffects = 2;
		    efftab[1].name = effp->name;
		}
		if ((informat.info.channels == 2) &&
		    (! (effp->h->flags & ST_EFF_MCHAN)))
		        efftabR[1].name = effp->name;
	    }
	} else {	/* not preview mode */
	    /* [ sum to mono,] [ then rate,] then effect */
	    /* not the purest, but much faster */
	    if (needchan && 
			(informat.info.channels > outformat.info.channels)) {
	        if (needrate && (informat.info.rate != outformat.info.rate)) {
		    neffects = 4;
		    efftab[1].name = "avg";
		    efftab[2].name = effp->name;
		    efftab[2].outinfo.rate = informat.info.rate;
		    efftab[2].outinfo.channels = informat.info.channels;
		    efftab[3].name = "rate";
		} else {
		    neffects = 3;
		    efftab[1].name = "avg";
		    efftab[2].name = effp->name;
		    efftab[2].outinfo.rate = informat.info.rate;
		    efftab[2].outinfo.channels = informat.info.channels;
		}
	    } else if (needchan && 
			(informat.info.channels < outformat.info.channels)) {
	        if (needrate) {
		    neffects = 4;
		    efftab[1].name = effp->name;
		    if (! (effp->h->flags & ST_EFF_MCHAN))
			    efftabR[1].name = effp->name;
		    efftab[1].outinfo.rate = informat.info.rate;
		    efftab[1].outinfo.channels = informat.info.channels;
		    efftab[2].name = "resample";
		    efftab[3].name = "avg";
		} else {
		    neffects = 3;
		    efftab[1].name = effp->name;
		    if (! (effp->h->flags & ST_EFF_MCHAN))
			    efftabR[1].name = effp->name;
		    efftab[1].outinfo.channels = informat.info.channels;
		    efftab[2].name = "avg";
		}
	    } else {
	        if (needrate) {
		    neffects = 3;
		    efftab[1].name = effp->name;
		    efftab[1].outinfo.rate = informat.info.rate;
		    efftab[2].name = "resample";
		    if (informat.info.channels == 2)
			    efftabR[2].name = "rate";
		} else {
		    neffects = 2;
		    efftab[1].name = effp->name;
		}
		if ((informat.info.channels == 2) &&
		    (! (effp->h->flags & ST_EFF_MCHAN)))
		        efftabR[1].name = effp->name;
	    }
	}

	for(i = 1; i < neffects; i++) {
		/* pointer comparison OK here */
		/* shallow copy of initialized effect data */
		/* XXX this assumes that effect_getopt() doesn't malloc() */
		if (efftab[i].name == effp->name) {
			memcpy(&efftab[i], &eff, sizeof(struct st_effect));
			if (efftabR[i].name) 
			    memcpy(&efftabR[i], &eff, sizeof(struct st_effect));
		} else {
			/* set up & give default opts for added effects */
			st_geteffect(&efftab[i]);
			(* efftab[i].h->getopts)(&efftab[i],(int)0,(char **)0);
			if (efftabR[i].name) 
			    memcpy(&efftabR[i], &efftab[i], 
				sizeof(struct st_effect));
		}
	}
	
    /* If a user doesn't specify an effect then a null entry could
     * have been placed in the middle of the list above.  Remove
     * those entries here.
     */
	for(i = 1; i < neffects; i++)
	    if (! strcmp(efftab[i].name, "null")) {
		for(; i < neffects; i++) {
		    efftab[i] = efftab[i+1];
		    efftabR[i] = efftabR[i+1];
		}
		neffects--;
	    }
}

/* Guido Van Rossum fix */
static void statistics(P0) {
	if (informat.info.dovol && clipped > 0)
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
"[ gopts ] [ fopts ] ifile [ fopts ] ofile [ effect [ effopts ] ]";

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
	    fprintf(stderr,"fopts: -r rate -c channels -s/-u/-U/-A/-a/-i/-g -b/-w/-l/-f/-d/-D -x\n\n");
	    fprintf(stderr, "effect: ");
	    for (i = 1; st_effects[i].name != NULL; i++) {
		fprintf(stderr, "%s ", st_effects[i].name);
	    }
	    fprintf(stderr, "\n\neffopts: depends on effect\n\n");
	    fprintf(stderr, "Supported file formats: ");
	    for (i = 0; st_formats[i].names != NULL; i++) {
		/* only print the first name */
		fprintf(stderr, "%s ", st_formats[i].names[0]);
	    }
	    fputc('\n', stderr);
	}
	exit(1);
}


/* called from util.c:fail */
void cleanup(P0) {
	/* Close the input file and outputfile before exiting*/
	if (informat.fp)
		fclose(informat.fp);
	if (outformat.fp) {
		fclose(outformat.fp);
		/* remove the output file because we failed, if it's ours. */
		/* Don't if its not a regular file. */
		if (filetype(fileno(outformat.fp)) == S_IFREG)
		    REMOVE(outformat.filename);
	}
}
