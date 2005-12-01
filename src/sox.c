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
#include <signal.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
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

#include <sys/types.h> /* for fstat() */
#include <sys/stat.h> /* for fstat() */
#ifdef _MSC_VER
/*
 * __STDC__ is defined, so these symbols aren't created.
 */
#define S_IFMT   _S_IFMT
#define S_IFREG  _S_IFREG
#define fstat _fstat
#define strdup _strdup
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

static int user_abort = 0;

static int quiet = 0;
static int status = 0;
static unsigned long input_samples = 0;
static unsigned long read_samples = 0;
static unsigned long output_samples = 0;

static st_sample_t ibufl[ST_BUFSIZ/2];    /* Left/right interleave buffers */
static st_sample_t ibufr[ST_BUFSIZ/2];
static st_sample_t obufl[ST_BUFSIZ/2];
static st_sample_t obufr[ST_BUFSIZ/2];

typedef struct file_options
{
    char *filename;
    char *filetype;
    st_signalinfo_t info;
    double volume;
    char uservolume;
    char *comment;
} file_options_t;

/* local forward declarations */
static void doopts(file_options_t *fo, int, char **);
static void usage(char *) NORET;
static void process(void);
static void print_input_status(int input);
static void update_status(void);
static void statistics(void);
static st_sample_t volumechange(st_sample_t *buf, st_ssize_t ct, double vol);
static void parse_effects(int argc, char **argv);
static void check_effects(void);
static void start_effects(void);
static void reserve_effect_buf(void);
static int flow_effect_out(void);
static int flow_effect(int);
static int drain_effect_out(void);
static int drain_effect(int);
static void release_effect_buf(void);
static void stop_effects(void);
void cleanup(void);
static void sigint(int s);

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
 * efftab[0] is a dummy entry used only as an input buffer for
 * reading input data into.
 *
 * If one was to support effects for quad-channel files, there would
 * need to be an effect table for each channel to handle effects
 * that done st ST_EFF_MCHAN.
 */

static struct st_effect efftab[MAX_EFF]; /* left/mono channel effects */
static struct st_effect efftabR[MAX_EFF];/* right channel effects */
static int neffects;                     /* # of effects to run on data */
static int input_eff;                    /* last input effect with data */

static struct st_effect user_efftab[MAX_USER_EFF];
static int nuser_effects;

int main(int argc, char **argv)
{
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
            fo = (file_options_t *)calloc(sizeof(file_options_t), 1);
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

    /* Make sure we got at least the required # of input filenames */
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
            file_opts[i]->volume = 1.0 / input_count;
#endif
        file_desc[i] = st_open_read(file_opts[i]->filename,
                                    &file_opts[i]->info, 
                                    file_opts[i]->filetype);
        if (!file_desc[i])
        {
            /* st_open_read() will call st_warn for most errors.
             * Rely on that printing something.
             */
            cleanup();
            exit(2);
        }
    }

    /* Loop through the reset of the arguments looking for effects */
    parse_effects(argc, argv);

    signal(SIGINT, sigint);
    signal(SIGTERM, sigint);

    process();
    statistics();

    for (i = 0; i < file_count; i++)
        free(file_desc[i]);

    if (status)
    {
        if (user_abort)
            fprintf(stderr, "\n\nAborted.\n");
        else
            fprintf(stderr, "\n\nDone.\n");
    }

    return(0);
}

#ifdef HAVE_GETOPT_H
static char *getoptstr = "+r:v:t:c:phsuUAaigbwlfdxVSq";
#else
static char *getoptstr = "r:v:t:c:phsuUAaigbwlfdxVSq";
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
                fo->info.swap = 1;
                break;

            case 'V':
                verbose = 1;
                break;

            case 'S':
                status = 1;
                quiet = 0;
                break;

            case 'q':
                status = 0;
                quiet = 1;
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

void optimize_trim(void)
{
    /* Speed hack.  If the "trim" effect is the first effect then
     * peak inside its "effect descriptor" and see what the
     * start location is.  This has to be done after its start()
     * is called to have the correct location.
     * Also, only do this when only working with one input file.
     * This is because the logic to do it for multiple files is
     * complex and problably never used.
     */
    if (input_count == 1 && neffects > 1 && 
        strcmp(efftab[1].name, "trim") == 0)
    {
        if ((file_desc[0]->h->flags & ST_FILE_SEEK) && file_desc[0]->seekable)
        {
            if (st_seek(file_desc[0], st_trim_get_start(&efftab[1]),
                        ST_SEEK_SET) != ST_EOF)
            {
                /* Assuming a failed seek stayed where it was.  If the
                 * seek worked then reset the start location of
                 * trim so that it thinks user didn't request a skip.
                 */
                st_trim_clear_start(&efftab[1]);
            }
        }
    }
}

/*
 * Process input file -> effect table -> output file
 *      one buffer at a time
 */

static void process(void) {
    int e, f, flowstatus = ST_SUCCESS;
#ifndef SOXMIX
    int current_input;
    st_ssize_t ilen;
#else
    st_size_t s;
    st_ssize_t ilen[MAX_INPUT_FILES];
    st_sample_t *ibuf[MAX_INPUT_FILES];
#endif

    for (f = 0; f < input_count; f++)
    {
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
        st_loopinfo_t loops[ST_MAX_NLOOPS];
        double factor;
        int i;

        if (file_opts[file_count-1]->info.rate == 0)
            file_opts[file_count-1]->info.rate = file_desc[0]->info.rate;
        if (file_opts[file_count-1]->info.size == -1)
            file_opts[file_count-1]->info.size = file_desc[0]->info.size;
        if (file_opts[file_count-1]->info.encoding == -1)
            file_opts[file_count-1]->info.encoding = 
                file_desc[0]->info.encoding;
        if (file_opts[file_count-1]->info.channels == -1)
            file_opts[file_count-1]->info.channels = 
                file_desc[0]->info.channels;

        /*
         * copy loop info, resizing appropriately
         * it's in samples, so # channels don't matter
         * FIXME: This doesn't work for multi-file processing or
         * effects that change file length.
         */
        factor = (double) file_opts[file_count-1]->info.rate / (double) 
            file_desc[0]->info.rate;
        for(i = 0; i < ST_MAX_NLOOPS; i++) {
            loops[i].start = file_desc[0]->loops[i].start * factor;
            loops[i].length = file_desc[0]->loops[i].length * factor;
            loops[i].count = file_desc[0]->loops[i].count;
            loops[i].type = file_desc[0]->loops[i].type;
        }

        file_desc[file_count-1] = 
            st_open_write_instr(file_opts[file_count-1]->filename,
                                &file_opts[file_count-1]->info, 
                                file_opts[file_count-1]->filetype,
                                file_desc[0]->comment,
                                &file_desc[0]->instr,
                                loops);

        if (!file_desc[file_count-1])
        {
            /* st_open_write() will call st_warn for most errors.
             * Rely on that printing something.
             */
            cleanup();
            exit(2);
        }

        /* When writing to an audio device, auto turn on the
         * status display to match behavior of ogg123 status.
         * That is unless user requested us not to display]
         * anything.
         */
        if (strcmp(file_desc[file_count-1]->filetype, "alsa") == 0 ||
            strcmp(file_desc[file_count-1]->filetype, "ossdsp") == 0 ||
            strcmp(file_desc[file_count-1]->filetype, "sunau") == 0)
        {
            if (!quiet)
                status = 1;
        }

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
    check_effects();

    /* Start all effects */
    start_effects();

    /* Reserve an output buffer for all effects */
    reserve_effect_buf();

    /* Try to save some time if first effect is "trim" by seeking */
    optimize_trim();

#ifdef SOXMIX
    for (f = 0; f < input_count; f++)
    {
        /* Treat overall length the same as longest input file. */
        if (file_desc[f]->length > input_samples)
            input_samples = file_desc[f]->length;

        ibuf[f] = (st_sample_t *)malloc(ST_BUFSIZ * sizeof(st_sample_t));
        if (!ibuf[f])
        {
            st_fail("could not allocate memory");
        }

        if (status)
            print_input_status(f);
    }
#else
    current_input = 0;
    input_samples = file_desc[current_input]->length;

    if (status)
        print_input_status(current_input);
#endif

    /*
     * Just like errno, we must set st_errno to known values before
     * calling I/O operations.
     */
    for (f = 0; f < file_count; f++)
        file_desc[f]->st_errno = 0;

    input_eff = 0;

    /* mark chain as empty */
    for(e = 1; e < neffects; e++)
        efftab[e].odone = efftab[e].olen = 0;

    /* Run input data through effects and get more until olen == 0 
     * (or ST_EOF).
     */
    do {
#ifndef SOXMIX
        ilen = st_read(file_desc[current_input], efftab[0].obuf, 
                       (st_ssize_t)ST_BUFSIZ);
        if (ilen > ST_BUFSIZ)
        {
            st_warn("WARNING: Corrupt value of %d!  Assuming 0 bytes read.\n", ilen);
            ilen = 0;
        }

        /* FIXME: libst needs the feof() and ferror() concepts
         * to see if ST_EOF means a real failure.  Until then we
         * must treat ST_EOF as just hiting the end of the file.
         */
        if (ilen == ST_EOF)
            efftab[0].olen = 0;
        else
            efftab[0].olen = ilen;

        read_samples += efftab[0].olen;

        /* Some file handlers claim 0 bytes instead of returning
         * ST_EOF.  In either case, attempt to go to the next
         * input file.
         */
        if (ilen == ST_EOF || efftab[0].olen == 0)
        {
            if (current_input < input_count-1)
            {
                current_input++;
                input_samples = file_desc[current_input]->length;
                read_samples = 0;

                if (status)
                    print_input_status(current_input);

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
            ilen[f] = st_read(file_desc[f], ibuf[f], (st_ssize_t)ST_BUFSIZ);

            /* FIXME: libst needs the feof() and ferror() concepts
             * to see if ST_EOF means a real failure.  Until then we
             * must treat ST_EOF as just hiting the end of the buffer.
             */
            if (ilen[f] == ST_EOF)
                ilen[f] = 0;

            /* Only count read samples for first file in mix */
            if (f == 0)
                read_samples += efftab[0].olen;

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
         * useful to print out info about input file header and quiet.
         */
        if (!writing && neffects == 1)
            efftab[0].olen = 0;

        if (efftab[0].olen == 0)
            break;

        flowstatus = flow_effect_out();

        if (status)
            update_status();

        /* Quite reading/writing on user aborts.  This will close
         * done the files nicely as if an EOF was reached on read.
         */
        if (user_abort)
            break;

        /* Negative flowstatus says no more output will ever be generated. */
        if (flowstatus == ST_EOF || 
            (writing && file_desc[file_count-1]->st_errno))
            break;

    } while (1); 

    /* This will drain the effects */
    drain_effect_out();

#ifdef SOXMIX
    /* Free input buffers now that they are not used */
    for (f = 0; f < input_count; f++)
    {
        free(ibuf[f]);
    }
#endif

    /* Free output buffers now that they won't be used */
    release_effect_buf();

    /* Very Important:
     * Effect stop is called BEFORE files close.
     * Effect may write out more data after.
     */
    stop_effects();

    for (f = 0; f < input_count; f++)
    {
        /* If problems closing input file, just warn user since
         * we are exiting anyways.
         */
        if (st_close(file_desc[f]) == ST_EOF)
            st_warn(file_desc[f]->st_errstr);
    }

    if (writing)
    {
        /* problem closing output file, just warn user since we
         * are exiting anyways.
         */
        if (st_close(file_desc[file_count-1]) == ST_EOF)
            st_warn(file_desc[file_count-1]->st_errstr);
    }
}

static void parse_effects(int argc, char **argv)
{
    int argc_effect;

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

        (*user_efftab[nuser_effects].h->getopts)
            (&user_efftab[nuser_effects],
             argc_effect,
             &argv[optind]);

        /* Skip past the effect arguments */
        optind += argc_effect;
        nuser_effects++;
    }
}

/*
 * If no effect given, decide what it should be.
 * Smart ruleset for multiple effects in sequence.
 *      Puts user-specified effect in right place.
 */
static void check_effects(void)
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

static void start_effects(void)
{
    int e;

    for(e = 1; e < neffects; e++) {
        (*efftab[e].h->start)(&efftab[e]);
        if (efftabR[e].name)
            (*efftabR[e].h->start)(&efftabR[e]);
    }
}

static void reserve_effect_buf(void)
{
    int e;

    for(e = 0; e < neffects; e++)
    {
        efftab[e].obuf = (st_sample_t *)malloc(ST_BUFSIZ * 
                                                sizeof(st_sample_t));
        if (efftab[e].obuf == NULL)
        {
            st_fail("could not allocate memory");
        }
        if (efftabR[e].name)
        {
            efftabR[e].obuf = (st_sample_t *)malloc(ST_BUFSIZ * 
                                                     sizeof(st_sample_t));
            if (efftabR[e].obuf == NULL)
            {
                st_fail("could not allocate memory");
            }
        }
    }
}

static int flow_effect_out(void)
{
    int e, havedata, flowstatus = 0;
    int len, total;

    do {
      /* run entire chain BACKWARDS: pull, don't push.*/
      /* this is because buffering system isn't a nice queueing system */
      for(e = neffects - 1; e >= input_eff; e--)
      {
          /* flow_effect returns ST_EOF when it will not process
           * any more samples.  This is used to bail out early.
           * Since we are "pulling" data, it is OK that we are not
           * calling any more previous effects since their output
           * would not be looked at anyways.
           */
          flowstatus  = flow_effect(e);
          if (flowstatus == ST_EOF)
              input_eff = e+1;

          /* If this buffer contains more input data then break out
           * of this loop now.  This will allow us to loop back around
           * and reprocess the rest of this input buffer.
           * I suppose this could be an issue with some effects
           * if they crash when given small input buffers.
           * But I was more concerned that we would need to do
           * some type of garbage collection otherwise.  By this I
           * mean that if we went ahead and processed an effect
           * lower in the chain, it might only have like 2 bytes
           * left at the end of this buffer to place its data in.
           * Software is more likely to refuse to handle that.
           */
          if (efftab[e].odone < efftab[e].olen)
          {
#if 0
              fprintf(stderr, "Breaking out of loop to flush buffer\n");
#endif
              break;
          }
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
          total = 0;
          do
          {
              len = st_write(file_desc[file_count-1], 
                             &efftab[neffects-1].obuf[total],
                             (st_ssize_t)efftab[neffects-1].olen-total);

              if (len < 0)
              {
                  st_warn("Error writing: %s",
                          file_desc[file_count-1]->st_errstr);
                  return ST_EOF;
              }
              total += len;
          } while (total < efftab[neffects-1].olen);
          output_samples += (total / file_desc[file_count-1]->info.channels);
          efftab[neffects-1].odone = efftab[neffects-1].olen = 0;
      }
      else
      {
          /* Make it look like everything was consumed */
          output_samples += (efftab[neffects-1].olen / 
                             file_desc[file_count-1]->info.channels);
          efftab[neffects-1].odone = efftab[neffects-1].olen = 0;
      }

      /* if stuff still in pipeline, set up to flow effects again */
      /* When all effects have reported ST_EOF then this check will
       * show no more data.
       */
      havedata = 0;
      for(e = neffects - 1; e >= input_eff; e--)
      {
          /* If odone and olen are the same then this buffer
           * can be reused.
           */
          if (efftab[e].odone == efftab[e].olen)
              efftab[e].odone = efftab[e].olen = 0;

          if (efftab[e].odone < efftab[e].olen) 
          {
              /* Only mark that we have more data if a full
               * frame that can be written.
               * FIXME: If this error case happens for the
               * input buffer then the data will be lost and
               * will cause stereo channels to be inversed.
               */
              if ((efftab[e].olen - efftab[e].odone) >= 
                  file_desc[file_count-1]->info.channels)
                  havedata = 1;
              else
                  st_warn("Received buffer with incomplete amount of samples.");
              /* Don't break out because other things are being
               * done in loop.
               */
          }
      }

      if (!havedata && input_eff > 0)
      {
          /* If the input file is not returning data then
           * we must prime the pump using the drain effect.
           * After its primed, the loop will suck the data
           * threw.  Once an input_eff stop reporting samples,
           * we will continue to the next until all are drained.
           */
          while (input_eff < neffects)
          {
              int rc;

              rc = drain_effect(input_eff);

              if (rc == ST_EOF || efftab[input_eff].olen == 0)
                  input_eff++;
              else
              {
                  havedata = 1;
                  break;
              }
          }
      }
    } while (havedata);

    /* If input_eff isn't pointing at fake first entry then there
     * is no need to read any more data from disk.  Return this
     * fact to caller.
     */
    if (input_eff > 0)
    {
#if 0
        fprintf(stderr, "Effect return ST_EOF\n");
#endif
        return ST_EOF;
    }

    return ST_SUCCESS;
}

static int flow_effect(int e)
{
    st_ssize_t i, done, idone, odone, idonel, odonel, idoner, odoner;
    st_sample_t *ibuf, *obuf;
    int effstatus;

    /* I have no input data ? */
    if (efftab[e-1].odone == efftab[e-1].olen)
    {
#if 0
        fprintf(stderr, "%s no data to pull to me!\n", efftab[e].name);
#endif
        return 0;
    }

    if (! efftabR[e].name) {
        /* No stereo data, or effect can handle stereo data so
         * run effect over entire buffer.
         */
        idone = efftab[e-1].olen - efftab[e-1].odone;
        odone = ST_BUFSIZ - efftab[e].olen;
#if 0
        fprintf(stderr, "pre %s idone=%d, odone=%d\n", efftab[e].name, idone, odone);
        fprintf(stderr, "pre %s odone1=%d, olen1=%d odone=%d olen=%d\n", efftab[e].name, efftab[e-1].odone, efftab[e-1].olen, efftab[e].odone, efftab[e].olen); 
#endif

        effstatus = (* efftab[e].h->flow)(&efftab[e],
                                          &efftab[e-1].obuf[efftab[e-1].odone],
                                          &efftab[e].obuf[efftab[e].olen], 
                                          (st_size_t *)&idone, 
                                          (st_size_t *)&odone);

        efftab[e-1].odone += idone;
        /* Leave efftab[e].odone were it was since we didn't consume data */
        /*efftab[e].odone = 0;*/
        efftab[e].olen += odone; 
#if 0
        fprintf(stderr, "post %s idone=%d, odone=%d\n", efftab[e].name, idone, odone); 
        fprintf(stderr, "post %s odone1=%d, olen1=%d odone=%d olen=%d\n", efftab[e].name, efftab[e-1].odone, efftab[e-1].olen, efftab[e].odone, efftab[e].olen);
#endif

        done = idone + odone;
    } else {

        /* Put stereo data in two seperate buffers and run effect
         * on each of them.
         */
        idone = efftab[e-1].olen - efftab[e-1].odone;
        odone = ST_BUFSIZ - efftab[e].olen;

        ibuf = &efftab[e-1].obuf[efftab[e-1].odone];
        for(i = 0; i < idone; i += 2) {
            ibufl[i/2] = *ibuf++;
            ibufr[i/2] = *ibuf++;
        }

        /* left */
        idonel = (idone + 1)/2;         /* odd-length logic */
        odonel = odone/2;
#if 0
        fprintf(stderr, "pre %s idone=%d, odone=%d\n", efftab[e].name, idone, odone);
        fprintf(stderr, "pre %s odone1=%d, olen1=%d odone=%d olen=%d\n", efftab[e].name, efftab[e-1].odone, efftab[e-1].olen, efftab[e].odone, efftab[e].olen); 
#endif

        effstatus = (* efftab[e].h->flow)(&efftab[e],
                                          ibufl, obufl, (st_size_t *)&idonel, 
                                          (st_size_t *)&odonel);

        /* right */
        idoner = idone/2;               /* odd-length logic */
        odoner = odone/2;
        /* FIXME: effstatus of previous operation is lost. */
        effstatus = (* efftabR[e].h->flow)(&efftabR[e],
                                           ibufr, obufr, (st_size_t *)&idoner, 
                                           (st_size_t *)&odoner);

        obuf = &efftab[e].obuf[efftab[e].olen];
         /* This loop implies left and right effect will always output
          * the same amount of data.
          */
        for(i = 0; i < odoner; i++) {
            *obuf++ = obufl[i];
            *obuf++ = obufr[i];
        }
        efftab[e-1].odone += idonel + idoner;
        /* Don't clear since nothng has been consumed yet */
        /*efftab[e].odone = 0;*/
        efftab[e].olen += odonel + odoner;
#if 0
        fprintf(stderr, "post %s idone=%d, odone=%d\n", efftab[e].name, idone, odone); 
        fprintf(stderr, "post %s odone1=%d, olen1=%d odone=%d olen=%d\n", efftab[e].name, efftab[e-1].odone, efftab[e-1].olen, efftab[e].odone, efftab[e].olen);
#endif


        done = idonel + idoner + odonel + odoner;
    }
    if (effstatus == ST_EOF)
    {
        return ST_EOF;
    }
    if (done == 0)
        st_fail("Effect took & gave no samples!");
    return ST_SUCCESS;
}

static int drain_effect_out(void)
{
    /* Skip past input effect since we know thats not needed */
    if (input_eff == 0)
        input_eff = 1;

    /* Try to prime the pump with some data */
    while (input_eff < neffects)
    {
        int rc;

        rc = drain_effect(input_eff);

        if (rc == ST_EOF || efftab[input_eff].olen == 0)
            input_eff++;
        else
            break;
    }

    /* Just do standard flow routines after the priming. */
    return flow_effect_out();
}

static int drain_effect(int e)
{
    st_ssize_t i, olen, olenl, olenr;
    st_sample_t *obuf;
    int rc;

    if (! efftabR[e].name) {
        efftab[e].olen = ST_BUFSIZ;
        rc = (* efftab[e].h->drain)(&efftab[e],efftab[e].obuf,
                                    &efftab[e].olen);
        efftab[e].odone = 0;
    }
    else {
        int rc_l, rc_r;

        olen = ST_BUFSIZ;

        /* left */
        olenl = olen/2;
        rc_l = (* efftab[e].h->drain)(&efftab[e], obufl, 
                                      (st_size_t *)&olenl);

        /* right */
        olenr = olen/2;
        rc_r = (* efftab[e].h->drain)(&efftabR[e], obufr, 
                                      (st_size_t *)&olenr);

        if (rc_l == ST_EOF || rc_r == ST_EOF)
            rc = ST_EOF;
        else
            rc = ST_SUCCESS;

        obuf = efftab[e].obuf;
        /* This loop implies left and right effect will always output
         * the same amount of data.
         */
        for(i = 0; i < olenr; i++) {
            *obuf++ = obufl[i];
            *obuf++ = obufr[i];
        }
        efftab[e].olen = olenl + olenr;
        efftab[e].odone = 0;
    }
    return rc;
}

static void release_effect_buf(void)
{
    int e;
    
    for(e = 0; e < neffects; e++)
    {
        free(efftab[e].obuf);
        if (efftabR[e].obuf)
            free(efftabR[e].obuf);
    }
}

static void stop_effects(void)
{
    int e;

    for (e = 1; e < neffects; e++) {
        (*efftab[e].h->stop)(&efftab[e]);
        if (efftabR[e].name)
            (* efftabR[e].h->stop)(&efftabR[e]);
    }
}

static void print_input_status(int input)
{
    fprintf(stderr, "\nInput Filename : %s\n", file_desc[input]->filename);
    fprintf(stderr, "Sample Size    : %s\n", 
            st_size_bits_str[file_desc[input]->info.size]);
    fprintf(stderr, "Sample Encoding: %s\n", 
            st_encodings_str[file_desc[input]->info.encoding]);
    fprintf(stderr, "Channels       : %d\n", file_desc[input]->info.channels);
    fprintf(stderr, "Sample Rate    : %d\n", file_desc[input]->info.rate);

    if (file_desc[input]->comment && *file_desc[input]->comment)
        fprintf(stderr, "Comments       :\n%s\n", file_desc[input]->comment);
    fprintf(stderr, "\n");
}
 
static void update_status(void)
{
    int read_min, left_min, in_min;
    double read_sec, left_sec, in_sec;
    double read_time, left_time, in_time;
    float completed;
    double out_size;
    char unit;

    /* Currently, for both sox and soxmix, all input files must have
     * the same sample rate.  So we can always just use the rate
     * of the first input file to compute time.
     */
    read_time = (double)read_samples / (double)file_desc[0]->info.rate /
                (double)file_desc[0]->info.channels;

    read_min = read_time / 60;
    read_sec = (double)read_time - 60.0f * (double)read_min;

    out_size = output_samples / 1000000000.0;
    if (out_size >= 1.0)
        unit = 'G';
    else
    {
        out_size = output_samples / 1000000.0;
        if (out_size >= 1.0)
            unit = 'M';
        else
        {
            out_size = output_samples / 1000.0;
            if (out_size >= 1.0)
                unit = 'K';
            else
                unit = ' ';
        }
    }

    if (input_samples)
    {
        in_time = (double)input_samples / (double)file_desc[0]->info.rate /
                  (double)file_desc[0]->info.channels;
        left_time = in_time - read_time;
        if (left_time < 0)
            left_time = 0;

        completed = ((double)read_samples / (double)input_samples) * 100;
        if (completed < 0)
            completed = 0;
    }
    else
    {
        in_time = 0;
        left_time = 0;
        completed = 0;
    }

    left_min = left_time / 60;
    left_sec = (double)left_time - 60.0f * (double)left_min;

    in_min = in_time / 60;
    in_sec = (double)in_time - 60.0f * (double)in_min;

    fprintf(stderr, "\rTime: %02i:%05.2f [%02i:%05.2f] of %02i:%05.2f (% 5.1f%%) Output Buffer:% 7.2f%c", read_min, read_sec, left_min, left_sec, in_min, in_sec, completed, out_size, unit);
}

static void statistics(void) 
{
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
            *p++ = y;
        }
        return clips;
}

#ifdef SOXMIX
static char *usagestr =
"[ gopts ] [ fopts ] ifile1 [fopts] ifile2 [ fopts ] ofile [ effect [ effopts ] ]";
#else
static char *usagestr =
"[ gopts ] [ fopts ] ifile [ fopts ] ofile [ effect [ effopts ] ]";
#endif

static void usage(char *opt)
{
    int i;

        fprintf(stderr, "%s: ", myname);
        if (verbose || !opt)
                fprintf(stderr, "%s\n\n", st_version());
        fprintf(stderr, "Usage: %s\n\n", usagestr);
        if (opt)
                fprintf(stderr, "Failed: %s\n", opt);
        else {
            fprintf(stderr,"gopts: -e -h -p -q -S -V\n\n");
            fprintf(stderr,"fopts: -r rate -c channels -s/-u/-U/-A/-a/-i/-g/-f -b/-w/-l/-d -v volume -x\n\n");
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
void cleanup(void) 
{
    int i;
    struct stat st;
    char *fn;

    /* Close the input file and outputfile before exiting*/
    for (i = 0; i < input_count; i++)
    {
        if (file_desc[i])
        {
            st_close(file_desc[i]);
            free(file_desc[i]);
        }
    }
    if (writing && file_desc[file_count-1])
    {
        fstat(fileno(file_desc[file_count-1]->fp), &st);
        fn = strdup(file_desc[file_count-1]->filename);
        st_close(file_desc[file_count-1]);

        /* remove the output file because we failed, if it's ours. */
        /* Don't if its not a regular file. */
        if ((st.st_mode & S_IFMT) == S_IFREG)
            unlink(fn);
        free(fn);
        if (file_desc[file_count-1])
            free(file_desc[file_count-1]);
    }
}

static void sigint(int s)
{
    if (s == SIGINT || s == SIGTERM)
    {
        user_abort = 1;
    }
}
