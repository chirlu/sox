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

static int clipped = 0;         /* Volume change clipping errors */
static int writing = 1;         /* are we writing to a file? assume yes. */
static int soxpreview = 0;      /* preview mode */

static st_sample_t ibufl[ST_BUFSIZ/2];    /* Left/right interleave buffers */
static st_sample_t ibufr[ST_BUFSIZ/2];
static st_sample_t obufl[ST_BUFSIZ/2];
static st_sample_t obufr[ST_BUFSIZ/2];

typedef struct file_options
{
    char *filename;
    char *filetype;
    st_signalinfo_t info;
    char swap;
    double volume;
    char uservolume;
    char *comment;
} file_options_t;

/* local forward declarations */
static void doopts(file_options_t *fo, int, char **);
static void copy_input(int offset);
static void open_input(ft_t);
static void copy_output(int offset);
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

#define MAX_INPUT_FILES 32
#define MAX_FILES MAX_INPUT_FILES + 1
#ifdef SOXMIX
#define REQUIRED_INPUT_FILES 2
#else
#define REQUIRED_INPUT_FILES 1
#endif

/* Array's tracking input and output files */
static file_options_t *file_opts[MAX_FILES];
static ft_t file_desc[MAX_FILES];
static int file_count = 0;
static int input_count = 0;

/* We parse effects into a temporary effects table and then place into
 * the real effects table.  This makes it easier to reorder some effects
 * as needed.  For instance, we can run a resampling effect before
 * converting a mono file to stereo.  This allows the resample to work
 * on half the data.
 *
 * Real effects table only needs to be 2 entries bigger then the user
 * specified table.  This is because at most we will need to add
 * a resample effect and a channel averaging effect.
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

int main(int argc, char **argv)
{
    int argc_effect;
    file_options_t *fo;
    int i;

    myname = argv[0];

    /* Loop over arguments and filenames, stop when an effect name is 
     * found.
     */
    while (optind < argc && st_checkeffect(argv[optind]) != ST_SUCCESS)
    {
        if (file_count >= MAX_FILES)
        {
            st_fail("to many filenames. max of %d input files and 1 output files\n", MAX_INPUT_FILES);
        }

        /*
         * Its possible to not specify the output filename by using
         * "-e" option. This allows effects to be ran on data but
         * no output file to be written.
         */
        if (strcmp(argv[optind], "-e") == 0)
        {
            /* -e option found.  Only thing valid after an -e
             * option are effects.
             */
            optind++;
            if (optind >= argc ||
                st_checkeffect(argv[optind]) == ST_SUCCESS)
            {
                writing = 0;
            }
            else
            {
                usage("Can only specify \"-e\" for output filenames");
            }
        }
        else
        {
            fo = calloc(sizeof(file_options_t), 1);
            fo->info.size = -1;
            fo->info.encoding = -1;
            fo->info.channels = -1;
            fo->volume = 1.0;
            file_opts[file_count++] = fo;

            doopts(fo, argc, argv);

            if (optind < argc)
            {
                fo->filename = strdup(argv[optind]);
                optind++;
            }
            else
            {
                usage("missing filename");
            }
        }
    } /* while (commandline options) */

    if (writing)
        input_count = file_count - 1;
    else
        input_count = file_count;

    /* Make sure we got at least the required # of input filename */
    if (input_count < REQUIRED_INPUT_FILES)
        usage("Not enough input or output filenames specified");

    for (i = 0; i < input_count; i++)
    {
#ifdef SOXMIX
        /* When mixing audio, default to input side volume
         * adjustments that will make sure no clipping will
         * occur.  Users most likely won't be happy with
         * this and will want to override it.
         */
        if (!file_opts[i]->uservolume)
            file_opts[i]->volume = 1 / input_count;
#endif
        copy_input(i);
        open_input(file_desc[i]);
    }

    if (writing)
    {
        copy_output(file_count-1);
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

    process();
    statistics();

    for (i = 0; i < file_count; i++)
        free(file_desc[i]);

    return(0);
}

static void copy_input(int offset)
{
    /* FIXME: Check for malloc for failures */
    file_desc[offset] = st_initformat();
    file_desc[offset]->info = file_opts[offset]->info;
    file_desc[offset]->filename = file_opts[offset]->filename;
    /* Let auto effect do the work if user is not overriding. */
    if (!file_opts[offset]->filetype)
        file_desc[offset]->filetype = "auto";
    else
        file_desc[offset]->filetype = strdup(file_opts[offset]->filetype);

    if (st_gettype(file_desc[offset]))
        st_fail("Unknown input file format for '%s':  %s", 
                file_desc[offset]->filename, 
                file_desc[offset]->st_errstr);
}

static void open_input(ft_t ft)
{
    /* Open file handler based on input name.  Used stdin file handler
     * if the filename is "-"
     */
    if (!strcmp(ft->filename, "-"))
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

static void copy_output(int offset)
{
    /* FIXME: Check for malloc for failures */
    file_desc[offset] = st_initformat();;
    file_desc[offset]->info = file_opts[offset]->info;
    file_desc[offset]->filename = file_opts[offset]->filename;
    file_desc[offset]->filetype = file_opts[offset]->filetype;
 
    if (writing && !file_desc[offset]->filetype) {
        /* Use filename extension to determine audio type. */

        /* First, chop off any path portions of filename.  This
         * prevents the next search from considering that part. */
        /* FIXME: using strrchr can only lead to problems when knowing
         * what to free()
         */
        if ((file_desc[offset]->filetype = 
             strrchr(file_desc[offset]->filename, LASTCHAR)) == NULL)
            file_desc[offset]->filetype = file_desc[offset]->filename;

        /* Now look for an filename extension */
        if ((file_desc[offset]->filetype = 
             strrchr(file_desc[offset]->filetype, '.')) != NULL)
            file_desc[offset]->filetype++;
        else
            file_desc[offset]->filetype = NULL;
    }

    if (writing)
    {
        if (st_gettype(file_desc[offset]))
            st_fail("Unknown output file format for '%s': %s",
                    file_desc[offset]->filename, 
                    file_desc[offset]->st_errstr);

    }
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

static void doopts(file_options_t *fo, int argc, char **argv)
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
                fo->filetype = optarg;
                if (fo->filetype[0] == '.')
                    fo->filetype++;
                break;

            case 'r':
                str = optarg;
                if ((!sscanf(optarg, "%u", &fo->info.rate)) ||
                    (fo->info.rate <= 0))
                    st_fail("-r must be given a positive integer");
                break;
            case 'v':
                str = optarg;
                if (!sscanf(str, "%lf", &fo->volume))
                    st_fail("Volume value '%s' is not a number",
                            optarg);
                fo->uservolume = 1;
                if (fo->volume < 0.0)
                    st_report("Volume adjustment is negative.  This will result in a phase change\n");
                break;

            case 'c':
                str = optarg;
                if (!sscanf(str, "%d", &i))
                    st_fail("-c must be given a number");
                /* Since we use -1 as a special internal value,
                 * we must do some extra logic so user doesn't
                 * get confused when we translate -1 to mean
                 * something valid.
                 */
                if (i < 1)
                    st_fail("-c must be given a positive number");
                fo->info.channels = i;
                break;
            case 'b':
                fo->info.size = ST_SIZE_BYTE;
                break;
            case 'w':
                fo->info.size = ST_SIZE_WORD;
                break;
            case 'l':
                fo->info.size = ST_SIZE_DWORD;
                break;
            case 'd':
                fo->info.size = ST_SIZE_DDWORD;
                break;
            case 's':
                fo->info.encoding = ST_ENCODING_SIGN2;
                break;
            case 'u':
                fo->info.encoding = ST_ENCODING_UNSIGNED;
                break;
            case 'U':
                fo->info.encoding = ST_ENCODING_ULAW;
                if (fo->info.size == -1)
                    fo->info.size = ST_SIZE_BYTE;
                break;
            case 'A':
                fo->info.encoding = ST_ENCODING_ALAW;
                if (fo->info.size == -1)
                    fo->info.size = ST_SIZE_BYTE;
                break;
            case 'f':
                fo->info.encoding = ST_ENCODING_FLOAT;
                break;
            case 'a':
                fo->info.encoding = ST_ENCODING_ADPCM;
                break;
            case 'i':
                fo->info.encoding = ST_ENCODING_IMA_ADPCM;
                break;
            case 'g':
                fo->info.encoding = ST_ENCODING_GSM;
                break;

            case 'x':
                fo->swap = 1;
                break;

            case 'V':
                verbose = 1;
                break;
        }
    }
}

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

/*
 * Process input file -> effect table -> output file
 *      one buffer at a time
 */

static void process(void) {
    int e, f, flowstatus;
#ifndef SOXMIX
    int current_input;
#else
    st_size_t s;
    st_ssize_t ilen[MAX_INPUT_FILES];
    st_sample_t *ibuf[MAX_INPUT_FILES];
#endif

    for (f = 0; f < input_count; f++)
    {
        /* Read and write starters can change their formats. */
        if ((*file_desc[f]->h->startread)(file_desc[f]) != ST_SUCCESS)
        {
            st_fail("Failed reading %s: %s",file_desc[f]->filename,
                    file_desc[f]->st_errstr);
        }

        /* Go a head and assume 1 channel audio if nothing is detected.
         * This is because libst usually doesn't set this for mono file
         * formats (for historical reasons).
         */
        if (file_desc[f]->info.channels == -1)
            file_desc[f]->info.channels = 1;

        if (st_checkformat(file_desc[f]) )
            st_fail("bad input format for file %s: %s", file_desc[f]->filename,
                    file_desc[f]->st_errstr);

        st_report("Input file %s: using sample rate %lu\n\tsize %s, encoding %s, %d %s",
                  file_desc[f]->filename, file_desc[f]->info.rate,
                  st_sizes_str[(unsigned char)file_desc[f]->info.size],
                  st_encodings_str[(unsigned char)file_desc[f]->info.encoding],
                  file_desc[f]->info.channels,
                  (file_desc[f]->info.channels > 1) ? "channels" : "channel");

        if (file_desc[f]->comment)
            st_report("Input file %s: comment \"%s\"\n",
                      file_desc[f]->filename, file_desc[f]->comment);
    }

    for (f = 1; f < input_count; f++)
    {
        if (compare_input(file_desc[0], file_desc[f]) != ST_SUCCESS)
        {
            st_fail("Input files must have the same rate, channels, data size, and encoding");
        }
    }

    if (writing)
    {
        open_output(file_desc[file_count-1]);

        /* Always use first input file as a reference for output
         * file format.
         */
        st_copyformat(file_desc[0], file_desc[file_count-1]);

        if ((*file_desc[file_count-1]->h->startwrite)(file_desc[file_count-1]) == ST_EOF)
            st_fail(file_desc[file_count-1]->st_errstr);

        if (st_checkformat(file_desc[file_count-1]))
            st_fail("bad output format: %s", 
                    file_desc[file_count-1]->st_errstr);

        st_report("Output file %s: using sample rate %lu\n\tsize %s, encoding %s, %d %s",
                  file_desc[file_count-1]->filename, 
                  file_desc[file_count-1]->info.rate,
                  st_sizes_str[(unsigned char)file_desc[file_count-1]->info.size],
                  st_encodings_str[(unsigned char)file_desc[file_count-1]->info.encoding],
                  file_desc[file_count-1]->info.channels,
                  (file_desc[file_count-1]->info.channels > 1) ? "channels" : "channel");

        if (file_desc[file_count-1]->comment)
            st_report("Output file: comment \"%s\"\n", 
                      file_desc[file_count-1]->comment);
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
    for (f = 0; f < file_count; f++)
        file_desc[f]->st_errno = 0;

#ifndef SOXMIX
    current_input = 0;
#endif

    /* Run input data through effects and get more until olen == 0 
     * (or ST_EOF).
     */
    do {
#ifndef SOXMIX
        efftab[0].olen = (*file_desc[current_input]->h->read)(file_desc[current_input],
                                                              efftab[0].obuf, 
                                                              (st_ssize_t)ST_BUFSIZ);
        /* FIXME: libst needs the feof() and ferror() concepts
         * to see if ST_EOF means a real failure.  Until then we
         * must treat ST_EOF as just hiting the end of the buffer.
         */
        if (efftab[0].olen == ST_EOF)
        {
            efftab[0].olen = 0;
        }

        /* Some file handlers claim 0 bytes instead of returning
         * ST_EOF.  In either case, attempt to go to the next
         * input file.
         */
        if (efftab[0].olen == 0)
        {
            if (current_input < input_count-1)
            {
                current_input++;
                continue;
            }
        }

        /* Adjust input side volume based on value specified
         * by user for this file.
         */
        if (file_opts[current_input]->volume != 1.0)
            clipped += volumechange(efftab[0].obuf, 
                                    efftab[0].olen,
                                    file_opts[current_input]->volume);
#else
        for (f = 0; f < input_count; f++)
        {
            ilen[f] = (*file_desc[f]->h->read)(file_desc[f],
                                               ibuf[f], 
                                               (st_ssize_t)ST_BUFSIZ);
            /* FIXME: libst needs the feof() and ferror() concepts
             * to see if ST_EOF means a real failure.  Until then we
             * must treat ST_EOF as just hiting the end of the buffer.
             */
            if (ilen[f] == ST_EOF)
                ilen[f] = 0;

            /* Adjust input side volume based on value specified
             * by user for this file.
             */
            if (file_opts[f]->volume != 1.0)
                clipped += volumechange(ibuf[f], 
                                        ilen[f],
                                        file_opts[f]->volume);
        }

        /* FIXME: Should report if the size of the reads are not
         * the same.
         */
        efftab[0].olen = 0;
        for (f = 0; f < input_count; f++)
            if ((st_size_t)ilen[f] > efftab[0].olen)
                efftab[0].olen = ilen[f];

        for (s = 0; s < efftab[0].olen; s++)
        {
            /* Mix data together by summing samples together.
             * It is assumed that input side volume adjustments
             * will take care of any possible overflow.
             * By default, SoX sets the volume adjustment
             * to 1/input_count but the user can override this.
             * They probably will and some clipping will probably
             * occur because of this.
             */
            for (f = 0; f < input_count; f++)
            {
                if (f == 0)
                    efftab[0].obuf[s] =
                        (s<(st_size_t)ilen[f]) ? ibuf[f][s] : 0;
                else
                    if (s < (st_size_t)ilen[f])
                    {
                        double sample;
                        sample = efftab[0].obuf[s] + ibuf[f][s];
                        if (sample < ST_SAMPLE_MIN)
                        {
                            sample = ST_SAMPLE_MIN;
                            clipped++;
                        }
                        else if (sample > ST_SAMPLE_MAX)
                        {
                            sample = ST_SAMPLE_MAX;
                            clipped++;
                        }
                        efftab[0].obuf[s] = sample;
                    }
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

        /* mark chain as empty */
        for(e = 1; e < neffects; e++)
            efftab[e].odone = efftab[e].olen = 0;

        flowstatus = flow_effect_out();

        /* Negative flowstatus says no more output will ever be generated. */
        if (flowstatus < 0 || 
            (writing && file_desc[file_count-1]->st_errno))
            break;

    } while (1); /* break; efftab[0].olen == 0 */

#ifdef SOXMIX
    /* Free input buffers now that they are not used */
    for (f = 0; f < MAX_INPUT_FILES; f++)
    {
        free(ibuf[f]);
    }
#endif

    /* Drain the effects out first to last,
     * pushing residue through subsequent effects */
    /* oh, what a tangled web we weave */
    for(f = 1; f < neffects; f++)
    {
        while (1) {

            if (drain_effect(f) == 0)
                break;          /* out of while (1) */

            /* Change the volume of this output data if needed. */
            if (writing && file_opts[file_count-1]->volume != 1.0)
                clipped += volumechange(efftab[neffects-1].obuf, 
                                        efftab[neffects-1].olen,
                                        file_opts[file_count-1]->volume);

            /* FIXME: Need to look at return code and abort on failure */
            if (writing && efftab[neffects-1].olen > 0)
                (*file_desc[file_count-1]->h->write)(file_desc[file_count-1], 
                                                     efftab[neffects-1].obuf,
                                                     (st_ssize_t)efftab[neffects-1].olen);

            if (efftab[f].olen != ST_BUFSIZ)
                break;
        }
    }

    /* Free output buffers now that they won't be used */
    for(e = 0; e < neffects; e++)
    {
        free(efftab[e].obuf);
        if (efftabR[e].obuf)
            free(efftabR[e].obuf);
    }

    /* Very Important:
     * Effect stop is called BEFORE files close.
     * Effect may write out more data after.
     */

    for (e = 1; e < neffects; e++) {
        (*efftab[e].h->stop)(&efftab[e]);
        if (efftabR[e].name)
            (* efftabR[e].h->stop)(&efftabR[e]);
    }

    for (f = 0; f < input_count; f++)
    {
        /* If problems closing input file, just warn user since
         * we are exiting anyways.
         */
        if ((*file_desc[f]->h->stopread)(file_desc[f]) == ST_EOF)
            st_warn(file_desc[f]->st_errstr);
        fclose(file_desc[f]->fp);
        free(file_desc[f]->filename);
    }

    if (writing)
    {
        /* problem closing output file, just warn user since we
         * are exiting anyways.
         */
        if ((*file_desc[file_count-1]->h->stopwrite)(file_desc[file_count-1]) == ST_EOF)
            st_warn(file_desc[file_count-1]->st_errstr);
        free(file_desc[file_count-1]->filename);
        free(file_desc[file_count-1]->comment);
        fclose(file_desc[file_count-1]->fp);
    }
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
      if (writing && (efftab[neffects-1].olen>efftab[neffects-1].odone))
      {
          /* Change the volume of this output data if needed. */
          if (writing && file_opts[file_count-1]->volume != 1.0)
              clipped += volumechange(efftab[neffects-1].obuf, 
                                      efftab[neffects-1].olen,
                                      file_opts[file_count-1]->volume);


          /* FIXME: Should look at return code and abort
           * on ST_EOF
           */
          (*file_desc[file_count-1]->h->write)(file_desc[file_count-1],
                                               efftab[neffects-1].obuf,
                                               (st_ssize_t)efftab[neffects-1].olen);
          efftab[neffects-1].odone = efftab[neffects-1].olen;

          if (file_desc[file_count-1]->st_errno)
          {
              st_warn("Error writing: %s", file_desc[file_count-1]->st_errstr);
              break;
          }
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
        /* FIXME: Should look at return code and abort on ST_EOF */
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
        /* FIXME: Should look at return code and abort on ST_EOF */
        effstatus = (* efftab[e].h->flow)(&efftab[e],
                                          ibufl, obufl, &idonel, &odonel);

        /* right */
        idoner = idone/2;               /* odd-length logic */
        odoner = odone/2;
        /* FIXME: Should look at return code and abort on ST_EOF */
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
        /* FIXME: Should look at return code and abort on ST_EOF */
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

        if (writing)
        {
            needrate = (file_desc[0]->info.rate != file_desc[file_count-1]->info.rate);
            needchan = (file_desc[0]->info.channels != file_desc[file_count-1]->info.channels);
        }

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
            (file_desc[0]->info.channels > file_desc[file_count-1]->info.channels))
        {
            /* Find effect and update initial pointers */
            st_geteffect(&efftab[neffects], "avg");

            /* give default opts for added effects */
            /* FIXME: Should look at return code and abort on ST_EOF */
            (* efftab[neffects].h->getopts)(&efftab[neffects],(int)0,
                                            (char **)0);

            /* Copy format info to effect table */
            effects_mask = st_updateeffect(&efftab[neffects], 
                                           &file_desc[0]->info,
                                           &file_desc[file_count-1]->info, 
                                           effects_mask);

            neffects++;
        }

        /* If reducing the number of samples, its faster to run all effects
         * after the resample effect.
         */
        if (needrate && !(hasrate) &&
            (file_desc[0]->info.rate > file_desc[file_count-1]->info.rate))
        {
            if (soxpreview)
                st_geteffect(&efftab[neffects], "rate");
            else
                st_geteffect(&efftab[neffects], "resample");

            /* set up & give default opts for added effects */
            /* FIXME: Should look at return code and abort on ST_EOF */
            (* efftab[neffects].h->getopts)(&efftab[neffects],(int)0,
                                            (char **)0);

            /* Copy format info to effect table */
            effects_mask = st_updateeffect(&efftab[neffects], 
                                           &file_desc[0]->info,
                                           &file_desc[file_count-1]->info, 
                                           effects_mask);

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
            effects_mask = st_updateeffect(&efftab[neffects], 
                                           &file_desc[0]->info,
                                           &file_desc[file_count-1]->info, 
                                           effects_mask);

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
            /* FIXME: Should look at return code and abort on ST_EOF */
            (* efftab[neffects].h->getopts)(&efftab[neffects],(int)0,
                                            (char **)0);

            /* Copy format info to effect table */
            effects_mask = st_updateeffect(&efftab[neffects], 
                                           &file_desc[0]->info,
                                           &file_desc[file_count-1]->info, 
                                           effects_mask);

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
            /* FIXME: Should look at return code and abort on ST_EOF */
            (* efftab[neffects].h->getopts)(&efftab[neffects],(int)0,
                                            (char **)0);

            /* Copy format info to effect table */
            effects_mask = st_updateeffect(&efftab[neffects], 
                                           &file_desc[0]->info,
                                           &file_desc[file_count-1]->info, 
                                           effects_mask);

            neffects++;
        }
}

static void statistics(void) {
        if (clipped > 0)
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
            if (y < ST_SAMPLE_MIN) {
                y = ST_SAMPLE_MIN;
                clips++;
            }
            else if (y > ST_SAMPLE_MAX) {
                y = ST_SAMPLE_MAX;
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
            fprintf(stderr,"fopts: -r rate -c channels -s/-u/-U/-A/-a/-i/-g/-f -b/-w/-l/-d -x\n\n");
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


/* called from util.c::st_fail() */
void cleanup(void) {
    int i;

    /* Close the input file and outputfile before exiting*/
    for (i = 0; i < input_count; i++)
    {
        if (file_desc[i] && file_desc[i]->fp)
                fclose(file_desc[i]->fp);
        if (file_desc[i])
            free(file_desc[i]);
    }
    if (writing && file_desc[file_count-1] && file_desc[file_count-1]->fp) {
        fclose(file_desc[file_count-1]->fp);
        /* remove the output file because we failed, if it's ours. */
        /* Don't if its not a regular file. */
        if (filetype(fileno(file_desc[file_count-1]->fp)) == S_IFREG)
            unlink(file_desc[file_count-1]->filename);
        if (file_desc[file_count-1])
            free(file_desc[file_count-1]);
    }
}
