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

#include "st_i.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>             /* for malloc() */
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/types.h>          /* for fstat() */
#include <sys/stat.h>           /* for fstat() */
#ifdef HAVE_UNISTD_H
#include <unistd.h>             /* for unlink() */
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
static int clipped = 0;         /* Volume change clipping errors */
static int writing = 0;         /* are we writing to a file? */
static int soxpreview = 0;      /* preview mode */

static st_sample_t ibufl[ST_BUFSIZ/2];    /* Left/right interleave buffers */
static st_sample_t ibufr[ST_BUFSIZ/2];
static st_sample_t obufl[ST_BUFSIZ/2];
static st_sample_t obufr[ST_BUFSIZ/2];

/* local forward declarations */
static void doopts(ft_t, int, char **);
static void copy_input(ft_t);
static void open_input(ft_t);
static void copy_output(ft_t);
static void open_output(ft_t);
static void usage(char *) NORET;
static int filetype(int);
static void process(void);
static void statistics(void);
static st_sample_t volumechange(st_sample_t *buf, st_ssize_t ct, double vol);
static void checkeffect(void);
static int flow_effect_out(void);
static int flow_effect(int);
static int drain_effect(int);

#ifdef SOXMIX
#define MAX_INPUT_FILES 2
#define REQUIRED_INPUT_FILES 2
#else
#define MAX_INPUT_FILES 1
#define REQUIRED_INPUT_FILES 1
#endif

static ft_t informat[MAX_INPUT_FILES] = { 0 };
static int input_count = 0;

static ft_t outformat = 0;
static int output_count = 0;

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
static int neffects;                     /* # of effects to run on data */

static struct st_effect user_efftab[MAX_USER_EFF];
static int nuser_effects;

int main(argc, argv)
int argc;
char **argv;
{
        int argc_effect;
        ft_t ft;
        int parsing_output = 0;

        myname = argv[0];

        /* Loop over arguments and filenames, stop when an effect name is found */
        while (optind < argc && output_count < 1 &&
               st_checkeffect(argv[optind]) != ST_SUCCESS)
        {

            /*
             * Its possible to not specify the output filename by using
             * "-e" option. This allows effects to be ran on data but
             * no output file to be written.
             */
            if (strcmp(argv[optind], "-e"))
            {
                /* No -e option appears to be set.  Attempt to see if
                 * this is the last filename or not.  If we see no more
                 * options or an effect name then we assume this is
                 * the last filename.
                 */
                if (input_count >= REQUIRED_INPUT_FILES &&
                    (optind == (argc-1) ||
                     st_checkeffect(argv[optind+1]) == ST_SUCCESS ||
                     input_count >= MAX_INPUT_FILES))
                {
                    parsing_output = 1;
                }

                if (parsing_output)
                    writing = 1;
            }
            else
            {
                /* -e option found.  Make sure this was done on the
                 * output side.  Should be no more parameters or
                 * a valid effects name.
                 */
                if (input_count >= REQUIRED_INPUT_FILES &&
                    (optind == (argc-1) ||
                     st_checkeffect(argv[optind+1]) == ST_SUCCESS ||
                     input_count >= MAX_INPUT_FILES))
                {
                    parsing_output = 1;
                }

                if (parsing_output)
                {
                    writing = 0;
                    optind++;  /* Move passed -e */
                }
                else
                {
                    usage("Can only specify \"-e\" for output filenames");
                }
            }

            ft = (ft_t)malloc(sizeof(struct st_soundstream));
            st_initformat(ft);

            doopts(ft, argc, argv);

            /* See if we have all the input filenames we need.
             * If not, then parse this as input filenames.
             *
             * If we have enough but haven't reached the max #
             * of inputs, then only treat this as an input filename
             * if it looks like we have more filenames left.
             */
            if (!parsing_output)
            {
                if (optind < argc)
                {
                    ft->filename = argv[optind];
                    optind++;

                    copy_input(ft);
                    open_input(ft);
                }
            }
            else
            {
                if (optind < argc && writing)
                {
                    ft->filename = argv[optind];
                    optind++;
                }
                else
                {
                    ft->filename = 0;
                }

                copy_output(ft);
                /* Hold off on opening file until the very last minute.
                 * This allows us to verify header in input files are
                 * what they should be and parse effect command lines.
                 * That way if anything is found invalid, we will abort
                 * without truncating any existing file that has the same
                 * output filename.
                 */
            }
        }

        /* Make sure we got at least the required # of input filename */
        if (input_count < REQUIRED_INPUT_FILES ||
            !informat[REQUIRED_INPUT_FILES-1] ||
            !informat[REQUIRED_INPUT_FILES-1]->filename)
            usage("Not enough input files specified");

        if (!outformat || (!outformat->filename && writing))
            usage("No output file?");

        /* Loop through the reset of the arguments looking for effects */
        nuser_effects = 0;

        while (optind < argc)
        {
            if (nuser_effects >= MAX_USER_EFF)
            {
                st_fail("To many effects specified.\n");
            }

            argc_effect = st_geteffect_opt(&user_efftab[nuser_effects],
                                           argc - optind, &argv[optind]);

            if (argc_effect == ST_EOF)
            {
                int i1;
                fprintf(stderr, "%s: Known effects: ",myname);
                for (i1 = 0; st_effects[i1].name; i1++)
                    fprintf(stderr, "%s ", st_effects[i1].name);
                fprintf(stderr, "\n\n");
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

        if (dovolume)
            st_report("Volume factor: %f\n", volume);

        if (dovolume && volume < 0.0)
            st_report("Volume adjustment is negative.  This will result in a phase change\n");

        process();
        statistics();
        return(0);
}

static void copy_input(ft_t ft)
{
    informat[input_count] = ft;

    /* Let auto effect do the work if user is not overriding. */
    if (!ft->filetype)
        ft->filetype = "auto";

    if ( st_gettype(ft) )
        st_fail("Unknown input file format for '%s':  %s", 
		ft->filename, ft->st_errstr);

    /* Default the input comment to the filename if not set from
     * command line.
     * FIXME: Should be a memory copy, not a pointer asignment.
     */
    if (!ft->comment)
        ft->comment = ft->filename;

    input_count++;
}

static void open_input(ft_t ft)
{
    /* Open file handler based on input name.  Used stdin file handler
     * if the filename is "="
     */
    if (! strcmp(ft->filename, "-"))
        ft->fp = stdin;
    else if ((ft->fp = fopen(ft->filename, "rb")) == NULL)
        st_fail("Can't open input file '%s': %s", ft->filename,
                strerror(errno));

    /* See if this file is seekable or not */
#if     defined(DUMB_FILESYSTEM)
    ft->seekable = 0;
#else
    ft->seekable = (filetype(fileno(ft->fp)) == S_IFREG);
#endif
}

#if defined(DOS) || defined(WIN32)
#define LASTCHAR '\\'
#else
#define LASTCHAR '/'
#endif

static void copy_output(ft_t ft)
{
    outformat = ft;

    if (writing && !ft->filetype && ft->filename) {
        /* Use filename extension to determine audio type. */

        /* First, chop off any path portions of filename.  This
         * prevents the next search from considering that part. */
        if ((ft->filetype = strrchr(ft->filename, LASTCHAR)) == NULL)
            ft->filetype = ft->filename;

        /* Now look for an filename extension */
        if ((ft->filetype = strrchr(ft->filetype, '.')) != NULL)
            ft->filetype++;
        else
            ft->filetype = NULL;
    }

    if (writing && ft->filename)
    {
        if ( st_gettype(ft) )
            st_fail("Unknown output file format for '%s': %s",
		    ft->filename, ft->st_errstr);

    }

    output_count++;
}

static void open_output(ft_t ft)
{
    if (writing) {
        /*
         * There are two choices here:
         *      1) stomp the old file - normal shell "> file" behavior
         *      2) fail if the old file already exists - csh mode
         */
        if (! strcmp(ft->filename, "-"))
        {
            ft->fp = stdout;

            /* stdout tends to be line-buffered.  Override this */
            /* to be Full Buffering. */
            if (setvbuf (ft->fp,NULL,_IOFBF,sizeof(char)*ST_BUFSIZ))
            {
                st_fail("Can't set write buffer");
            }
        }
        else {

            ft->fp = fopen(ft->filename, "wb");

            if (ft->fp == NULL)
                st_fail("Can't open output file '%s': %s",
                        ft->filename, strerror(errno));

            /* stdout tends to be line-buffered.  Override this */
            /* to be Full Buffering. */
            if (setvbuf (ft->fp,NULL,_IOFBF,sizeof(char)*ST_BUFSIZ))
            {
                st_fail("Can't set write buffer");
            }

        } /* end of else != stdout */
#if     defined(DUMB_FILESYSTEM)
        ft->seekable = 0;
#else
        ft->seekable  = (filetype(fileno(ft->fp)) == S_IFREG);
#endif
    }
}

#ifdef HAVE_GETOPT_H
static char *getoptstr = "+r:v:t:c:phsuUAaigbwlfdxV";
#else
static char *getoptstr = "r:v:t:c:phsuUAaigbwlfdxV";
#endif

static void doopts(ft_t ft, int argc, char **argv)
{
        int c, i;
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
                        if ((! sscanf(str, "%u", &ft->info.rate)) ||
                                        (ft->info.rate <= 0))
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
                        if (! sscanf(str, "%d", &i))
                                st_fail("-c must be given a number");
			ft->info.channels = i;
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
                case 'f':
                        if (! ft) usage("-f");
                        ft->info.encoding = ST_ENCODING_FLOAT;
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

#ifdef SOXMIX
static int compare_input(ft_t ft1, ft_t ft2)
{
    if (ft1->info.rate != ft2->info.rate)
        return ST_EOF;
    if (ft1->info.size != ft2->info.size)
        return ST_EOF;
    if (ft1->info.encoding != ft2->info.encoding)
        return ST_EOF;
    if (ft1->info.channels != ft2->info.channels)
        return ST_EOF;

    return ST_SUCCESS;
}
#endif

/*
 * Process input file -> effect table -> output file
 *      one buffer at a time
 */

static void process(void) {
    int e, f, flowstatus;
#ifdef SOXMIX
    int s;
    st_ssize_t ilen[MAX_INPUT_FILES];
    st_sample_t *ibuf[MAX_INPUT_FILES];
#endif

    for (f = 0; f < input_count; f++)
    {
        /* Read and write starters can change their formats. */
        if ((* informat[f]->h->startread)(informat[f]) != ST_SUCCESS)
        {
            st_fail("Failed reading %s: %s",informat[f]->filename,
                    informat[f]->st_errstr);
        }

        /* Go a head and assume 1 channel audio if nothing is detected.
         * This is because libst usually doesn't set this for mono file
         * formats (for historical reasons).
         */
        if (informat[f]->info.channels == -1)
            informat[f]->info.channels = 1;

        if ( st_checkformat(informat[f]) )
            st_fail("bad input format for file %s: %s",informat[f]->filename,informat[f]->st_errstr);

        st_report("Input file %s: using sample rate %lu\n\tsize %s, encoding %s, %d %s",
                  informat[f]->filename, informat[f]->info.rate,
                  st_sizes_str[(unsigned char)informat[f]->info.size],
                  st_encodings_str[(unsigned char)informat[f]->info.encoding],
                  informat[f]->info.channels,
                  (informat[f]->info.channels > 1) ? "channels" : "channel");

        if (informat[f]->comment)
            st_report("Input file %s: comment \"%s\"\n",
                      informat[f]->filename, informat[f]->comment);
    }

#ifdef SOXMIX
    for (f = 1; f < input_count; f++)
    {
        if (compare_input(informat[0], informat[f]) != ST_SUCCESS)
        {
            st_fail("Input files must have the same rate, channels, data size, and encoding");
        }
    }
#endif

    if (writing)
    {
        open_output(outformat);

        /* Always use first input file as a reference for output
         * file format.
         */
        st_copyformat(informat[0], outformat);

        if ((*outformat->h->startwrite)(outformat) == ST_EOF)
        {
            st_fail(outformat->st_errstr);
        }

        if (st_checkformat(outformat))
                st_fail("bad output format: %s",outformat->st_errstr);

        st_report("Output file %s: using sample rate %lu\n\tsize %s, encoding %s, %d %s",
               outformat->filename, outformat->info.rate,
               st_sizes_str[(unsigned char)outformat->info.size],
               st_encodings_str[(unsigned char)outformat->info.encoding],
               outformat->info.channels,
               (outformat->info.channels > 1) ? "channels" : "channel");

        if (outformat->comment)
            st_report("Output file: comment \"%s\"\n", outformat->comment);
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
        efftab[e].obuf = (st_sample_t *) malloc(ST_BUFSIZ * 
		                                sizeof(st_sample_t));
        if (efftab[e].obuf == NULL)
        {
            st_fail("could not allocate memory");
        }
        if (efftabR[e].name)
        {
            efftabR[e].obuf = (st_sample_t *) malloc(ST_BUFSIZ * 
		                                     sizeof(st_sample_t));
            if (efftabR[e].obuf == NULL)
            {
                st_fail("could not allocate memory");
            }
        }
    }

#ifdef SOXMIX
    for (f = 0; f < MAX_INPUT_FILES; f++)
    {
        ibuf[f] = (st_sample_t *)malloc(ST_BUFSIZ * sizeof(st_sample_t));
        if (!ibuf[f])
        {
            st_fail("could not allocate memory");
        }
    }
#endif

    /*
     * Just like errno, we must set st_errno to known values before
     * calling I/O operations.
     */
    for (f = 0; f < input_count; f++)
        informat[f]->st_errno = 0;
    outformat->st_errno = 0;

    /* Run input data through effects and get more until olen == 0 */
    do {

#ifndef SOXMIX
        efftab[0].olen = (*informat[0]->h->read)(informat[0],
                                                 efftab[0].obuf, 
						 (st_ssize_t)ST_BUFSIZ);
#else
        for (f = 0; f < input_count; f++)
        {
            ilen[f] = (*informat[f]->h->read)(informat[f],
                                              ibuf[f], 
					      (st_ssize_t)ST_BUFSIZ);
        }

        efftab[0].olen = 0;
        for (f = 0; f < input_count; f++)
            if (ilen[f] > efftab[0].olen)
                efftab[0].olen = ilen[f];

        for (s = 0; s < efftab[0].olen; s++)
        {
            /* Mix data together by dividing by the number
             * of audio files and then summing up.  This prevents
             * overflows.
             */
            for (f = 0; f < input_count; f++)
            {
                if (f == 0)
                    efftab[0].obuf[s] =
                        (s<ilen[f]) ? (ibuf[f][s]/input_count) : 0;
                else
                    if (s < ilen[f])
                        efftab[0].obuf[s] += ibuf[f][s]/input_count;
            }
        }
#endif

        efftab[0].odone = 0;

	/* If not writing and no effects are occuring then not much
	 * reason to continue reading.  This allows this case.  Mainly
	 * useful to print out info about input file header and quite.
	 */
	if (!writing && neffects == 1)
	    efftab[0].olen = 0;

        if (efftab[0].olen == 0)
            break;

        /* Change the volume of this initial input data if needed. */
        if (dovolume)
            clipped += volumechange(efftab[0].obuf, efftab[0].olen,
                                    volume);

        /* mark chain as empty */
        for(e = 1; e < neffects; e++)
            efftab[e].odone = efftab[e].olen = 0;

        flowstatus = flow_effect_out();

        /* Negative flowstatus says no more output will ever be generated. */
        if (flowstatus < 0 || outformat->st_errno)
            break;

    } while (1); /* break; efftab[0].olen == 0 */

    /* Drain the effects out first to last,
     * pushing residue through subsequent effects */
    /* oh, what a tangled web we weave */
    for(f = 1; f < neffects; f++)
    {
        while (1) {

            if (drain_effect(f) == 0)
                break;          /* out of while (1) */

            if (writing&&efftab[neffects-1].olen > 0)
                (* outformat->h->write)(outformat, efftab[neffects-1].obuf,
                                       (st_ssize_t) efftab[neffects-1].olen);

            if (efftab[f].olen != ST_BUFSIZ)
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

    for (f = 0; f < input_count; f++)
    {
        /* If problems closing input file, just warn user since
         * we are exiting anyways.
         */
        if ((* informat[f]->h->stopread)(informat[f]) == ST_EOF)
            st_warn(informat[f]->st_errstr);
        fclose(informat[f]->fp);
    }

    if (writing)
    {
        /* problem closing output file, just warn user since we
         * are exiting anyways.
         */
        if ((* outformat->h->stopwrite)(outformat) == ST_EOF)
            st_warn(outformat->st_errstr);
    }
    if (writing)
        fclose(outformat->fp);
}

static int flow_effect_out(void)
{
    int e, havedata, flowstatus = 0;

    do {
      /* run entire chain BACKWARDS: pull, don't push.*/
      /* this is because buffering system isn't a nice queueing system */
      for(e = neffects - 1; e > 0; e--)
      {
          flowstatus = flow_effect(e);
          if (flowstatus)
              break;
      }

      /* If outputing and output data was generated then write it */
      if (writing&&(efftab[neffects-1].olen>efftab[neffects-1].odone))
      {
          (* outformat->h->write)(outformat,
                                  efftab[neffects-1].obuf,
                                  (st_ssize_t)efftab[neffects-1].olen);
          efftab[neffects-1].odone = efftab[neffects-1].olen;
      }

      if (outformat->st_errno)
      {
          st_warn("Error writing: %s",outformat->st_errstr);
          break;
      }

      /* If any effect will never again produce data, give up.  This
       * works because of the pull status: the effect won't be able to
       * shut us down until all downstream buffers have been emptied.
       */
      if (flowstatus < 0)
          break;

      /* if stuff still in pipeline, set up to flow effects again */
      havedata = 0;
      for(e = 0; e < neffects - 1; e++)
          if (efftab[e].odone < efftab[e].olen) {
              havedata = 1;
              break;
          }
    } while (havedata);

    return flowstatus;
}

static int flow_effect(e)
int e;
{
    st_ssize_t i, done, idone, odone, idonel, odonel, idoner, odoner;
    st_sample_t *ibuf, *obuf;
    int effstatus;

    /* I have no input data ? */
    if (efftab[e-1].odone == efftab[e-1].olen)
        return 0;

    if (! efftabR[e].name) {
        /* No stereo data, or effect can handle stereo data so
         * run effect over entire buffer.
         */
        idone = efftab[e-1].olen - efftab[e-1].odone;
        odone = ST_BUFSIZ;
        effstatus = (* efftab[e].h->flow)(&efftab[e],
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
        odone = ST_BUFSIZ;
        ibuf = &efftab[e-1].obuf[efftab[e-1].odone];
        for(i = 0; i < idone; i += 2) {
            ibufl[i/2] = *ibuf++;
            ibufr[i/2] = *ibuf++;
        }

        /* left */
        idonel = (idone + 1)/2;         /* odd-length logic */
        odonel = odone/2;
        effstatus = (* efftab[e].h->flow)(&efftab[e],
                                          ibufl, obufl, &idonel, &odonel);

        /* right */
        idoner = idone/2;               /* odd-length logic */
        odoner = odone/2;
        effstatus = (* efftabR[e].h->flow)(&efftabR[e],
                                           ibufr, obufr, &idoner, &odoner);

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
    if (effstatus == ST_EOF)
        return -1;
    return 1;
}

static int drain_effect(e)
int e;
{
    st_ssize_t i, olen, olenl, olenr;
    st_sample_t *obuf;

    if (! efftabR[e].name) {
        efftab[e].olen = ST_BUFSIZ;
        (* efftab[e].h->drain)(&efftab[e],efftab[e].obuf,
                               &efftab[e].olen);
    }
    else {
        olen = ST_BUFSIZ;

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
 *      Puts user-specified effect in right place.
 */
static void
checkeffect()
{
        int i;
        int needchan = 0, needrate = 0, haschan = 0, hasrate = 0;
        int effects_mask = 0;

        needrate = (informat[0]->info.rate != outformat->info.rate);
        needchan = (informat[0]->info.channels != outformat->info.channels);

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
            st_report("Can not specify multiple effects that change sample rate");

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
            (informat[0]->info.channels > outformat->info.channels))
        {
            /* Find effect and update initial pointers */
            st_geteffect(&efftab[neffects], "avg");

            /* give default opts for added effects */
            (* efftab[neffects].h->getopts)(&efftab[neffects],(int)0,
                                            (char **)0);

            /* Copy format info to effect table */
            effects_mask = st_updateeffect(&efftab[neffects], informat[0],
                                           outformat, effects_mask);

            neffects++;
        }

        /* If reducing the number of samples, its faster to run all effects
         * after the resample effect.
         */
        if (needrate && !(hasrate) &&
            (informat[0]->info.rate > outformat->info.rate))
        {
            if (soxpreview)
                st_geteffect(&efftab[neffects], "rate");
            else
                st_geteffect(&efftab[neffects], "resample");

            /* set up & give default opts for added effects */
            (* efftab[neffects].h->getopts)(&efftab[neffects],(int)0,
                                            (char **)0);

            /* Copy format info to effect table */
            effects_mask = st_updateeffect(&efftab[neffects], informat[0],
                                           outformat, effects_mask);

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
            effects_mask = st_updateeffect(&efftab[neffects], informat[0],
                                           outformat, effects_mask);

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
            effects_mask = st_updateeffect(&efftab[neffects], informat[0],
                                           outformat, effects_mask);

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

        /* If code up until now still hasn't added avg effect then
         * do it now.
         */
        if (needchan && !(effects_mask & ST_EFF_CHAN))
        {
            st_geteffect(&efftab[neffects], "avg");

            /* set up & give default opts for added effects */
            (* efftab[neffects].h->getopts)(&efftab[neffects],(int)0,
                                            (char **)0);

            /* Copy format info to effect table */
            effects_mask = st_updateeffect(&efftab[neffects], informat[0],
                                           outformat, effects_mask);

            neffects++;
        }
}

static void statistics(void) {
        if (dovolume && clipped > 0)
                st_report("Volume change clipped %d samples", clipped);
}

static st_sample_t volumechange(st_sample_t *buf, st_ssize_t ct, 
	                        double vol)
{
        double y;
        st_sample_t *p,*top;
        st_ssize_t clips=0;

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

#ifdef SOXMIX
static char *usagestr =
"[ gopts ] [ fopts ] ifile1 [fopts] ifile2 [ fopts ] ofile [ effect [ effopts ] ]";
#else
static char *usagestr =
"[ gopts ] [ fopts ] ifile [ fopts ] ofile [ effect [ effopts ] ]";
#endif

static void usage(opt)
char *opt;
{
    int i;

        fprintf(stderr, "%s: ", myname);
        if (verbose || !opt)
                fprintf(stderr, "%s\n\n", st_version());
        fprintf(stderr, "Usage: %s\n\n", usagestr);
        if (opt)
                fprintf(stderr, "Failed: %s\n", opt);
        else {
            fprintf(stderr,"gopts: -e -h -p -v volume -V\n\n");
            fprintf(stderr,"fopts: -r rate -c channels -s/-u/-U/-A/-a/-i/-g/-f -b/-w/-l -x\n\n");
            fprintf(stderr, "effect: ");
            for (i = 0; st_effects[i].name != NULL; i++) {
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
void cleanup(void) {
        /* Close the input file and outputfile before exiting*/
        if (informat[0] && informat[0]->fp)
                fclose(informat[0]->fp);
        if (outformat && outformat->fp) {
                fclose(outformat->fp);
                /* remove the output file because we failed, if it's ours. */
                /* Don't if its not a regular file. */
                if (filetype(fileno(outformat->fp)) == S_IFREG)
                    unlink(outformat->filename);
        }
}
