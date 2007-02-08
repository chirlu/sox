/*
 * SoX - The Swiss Army Knife of Audio Manipulation.
 *
 * This is the main function for the command line sox program.
 *
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * Copyright 1998-2007 Chris Bagnall and SoX contributors
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
#include <sys/time.h>
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

static st_bool play = st_false, rec = st_false;
static enum {SOX_sequence, SOX_concatenate, SOX_mix, SOX_merge} combine_method = SOX_concatenate;
static st_size_t mixing_clips = 0;
static st_bool repeatable_random = st_false;  /* Whether to invoke srand. */
static st_bool interactive = st_false;
static st_globalinfo_t globalinfo = {st_false, 1};
static st_bool uservolume = st_false;
typedef enum {RG_off, RG_track, RG_album} rg_mode;
static rg_mode replay_gain_mode = RG_off;

static st_bool user_abort = st_false;
static st_bool user_skip = st_false;
static int success = 0;

static st_option_t show_progress = ST_OPTION_DEFAULT;
static unsigned long input_wide_samples = 0;
static unsigned long read_wide_samples = 0;
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
  double replay_gain;
  char *comment;
  st_size_t volume_clips;
  ft_t desc;                              /* stlib file descriptor */
} *file_t;

/* local forward declarations */
static st_bool doopts(file_t, int, char **);
static void usage(char const *) NORET;
static void usage_effect(char *) NORET;
static int process(void);
static void update_status(void);
static void report_file_info(file_t f);
static void parse_effects(int argc, char **argv);
static void build_effects_table(void);
static int start_all_effects(void);
static int flow_effect_out(void);
static int flow_effect(int);
static int drain_effect_out(void);
static int drain_effect(int);
static void stop_effects(void);
static void delete_effects(void);

#define MAX_INPUT_FILES 32
#define MAX_FILES MAX_INPUT_FILES + 2 /* 1 output file plus record input */

static file_t files[MAX_FILES]; /* Array tracking input and output files */
#define ofile files[file_count - 1]
static size_t file_count = 0;
static size_t input_count = 0;
static size_t current_input = 0;
static st_signalinfo_t combiner, ofile_signal;

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

static void sox_output_message(int level, const char *filename, const char *fmt, va_list ap)
{
  if (st_output_verbosity_level >= level) {
    fprintf(stderr, "%s ", myname);
    st_output_message(stderr, filename, fmt, ap);
    fprintf(stderr, "\n");
  }
}

static st_bool overwrite_permitted(char const * filename)
{
  char c;

  if (!interactive) {
    st_report("Overwriting '%s'", filename);
    return st_true;
  }
  st_warn("Output file '%s' already exists", filename);
  if (!isatty(fileno(stdin)))
    return st_false;
  do fprintf(stderr, "%s sox: overwrite '%s' (y/n)? ", myname, filename);
  while (scanf(" %c%*[^\n]", &c) != 1 || !strchr("yYnN", c));
  return c == 'y' || c == 'Y';
}

/* Cleanup atexit() function, hence always called. */
static void cleanup(void)
{
  size_t i;

  /* Close the input and output files before exiting. */
  for (i = 0; i < input_count; i++) {
    if (files[i]->desc) {
      st_close(files[i]->desc);
      free(files[i]->desc);
    }
    free(files[i]);
  }

  if (file_count) {
    if (ofile->desc) {
      if (!(ofile->desc->h->flags & ST_FILE_NOSTDIO)) {
        struct stat st;
        fstat(fileno(ofile->desc->fp), &st);

        /* If we didn't succeed and we created an output file, remove it. */
        if (!success && (st.st_mode & S_IFMT) == S_IFREG)
          unlink(ofile->desc->filename);
      }

      /* Assumption: we can unlink a file before st_closing it. */
      st_close(ofile->desc);
      free(ofile->desc);
    }
    free(ofile);
  }
}

static file_t new_file(void)
{
  file_t f = xcalloc(sizeof(*f), 1);

  f->signal.size = -1;
  f->signal.encoding = ST_ENCODING_UNKNOWN;
  f->signal.channels = 0;
  f->signal.reverse_bytes = ST_OPTION_DEFAULT;
  f->signal.reverse_nibbles = ST_OPTION_DEFAULT;
  f->signal.reverse_bits = ST_OPTION_DEFAULT;
  f->signal.compression = HUGE_VAL;
  f->volume = HUGE_VAL;
  f->replay_gain = HUGE_VAL;
  f->volume_clips = 0;

  return f;
}

static void set_device(file_t f)
{
#if defined(HAVE_ALSA)
  f->filetype = "alsa";
  f->filename = xstrdup("default");
#elif defined(HAVE_OSS)
  f->filetype = "ossdsp";
  f->filename = xstrdup("/dev/dsp");
#elif defined (HAVE_SUN_AUDIO)
  char *device = getenv("AUDIODEV");
  f->filetype = "sunau";
  f->filename = xstrdup(device ? device : "/dev/audio");
#else
  st_fail("Sorry, there is no default audio device configured");
  exit(1);
#endif
}

static void set_replay_gain(char const * comment, file_t f)
{
  rg_mode rg = replay_gain_mode;
  int try = 2; /* Will try to find the other GAIN if preferred one not found */

  if (rg != RG_off) while (try--) {
    char const * p = comment;
    char const * target =
      rg == RG_track? "REPLAYGAIN_TRACK_GAIN=" : "REPLAYGAIN_ALBUM_GAIN=";
    do {
      if (strncasecmp(p, target, strlen(target)) == 0) {
        f->replay_gain = atof(p + strlen(target));
        return;
      }
      while (*p && *p!= '\n') ++p;
      while (*p && strchr("\r\n\t\f ", *p)) ++p;
    } while (*p);
    rg ^= RG_track ^ RG_album;
  }
}

static void parse_options_and_filenames(int argc, char **argv)
{
  file_t f = NULL;
  struct file_info fi_none;

  while (optind < argc && !is_effect_name(argv[optind])) {
    f = new_file();
    fi_none = *f;

    if (file_count >= MAX_FILES) {
      st_fail("Too many filenames; maximum is %d input files and 1 output file", MAX_INPUT_FILES);
      exit(1);
    }

    if (doopts(f, argc, argv)) { /* is null file? */
      if (f->filetype != NULL && strcmp(f->filetype, "null") != 0)
        st_warn("Ignoring '-t %s'.", f->filetype);
      f->filetype = "null";
      f->filename = xstrdup("-n");
    } else {
      if (optind >= argc || is_effect_name(argv[optind]))
        break;
      f->filename = xstrdup(argv[optind++]);
    }
    files[file_count++] = f;
    f = NULL;
  }

  if (play) {
    if (file_count >= MAX_FILES) {
      st_fail("Too many filenames; maximum is %d input files and 1 output file", MAX_INPUT_FILES);
      exit(1);
    }

    f = f? f : new_file();
    set_device(f);
    files[file_count++] = f;
  }
  else if (f) {
    if (memcmp(f, &fi_none, sizeof(*f)) != 0) /* fopts but no file */
      usage("missing filename"); /* No return */
    free(f); /* No file opts and no filename, so that's okay */
  }

  if (rec) {
    st_size_t i;

    if (file_count >= MAX_FILES) {
      st_fail("Too many filenames; maximum is %d input files and 1 output file", MAX_INPUT_FILES);
      exit(1);
    }

    for (i = file_count; i > 0; -- i)
      files[i] = files[i - 1];
    file_count++;

    f = new_file();
    set_device(f);
    files[0] = f;
  }
}

int main(int argc, char **argv)
{
  size_t i;

  myname = argv[0];
  atexit(cleanup);
  st_output_message_handler = sox_output_message;

  i = strlen(myname);
  if (i >= sizeof("play") - 1 &&
      strcmp(myname + i - (sizeof("play") - 1), "play") == 0) {
    play = st_true;
    replay_gain_mode = RG_track;
    combine_method = SOX_sequence;
  } else if (i >= sizeof("rec") - 1 &&
      strcmp(myname + i - (sizeof("rec") - 1), "rec") == 0) {
    rec = st_true;
  }
  parse_options_and_filenames(argc, argv);

  /* Make sure we got at least the required # of input filenames */
  input_count = file_count ? file_count - 1 : 0;
  if (input_count < (combine_method <= SOX_concatenate ? 1 : 2))
    usage("Not enough input filenames specified");

  /* Check for misplaced input/output-specific options */
  for (i = 0; i < input_count; ++i) {
    if (files[i]->signal.compression != HUGE_VAL)
      usage("A compression factor can only be given for an output file");
    if (files[i]->comment != NULL)
      usage("A comment can only be given for an output file");
  }
  if (ofile->volume != HUGE_VAL)
    usage("-v can only be given for an input file;\n"
            "\tuse 'vol' to set the output file volume");

  for (i = 0; i < input_count; i++) {
    int j = input_count - 1 - i; /* Open in reverse order 'cos of rec (below) */
    file_t f = files[j];

    /* When mixing audio, default to input side volume adjustments that will
     * make sure no clipping will occur.  Users probably won't be happy with
     * this, and will override it, possibly causing clipping to occur. */
    if (combine_method == SOX_mix && !uservolume)
      f->volume = 1.0 / input_count;

    if (rec && !j) { /* Set the recording sample rate & # of channels: */
      if (input_count > 1) {   /* Get them from the next input file: */
        f->signal.rate = files[1]->desc->signal.rate;
        f->signal.channels = files[1]->desc->signal.channels;
      }
      else { /* Get them from the output file (which is not open yet): */
        f->signal.rate = files[1]->signal.rate;
        f->signal.channels = files[1]->signal.channels;
      }
    }
    files[j]->desc = st_open_read(f->filename, &f->signal, f->filetype);
    if (!files[j]->desc)
      /* st_open_read() will call st_warn for most errors.
       * Rely on that printing something. */
      exit(2);
    if (show_progress == ST_OPTION_DEFAULT &&
        (files[j]->desc->h->flags & ST_FILE_DEVICE) != 0 &&
        (files[j]->desc->h->flags & ST_FILE_PHONY) == 0)
      show_progress = ST_OPTION_YES;
    if (files[j]->desc->comment)
      set_replay_gain(files[j]->desc->comment, f);
  }

  /* Loop through the rest of the arguments looking for effects */
  parse_effects(argc, argv);

  /* Not the greatest way for users to do this perhaps, but they're used
   * to it, so it ought to stay until we replace it with something better. */
  if (!nuser_effects && ofile->filetype && !strcmp(ofile->filetype, "null")) {
    for (i = 0; i < input_count; i++)
      report_file_info(files[i]);
    exit(0);
  }

  if (repeatable_random)
    st_debug("Not reseeding PRNG; randomness is repeatable");
  else {
    time_t t;

    time(&t);
    srand((unsigned)t);
  }

  ofile_signal = ofile->signal;
  if (combine_method == SOX_sequence) do {
    if (ofile->desc)
      st_close(ofile->desc);
    free(ofile->desc);
  } while (process() != ST_EOF && !user_abort && current_input < input_count);
  else process();

  delete_effects();

  for (i = 0; i < file_count; ++i)
    if (files[i]->desc->clips != 0)
      st_warn(i < input_count?"%s: input clipped %u samples" :
                              "%s: output clipped %u samples; decrease volume?",
          (files[i]->desc->h->flags & ST_FILE_DEVICE)?
                       files[i]->desc->h->names[0] : files[i]->desc->filename,
          files[i]->desc->clips);

  if (mixing_clips > 0)
    st_warn("mix-combining clipped %u samples; decrease volume?", mixing_clips);

  for (i = 0; i < file_count; i++)
    if (files[i]->volume_clips > 0)
      st_warn("%s: balancing clipped %u samples; decrease volume?", files[i]->filename,
              files[i]->volume_clips);

  if (show_progress) {
    if (user_abort)
      fprintf(stderr, "Aborted.\n");
    else
      fprintf(stderr, "Done.\n");
  }

  success = 1; /* Signal success to cleanup so the output file isn't removed. */
  return 0;
}

static char * read_comment_file(char const * const filename)
{
  st_bool file_error;
  int file_length = 0;
  char * result;
  FILE * file = fopen(filename, "rt");

  if (file == NULL) {
    st_fail("Cannot open comment file %s", filename);
    exit(1);
  }
  file_error = fseeko(file, (off_t)0, SEEK_END);
  if (!file_error) {
    file_length = ftello(file);
    file_error |= file_length < 0;
    if (!file_error) {
      result = xmalloc((unsigned)file_length + 1);
      rewind(file);
      file_error |= fread(result, (unsigned)file_length, 1, file) != 1;
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

static char *getoptstr = "+abc:defghilmnoqr:st:uv:wxABC:DLMNRSUV::X12348";

static struct option long_options[] =
  {
    {"combine"         , required_argument, NULL, 0},
    {"comment-file"    , required_argument, NULL, 0},
    {"comment"         , required_argument, NULL, 0},
    {"endian"          , required_argument, NULL, 0},
    {"interactive"     ,       no_argument, NULL, 0},
    {"help-effect"     , required_argument, NULL, 0},
    {"octave"          ,       no_argument, NULL, 0},
    {"replay-gain"     , required_argument, NULL, 0},
    {"version"         ,       no_argument, NULL, 0},

    {"channels"        , required_argument, NULL, 'c'},
    {"compression"     , required_argument, NULL, 'C'},
    {"help"            ,       no_argument, NULL, 'h'},
    {"no-show-progress",       no_argument, NULL, 'q'},
    {"rate"            , required_argument, NULL, 'r'},
    {"reverse-bits"    ,       no_argument, NULL, 'X'},
    {"reverse-nibbles" ,       no_argument, NULL, 'N'},
    {"show-progress"   ,       no_argument, NULL, 'S'},
    {"type"            , required_argument, NULL, 't'},
    {"volume"          , required_argument, NULL, 'v'},

    {NULL, 0, NULL, 0}
  };

static enum_item const combine_methods[] = {
  ENUM_ITEM(SOX_,sequence)
  ENUM_ITEM(SOX_,concatenate)
  ENUM_ITEM(SOX_,mix)
  ENUM_ITEM(SOX_,merge)
  {0, 0}};

static enum_item const rg_modes[] = {
  ENUM_ITEM(RG_,off)
  ENUM_ITEM(RG_,track)
  ENUM_ITEM(RG_,album)
  {0, 0}};

enum {ENDIAN_little, ENDIAN_big, ENDIAN_swap};
static enum_item const endian_options[] = {
  ENUM_ITEM(ENDIAN_,little)
  ENUM_ITEM(ENDIAN_,big)
  ENUM_ITEM(ENDIAN_,swap)
  {0, 0}};

static int enum_option(int option_index, enum_item const * items)
{
  enum_item const * p = find_enum_text(optarg, items);
  if (p == NULL) {
    unsigned len = 1;
    char * set = xmalloc(len);
    *set = 0;
    for (p = items; p->text; ++p) {
      set = xrealloc(set, len += 2 + strlen(p->text));
      strcat(set, ", "); strcat(set, p->text);
    }
    st_fail("--%s: '%s' is not one of: %s.",
        long_options[option_index].name, optarg, set + 2);
    free(set);
    exit(1);
  }
  return p->value;
}

static void optimize_trim(void)          
{
    /* Speed hack.  If the "trim" effect is the first effect then
     * peak inside its "effect descriptor" and see what the
     * start location is.  This has to be done after its start()
     * is called to have the correct location.
     * Also, only do this when only working with one input file.
     * This is because the logic to do it for multiple files is
     * complex and problably never used.
     * This hack is a huge time savings when trimming
     * gigs of audio data into managable chunks
     */ 
    if (input_count == 1 && neffects > 1 &&
        strcmp(efftab[1].name, "trim") == 0)
    {
        if ((files[0]->desc->h->flags & ST_FILE_SEEK) &&
            files[0]->desc->seekable)
        { 
            if (st_seek(files[0]->desc, st_trim_get_start(&efftab[1]), 
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

static st_bool doopts(file_t f, int argc, char **argv)
{
  while (st_true) {
    int option_index;
    int i;          /* Needed since scanf %u allows negative numbers :( */
    char dummy;     /* To check for extraneous chars in optarg. */

    switch (getopt_long(argc, argv, getoptstr, long_options, &option_index)) {
    case -1:        /* @ one of: file-name, effect name, end of arg-list. */
      return st_false; /* I.e. not null file. */

    case 0:         /* Long options with no short equivalent. */
      switch (option_index) {
      case 0:
        combine_method = enum_option(option_index, combine_methods);
        break;

      case 1:
        f->comment = read_comment_file(optarg);
        break;

      case 2:
        f->comment = xstrdup(optarg);
        break;

      case 3:
        switch (enum_option(option_index, endian_options)) {
          case ENDIAN_little: f->signal.reverse_bytes = ST_IS_BIGENDIAN; break;
          case ENDIAN_big: f->signal.reverse_bytes = ST_IS_LITTLEENDIAN; break;
          case ENDIAN_swap: f->signal.reverse_bytes = st_true; break;
        }
        break;

      case 4:
        interactive = st_true;
        break;

      case 5:
        usage_effect(optarg);
        break;

      case 6:
        globalinfo.octave_plot_effect = st_true;
        break;

      case 7:
        replay_gain_mode = enum_option(option_index, rg_modes);
        break;

      case 8:
        printf("%s: v%s\n", myname, PACKAGE_VERSION);
        exit(0);
        break;
      }
      break;

    case 'm':
      combine_method = SOX_mix;
      break;

    case 'M':
      combine_method = SOX_merge;
      break;

    case 'R': /* Useful for regression testing. */
      repeatable_random = st_true;
      break;

    case 'e': case 'n':
      return st_true;  /* I.e. is null file. */
      break;

    case 'h': case '?':
      usage(NULL);              /* No return */
      break;

    case 't':
      f->filetype = optarg;
      if (f->filetype[0] == '.')
        f->filetype++;
      break;

    case 'r':
      if (sscanf(optarg, "%i %c", &i, &dummy) != 1 || i <= 0) {
        st_fail("Rate value '%s' is not a positive integer", optarg);
        exit(1);
      }
      f->signal.rate = i;
      break;

    case 'v':
      if (sscanf(optarg, "%lf %c", &f->volume, &dummy) != 1) {
        st_fail("Volume value '%s' is not a number", optarg);
        exit(1);
      }
      uservolume = st_true;
      if (f->volume < 0.0)
        st_report("Volume adjustment is negative; "
                  "this will result in a phase change");
      break;

    case 'c':
      if (sscanf(optarg, "%i %c", &i, &dummy) != 1 || i <= 0) {
        st_fail("Channels value '%s' is not a positive integer", optarg);
        exit(1);
      }
      f->signal.channels = i;
      break;

    case 'C':
      if (sscanf(optarg, "%lf %c", &f->signal.compression, &dummy) != 1) {
        st_fail("Compression value '%s' is not a number", optarg);
        exit(1);
      }
      break;

    case '1': case 'b': f->signal.size = ST_SIZE_BYTE;   break;
    case '2': case 'w': f->signal.size = ST_SIZE_16BIT;   break;
    case '3':           f->signal.size = ST_SIZE_24BIT;  break;
    case '4': case 'l': f->signal.size = ST_SIZE_32BIT;  break;
    case '8': case 'd': f->signal.size = ST_SIZE_64BIT; break;

    case 's': f->signal.encoding = ST_ENCODING_SIGN2;     break;
    case 'u': f->signal.encoding = ST_ENCODING_UNSIGNED;  break;
    case 'f': f->signal.encoding = ST_ENCODING_FLOAT;     break;
    case 'a': f->signal.encoding = ST_ENCODING_ADPCM;     break;
    case 'D': f->signal.encoding = ST_ENCODING_MS_ADPCM;  break;
    case 'i': f->signal.encoding = ST_ENCODING_IMA_ADPCM; break;
    case 'o': f->signal.encoding = ST_ENCODING_OKI_ADPCM; break;
    case 'g': f->signal.encoding = ST_ENCODING_GSM;       break;

    case 'U': f->signal.encoding = ST_ENCODING_ULAW;
      if (f->signal.size == -1)
        f->signal.size = ST_SIZE_BYTE;
      break;

    case 'A': f->signal.encoding = ST_ENCODING_ALAW;
      if (f->signal.size == -1)
        f->signal.size = ST_SIZE_BYTE;
      break;

    case 'L': f->signal.reverse_bytes   = ST_IS_BIGENDIAN;    break;
    case 'B': f->signal.reverse_bytes   = ST_IS_LITTLEENDIAN; break;
    case 'x': f->signal.reverse_bytes   = ST_OPTION_YES;      break;
    case 'X': f->signal.reverse_bits    = ST_OPTION_YES;      break;
    case 'N': f->signal.reverse_nibbles = ST_OPTION_YES;      break;

    case 'S': show_progress = ST_OPTION_YES; break;
    case 'q': show_progress = ST_OPTION_NO;  break;

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
    }
  }
}

static char const * str_time(double duration)
{
  static char string[16][50];
  static int i;
  int mins = duration / 60;
  i = (i+1) & 15;
  sprintf(string[i], "%02i:%05.2f", mins, duration - mins * 60);
  return string[i];
}

static void display_file_info(file_t f, st_bool full)
{
  static char const * const no_yes[] = {"no", "yes"};

  fprintf(stderr, "\n%s: '%s'",
    f->desc->mode == 'r'? "Input File     " : "Output File    ", f->desc->filename);
  if (strcmp(f->desc->filename, "-") == 0 || (f->desc->h->flags & ST_FILE_DEVICE))
    fprintf(stderr, " (%s)", f->desc->h->names[0]);

  fprintf(stderr, "\n"
    "Sample Size    : %s (%s)\n"
    "Sample Encoding: %s\n"
    "Channels       : %u\n"
    "Sample Rate    : %u\n",
    st_size_bits_str[f->desc->signal.size], st_sizes_str[f->desc->signal.size],
    st_encodings_str[f->desc->signal.encoding],
    f->desc->signal.channels,
    f->desc->signal.rate);

  if (full) {
    if (f->desc->length && f->desc->signal.channels && f->desc->signal.rate) {
      st_size_t ws = f->desc->length / f->desc->signal.channels;
      fprintf(stderr,
        "Duration       : %s = %u samples = %g CDDA sectors\n",
        str_time((double)ws / f->desc->signal.rate),
        ws, (double)ws / 588);
    }
    fprintf(stderr,
      "Endian Type    : %s\n"
      "Reverse Nibbles: %s\n"
      "Reverse Bits   : %s\n",
      f->desc->signal.size == 1? "N/A" :
        f->desc->signal.reverse_bytes != ST_IS_BIGENDIAN ? "big" : "little",
      no_yes[f->desc->signal.reverse_nibbles],
      no_yes[f->desc->signal.reverse_bits]);
  }

  if (f->replay_gain != HUGE_VAL)
    fprintf(stderr, "Replay gain    : %+g dB\n" , f->replay_gain);
  if (f->volume != HUGE_VAL)
    fprintf(stderr, "Level adjust   : %g (linear gain)\n" , f->volume);

  if (!(f->desc->h->flags & ST_FILE_DEVICE) && f->desc->comment) {
    if (strchr(f->desc->comment, '\n'))
      fprintf(stderr, "Comments       : \n%s\n", f->desc->comment);
    else
      fprintf(stderr, "Comment        : '%s'\n", f->desc->comment);
  }
  fprintf(stderr, "\n");
}

static void report_file_info(file_t f)
{
  if (st_output_verbosity_level > 2)
    display_file_info(f, st_true);
}

static void progress_to_file(file_t f)
{
  read_wide_samples = 0;
  input_wide_samples = f->desc->length / f->desc->signal.channels;
  if (show_progress && (st_output_verbosity_level < 3 ||
                        (combine_method <= SOX_concatenate && input_count > 1)))
    display_file_info(f, st_false);
  if (f->volume == HUGE_VAL)
    f->volume = 1;
  if (f->replay_gain != HUGE_VAL)
    f->volume *= pow(10.0, f->replay_gain / 20);
  f->desc->st_errno = errno = 0;
}

static void sigint(int s)
{
  static struct timeval then;
  struct timeval now;
  time_t secs;
  gettimeofday(&now, NULL);
  secs = now.tv_sec - then.tv_sec;
  if (show_progress && s == SIGINT && combine_method <= SOX_concatenate &&
      (secs > 1 || 1000000 * secs + now.tv_usec - then.tv_usec > 999999))
    user_skip = st_true;
  else
    user_abort = st_true;
  then = now;
}

static st_bool can_segue(st_size_t i)
{
  return
    files[i]->desc->signal.channels == files[i - 1]->desc->signal.channels &&
    files[i]->desc->signal.rate     == files[i - 1]->desc->signal.rate;
}

static st_size_t st_read_wide(ft_t desc, st_sample_t * buf)
{
  st_size_t len = ST_BUFSIZ / combiner.channels;
  len = st_read(desc, buf, len * desc->signal.channels) / desc->signal.channels;
  if (!len && desc->st_errno)
    st_fail("%s: %s (%s)", desc->filename, desc->st_errstr, strerror(desc->st_errno));
  return len;
}

static void balance_input(st_sample_t * buf, st_size_t ws, file_t f)
{
  st_size_t s = ws * f->desc->signal.channels;

  if (f->volume != 1)
    while (s--) {
      double d = f->volume * *buf;
      *buf++ = ST_ROUND_CLIP_COUNT(d, f->volume_clips);
    }
}

/*
 * Process:   Input(s) -> Balancing -> Combiner -> Effects -> Output
 */

static int process(void) {
  int e, flowstatus = 0;
  st_size_t ws, s, i;
  st_size_t ilen[MAX_INPUT_FILES];
  st_sample_t *ibuf[MAX_INPUT_FILES];

  combiner = files[current_input]->desc->signal;
  if (combine_method == SOX_sequence) {
    if (!current_input) for (i = 0; i < input_count; i++)
      report_file_info(files[i]);
  } else {
    st_size_t total_channels = 0;
    st_size_t min_channels = ST_SIZE_MAX;
    st_size_t max_channels = 0;
    st_size_t min_rate = ST_SIZE_MAX;
    st_size_t max_rate = 0;

    for (i = 0; i < input_count; i++) { /* Report all inputs, then check */
      report_file_info(files[i]);
      total_channels += files[i]->desc->signal.channels;
      min_channels = min(min_channels, files[i]->desc->signal.channels);
      max_channels = max(max_channels, files[i]->desc->signal.channels);
      min_rate = min(min_rate, files[i]->desc->signal.rate);
      max_rate = max(max_rate, files[i]->desc->signal.rate);
    }
    if (min_rate != max_rate)
      st_fail("Input files must have the same sample-rate");
    if (min_channels != max_channels) {
      if (combine_method == SOX_concatenate) {
        st_fail("Input files must have the same # channels");
        exit(1);
      } else if (combine_method == SOX_mix)
        st_warn("Input files don't have the same # channels");
    }
    if (min_rate != max_rate)
      exit(1);

    combiner.channels = 
      combine_method == SOX_merge? total_channels : max_channels;
  }

  ofile->signal = ofile_signal;
  if (ofile->signal.rate == 0)
    ofile->signal.rate = combiner.rate;
  if (ofile->signal.size == -1)
    ofile->signal.size = combiner.size;
  if (ofile->signal.encoding == ST_ENCODING_UNKNOWN)
    ofile->signal.encoding = combiner.encoding;
  if (ofile->signal.channels == 0)
    ofile->signal.channels = combiner.channels;

  combiner.rate = combiner.rate * globalinfo.speed + .5;

  {
    st_loopinfo_t loops[ST_MAX_NLOOPS];
    double factor;
    int i;
    char const *comment = NULL;

    if (ofile->comment == NULL)
      comment = files[0]->desc->comment ? files[0]->desc->comment : "Processed by SoX";
    else if (*ofile->comment != '\0')
        comment = ofile->comment;

    /*
     * copy loop ofile, resizing appropriately
     * it's in samples, so # channels don't matter
     * FIXME: This doesn't work for multi-file processing or
     * effects that change file length.
     */
    factor = (double) ofile->signal.rate / combiner.rate;
    for (i = 0; i < ST_MAX_NLOOPS; i++) {
      loops[i].start = files[0]->desc->loops[i].start * factor;
      loops[i].length = files[0]->desc->loops[i].length * factor;
      loops[i].count = files[0]->desc->loops[i].count;
      loops[i].type = files[0]->desc->loops[i].type;
    }

    ofile->desc = st_open_write(overwrite_permitted,
                          ofile->filename,
                          &ofile->signal,
                          ofile->filetype,
                          comment,
                          &files[0]->desc->instr,
                          loops);

    if (!ofile->desc)
      /* st_open_write() will call st_warn for most errors.
       * Rely on that printing something. */
      exit(2);

    /* When writing to an audio device, auto turn on the
     * progress display to match behavior of ogg123,
     * unless the user requested us not to display anything. */
    if (show_progress == ST_OPTION_DEFAULT)
      show_progress = (ofile->desc->h->flags & ST_FILE_DEVICE) != 0 &&
                      (ofile->desc->h->flags & ST_FILE_PHONY) == 0;

    report_file_info(ofile);
  }

  build_effects_table();

  if (start_all_effects() != ST_SUCCESS)
    exit(2); /* Failing effect should have displayed an error message */

  /* Allocate output buffers for effects */
  for (e = 0; e < neffects; e++) {
    efftab[e].obuf = (st_sample_t *)xmalloc(ST_BUFSIZ * sizeof(st_sample_t));
    if (efftabR[e].name)
      efftabR[e].obuf = (st_sample_t *)xmalloc(ST_BUFSIZ * sizeof(st_sample_t));
  }

  if (combine_method <= SOX_concatenate)
    progress_to_file(files[current_input]);
  else {
    ws = 0;
    for (i = 0; i < input_count; i++) {
      ibuf[i] = (st_sample_t *)xmalloc(ST_BUFSIZ * sizeof(st_sample_t));
      progress_to_file(files[i]);
      ws = max(ws, input_wide_samples);
    }
    input_wide_samples = ws; /* Output length is that of longest input file. */
  }

  optimize_trim();

  input_eff = 0;
  input_eff_eof = 0;

  /* mark chain as empty */
  for(e = 1; e < neffects; e++)
    efftab[e].odone = efftab[e].olen = 0;

  signal(SIGINT, sigint);
  signal(SIGTERM, sigint);
  /* Run input data through effects until EOF (olen == 0) or user-abort. */
  do {
    efftab[0].olen = 0;
    if (combine_method <= SOX_concatenate) {
      if (user_skip) {
        user_skip = st_false;
        fprintf(stderr, "\nSkipped.");
      } else efftab[0].olen =
        st_read_wide(files[current_input]->desc, efftab[0].obuf);
      if (efftab[0].olen == 0) {   /* If EOF, go to the next input file. */
        if (++current_input < input_count) {
          if (combine_method == SOX_sequence && !can_segue(current_input))
            break;
          progress_to_file(files[current_input]);
          continue;
        }
      }
      balance_input(efftab[0].obuf, efftab[0].olen, files[current_input]);
    } else {
      st_sample_t * p = efftab[0].obuf;
      for (i = 0; i < input_count; ++i) {
        ilen[i] = st_read_wide(files[i]->desc, ibuf[i]);
        balance_input(ibuf[i], ilen[i], files[i]);
        efftab[0].olen = max(efftab[0].olen, ilen[i]);
      }
      for (ws = 0; ws < efftab[0].olen; ++ws) /* wide samples */
        if (combine_method == SOX_mix) {          /* sum samples together */
          for (s = 0; s < combiner.channels; ++s, ++p) {
            *p = 0;
            for (i = 0; i < input_count; ++i)
              if (ws < ilen[i] && s < files[i]->desc->signal.channels) {
                /* Cast to double prevents integer overflow */
                double sample = *p + (double)ibuf[i][ws * files[i]->desc->signal.channels + s];
                *p = ST_ROUND_CLIP_COUNT(sample, mixing_clips);
            }
          }
        } else { /* SOX_merge: like a multi-track recorder */
          for (i = 0; i < input_count; ++i)
            for (s = 0; s < files[i]->desc->signal.channels; ++s)
              *p++ = (ws < ilen[i]) * ibuf[i][ws * files[i]->desc->signal.channels + s];
      }
    }
    if (efftab[0].olen == 0)
      break;

    efftab[0].odone = 0;
    read_wide_samples += efftab[0].olen;
    efftab[0].olen *= combiner.channels;

    flowstatus = flow_effect_out();

    if (show_progress)
      update_status();

    /* Quit reading/writing on user aborts.  This will close
     * the files nicely as if an EOF was reached on read. */
    if (user_abort)
      break;

    /* If there's an error, don't try to write more. */
    if (ofile->desc->st_errno)
      break;
  } while (flowstatus == 0);

  /* Drain the effects; don't write if output is indicating errors. */
  if (ofile->desc->st_errno == 0)
    drain_effect_out();

  if (show_progress)
    fputs("\n\n", stderr);

  if (combine_method > SOX_concatenate)
    /* Free input buffers now that they are not used */
    for (i = 0; i < input_count; i++)
      free(ibuf[i]);

  /* Free output buffers now that they won't be used */
  for (e = 0; e < neffects; e++) {
    free(efftab[e].obuf);
    free(efftabR[e].obuf);
  }

  /* N.B. more data may be written during stop_effects */
  stop_effects();
  return flowstatus;
}

static void parse_effects(int argc, char **argv)
{
  int argc_effect;

  for (nuser_effects = 0; optind < argc; ++nuser_effects) {
    struct st_effect * e = &user_efftab[nuser_effects];
    int (*getopts)(eff_t effp, int argc, char *argv[]);

    if (nuser_effects >= MAX_USER_EFF) {
      st_fail("too many effects specified (at most %i allowed)", MAX_USER_EFF);
      exit(1);
    }

    argc_effect = st_geteffect_opt(e, argc - optind, &argv[optind]);
    if (argc_effect == ST_EOF) {
      st_fail("Effect `%s' does not exist!", argv[optind]);
      exit(1);
    }

    optind++; /* Skip past effect name */
    e->globalinfo = &globalinfo;
    getopts = e->h->getopts?  e->h->getopts : st_effect_nothing_getopts;
    if (getopts(e, argc_effect, &argv[optind]) == ST_EOF)
      exit(2);

    optind += argc_effect; /* Skip past the effect arguments */
  }
}

static void add_effect(int * effects_mask)
{
  struct st_effect * e = &efftab[neffects];

  /* Copy format info to effect table */
  *effects_mask =
    st_updateeffect(e, &combiner, &ofile->desc->signal, *effects_mask);

  /* If this effect can't handle multiple channels then account for this. */
  if (e->ininfo.channels > 1 && !(e->h->flags & ST_EFF_MCHAN))
    memcpy(&efftabR[neffects], e, sizeof(*e));
  else memset(&efftabR[neffects], 0, sizeof(*e));

  ++neffects;
}

static void add_default_effect(char const * name, int * effects_mask)
{
  struct st_effect * e = &efftab[neffects];
  int (*getopts)(eff_t effp, int argc, char *argv[]);

  /* Find effect and update initial pointers */
  st_geteffect(e, name);

  /* Set up & give default opts for added effects */
  e->globalinfo = &globalinfo;
  getopts = e->h->getopts?  e->h->getopts : st_effect_nothing_getopts;
  if (getopts(e, 0, NULL) == ST_EOF)
    exit(2);

  add_effect(effects_mask);
}

/* If needed effects are not given, auto-add at (performance) optimal point.
 */
static void build_effects_table(void)
{
  int i;
  int effects_mask = 0;
  st_bool need_rate = combiner.rate     != ofile->desc->signal.rate;
  st_bool need_chan = combiner.channels != ofile->desc->signal.channels;

  { /* Check if we have to add effects to change rate/chans or if the
       user has specified effects to do this, in which case, check if
       too many rate/channel-changing effects have been specified:     */
    int user_chan_effects = 0, user_rate_effects = 0;

    for (i = 0; i < nuser_effects; i++) {
      if (user_efftab[i].h->flags & ST_EFF_CHAN) {
        need_chan = st_false;
        ++user_chan_effects;
      }
      if (user_efftab[i].h->flags & ST_EFF_RATE) {
        need_rate = st_false;
        ++user_rate_effects;
      }
    }
    if (user_chan_effects > 1) {
      st_fail("Cannot specify multiple effects that change number of channels");
      exit(2);
    }
    if (user_rate_effects > 1)
      st_report("Cannot specify multiple effects that change sample rate");
      /* FIXME: exit here or add comment as to why not */
  }

  /* --------- add the effects ------------------------ */

  /* efftab[0] is always the input stream and always exists */
  neffects = 1;

  /* If reducing channels, it's faster to do so before all other effects: */
  if (need_chan && combiner.channels > ofile->desc->signal.channels) {
    add_default_effect("mixer", &effects_mask);
    need_chan = st_false;
  }
  /* If reducing rate, it's faster to do so before all other effects
   * (except reducing channels): */
  if (need_rate && combiner.rate > ofile->desc->signal.rate) {
    add_default_effect("resample", &effects_mask);
    need_rate = st_false;
  }
  /* Copy user specified effects into the real efftab */
  for (i = 0; i < nuser_effects; i++) {
    memcpy(&efftab[neffects], &user_efftab[i], sizeof(efftab[0]));
    add_effect(&effects_mask);
  }
  /* If rate/channels-changing effects are needed but haven't yet been
   * added, then do it here.  Change rate before channels because it's
   * faster to change rate on a smaller # of channels and # of channels
   * can not be reduced, only increased, at this point. */
  if (need_rate)
    add_default_effect("resample", &effects_mask);
  if (need_chan)
    add_default_effect("mixer", &effects_mask);
}

static int start_all_effects(void)
{
  int i, j, ret = ST_SUCCESS;

  for (i = 1; i < neffects; i++) {
    struct st_effect * e = &efftab[i];
    st_bool is_always_null = (e->h->flags & ST_EFF_NULL) != 0;
    int (*start)(eff_t effp) = e->h->start? e->h->start : st_effect_nothing;

    if (is_always_null)
      st_report("'%s' has no effect (is a proxy effect)", e->name);
    else {
      e->clips = 0;
      ret = start(e);
      if (ret == ST_EFF_NULL)
        st_warn("'%s' has no effect in this configuration", e->name);
      else if (ret != ST_SUCCESS)
        return ST_EOF;
    }
    if (is_always_null || ret == ST_EFF_NULL) { /* remove from the chain */
      int (*delete)(eff_t effp) = e->h->kill? e->h->kill: st_effect_nothing;

      /* No left & right delete as there is no left & right getopts */
      delete(e);
      --neffects;
      for (j = i--; j < neffects; ++j) {
        efftab[j] = efftab[j + 1];
        efftabR[j] = efftabR[j + 1];
      }
    }
    /* No null checks here; the left channel looks after this */
    else if (efftabR[i].name) {
      efftabR[i].clips = 0;
      if (start(&efftabR[i]) != ST_SUCCESS)
        return ST_EOF;
    }
  }
  for (i = 1; i < neffects; ++i) {
    struct st_effect * e = &efftab[i];
    st_report("Effects chain: %-10s %-6s %uHz", e->name,
        e->ininfo.channels < 2 ? "mono" :
        (e->h->flags & ST_EFF_MCHAN)? "multi" : "stereo", e->ininfo.rate);
  }
  return ST_SUCCESS;
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
        st_debug_more("Breaking out of loop to flush buffer");
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

        len = st_write(ofile->desc,
                       &efftab[neffects - 1].obuf[total],
                       efftab[neffects - 1].olen - total);

        if (len != efftab[neffects - 1].olen - total || ofile->desc->eof) {
          st_warn("Error writing: %s", ofile->desc->st_errstr);
          return ST_EOF;
        }
        total += len;
      } while (total < efftab[neffects-1].olen);
      output_samples += (total / ofile->desc->signal.channels);
      efftab[neffects-1].odone = efftab[neffects-1].olen = 0;
    } else {
      /* Make it look like everything was consumed */
      output_samples += (efftab[neffects-1].olen /
                         ofile->desc->signal.channels);
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
            ofile->desc->signal.channels)
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
  int (*flow)(eff_t, st_sample_t const*, st_sample_t*, st_size_t*, st_size_t*) =
    efftab[e].h->flow? efftab[e].h->flow : st_effect_nothing_flow;

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
    st_debug_more("pre %s idone=%d, odone=%d", efftab[e].name, idone, odone);
    st_debug_more("pre %s odone1=%d, olen1=%d odone=%d olen=%d", efftab[e].name, efftab[e-1].odone, efftab[e-1].olen, efftab[e].odone, efftab[e].olen);

    effstatus = flow(&efftab[e],
                     &efftab[e - 1].obuf[efftab[e - 1].odone],
                     &efftab[e].obuf[efftab[e].olen],
                     (st_size_t *)&idone,
                     (st_size_t *)&odone);

    efftab[e - 1].odone += idone;
    /* Don't update efftab[e].odone as we didn't consume data */
    efftab[e].olen += odone;
    st_debug_more("post %s idone=%d, odone=%d", efftab[e].name, idone, odone);
    st_debug_more("post %s odone1=%d, olen1=%d odone=%d olen=%d", efftab[e].name, efftab[e-1].odone, efftab[e-1].olen, efftab[e].odone, efftab[e].olen);

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
    st_debug_more("pre %s idone=%d, odone=%d", efftab[e].name, idone, odone);
    st_debug_more("pre %s odone1=%d, olen1=%d odone=%d olen=%d", efftab[e].name, efftab[e - 1].odone, efftab[e - 1].olen, efftab[e].odone, efftab[e].olen);

    effstatusl = flow(&efftab[e],
                      ibufl, obufl, (st_size_t *)&idonel,
                      (st_size_t *)&odonel);

    /* right */
    idoner = idone / 2;               /* odd-length logic */
    odoner = odone / 2;
    effstatusr = flow(&efftabR[e],
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
    st_debug_more("post %s idone=%d, odone=%d", efftab[e].name, idone, odone);
    st_debug_more("post %s odone1=%d, olen1=%d odone=%d olen=%d", efftab[e].name, efftab[e - 1].odone, efftab[e - 1].olen, efftab[e].odone, efftab[e].olen);

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
  int (*drain)(eff_t effp, st_sample_t *obuf, st_size_t *osamp) =
    efftab[e].h->drain? efftab[e].h->drain : st_effect_nothing_drain;

  if (! efftabR[e].name) {
    efftab[e].olen = ST_BUFSIZ;
    rc = drain(&efftab[e],efftab[e].obuf, &efftab[e].olen);
    efftab[e].odone = 0;
  } else {
    int rc_l, rc_r;

    olen = ST_BUFSIZ;

    /* left */
    olenl = olen/2;
    rc_l = drain(&efftab[e], obufl, (st_size_t *)&olenl);

    /* right */
    olenr = olen/2;
    rc_r = drain(&efftabR[e], obufr, (st_size_t *)&olenr);

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
    st_size_t clips;
    int (*stop)(eff_t effp) =
       efftab[e].h->stop? efftab[e].h->stop : st_effect_nothing;

    stop(&efftab[e]);
    clips = efftab[e].clips;

    if (efftabR[e].name) {
      stop(&efftabR[e]);
      clips += efftab[e].clips;
    }
    if (clips != 0)
      st_warn("'%s' clipped %u samples; decrease volume?",efftab[e].name,clips);
  }
}

static void delete_effects(void)
{
  int e;

  for (e = 1; e < neffects; e++) {
    int (*delete)(eff_t effp) =
       efftab[e].h->kill? efftab[e].h->kill : st_effect_nothing;

    /* No left & right delete as there is no left & right getopts */
    delete(&efftab[e]);
  }
}

static void update_status(void)
{
  double read_time, left_time, in_time;
  float completed;
  double out_size;
  char unit;

  read_time = (double)read_wide_samples / combiner.rate;

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

  if (input_wide_samples) {
    in_time = (double)input_wide_samples / combiner.rate;
    left_time = in_time - read_time;
    if (left_time < 0)
      left_time = 0;

    completed = (double)read_wide_samples / input_wide_samples * 100;
    if (completed < 0)
      completed = 0;
  } else {
    in_time = 0;
    left_time = 0;
    completed = 0;
  }

  fprintf(stderr, "\rTime: %s [%s] of %s (% 5.1f%%) Output Buffer:% 7.2f%c",
      str_time(read_time), str_time(left_time), str_time(in_time),
      completed, out_size, unit);
}

static int strcmp_p(const void *p1, const void *p2)
{
  return strcmp(*(const char **)p1, *(const char **)p2);
}

static void usage(char const *message)
{
  size_t i, formats;
  const char **format_list;
  const st_effect_t *e;

  printf("%s: ", myname);
  printf("SoX Version %s\n\n", PACKAGE_VERSION);
  if (message)
    fprintf(stderr, "Failed: %s\n\n", message);
  printf("Usage summary: [gopts] [[fopts] infile]... [fopts]%s [effect [effopts]]...\n\n",
         play? "" : " outfile");
  printf("SPECIAL FILENAMES:\n"
         "-               stdin (infile) or stdout (outfile)\n"
         "-n              use the null file handler; for use with e.g. synth & stat\n"
         "\n"
         "GLOBAL OPTIONS (gopts) (can be specified at any point before the first effect):\n"
         "--combine concatenate  concatenate multiple input files (default for sox, rec)\n"
         "--combine sequence  sequence multiple input files (default for play)\n"
         "-h, --help      display version number and usage information\n"
         "--help-effect name  display usage of specified effect; use 'all' to display all\n"
         "--interactive   prompt to overwrite output file\n"
         "-m, --combine mix  mix multiple input files (instead of concatenating)\n"
         "-M, --combine merge  merge multiple input files (instead of concatenating)\n"
         "--octave        generate Octave commands to plot response of filter effect\n"
         "-q, --no-show-progress  run in quiet mode; opposite of -S\n"
         "--replay-gain track|album|off  default: off (sox, rec), track (play)\n"
         "-R              use default random numbers (same on each run of SoX)\n"
         "-S, --show-progress  display progress while processing audio data\n"
         "--version       display version number of SoX and exit\n"
         "-V[level]       increment or set verbosity level (default 2); levels are:\n"
         "                  1: failure messages\n"
         "                  2: warnings\n"
         "                  3: details of processing\n"
         "                  4-6: increasing levels of debug messages\n"
         "\n"
         "FORMAT OPTIONS (fopts):\n"
         "Format options only need to be supplied for input files that are headerless,\n"
         "otherwise they are obtained automatically.  Output files will default to the\n"
         "same format options as the input file unless otherwise specified.\n"
         "\n"
         "-c, --channels channels  number of channels in audio data\n"
         "-C compression  compression factor for variably compressing output formats\n"
         "--comment text  Specify comment text for the output file\n"
         "--comment-file filename  file containing comment text for the output file\n"
         "--endian little|big|swap  set endianness; swap means opposite to default\n"
         "-r, --rate rate  sample rate of audio\n"
         "-t, --type filetype  file type of audio\n"
         "-x              invert auto-detected endianness\n"
         "-N, --reverse-nibbles  nibble-order\n"
         "-X, --reverse-bits  bit-order of data\n"
         "-B/-L           force endianness to big/little\n"
         "-s/-u/-U/-A/    sample encoding: signed/unsigned/u-law/A-law\n"
         "  -a/-i/-g/-f   ADPCM/IMA_ADPCM/GSM/floating point\n"
         "-1/-2/-3/-4/-8  sample size in bytes\n"
         "-v, --volume    volume input file volume adjustment factor (real number)\n"
         "\n");

  printf("SUPPORTED FILE FORMATS:");
  for (i = 0, formats = 0; st_format_fns[i]; i++) {
    char const * const *names = st_format_fns[i]()->names;
    while (*names++)
      formats++;
  }
  format_list = (const char **)xmalloc(formats * sizeof(char *));
  for (i = 0, formats = 0; st_format_fns[i]; i++) {
    char const * const *names = st_format_fns[i]()->names;
    while (*names)
      format_list[formats++] = *names++;
  }
  qsort(format_list, formats, sizeof(char *), strcmp_p);
  for (i = 0; i < formats; i++)
    printf(" %s", format_list[i]);
  free(format_list);

  printf("\n\nSUPPORTED EFFECTS:");
  for (i = 0; st_effect_fns[i]; i++) {
    e = st_effect_fns[i]();
    if (e && e->name && !(e->flags & ST_EFF_DEPRECATED))
      printf(" %s", e->name);
  }

  printf("\n\neffopts: depends on effect\n");

  if (message)
    exit(1);
  else
    exit(0);
}

static void usage_effect(char *effect)
{
  int i;
  const st_effect_t *e;

  printf("%s: ", myname);
  printf("v%s\n\n", PACKAGE_VERSION);

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
