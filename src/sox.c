/*)
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

static int dovolume = 0;        /* User wants volume change */
static double volume = 1.0;     /* Linear volume change */
static int clipped = 0;		/* Volume change clipping errors */
static int writing = 0;	        /* are we writing to a file? */
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
static void checkeffect(P0);
static int flow_effect(P1(int));
static int drain_effect(P1(int));

static struct st_soundstream informat, outformat;

static ft_t ft;

/* We parse effects into a temporary effects table and then place into
 * the real effects table.  This makes it easier to reorder some effects
 * as needed.  For instance, we can run a resampling effect before
 * converting a mono file to stereo.  This allows the resample to work
 * on half the data.
 *
 * Real effects table only needs to be 2 entries bigger then the user
 * specified table.  This is because at most we will need to add
 * a resample effect and an channel averaging effect.
 */
#define MAX_EFF 16 
#define MAX_USER_EFF 14

/* 
 * In efftab's, location 0 is always the input stream.
 *
 * If one was to support effects for quad-channel files, there would 
 * need to be an effect tabel for each channel.
 */

static struct st_effect efftab[MAX_EFF]; /* left/mono channel effects */
static struct st_effect efftabR[MAX_EFF];/* right channel effects */
static int neffects;			 /* # of effects to run on data */

static struct st_effect user_efftab[MAX_USER_EFF];
static int nuser_effects;

static char *ifile, *ofile;

int main(argc, argv)
int argc;
char **argv;
{

        int argc_effect;

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


	/* Loop through the reset of the arguments looking for effects */
	nuser_effects = 0;

        while (optind < argc)
        {
	    if (nuser_effects >= MAX_USER_EFF)
	    {
	        st_fail("Sorry, too many effects specified.\n");
	    }

	    argc_effect = st_geteffect_opt(&user_efftab[nuser_effects], 
		                           argc - optind, &argv[optind]);

	    if (argc_effect == ST_EOF)
	    {
	        int i1;
	        fprintf(stderr, "%s: Known effects: ",myname);
	        for (i1 = 1; st_effects[i1].name; i1++)
	            fprintf(stderr, "%s ", st_effects[i1].name);
	        fprintf(stderr, "\n");
	        st_fail("Effect '%s' is not known!", argv[optind]);
	    }


	    /* Skip past effect name */
	    optind++;

	    (*user_efftab[nuser_effects].h->getopts)(&user_efftab[nuser_effects], 
			                             argc_effect, 
						     &argv[optind]);

	    /* Skip past the effect arguments */
	    optind += argc_effect;
	    nuser_effects++;
	}

	/* Check global arguments */
	if (dovolume && volume == 0.0)
		st_fail("Volume must be non-zero");

	/* negative volume is phase-reversal */
	if (volume < 0.0)
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
static char *getoptstr = "+r:v:t:c:phsuUAaigbwlfdDxV";
#else
static char *getoptstr = "r:v:t:c:phsuUAaigbwlfdDxV";
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
			if (!ft || dovolume) usage("-v");
			str = optarg;
			if (! sscanf(str, "%lf", &volume))
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

static void init(P0) {

	/* init files */
	informat.info.rate      = outformat.info.rate  = 0;
	informat.info.size      = outformat.info.size  = -1;
	informat.info.encoding  = outformat.info.encoding = -1;
	informat.info.channels  = outformat.info.channels = -1;
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
    
    if (dovolume)
	st_report("Volume factor: %f\n", volume);
    
    st_report("Input file: using sample rate %lu\n\tsize %s, encoding %s, %d %s",
	   informat.info.rate, st_sizes_str[informat.info.size], 
	   st_encodings_str[informat.info.encoding], informat.info.channels, 
	   (informat.info.channels > 1) ? "channels" : "channel");
    if (informat.comment)
	st_report("Input file: comment \"%s\"\n", informat.comment);
	
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

    /* build efftab */
    checkeffect();

    /* Start all effects */
    for(e = 1; e < neffects; e++) {
	(* efftab[e].h->start)(&efftab[e]);
	if (efftabR[e].name) 
	    (* efftabR[e].h->start)(&efftabR[e]);
    }

    /* Reserve an output buffer for all effects */
    for(e = 0; e < neffects; e++) 
    {
	efftab[e].obuf = (LONG *) malloc(BUFSIZ * sizeof(LONG));
	if (efftab[e].obuf == NULL)
	{
	    st_fail("could not allocate memory");
	}
	if (efftabR[e].name) 
	{
	    efftabR[e].obuf = (LONG *) malloc(BUFSIZ * sizeof(LONG));
	    if (efftabR[e].obuf == NULL)
	    {
		st_fail("could not allocate memory");
	    }
	}
    }


    /* 
     * Just like errno, we must set st_errno to known values before
     * calling I/O operations.
     */
    informat.st_errno = 0;
    outformat.st_errno = 0;

    /* Prime while() loop by reading initial chunk of input data. */
    efftab[0].olen = (*informat.h->read)(&informat, 
                                         efftab[0].obuf, (LONG) BUFSIZ);
    efftab[0].odone = 0;

    /* Change the volume of this initial input data if needed. */
    if (dovolume)
	clipped += volumechange(efftab[0].obuf, efftab[0].olen,
		                volume);

    /* Run input data through effects and get more until olen == 0 */
    while (efftab[0].olen > 0)
    {
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
	    if (writing&&(efftab[neffects-1].olen>efftab[neffects-1].odone)) 
	    {
		w = (* outformat.h->write)(&outformat, 
			                   efftab[neffects-1].obuf, 
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

        /* Read another chunk of input data. */
        efftab[0].olen = (*informat.h->read)(&informat, 
                                             efftab[0].obuf, (LONG) BUFSIZ);
        efftab[0].odone = 0;

        /* Change volume of these samples if needed. */
        if (dovolume)
	    clipped += volumechange(efftab[0].obuf, efftab[0].olen,
		                    volume);
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

/*
 * If no effect given, decide what it should be.
 * Smart ruleset for multiple effects in sequence.
 * 	Puts user-specified effect in right place.
 */
static void
checkeffect()
{
	int i;
	int needchan = 0, needrate = 0, haschan = 0, hasrate = 0;
	int effects_mask = 0;

	needrate = (informat.info.rate != outformat.info.rate);
	needchan = (informat.info.channels != outformat.info.channels);

	for (i = 0; i < nuser_effects; i++)
	{
	    if (user_efftab[i].h->flags & ST_EFF_CHAN)
	    {
		haschan++;
	    }
	    if (user_efftab[i].h->flags & ST_EFF_RATE)
	    {
		hasrate++;
	    }
	}

	if (haschan > 1)
	    st_fail("Can not specify multiple effects that modify channel #");
	if (hasrate > 1)
	    st_fail("Can not specify multiple effects that change sampel rate");
	if (haschan && !needchan)
	    st_fail("Can not specify channel effects when input and output channel # are equal");
	if (hasrate && !needrate)
	    st_fail("Can not specify sample rate effects when input and output rate are equal");

	/* If not writing output then do not worry about adding
	 * channel and rate effects.  This is just to speed things
	 * up.
	 */
	if (!writing)
	{
	    needchan = 0;
	    needrate = 0;
	}

	/* --------- add the effects ------------------------ */

	/* efftab[0] is always the input stream and always exists */
	neffects = 1;

	/* If reducing channels then its faster to run all effects
	 * after the avg effect.
	 */
        if (needchan && !(haschan) &&
	    (informat.info.channels > outformat.info.channels))
        {
	    /* Find effect and update initial pointers */
	    st_geteffect(&efftab[neffects], "avg");

	    /* give default opts for added effects */
	    (* efftab[neffects].h->getopts)(&efftab[neffects],(int)0,
					    (char **)0);

	    /* Copy format info to effect table */
	    effects_mask = st_updateeffect(&efftab[neffects], &informat, 
		                           &outformat, effects_mask);

	    neffects++;
	}

	/* If reducing the number of samples, its faster to run all effects
	 * after the resample effect.
	 */
	if (needrate && !(hasrate) &&
	    (informat.info.rate > outformat.info.rate))
	{
	    if (soxpreview)
	        st_geteffect(&efftab[neffects], "rate");
	    else
	        st_geteffect(&efftab[neffects], "resample");

	    /* set up & give default opts for added effects */
	    (* efftab[neffects].h->getopts)(&efftab[neffects],(int)0,
					    (char **)0);

	    /* Copy format info to effect table */
	    effects_mask = st_updateeffect(&efftab[neffects], &informat, 
		                           &outformat, effects_mask);

	    /* Rate can't handle multiple channels so be sure and
	     * account for that.
	     */
	    if (efftab[neffects].ininfo.channels > 1)
	    {
	        memcpy(&efftabR[neffects], &efftab[neffects], 
		       sizeof(struct st_effect));
	    }

	    neffects++;
        }

	/* Copy over user specified effects into real efftab */
	for(i = 0; i < nuser_effects; i++) 
	{
	    memcpy(&efftab[neffects], &user_efftab[i], 
		   sizeof(struct st_effect));

	    /* Copy format info to effect table */
	    effects_mask = st_updateeffect(&efftab[neffects], &informat, 
		                           &outformat, effects_mask);

	    /* If this effect can't handle multiple channels then
	     * account for this.
	     */
	    if ((efftab[neffects].ininfo.channels > 1) &&
		!(efftab[neffects].h->flags & ST_EFF_MCHAN))
	    {
	        memcpy(&efftabR[neffects], &efftab[neffects], 
		       sizeof(struct st_effect));
	    }

	    neffects++;
	}

	/* If rate effect hasn't been added by now then add it here.
	 * Check adding rate before avg because its faster to run
	 * rate on less channels then more.
	 */
	if (needrate && !(effects_mask & ST_EFF_RATE))
	{
	    if (soxpreview)
	        st_geteffect(&efftab[neffects], "rate");
	    else
	        st_geteffect(&efftab[neffects], "resample");

	    /* set up & give default opts for added effects */
	    (* efftab[neffects].h->getopts)(&efftab[neffects],(int)0,
					    (char **)0);

	    /* Copy format info to effect table */
	    effects_mask = st_updateeffect(&efftab[neffects], &informat, 
		                           &outformat, effects_mask);

	    /* Rate can't handle multiple channels so be sure and
	     * account for that.
	     */
	    if (efftab[neffects].ininfo.channels > 1)
	    {
	        memcpy(&efftabR[neffects], &efftab[neffects], 
		       sizeof(struct st_effect));
	    }

	    neffects++;
        }

	/* If code up until know still hasn't added avg effect then
	 * do it now.
	 */
	if (needchan && !(effects_mask & ST_EFF_CHAN))
        {
	    st_geteffect(&efftab[neffects], "avg");

	    /* set up & give default opts for added effects */
	    (* efftab[neffects].h->getopts)(&efftab[neffects],(int)0,
					    (char **)0);

	    /* Copy format info to effect table */
	    effects_mask = st_updateeffect(&efftab[neffects], &informat, 
		                           &outformat, effects_mask);

	    neffects++;
	}
}

static void statistics(P0) {
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
