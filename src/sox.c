/*
 * SoX - The Swiss Army Knife of Audio Manipulation.
 *
 * This is the main function for the command line sox program.
 *
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * Copyright 1998-2006 Chris Bagnall and SoX contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, write to the Free Software
 * Foundation, Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301,
 * USA.
 */

#include "st_i.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>             /* for unlink() */
#endif

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#else
#include "getopt.h"
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
#define isatty _isatty
#include <io.h>
#endif

static enum {SOX_CONCAT, SOX_MIX, SOX_MERGE} combine_method = SOX_CONCAT;
static st_size_t mixing_clips = 0;
static bool repeatable_random = false;  /* Whether to invoke srand. */
static bool interactive = false;
static st_globalinfo_t globalinfo = {false, 1};
static char uservolume = 0;

static int user_abort = 0;
static int success = 0;

static int quiet = 0;
static int status = 0;
static unsigned long input_samples = 0;
static unsigned long read_samples = 0;
static unsigned long output_samples = 0;

static st_sample_t ibufl[ST_BUFSIZ / 2]; /* Left/right interleave buffers */
static st_sample_t ibufr[ST_BUFSIZ / 2];
static st_sample_t obufl[ST_BUFSIZ / 2];
static st_sample_t obufr[ST_BUFSIZ / 2];

typedef struct file_info
{
  char *filename;
  char *filetype;
  st_signalinfo_t signal;
  double volume;
  char *comment;
  st_size_t volume_clips;
} *file_info_t;

/* local forward declarations */
static bool doopts(file_info_t fo, int, char **);
static void usage(char const *) NORET;
static void usage_effect(char *) NORET;
static void process(void);
static void print_input_status(int input);
static void update_status(void);
static void volumechange(st_sample_t * buf, st_ssize_t len, file_info_t fo);
static void parse_effects(int argc, char **argv);
static void check_effects(void);
static int start_effects(void);
static int flow_effect_out(void);
static int flow_effect(int);
static int drain_effect_out(void);
static int drain_effect(int);
static void stop_effects(void);

#define MAX_INPUT_FILES 32
#define MAX_FILES MAX_INPUT_FILES + 1

/* Arrays tracking input and output files */
static file_info_t file_opts[MAX_FILES];
static ft_t file_desc[MAX_FILES];
static size_t file_count = 0;
static size_t input_count = 0;

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
 * that don't set ST_EFF_MCHAN.
 */

static struct st_effect efftab[MAX_EFF]; /* left/mono channel effects */
static struct st_effect efftabR[MAX_EFF];/* right channel effects */
static int neffects;                     /* # of effects to run on data */
static int input_eff;                    /* last input effect with data */
static int input_eff_eof;                /* has input_eff reached EOF? */

static struct st_effect user_efftab[MAX_USER_EFF];
static int nuser_effects;

static char *myname = NULL;


static void sox_output_message(int level, st_output_message_t m)
{
  if (st_output_verbosity_level >= level) {
    fprintf(stderr, "%s ", myname);
    st_output_message(stderr, m);
    fprintf(stderr, "\n");
  }
}

static bool overwrite_permitted(char const * filename)
{
  char c;

  if (!interactive) {
    st_report("Overwriting '%s'", filename);
    return true;
  }
  st_warn("Output file '%s' already exists", filename);
  if (!isatty(fileno(stdin)))
    return false;
  do fprintf(stderr, "%s sox: overwrite '%s' (y/n)? ", myname, filename);
  while (scanf(" %c%*[^\n]", &c) != 1 || !strchr("yYnN", c));
  return c == 'y' || c == 'Y';
}

/* Cleanup atexit() function, hence always called. */
static void cleanup(void) 
{
  size_t i;
  ft_t ft = file_desc[file_count - 1];

  /* Close the input and output files before exiting. */
  for (i = 0; i < input_count; i++)
    if (file_desc[i]) {
      st_close(file_desc[i]);
      free(file_desc[i]);
    }

  if (ft) {
    if (!(ft->h->flags & ST_FILE_NOSTDIO)) {
      struct stat st;
      fstat(fileno(ft->fp), &st);
    
      /* If we didn't succeed and we created an output file, remove it. */
      if (!success && (st.st_mode & S_IFMT) == S_IFREG)
        unlink(ft->filename);
    }

    /* Assumption: we can unlink a file before st_closing it. */
    st_close(ft);
    free(ft);
  }
}

static void sigint(int s)
{
  if (s == SIGINT || s == SIGTERM)
    user_abort = 1;
}

int main(int argc, char **argv)
{
  size_t i;

  myname = argv[0];
  atexit(cleanup);
  st_output_message_handler = sox_output_message;

  /* Loop over arguments and filenames, stop when an effect name is 
   * found. */
  while (optind < argc && !is_effect_name(argv[optind])) {
    file_info_t fo;
    struct file_info fo_none;

    if (file_count >= MAX_FILES) {
      st_fail("Too many filenames; maximum is %d input files and 1 output file", MAX_INPUT_FILES);
      exit(1);
    }

    fo = xcalloc(sizeof(*fo), 1);
    fo->signal.size = -1;
    fo->signal.encoding = ST_ENCODING_UNKNOWN;
    fo->signal.channels = 0;
    fo->signal.swap_bytes = ST_SWAP_DEFAULT;
    fo->signal.compression = HUGE_VAL;
    fo->volume = HUGE_VAL;
    fo->volume_clips = 0;
    fo_none = *fo;
    
    if (doopts(fo, argc, argv) == true) { /* is null file? */
      if (fo->filetype != NULL && strcmp(fo->filetype, "null") != 0)
        st_warn("Ignoring \"-t %s\".", fo->filetype);
      fo->filetype = "null";
      fo->filename = xstrdup(fo->filetype);
    } else {
      if (optind >= argc || is_effect_name(argv[optind])) {
        if (memcmp(fo, &fo_none, sizeof(fo_none)) != 0) /* fopts but no file */
          usage("missing filename"); /* No return */
        free(fo); /* No file opts and no filename, so that's okay */
        continue;
      }
      fo->filename = xstrdup(argv[optind++]);
    }
    file_opts[file_count++] = fo;
  }

  /* Make sure we got at least the required # of input filenames */
  input_count = file_count ? file_count - 1 : 0;
  if (input_count < (combine_method == SOX_CONCAT ? 1 : 2))
    usage("Not enough input filenames specified");

  /* Check for misplaced input/output-specific options */
  for (i = 0; i < input_count; ++i) {
    if (file_opts[i]->signal.compression != HUGE_VAL)
      usage("A compression factor can only be given for an output file");
    if (file_opts[i]->comment != NULL)
      usage("A comment can only be given for an output file");
  }
  if (file_opts[i]->volume != HUGE_VAL)
    usage("-v can only be given for an input file;\n"
            "\tuse 'vol' to set the output file volume");
  
  for (i = 0; i < input_count; i++) {
    /* When mixing audio, default to input side volume
     * adjustments that will make sure no clipping will
     * occur.  Users most likely won't be happy with
     * this and will want to override it. */
    if (combine_method == SOX_MIX && !uservolume)
      file_opts[i]->volume = 1.0 / input_count;
      
    file_desc[i] = st_open_read(file_opts[i]->filename,
                                &file_opts[i]->signal, 
                                file_opts[i]->filetype);
    if (!file_desc[i])
      /* st_open_read() will call st_warn for most errors.
       * Rely on that printing something. */
      exit(2);
  }
    
  /* Loop through the reset of the arguments looking for effects */
  parse_effects(argc, argv);

  if (repeatable_random)
    st_debug("Not reseeding PRNG; randomness is repeatable");
  else {
    time_t t;

    time(&t);
    srand(t);
  }

  process();

  if (mixing_clips > 0)
    st_warn("-m clipped %u samples; decrease volume?", mixing_clips);

  for (i = 0; i < file_count; i++)
    if (file_opts[i]->volume_clips > 0)
      st_warn("%s: -v clipped %u samples; decrease volume?", file_opts[i]->filename,
              file_opts[i]->volume_clips);

  if (status) {
    if (user_abort)
      fprintf(stderr, "Aborted.\n");
    else
      fprintf(stderr, "Done.\n");
  }

  success = 1; /* Signal success to cleanup so the output file is not
                  removed. */
  
  return 0;
}

static char * read_comment_file(char const * const filename)
{
  bool file_error;
  long file_length;
  char * result;
  FILE * file = fopen(filename, "rt");

  if (file == NULL) {
    st_fail("Cannot open comment file %s", filename);
    exit(1);
  }
  file_error = fseeko(file, 0, SEEK_END);
  if (!file_error) {
    file_length = ftello(file);
    file_error |= file_length < 0;
    if (!file_error) {
      result = xmalloc(file_length + 1);
      rewind(file);
      file_error |= fread(result, file_length, 1, file) != 1;
    }
  }
  if (file_error) {
    st_fail("Error reading comment file %s", filename);
    exit(1);
  }
  fclose(file);

  while (file_length && result[file_length - 1] == '\n')
    --file_length;
  result[file_length] = '\0';
  return result;
}

static char *getoptstr = "+r:v:t:c:C:hsuUAaig1b2w34lf8dxV::SqoenmMRLBX";

static struct option long_options[] =
  {
    {"comment-file"    , required_argument, NULL, 0},
    {"comment"         , required_argument, NULL, 0},
    {"endian"          , required_argument, NULL, 0},
    {"interactive"     ,       no_argument, NULL, 0},
    {"help-effect"     , required_argument, NULL, 0},
    {"version"         ,       no_argument, NULL, 0},

    {"channels"        , required_argument, NULL, 'c'},
    {"compression"     , required_argument, NULL, 'C'},
    {"help"            ,       no_argument, NULL, 'h'},
    {"merge"           ,       no_argument, NULL, 'M'},
    {"mix"             ,       no_argument, NULL, 'm'},
    {"no-show-progress",       no_argument, NULL, 'q'},
    {"octave"          ,       no_argument, NULL, 'o'},
    {"rate"            , required_argument, NULL, 'r'},
    {"reverse-bits"    ,       no_argument, NULL, 'X'},
    {"show-progress"   ,       no_argument, NULL, 'S'},
    {"type"            ,       no_argument, NULL, 't'},
    {"volume"          , required_argument, NULL, 'v'},

    {NULL, 0, NULL, 0}
  };

static bool doopts(file_info_t fo, int argc, char **argv)
{
  while (true) {
    int i;          /* Needed since scanf %u allows negative numbers :( */
    char dummy;     /* To check for extraneous chars in optarg. */
    int option_index;
    int c = getopt_long(argc, argv, getoptstr, long_options, &option_index);

    if (c == -1)    /* No more options. */
      return false; /* Is not null file. */

    switch (c) {
    case 0:       /* Long options with no short equivalent. */
      switch (option_index) {
      case 0:
        fo->comment = read_comment_file(optarg);
        break;

      case 1:
        fo->comment = xstrdup(optarg);
        break;

      case 2:
        if (!strcmp(optarg, "little"))
          fo->signal.swap_bytes = ST_IS_BIGENDIAN;
        else if (!strcmp(optarg, "big"))
          fo->signal.swap_bytes = ST_IS_LITTLEENDIAN;
        else if (!strcmp(optarg, "swap"))
          fo->signal.swap_bytes = true;
        break;

      case 3:
        interactive = true;
        break;

      case 4:
        usage_effect(optarg);
        break;

      case 5:
        printf("%s: v%s\n", myname, st_version());
        exit(0);
        break;
      }
      break;

    case 'm':
      combine_method = SOX_MIX;
      break;

    case 'M':
      combine_method = SOX_MERGE;
      break;

    case 'R': /* Useful for regression testing. */
      repeatable_random = true;
      break;

    case 'e': case 'n':
      return true;            /* Is null file. */

    case 'o':
      globalinfo.octave_plot_effect = true;
      break;

    case 'h': case '?':
      usage((char *) 0);      /* No return */
      break;

    case 't':
      fo->filetype = optarg;
      if (fo->filetype[0] == '.')
        fo->filetype++;
      break;

    case 'r':
      if (sscanf(optarg, "%i %c", &i, &dummy) != 1 || i <= 0) {
        st_fail("Rate value '%s' is not a positive integer", optarg);
        exit(1);
      }
      fo->signal.rate = i;
      break;

    case 'v':
      if (sscanf(optarg, "%lf %c", &fo->volume, &dummy) != 1) {
        st_fail("Volume value '%s' is not a number", optarg);
        exit(1);
      }
      uservolume = 1;
      if (fo->volume < 0.0)
        st_report("Volume adjustment is negative; "
                  "this will result in a phase change");
      break;

    case 'c':
      if (sscanf(optarg, "%i %c", &i, &dummy) != 1 || i <= 0) {
        st_fail("Channels value '%s' is not a positive integer", optarg);
        exit(1);
      }
      fo->signal.channels = i;
      break;

    case 'C':
      if (sscanf(optarg, "%lf %c", &fo->signal.compression, &dummy) != 1) {
        st_fail("Compression value '%s' is not a number", optarg);
        exit(1);
      }
      break;

    case '1': case 'b': fo->signal.size = ST_SIZE_BYTE;   break;
    case '2': case 'w': fo->signal.size = ST_SIZE_WORD;   break;
    case '3':           fo->signal.size = ST_SIZE_24BIT;  break;
    case '4': case 'l': fo->signal.size = ST_SIZE_DWORD;  break;
    case '8': case 'd': fo->signal.size = ST_SIZE_DDWORD; break;

    case 's': fo->signal.encoding = ST_ENCODING_SIGN2;     break;
    case 'u': fo->signal.encoding = ST_ENCODING_UNSIGNED;  break;
    case 'f': fo->signal.encoding = ST_ENCODING_FLOAT;     break;
    case 'a': fo->signal.encoding = ST_ENCODING_ADPCM;     break;
    case 'i': fo->signal.encoding = ST_ENCODING_IMA_ADPCM; break;
    case 'g': fo->signal.encoding = ST_ENCODING_GSM;       break;

    case 'U': fo->signal.encoding = ST_ENCODING_ULAW;
      if (fo->signal.size == -1)
        fo->signal.size = ST_SIZE_BYTE;
      break;

    case 'A': fo->signal.encoding = ST_ENCODING_ALAW;
      if (fo->signal.size == -1)
        fo->signal.size = ST_SIZE_BYTE;
      break;

    case 'L':
      fo->signal.swap_bytes = ST_IS_BIGENDIAN;
      break;

    case 'B':
      fo->signal.swap_bytes = ST_IS_LITTLEENDIAN;
      break;

    case 'x':
      fo->signal.swap_bytes = ST_SWAP_YES;
      break;

    case 'X':
      fo->signal.reverse_bits = true;
      break;

    case 'V':
      if (optarg == NULL)
        ++st_output_verbosity_level;
      else if (sscanf(optarg, "%i %c", &st_output_verbosity_level, &dummy) != 1
          || st_output_verbosity_level < 0) {
        st_output_verbosity_level = 2;
        st_fail("Verbosity value '%s' is not an integer >= 0", optarg);
        exit(1);
      }
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
  if (ft1->signal.rate != ft2->signal.rate)
    return ST_EOF;
  if (ft1->signal.channels != ft2->signal.channels)
    return ST_EOF;

  return ST_SUCCESS;
}

/*
 * Process input file -> effect table -> output file one buffer at a time
 */

static void process(void) {
  int e, flowstatus = ST_SUCCESS;
  size_t current_input = 0;
  st_size_t s, f;
  st_ssize_t ilen[MAX_INPUT_FILES];
  st_sample_t *ibuf[MAX_INPUT_FILES];

  for (f = 0; f < input_count; f++) {
    st_report("Input file %s: using sample rate %lu\n\tsize %s, encoding %s, %d %s, volume %g",
              file_desc[f]->filename, file_desc[f]->signal.rate,
              st_sizes_str[(unsigned char)file_desc[f]->signal.size],
              st_encodings_str[(unsigned char)file_desc[f]->signal.encoding],
              file_desc[f]->signal.channels,
              (file_desc[f]->signal.channels > 1) ? "channels" : "channel",
              file_opts[f]->volume == HUGE_VAL? 1 : file_opts[f]->volume);
    
    if (file_desc[f]->comment)
      st_report("Input file %s: comment \"%s\"",
                file_desc[f]->filename, file_desc[f]->comment);
  }

  for (f = 0; f < input_count; f++) {
    if (combine_method == SOX_MERGE)
      file_desc[f]->signal.channels *= input_count;
    if (f && compare_input(file_desc[0], file_desc[f]) != ST_SUCCESS) {
      st_fail("Input files must have the same rate and # of channels");
      exit(1);
    }
  }
  
  {
    st_loopinfo_t loops[ST_MAX_NLOOPS];
    double factor;
    int i;
    file_info_t info = file_opts[file_count - 1];
    char const *comment = NULL;
    
    if (info->signal.rate == 0)
      info->signal.rate = file_desc[0]->signal.rate;
    if (info->signal.size == -1)
      info->signal.size = file_desc[0]->signal.size;
    if (info->signal.encoding == ST_ENCODING_UNKNOWN)
      info->signal.encoding = file_desc[0]->signal.encoding;
    if (info->signal.channels == 0)
      info->signal.channels = file_desc[0]->signal.channels;
    
    if (info->comment != NULL) {
      if (*info->comment == '\0')
        free(info->comment);
      else
        comment = info->comment;
    } else
      comment = file_desc[0]->comment ? file_desc[0]->comment : "Processed by SoX";
    
    /*
     * copy loop info, resizing appropriately
     * it's in samples, so # channels don't matter
     * FIXME: This doesn't work for multi-file processing or
     * effects that change file length.
     */
    factor = (double) info->signal.rate / (double) 
      file_desc[0]->signal.rate;
    for (i = 0; i < ST_MAX_NLOOPS; i++) {
      loops[i].start = file_desc[0]->loops[i].start * factor;
      loops[i].length = file_desc[0]->loops[i].length * factor;
      loops[i].count = file_desc[0]->loops[i].count;
      loops[i].type = file_desc[0]->loops[i].type;
    }
    
    file_desc[file_count - 1] = 
      st_open_write(overwrite_permitted,
                          info->filename,
                          &info->signal, 
                          info->filetype,
                          comment,
                          &file_desc[0]->instr,
                          loops);
    
    if (!file_desc[file_count - 1])
      /* st_open_write() will call st_warn for most errors.
       * Rely on that printing something. */
      exit(2);
    
    /* When writing to an audio device, auto turn on the
     * status display to match behavior of ogg123 status,
     * unless the user requested us not to display anything. */
    if ((strcmp(file_desc[file_count - 1]->filetype, "alsa") == 0 ||
        strcmp(file_desc[file_count - 1]->filetype, "ossdsp") == 0 ||
         strcmp(file_desc[file_count - 1]->filetype, "sunau") == 0) &&
        !quiet)
      status = 1;

    st_report("Output file %s: using sample rate %lu\n\tsize %s, encoding %s, %d %s",
              file_desc[file_count-1]->filename, 
              file_desc[file_count-1]->signal.rate,
              st_sizes_str[(unsigned char)file_desc[file_count-1]->signal.size],
              st_encodings_str[(unsigned char)file_desc[file_count-1]->signal.encoding],
              file_desc[file_count-1]->signal.channels,
              (file_desc[file_count-1]->signal.channels > 1) ? "channels" : "channel");
    
    if (file_desc[file_count - 1]->comment)
      st_report("Output file: comment \"%s\"", 
                file_desc[file_count - 1]->comment);
  }
  
  /* Adjust the input rate for the speed effect */
  for (f = 0; f < input_count; f++)
    file_desc[f]->signal.rate = file_desc[f]->signal.rate * globalinfo.speed + .5;

  /* build efftab */
  check_effects();

  /* Start all effects */
  flowstatus = start_effects();

  /* Allocate output buffers for effects */
  for (e = 0; e < neffects; e++) {
    efftab[e].obuf = (st_sample_t *)xmalloc(ST_BUFSIZ * sizeof(st_sample_t));
    if (efftabR[e].name)
      efftabR[e].obuf = (st_sample_t *)xmalloc(ST_BUFSIZ * sizeof(st_sample_t));
  }

  if (combine_method != SOX_CONCAT) {
    for (f = 0; f < input_count; f++) {
      st_size_t alloc_size = ST_BUFSIZ * sizeof(st_sample_t);
      /* Treat overall length the same as longest input file. */
      if (file_desc[f]->length > input_samples)
        input_samples = file_desc[f]->length;
      
      if (combine_method == SOX_MERGE) {
        alloc_size /= input_count;
        file_desc[f]->signal.channels /= input_count;
      }
      ibuf[f] = (st_sample_t *)xmalloc(alloc_size);
      
      if (status)
        print_input_status(f);
    }
  } else {
    current_input = 0;
    input_samples = file_desc[current_input]->length;
        
    if (status)
      print_input_status(current_input);
  }
      
  /*
   * Just like errno, we must set st_errno to known values before
   * calling I/O operations.
   */
  for (f = 0; f < file_count; f++)
    file_desc[f]->st_errno = 0;
      
  input_eff = 0;
  input_eff_eof = 0;
      
  /* mark chain as empty */
  for(e = 1; e < neffects; e++)
    efftab[e].odone = efftab[e].olen = 0;
      
  /* If start functions set flowstatus to ST_EOF, skip both flow and
     drain; we have to have this "if" because after flow flowstatus is
     supposed to be ST_EOF, so we can't test that in order to know
     whether to drain. */
  if (flowstatus == 0) {
    /* Run input data through effects and get more until olen == 0 
     * (or ST_EOF) or user-abort.
     */
    signal(SIGINT, sigint);
    signal(SIGTERM, sigint);
    do {
      if (combine_method == SOX_CONCAT) {
        ilen[0] = st_read(file_desc[current_input], efftab[0].obuf, 
                          (st_ssize_t)ST_BUFSIZ);
        if (ilen[0] > ST_BUFSIZ) {
          st_warn("WARNING: Corrupt value of %d!  Assuming 0 bytes read.", ilen);
          ilen[0] = 0;
        }
            
        if (ilen[0] == ST_EOF) {
          efftab[0].olen = 0;
          if (file_desc[current_input]->st_errno)
            fprintf(stderr, file_desc[current_input]->st_errstr);
        } else
          efftab[0].olen = ilen[0];
            
        read_samples += efftab[0].olen;
            
        /* Some file handlers claim 0 bytes instead of returning
         * ST_EOF.  In either case, attempt to go to the next
         * input file.
         */
        if (ilen[0] == ST_EOF || efftab[0].olen == 0) {
          if (current_input < input_count - 1) {
            current_input++;
            input_samples = file_desc[current_input]->length;
            read_samples = 0;
                    
            if (status)
              print_input_status(current_input);
            
            continue;
          }
        }
        volumechange(efftab[0].obuf, efftab[0].olen, file_opts[current_input]);
      } else if (combine_method == SOX_MIX) {
        for (f = 0; f < input_count; f++) {
          ilen[f] = st_read(file_desc[f], ibuf[f], (st_ssize_t)ST_BUFSIZ);
                
          if (ilen[f] == ST_EOF) {
            ilen[f] = 0;
            if (file_desc[f]->st_errno) 
              fprintf(stderr, file_desc[f]->st_errstr);
          }
                
          /* Only count read samples for first file in mix */
          if (f == 0)
            read_samples += efftab[0].olen;
          
          volumechange(ibuf[f], ilen[f], file_opts[f]);
        }
        
        /* FIXME: Should report if the size of the reads are not
         * the same.
         */
        efftab[0].olen = 0;
        for (f = 0; f < input_count; f++)
          if ((st_size_t)ilen[f] > efftab[0].olen)
            efftab[0].olen = ilen[f];
            
        for (s = 0; s < efftab[0].olen; s++) {
          /* Mix data together by summing samples together.
           * It is assumed that input side volume adjustments
           * will take care of any possible overflow.
           * By default, SoX sets the volume adjustment
           * to 1/input_count but the user can override this.
           * They probably will and some clipping will probably
           * occur because of this. */
          for (f = 0; f < input_count; f++) {
            if (f == 0)
              efftab[0].obuf[s] =
                (s<(st_size_t)ilen[f]) ? ibuf[f][s] : 0;
            else if (s < (st_size_t)ilen[f]) {
              /* Cast to double prevents integer overflow */
              double sample = efftab[0].obuf[s] + (double)ibuf[f][s];
              efftab[0].obuf[s] = ST_ROUND_CLIP_COUNT(sample, mixing_clips);
            }
          }
        }
      } else {                 /* combine_method == SOX_MERGE */
        efftab[0].olen = 0;
        for (f = 0; f < input_count; ++f) {
          ilen[f] = st_read(file_desc[f], ibuf[f], ST_BUFSIZ / input_count);
          if (ilen[f] == ST_EOF) {
            ilen[f] = 0;
            if (file_desc[f]->st_errno)
              fprintf(stderr, file_desc[f]->st_errstr);
          }
          if ((st_size_t)ilen[f] > efftab[0].olen)
            efftab[0].olen = ilen[f];
          volumechange(ibuf[f], ilen[f], file_opts[f]);
        }
          
        for (s = 0; s < efftab[0].olen; s++)
          for (f = 0; f < input_count; f++)
            efftab[0].obuf[s * input_count + f] =
              (s < (st_size_t)ilen[f]) * ibuf[f][s];

        read_samples += efftab[0].olen;
        efftab[0].olen *= input_count;
      }

      efftab[0].odone = 0;

      if (efftab[0].olen == 0)
        break;

      flowstatus = flow_effect_out();

      if (status)
        update_status();

      /* Quit reading/writing on user aborts.  This will close
       * the files nicely as if an EOF was reached on read. */
      if (user_abort)
        break;

      /* If there's an error, don't try to write more. */
      if (file_desc[file_count - 1]->st_errno)
        break;
    } while (flowstatus == 0);

    /* Drain the effects; don't write if output is indicating errors. */
    if (file_desc[file_count - 1]->st_errno == 0)
      drain_effect_out();
  }

  if (status)
    fputs("\n\n", stderr);

  if (combine_method != SOX_CONCAT)
    /* Free input buffers now that they are not used */
    for (f = 0; f < input_count; f++)
      free(ibuf[f]);

  /* Free output buffers now that they won't be used */
  for (e = 0; e < neffects; e++) {
    free(efftab[e].obuf);
    free(efftabR[e].obuf);
  }

  /* N.B. more data may be written during stop_effects */
  stop_effects();

  for (f = 0; f < input_count; f++)
    if (file_desc[f]->clippedCount != 0)
      st_warn("%s: input clipped %u samples", file_desc[f]->filename,
              file_desc[f]->clippedCount);

  if (file_desc[f]->clippedCount != 0)
    st_warn("%s: output clipped %u samples; decrease volume?",
            (file_desc[f]->h->flags & ST_FILE_NOFEXT)?
            file_desc[f]->h->names[0] : file_desc[f]->filename,
            file_desc[f]->clippedCount);
}
  
static void parse_effects(int argc, char **argv)
{
  int argc_effect;
  int effect_rc;

  nuser_effects = 0;

  while (optind < argc) {
    if (nuser_effects >= MAX_USER_EFF) {
      st_fail("too many effects specified (at most %d allowed)", MAX_USER_EFF);
      exit(1);
    }

    argc_effect = st_geteffect_opt(&user_efftab[nuser_effects],
                                   argc - optind, &argv[optind]);

    if (argc_effect == ST_EOF) {
      st_fail("Effect `%s' does not exist!", argv[optind]);
      exit(1);
    }

    /* Skip past effect name */
    optind++;
    
    user_efftab[nuser_effects].globalinfo = &globalinfo;
    effect_rc = (*user_efftab[nuser_effects].h->getopts)
      (&user_efftab[nuser_effects], argc_effect, &argv[optind]);
    
    if (effect_rc == ST_EOF) 
      exit(2);

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
  int i, j;
  int needchan = 0, needrate = 0, haschan = 0, hasrate = 0;
  int effects_mask = 0;
  int status;

  needrate = (file_desc[0]->signal.rate != file_desc[file_count-1]->signal.rate);
  needchan = (file_desc[0]->signal.channels != file_desc[file_count-1]->signal.channels);

  for (i = 0; i < nuser_effects; i++) {
    if (user_efftab[i].h->flags & ST_EFF_CHAN)
      haschan++;
    if (user_efftab[i].h->flags & ST_EFF_RATE)
      hasrate++;
  }

  if (haschan > 1) {
    st_fail("Cannot specify multiple effects that modify number of channels");
    exit(2);
  }
  if (hasrate > 1)
    st_report("Cannot specify multiple effects that change sample rate");

  /* --------- add the effects ------------------------ */

  /* efftab[0] is always the input stream and always exists */
  neffects = 1;

  /* If reducing channels then its faster to run all effects     
   * after the avg effect.      
   */   
  if (needchan && !(haschan) &&
      (file_desc[0]->signal.channels > file_desc[file_count-1]->signal.channels))
  { 
      /* Find effect and update initial pointers */
      st_geteffect(&efftab[neffects], "avg");

      /* give default opts for added effects */
      efftab[neffects].globalinfo = &globalinfo;
      status = (* efftab[neffects].h->getopts)(&efftab[neffects], (int)0,
                                               (char **)0);

      if (status == ST_EOF)
          exit(2);

      /* Copy format info to effect table */
      effects_mask = st_updateeffect(&efftab[neffects], 
                                     &file_desc[0]->signal,
                                     &file_desc[file_count-1]->signal,
                                     effects_mask);

      neffects++;
  }

  /* If reducing the number of samples, its faster to run all effects 
   * after the resample effect. 
   */
  if (needrate && !(hasrate) &&
      (file_desc[0]->signal.rate > file_desc[file_count-1]->signal.rate)) 
  {
      st_geteffect(&efftab[neffects], "resample");

      /* set up & give default opts for added effects */ 
      efftab[neffects].globalinfo = &globalinfo;
      status = (* efftab[neffects].h->getopts)(&efftab[neffects], (int)0,
                                               (char **)0);

      if (status == ST_EOF)
          exit(2);

      /* Copy format info to effect table */ 
      effects_mask = st_updateeffect(&efftab[neffects],
                                     &file_desc[0]->signal,
                                     &file_desc[file_count-1]->signal,
                                     effects_mask);

      /* Rate can't handle multiple channels so be sure and
       * account for that.
       */ 
      if (efftab[neffects].ininfo.channels > 1)   
          memcpy(&efftabR[neffects], &efftab[neffects], 
                 sizeof(struct st_effect));

      neffects++;
  } 

  /* Copy over user specified effects into real efftab */
  for (i = 0; i < nuser_effects; i++) {
    memcpy(&efftab[neffects], &user_efftab[i], sizeof(struct st_effect));

    /* Copy format info to effect table */
    effects_mask = st_updateeffect(&efftab[neffects], 
                                   &file_desc[0]->signal,
                                   &file_desc[file_count - 1]->signal, 
                                   effects_mask);
    
    /* If this effect can't handle multiple channels then
     * account for this. */
    if ((efftab[neffects].ininfo.channels > 1) &&
        !(efftab[neffects].h->flags & ST_EFF_MCHAN))
      memcpy(&efftabR[neffects], &efftab[neffects],
             sizeof(struct st_effect));

    neffects++;
  }

  /* If rate effect hasn't been added by now then add it here.
   * Check adding rate before avg because it's faster to run
   * rate on less channels then more.
   */
  if (needrate && !(effects_mask & ST_EFF_RATE)) {
    st_geteffect(&efftab[neffects], "resample");

    /* Set up & give default opts for added effect */
    efftab[neffects].globalinfo = &globalinfo;
    status = (* efftab[neffects].h->getopts)(&efftab[neffects], 0, NULL);

    if (status == ST_EOF)
      exit(2);

    /* Copy format info to effect table */
    effects_mask = st_updateeffect(&efftab[neffects], 
                                   &file_desc[0]->signal,
                                   &file_desc[file_count - 1]->signal, 
                                   effects_mask);

    /* Rate can't handle multiple channels so be sure and
     * account for that. */
    if (efftab[neffects].ininfo.channels > 1)
      memcpy(&efftabR[neffects], &efftab[neffects],
             sizeof(struct st_effect));

    neffects++;
  }

  /* If we haven't added avg effect yet then do it now.
   */
  if (needchan && !(effects_mask & ST_EFF_CHAN)) {
    st_geteffect(&efftab[neffects], "avg");

    /* set up & give default opts for added effects */
    efftab[neffects].globalinfo = &globalinfo;
    status = (* efftab[neffects].h->getopts)(&efftab[neffects], 0, NULL);
    if (status == ST_EOF)
      exit(2);

    /* Copy format info to effect table */
    effects_mask = st_updateeffect(&efftab[neffects], 
                                   &file_desc[0]->signal,
                                   &file_desc[file_count - 1]->signal, 
                                   effects_mask);
    
    neffects++;
  }
}

static int start_effects(void)
{
  int e, ret = ST_SUCCESS;

  for (e = 1; e < neffects; e++) {
    efftab[e].clippedCount = 0;
    if ((ret = (*efftab[e].h->start)(&efftab[e])) == ST_EOF)
      break;
    if (efftabR[e].name) {
      efftabR[e].clippedCount = 0;
      if ((ret = (*efftabR[e].h->start)(&efftabR[e])) != ST_SUCCESS)
        break;
    }
  }

  return ret;
}

static int flow_effect_out(void)
{
  int e, havedata, flowstatus = 0;
  size_t len, total;

  do {
    /* run entire chain BACKWARDS: pull, don't push.*/
    /* this is because buffering system isn't a nice queueing system */
    for (e = neffects - 1; e && e >= input_eff; e--) {
      /* Do not call flow effect on input if it has reported
       * EOF already as that's a waste of time and may
       * do bad things.
       */
      if (e == input_eff && input_eff_eof)
        continue;

      /* flow_effect returns ST_EOF when it will not process
       * any more samples.  This is used to bail out early.
       * Since we are "pulling" data, it is OK that we are not
       * calling any more previous effects since their output
       * would not be looked at anyways.
       */
      flowstatus = flow_effect(e);
      if (flowstatus == ST_EOF) {
        input_eff = e;
        /* Assume next effect hasn't reach EOF yet */
        input_eff_eof = 0;
      }

      /* If this buffer contains more input data then break out
       * of this loop now.  This will allow us to loop back around
       * and reprocess the rest of this input buffer: we finish each
       * effect before moving on to the next, so that each effect
       * starts with an empty output buffer.
       */
      if (efftab[e].odone < efftab[e].olen) {
        st_debug("Breaking out of loop to flush buffer");
        break;
      }
    }

    /* If outputting and output data was generated then write it */
    if (efftab[neffects - 1].olen > efftab[neffects - 1].odone) {
      total = 0;
      do {
        /* Do not do any more writing during user aborts as
         * we may be stuck in an infinite writing loop.
         */
        if (user_abort)
          return ST_EOF;
            
        len = st_write(file_desc[file_count - 1], 
                       &efftab[neffects - 1].obuf[total],
                       efftab[neffects - 1].olen - total);
            
        if (len != efftab[neffects - 1].olen - total || file_desc[file_count - 1]->eof) {
          st_warn("Error writing: %s", file_desc[file_count - 1]->st_errstr);
          return ST_EOF;
        }
        total += len;
      } while (total < efftab[neffects-1].olen);
      output_samples += (total / file_desc[file_count - 1]->signal.channels);
      efftab[neffects-1].odone = efftab[neffects-1].olen = 0;
    } else {
      /* Make it look like everything was consumed */
      output_samples += (efftab[neffects-1].olen / 
                         file_desc[file_count - 1]->signal.channels);
      efftab[neffects-1].odone = efftab[neffects-1].olen = 0;
    }

    /* if stuff still in pipeline, set up to flow effects again */
    /* When all effects have reported ST_EOF then this check will
     * show no more data.
     */
    havedata = 0;
    for (e = neffects - 1; e >= input_eff; e--) {
      /* If odone and olen are the same then this buffer
       * can be reused.
       */
      if (efftab[e].odone == efftab[e].olen)
        efftab[e].odone = efftab[e].olen = 0;
      
      if (efftab[e].odone < efftab[e].olen) {
        /* Only mark that we have more data if a full
         * frame that can be written.
         * FIXME: If this error case happens for the
         * input buffer then the data will be lost and
         * will cause stereo channels to be inversed.
         */
        if ((efftab[e].olen - efftab[e].odone) >= 
            file_desc[file_count - 1]->signal.channels)
          havedata = 1;
        else
          st_warn("Received buffer with incomplete amount of samples.");
        /* Don't break out because other things are being
         * done in loop.
         */
      }
    }

    if (!havedata && input_eff > 0) {
      /* When EOF has been detected, skip to the next input
       * before looking for more data.
       */
      if (input_eff_eof) {
        input_eff++;
        input_eff_eof = 0;
      }

      /* If the input file is not returning data then
       * we must prime the pump using the drain effect.
       * After it's primed, the loop will suck the data
       * through.  Once an input_eff stops reporting samples,
       * we will continue to the next until all are drained.
       */
      while (input_eff < neffects) {
        int rc = drain_effect(input_eff);
        
        if (efftab[input_eff].olen == 0) {
          input_eff++;
          /* Assume next effect hasn't reached EOF yet. */
          input_eff_eof = 0;
        } else {
          havedata = 1;
          input_eff_eof = (rc == ST_EOF) ? 1 : 0;
          break;
        }
      }
    }
  } while (havedata);

  /* If input_eff isn't pointing at fake first entry then there
   * is no need to read any more data from disk.  Return this
   * fact to caller.
   */
  if (input_eff > 0) {
    st_debug("Effect return ST_EOF");
    return ST_EOF;
  }

  return ST_SUCCESS;
}

static int flow_effect(int e)
{
  st_size_t i, done, idone, odone, idonel, odonel, idoner, odoner;
  const st_sample_t *ibuf;
  st_sample_t *obuf;
  int effstatus, effstatusl, effstatusr;

  /* Do not attempt to do any more effect processing during
   * user aborts as we may be stuck in an infinite flow loop.
   */
  if (user_abort)
    return ST_EOF;

  /* I have no input data ? */
  if (efftab[e - 1].odone == efftab[e - 1].olen) {
    st_debug("%s no data to pull to me!", efftab[e].name);
    return 0;
  }

  if (!efftabR[e].name) {
    /* No stereo data, or effect can handle stereo data so
     * run effect over entire buffer.
     */
    idone = efftab[e - 1].olen - efftab[e - 1].odone;
    odone = ST_BUFSIZ - efftab[e].olen;
    st_debug("pre %s idone=%d, odone=%d", efftab[e].name, idone, odone);
    st_debug("pre %s odone1=%d, olen1=%d odone=%d olen=%d", efftab[e].name, efftab[e-1].odone, efftab[e-1].olen, efftab[e].odone, efftab[e].olen); 

    effstatus = (* efftab[e].h->flow)(&efftab[e],
                                      &efftab[e - 1].obuf[efftab[e - 1].odone],
                                      &efftab[e].obuf[efftab[e].olen], 
                                      (st_size_t *)&idone, 
                                      (st_size_t *)&odone);

    efftab[e - 1].odone += idone;
    /* Don't update efftab[e].odone as we didn't consume data */
    efftab[e].olen += odone; 
    st_debug("post %s idone=%d, odone=%d", efftab[e].name, idone, odone); 
    st_debug("post %s odone1=%d, olen1=%d odone=%d olen=%d", efftab[e].name, efftab[e-1].odone, efftab[e-1].olen, efftab[e].odone, efftab[e].olen);

    done = idone + odone;
  } else {
    /* Put stereo data in two separate buffers and run effect
     * on each of them.
     */
    idone = efftab[e - 1].olen - efftab[e - 1].odone;
    odone = ST_BUFSIZ - efftab[e].olen;
    
    ibuf = &efftab[e - 1].obuf[efftab[e - 1].odone];
    for (i = 0; i < idone; i += 2) {
      ibufl[i / 2] = *ibuf++;
      ibufr[i / 2] = *ibuf++;
    }
    
    /* left */
    idonel = (idone + 1) / 2;   /* odd-length logic */
    odonel = odone / 2;
    st_debug("pre %s idone=%d, odone=%d", efftab[e].name, idone, odone);
    st_debug("pre %s odone1=%d, olen1=%d odone=%d olen=%d", efftab[e].name, efftab[e - 1].odone, efftab[e - 1].olen, efftab[e].odone, efftab[e].olen); 
    
    effstatusl = (* efftab[e].h->flow)(&efftab[e],
                                       ibufl, obufl, (st_size_t *)&idonel, 
                                       (st_size_t *)&odonel);
    
    /* right */
    idoner = idone / 2;               /* odd-length logic */
    odoner = odone / 2;
    effstatusr = (* efftabR[e].h->flow)(&efftabR[e],
                                        ibufr, obufr, (st_size_t *)&idoner, 
                                        (st_size_t *)&odoner);
    
    obuf = &efftab[e].obuf[efftab[e].olen];
    /* This loop implies left and right effect will always output
     * the same amount of data.
     */
    for (i = 0; i < odoner; i++) {
      *obuf++ = obufl[i];
      *obuf++ = obufr[i];
    }
    efftab[e-1].odone += idonel + idoner;
    /* Don't zero efftab[e].odone since nothing has been consumed yet */
    efftab[e].olen += odonel + odoner;
    st_debug("post %s idone=%d, odone=%d", efftab[e].name, idone, odone); 
    st_debug("post %s odone1=%d, olen1=%d odone=%d olen=%d", efftab[e].name, efftab[e - 1].odone, efftab[e - 1].olen, efftab[e].odone, efftab[e].olen);
    
    done = idonel + idoner + odonel + odoner;
    
    if (effstatusl)
      effstatus = effstatusl;
    else
      effstatus = effstatusr;
  }
  if (effstatus == ST_EOF)
    return ST_EOF;
  if (done == 0) {
    st_fail("Effect took & gave no samples!");
    exit(2);
  }
  return ST_SUCCESS;
}

static int drain_effect_out(void)
{
  /* Skip past input effect since we know thats not needed */
  if (input_eff == 0) {
    input_eff = 1;
    /* Assuming next effect hasn't reached EOF yet. */
    input_eff_eof = 0;
  }

  /* Try to prime the pump with some data */
  while (input_eff < neffects) {
    int rc = drain_effect(input_eff);
    
    if (efftab[input_eff].olen == 0) {
      input_eff++;
      /* Assuming next effect hasn't reached EOF yet. */
      input_eff_eof = 0;
    } else {
      input_eff_eof = (rc == ST_EOF) ? 1 : 0;
      break;
    }
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
  } else {
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
    for (i = 0; i < olenr; i++) {
      *obuf++ = obufl[i];
      *obuf++ = obufr[i];
    }
    efftab[e].olen = olenl + olenr;
    efftab[e].odone = 0;
  }
  return rc;
}

static void stop_effects(void)
{
  int e;

  for (e = 1; e < neffects; e++) {
    st_size_t clippedCount;
    (*efftab[e].h->stop)(&efftab[e]);
    clippedCount = efftab[e].clippedCount;
    (*efftab[e].h->delete)(&efftab[e]);
    if (efftabR[e].name) {
      (*efftabR[e].h->stop)(&efftabR[e]);
      clippedCount += efftab[e].clippedCount;
      (*efftabR[e].h->delete)(&efftabR[e]);
    }
    if (clippedCount != 0)
      st_warn("%s clipped %u samples; decrease volume?", efftab[e].name,
              clippedCount);
  }
}

static void print_input_status(int input)
{
  fprintf(stderr, "\nInput Filename : %s\n", file_desc[input]->filename);
  fprintf(stderr, "Sample Size    : %s\n", 
          st_size_bits_str[file_desc[input]->signal.size]);
  fprintf(stderr, "Sample Encoding: %s\n", 
          st_encodings_str[file_desc[input]->signal.encoding]);
  fprintf(stderr, "Channels       : %d\n", file_desc[input]->signal.channels);
  fprintf(stderr, "Sample Rate    : %d\n",
          (int)(file_desc[input]->signal.rate / globalinfo.speed + 0.5));

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

  /* Currently, for all sox modes, all input files must have
   * the same sample rate.  So we can always just use the rate
   * of the first input file to compute time. */
  read_time = (double)read_samples / (double)file_desc[0]->signal.rate /
    (double)file_desc[0]->signal.channels;

  read_min = read_time / 60;
  read_sec = (double)read_time - 60.0f * (double)read_min;

  out_size = output_samples / 1000000000.0;
  if (out_size >= 1.0)
    unit = 'G';
  else {
    out_size = output_samples / 1000000.0;
    if (out_size >= 1.0)
      unit = 'M';
    else {
      out_size = output_samples / 1000.0;
      if (out_size >= 1.0)
        unit = 'K';
      else
        unit = ' ';
    }
  }

  if (input_samples) {
    in_time = (double)input_samples / (double)file_desc[0]->signal.rate /
      (double)file_desc[0]->signal.channels;
    left_time = in_time - read_time;
    if (left_time < 0)
      left_time = 0;
    
    completed = ((double)read_samples / (double)input_samples) * 100;
    if (completed < 0)
      completed = 0;
  } else {
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

/* Adjust volume based on value specified by the -v option for this file. */
static void volumechange(st_sample_t * buf, st_ssize_t len, file_info_t fo)
{
  if (fo->volume != HUGE_VAL && fo->volume != 1)
    while (len--) {
      double d = fo->volume * *buf;
      *buf++ = ST_ROUND_CLIP_COUNT(d, fo->volume_clips);
    }
}

static void usage(char const * message)
{
  int i;
  const st_format_t *f;
  const st_effect_t *e;

  printf("%s: ", myname);
  printf("Version %s\n\n", st_version());
  if (message)
    fprintf(stderr, "Failed: %s\n\n", message);
  printf("Usage: [gopts] [fopts] %s outfile [effect [effopts]...]\n\n",
         combine_method == SOX_MIX ? "infile1 [fopts] infile2 [fopts]" : "infile [fopts]");
  printf(
         "Special filenames:\n"
         "\n"
         "-               stdin (infile) or stdout (outfile)\n"
         "-n, -e          use the null file handler; for use with e.g. synth & stat\n"
         "\n"
         "Global options (gopts) (can be specified at any point before the first effect):\n"
         "\n"
         "--force         overwrite output file without first prompting\n"
         "-h, --help      display version number and usage information\n"
         "--help-effect=name\n"
         "                display usage of specified effect.  use 'all' to display all\n"
         "-m, --mix       mix multiple input files (instead of concatenating)\n"
         "-M, --merge     merge multiple input files (instead of concatenating)\n"
         "-o              generate Octave commands to plot response of filter effect\n"
         "-q              run in quiet mode; opposite of -S\n"
         "-S              display status while processing audio data\n"
         "--version       display version number of SoX and exit\n"
         "-V[level]       increment or set verbosity level (default 2); levels are:\n"
         "\n"
         "                  1: failure messages\n"
         "                  2: warnings\n"
         "                  3: details of processing\n"
         "                  4-6: increasing levels of debug messages\n"
         "\n"
         "Format options (fopts):\n"
         "\n"
         "Format options only need to be supplied for input files that are\n"
         "headerless, otherwise they are obtained automatically.\n"
         "Output files will default to the same format options as the input\n"
         "file unless otherwise specified.\n"
         "\n"
         "-c channels     number of channels in audio data\n"
         "-C compression  compression factor for variably compressing output formats\n"
         "--comment text  Specify comment text for the output file\n"
         "--comment-file filename\n"
         "                Specify file containing comment text for the output file\n"
         "-r rate         sample rate of audio\n"
         "-t filetype     file type of audio\n"
         "-x              invert auto-detected endianess of data\n"
         "-s/-u/-U/-A/    sample encoding: signed/unsigned/u-law/A-law\n"
         "  -a/-i/-g/-f   ADPCM/IMA_ADPCM/GSM/floating point\n"
         "-1/-2/-3/-4/-8  sample size in bytes\n"
         "-b/-w/-l/-d     aliases for -1/-2/-4/-8 (byte, word, long, double-long)\n"
         "\n"
         "-v volume       input file volume adjustment factor (real number)\n"
         "-R              use default random numbers (same on each run of SoX)\n"
         "\n");

  printf("Supported file formats: ");
  for (i = 0; st_format_fns[i]; i++) {
    f = st_format_fns[i]();
    if (f && f->names)
      printf("%s ", f->names[0]); /* only print the first name */
  }

  printf("\n\nSupported effects: ");
  for (i = 0; st_effect_fns[i]; i++) {
    e = st_effect_fns[i]();
    if (e && e->name)
      printf("%s ", e->name);
  }

  printf( "\n\neffopts: depends on effect\n\n");
  exit(1);
}

static void usage_effect(char *effect)
{
  int i;
  const st_effect_t *e;

  printf("%s: ", myname);
  printf("v%s\n\n", st_version());

  printf("Effect usage:\n\n");

  for (i = 0; st_effect_fns[i]; i++) {
    e = st_effect_fns[i]();
    if (e && e->name && (!strcmp("all", effect) ||  !strcmp(e->name, effect))) {
      char *p = strstr(e->usage, "Usage: ");
      printf("%s\n\n", p ? p + 7 : e->usage);
    }
  }

  if (!effect)
    printf("see --help-effect=effect for effopts ('all' for effopts of all effects)\n\n");
  exit(1);
}
