/*
 * SoX - The Swiss Army Knife of Audio Manipulation.
 *
 * This is the main function for the command line sox program.
 *
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * Copyright 1998-2007 Chris Bagwell and SoX contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, write to the Free Software
 * Foundation, Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301,
 * USA.
 */

#include "sox_i.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#ifdef HAVE_IO_H
  #include <io.h>
#endif

#ifdef HAVE_SYS_TIME_H
  #include <sys/time.h>
#endif

#ifdef HAVE_SYS_TIMEB_H
  #include <sys/timeb.h>
#endif

#ifdef HAVE_UNISTD_H
  #include <unistd.h>
#endif

#ifdef HAVE_GETOPT_LONG
  #include <getopt.h>
#else
  #include "getopt.h"
#endif

#ifdef HAVE_GETTIMEOFDAY
  #define TIME_FRAC 1e6
#else
  #define timeval timeb
  #define gettimeofday(a,b) ftime(a)
  #define tv_sec time
  #define tv_usec millitm
  #define TIME_FRAC 1e3
#endif


/* argv[0] options */

static char const * myname = NULL;
static sox_bool play = sox_false, rec = sox_false;


/* gopts */

static enum {sox_sequence, sox_concatenate, sox_mix, sox_merge}
    combine_method = sox_concatenate;
static sox_bool repeatable_random = sox_false;  /* Whether to invoke srand. */
static sox_bool interactive = sox_false;
static sox_bool uservolume = sox_false;
typedef enum {RG_off, RG_track, RG_album} rg_mode;
static rg_mode replay_gain_mode = RG_off;
static sox_option_t show_progress = SOX_OPTION_DEFAULT;


/* Input & output files */

typedef struct file_info
{
  char * filename;

  /* fopts */
  char const * filetype;
  sox_signalinfo_t signal;
  double volume;
  double replay_gain;
  char const * comment;

  sox_format_t * ft;  /* libSoX file descriptor */
  sox_size_t volume_clips;
} * file_t;

#define MAX_INPUT_FILES 32
#define MAX_FILES MAX_INPUT_FILES + 2 /* 1 output file plus record input */
static file_t files[MAX_FILES]; /* Array tracking input and output files */
#define ofile files[file_count - 1]
static size_t file_count = 0;
static size_t input_count = 0;


/* Effects */

/* We parse effects into a temporary effects table and then place into
 * the real effects table.  This makes it easier to reorder some effects
 * as needed.  For instance, we can run a resampling effect before
 * converting a mono file to stereo.  This allows the resample to work
 * on half the data.
 *
 * User effects table must be 4 entries smaller then the real
 * effects table.  This is because at most we will need to add
 * a resample effect, a channel mixing effect, the input, and the output.
 */
#define MAX_USER_EFF (SOX_MAX_EFFECTS - 4)
static sox_effect_t user_efftab[MAX_USER_EFF];
static unsigned nuser_effects;
static sox_effects_chain_t ofile_effects_chain;



/* Flowing */

static sox_signalinfo_t combiner, ofile_signal;
static sox_size_t mixing_clips = 0;
static size_t current_input = 0;
static unsigned long input_wide_samples = 0;
static unsigned long read_wide_samples = 0;
static unsigned long output_samples = 0;
static sox_bool user_abort = sox_false;
static sox_bool user_skip = sox_false;
static int success = 0;

/* local forward declarations */

static sox_bool parse_gopts_and_fopts(file_t, int, char **);
static int process(void);
static void display_status(sox_bool all_done);
static void report_file_info(file_t f);


static void display_SoX_version(FILE * file)
{
  fprintf(file, "%s: SoX v%s\n", myname, PACKAGE_VERSION);
}

static int strcmp_p(const void *p1, const void *p2)
{
  return strcmp(*(const char **)p1, *(const char **)p2);
}

static void display_supported_formats(void)
{
  size_t i, formats;
  const char **format_list;

  printf("SUPPORTED FILE FORMATS:");
  for (i = 0, formats = 0; i < sox_formats; i++) {
    char const * const *names = sox_format_fns[i].fn()->names;
    while (*names++)
      formats++;
  }
  formats += 2;
  format_list = (const char **)xmalloc(formats * sizeof(char *));
  for (i = 0, formats = 0; i < sox_formats; i++) {
    char const * const *names = sox_format_fns[i].fn()->names;
    while (*names)
      format_list[formats++] = *names++;
  }
  format_list[formats++] = "m3u";
  format_list[formats++] = "pls";
  qsort(format_list, formats, sizeof(char *), strcmp_p);
  for (i = 0; i < formats; i++)
    printf(" %s", format_list[i]);
  free(format_list);
  puts("\n");
}

static void display_supported_effects(void)
{
  size_t i;
  const sox_effect_handler_t *e;

  printf("SUPPORTED EFFECTS:");
  for (i = 0; sox_effect_fns[i]; i++) {
    e = sox_effect_fns[i]();
    if (e && e->name && !(e->flags & SOX_EFF_DEPRECATED))
      printf(" %s", e->name);
  }
  puts("\n");
}

static void usage(char const * message)
{
  size_t i;
  static char const * lines[] = {
"SPECIAL FILENAMES:",
"-               stdin (infile) or stdout (outfile)",
"-n              use the null file handler; for use with e.g. synth & stat",
"",
"GLOBAL OPTIONS (gopts) (can be specified at any point before the first effect):",
"--buffer BYTES  set the buffer size (default 8192)",
"--combine concatenate  concatenate multiple input files (default for sox, rec)",
"--combine sequence  sequence multiple input files (default for play)",
"-h, --help      display version number and usage information",
"--help-effect NAME  display usage of specified effect; use `all' to display all",
"--interactive   prompt to overwrite output file",
"-m, --combine mix  mix multiple input files (instead of concatenating)",
"-M, --combine merge  merge multiple input files (instead of concatenating)",
"--plot gnuplot|octave  generate script to plot response of filter effect",
"-q, --no-show-progress  run in quiet mode; opposite of -S",
"--replay-gain track|album|off  default: off (sox, rec), track (play)",
"-R              use default random numbers (same on each run of SoX)",
"-S, --show-progress  display progress while processing audio data",
"--version       display version number of SoX and exit",
"-V[LEVEL]       increment or set verbosity level (default 2); levels are:",
"                  1: failure messages",
"                  2: warnings",
"                  3: details of processing",
"                  4-6: increasing levels of debug messages",
"",
"FORMAT OPTIONS (fopts):",
"Format options only need to be supplied for input files that are headerless,",
"otherwise they are obtained automatically.  Output files will default to the",
"same format options as the input file unless otherwise specified.",
"",
"-c, --channels CHANNELS  number of channels in audio data",
"-C, --compression FACTOR  compression factor for output format",
"--comment TEXT  Specify comment text for the output file",
"--comment-file FILENAME  file containing comment text for the output file",
"--endian little|big|swap  set endianness; swap means opposite to default",
"-r, --rate RATE  sample rate of audio",
"-t, --type FILETYPE  file type of audio",
"-x              invert auto-detected endianness",
"-N, --reverse-nibbles  nibble-order",
"-X, --reverse-bits  bit-order of data",
"-B/-L           force endianness to big/little",
"-s/-u/-U/-A/    sample encoding: signed/unsigned/u-law/A-law",
"  -a/-i/-g/-f   ADPCM/IMA_ADPCM/GSM/floating point",
"-1/-2/-3/-4/-8  sample size in bytes",
"-v, --volume FACTOR  volume input file volume adjustment factor (real number)",
""};

  display_SoX_version(stdout);
  putchar('\n');

  if (message)
    fprintf(stderr, "Failed: %s\n\n", message);  /* N.B. stderr */

  printf("Usage summary: [gopts] [[fopts] infile]... [fopts]%s [effect [effopts]]...\n\n",
         play? "" : " outfile");
  for (i = 0; i < array_length(lines); ++i)
    puts(lines[i]);
  display_supported_formats();
  display_supported_effects();
  printf("effopts: depends on effect\n");
  exit(message != NULL);
}

static void usage_effect(char const * name)
{
  int i;

  display_SoX_version(stdout);
  putchar('\n');

  if (strcmp("all", name) && !sox_find_effect(name)) {
    printf("Cannot find an effect called `%s'.", name);
    display_supported_effects();
  }
  else {
    printf("Effect usage:\n\n");

    for (i = 0; sox_effect_fns[i]; i++) {
      const sox_effect_handler_t *e = sox_effect_fns[i]();
      if (e && e->name && (!strcmp("all", name) || !strcmp(e->name, name)))
        printf("%s %s\n\n", e->name, e->usage? e->usage : "");
    }
  }
  exit(1);
}

static void output_message(unsigned level, const char *filename, const char *fmt, va_list ap)
{
  if (sox_globals.verbosity >= level) {
    fprintf(stderr, "%s ", myname);
    sox_output_message(stderr, filename, fmt, ap);
    fprintf(stderr, "\n");
  }
}

static sox_bool overwrite_permitted(char const * filename)
{
  char c;

  if (!interactive) {
    sox_report("Overwriting `%s'", filename);
    return sox_true;
  }
  sox_warn("Output file `%s' already exists", filename);
  if (!isatty(fileno(stdin)))
    return sox_false;
  do fprintf(stderr, "%s sox: overwrite `%s' (y/n)? ", myname, filename);
  while (scanf(" %c%*[^\n]", &c) != 1 || !strchr("yYnN", c));
  return c == 'y' || c == 'Y';
}

/* Cleanup atexit() function, hence always called. */
static void cleanup(void)
{
  size_t i;

  /* Close the input and output files before exiting. */
  for (i = 0; i < input_count; i++) {
    if (files[i]->ft) {
      sox_close(files[i]->ft);
      free(files[i]->ft);
    }
    free(files[i]);
  }

  if (file_count) {
    if (ofile->ft) {
      if (!(ofile->ft->handler->flags & SOX_FILE_NOSTDIO)) {
        struct stat st;
        fstat(fileno(ofile->ft->fp), &st);

        /* If we didn't succeed and we created an output file, remove it. */
        if (!success && (st.st_mode & S_IFMT) == S_IFREG)
          unlink(ofile->ft->filename);
      }

      /* Assumption: we can unlink a file before sox_closing it. */
      sox_close(ofile->ft);
      free(ofile->ft);
    }
    free(ofile);
  }

  sox_format_quit();
}

static file_t new_file(void)
{
  file_t f = xcalloc(sizeof(*f), 1);

  f->signal.size = -1;
  f->signal.encoding = SOX_ENCODING_UNKNOWN;
  f->signal.channels = 0;
  f->signal.rate = 0;
  f->signal.reverse_bytes = SOX_OPTION_DEFAULT;
  f->signal.reverse_nibbles = SOX_OPTION_DEFAULT;
  f->signal.reverse_bits = SOX_OPTION_DEFAULT;
  f->signal.compression = HUGE_VAL;
  f->volume = HUGE_VAL;
  f->replay_gain = HUGE_VAL;
  f->volume_clips = 0;

  return f;
}

static void set_device(file_t f, sox_bool recording UNUSED)
{
#ifdef HAVE_LIBAO
  if (!recording) {
    f->filetype = "ao";
    f->filename = xstrdup("default");
    return;
  }
#endif
#if defined(HAVE_ALSA)
  f->filetype = "alsa";
  f->filename = xstrdup("default");
#elif defined(HAVE_SYS_SOUNDCARD_H) || defined(HAVE_MACHINE_SOUNDCARD_H)
  f->filetype = "ossdsp";
  f->filename = xstrdup("/dev/dsp");
#elif defined(HAVE_SYS_AUDIOIO_H) || defined(HAVE_SUN_AUDIOIO_H)
  char *device = getenv("AUDIODEV");
  f->filetype = "sunau";
  f->filename = xstrdup(device ? device : "/dev/audio");
#else
  sox_fail("Sorry, there is no default audio device configured");
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

static sox_bool is_playlist(char const * filename)
{
  return strcaseends(filename, ".m3u") || strcaseends(filename, ".pls");
}

static void parse_playlist(const file_t f0, char const * const filename)
{
  sox_bool is_pls = strcaseends(filename, ".pls");
  int comment_char = "#;"[is_pls];
  size_t text_length = 100;
  char * text = xmalloc(text_length + 1);
  char * dirname = xstrdup(filename);
  char * slash_pos = LAST_SLASH(dirname);
  FILE * file = xfopen(filename, "r");
  int c;

  if (!slash_pos)
    *dirname = '\0';
  else
    *slash_pos = '\0';

  if (file == NULL) {
    sox_fail("Can't open playlist file `%s': %s", filename, strerror(errno));
    exit(1);
  }

  do {
    size_t i = 0;
    size_t begin = 0, end = 0;

    while (isspace(c = getc(file)));
    if (c == EOF)
      break;
    while (c != EOF && !strchr("\r\n", c) && c != comment_char) {
      if (i == text_length)
        text = xrealloc(text, (text_length <<= 1) + 1);
      text[i++] = c;
      if (!strchr(" \t\f", c))
        end = i;
      c = getc(file);
    }
    if (ferror(file))
      break;
    if (c == comment_char) {
      do c = getc(file);
      while (c != EOF && !strchr("\r\n", c));
      if (ferror(file))
        break;
    }
    text[end] = '\0';
    if (is_pls) {
      char dummy;
      if (!strncasecmp(text, "file", 4) && sscanf(text + 4, "%*u=%c", &dummy) == 1)
        begin = strchr(text + 5, '=') - text + 1;
      else end = 0;
    }
    if (begin != end) {
      char const * id = text + begin;
      file_t f;

      if (file_count >= MAX_FILES) {
        sox_fail("Too many filenames; maximum is %d input files and 1 output file", MAX_INPUT_FILES);
        exit(1);
      }

      f = new_file();
      *f = *f0;
      if (!dirname[0] || is_uri(id) || IS_ABSOLUTE(id))
        f->filename = xstrdup(id);
      else {
        f->filename = xmalloc(strlen(dirname) + strlen(id) + 2); 
        sprintf(f->filename, "%s/%s", dirname, id); 
      }
      if (is_playlist(f->filename)) {
        parse_playlist(f, f->filename);
        free(f->filename);
        free(f);
        continue;
      }
      files[file_count++] = f;
    }
  } while (c != EOF);
  if (ferror(file)) {
    sox_fail("Error reading playlist file `%s': %s", filename, strerror(errno));
    exit(1);
  }
  fclose(file);
  free(text);
  free(dirname);
}

static void parse_options_and_filenames(int argc, char **argv)
{
  file_t f = NULL;
  struct file_info fi_none;

  while (optind < argc && !sox_find_effect(argv[optind])) {
    f = new_file();
    fi_none = *f;

    if (file_count >= MAX_FILES) {
      sox_fail("Too many filenames; maximum is %d input files and 1 output file", MAX_INPUT_FILES);
      exit(1);
    }

    if (parse_gopts_and_fopts(f, argc, argv)) { /* is null file? */
      if (f->filetype != NULL && strcmp(f->filetype, "null") != 0)
        sox_warn("Ignoring `-t %s'.", f->filetype);
      f->filetype = "null";
      f->filename = xstrdup("-n");
    } else {
      if (optind >= argc || sox_find_effect(argv[optind]))
        break;
      if (is_playlist(argv[optind])) {
        parse_playlist(f, argv[optind++]);
        free(f);
        f = NULL;
        continue;
      }
      f->filename = xstrdup(argv[optind++]);
    }
    files[file_count++] = f;
    f = NULL;
  }

  if (play) {
    if (file_count >= MAX_FILES) {
      sox_fail("Too many filenames; maximum is %d input files and 1 output file", MAX_INPUT_FILES);
      exit(1);
    }

    f = f ? f : new_file();
    set_device(f, sox_false);
    files[file_count++] = f;
  }
  else if (f) {
    if (memcmp(f, &fi_none, sizeof(*f)) != 0) /* fopts but no file */
      usage("missing filename"); /* No return */
    free(f); /* No file opts and no filename, so that's okay */
  }

  if (rec) {
    sox_size_t i;

    if (file_count >= MAX_FILES) {
      sox_fail("Too many filenames; maximum is %d input files and 1 output file", MAX_INPUT_FILES);
      exit(1);
    }

    for (i = file_count; i > 0; i--)
      files[i] = files[i - 1];
    file_count++;

    f = new_file();
    set_device(f, sox_true);
    files[0] = f;
  }
}

static void parse_effects(int argc, char **argv)
{
  for (nuser_effects = 0; optind < argc; ++nuser_effects) {
    sox_effect_t *e = &user_efftab[nuser_effects];
    int i;

    if (nuser_effects >= MAX_USER_EFF) {
      sox_fail("too many effects specified (at most %i allowed)", MAX_USER_EFF);
      exit(1);
    }

    /* Name should always be correct! */
    sox_create_effect(e, sox_find_effect(argv[optind++]));

    for (i = 0; i < argc - optind && !sox_find_effect(argv[optind + i]); ++i);
    if (e->handler.getopts(e, i, &argv[optind]) == SOX_EOF)
      exit(1); /* The failing effect should have displayed an error message */

    optind += i; /* Skip past the effect arguments */

    if (e->handler.flags & SOX_EFF_DEPRECATED)
      sox_warn("Effect `%s' is deprecated; see sox(1) for an alternative", e->handler.name);
  }
}

int main(int argc, char **argv)
{
  size_t i;

  myname = argv[0];
  atexit(cleanup);
  sox_globals.output_message_handler = output_message;

  if (strends(myname, "play")) {
    play = sox_true;
    replay_gain_mode = RG_track;
    combine_method = sox_sequence;
  }
  else if (strends(myname, "rec"))
    rec = sox_true;

  if (sox_format_init() != SOX_SUCCESS)
      exit(1);
 
  parse_options_and_filenames(argc, argv);

  if (sox_globals.verbosity > 2)
    display_SoX_version(stderr);

 
  /* Make sure we got at least the required # of input filenames */
  input_count = file_count ? file_count - 1 : 0;
  if (input_count < (combine_method <= sox_concatenate ? 1 : 2))
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
            "\tuse `vol' to set the output file volume");

  signal(SIGINT, SIG_IGN); /* So child pipes aren't killed by track skip */
  for (i = 0; i < input_count; i++) {
    int j = input_count - 1 - i; /* Open in reverse order 'cos of rec (below) */
    file_t f = files[j];

    /* When mixing audio, default to input side volume adjustments that will
     * make sure no clipping will occur.  Users probably won't be happy with
     * this, and will override it, possibly causing clipping to occur. */
    if (combine_method == sox_mix && !uservolume)
      f->volume = 1.0 / input_count;

    if (rec && !j) { /* Set the recording sample rate & # of channels: */
      if (input_count > 1) {   /* Get them from the next input file: */
        f->signal.rate = files[1]->ft->signal.rate;
        f->signal.channels = files[1]->ft->signal.channels;
      }
      else { /* Get them from the output file (which is not open yet): */
        f->signal.rate = files[1]->signal.rate;
        f->signal.channels = files[1]->signal.channels;
      }
    }
    files[j]->ft = sox_open_read(f->filename, &f->signal, f->filetype);
    if (!files[j]->ft)
      /* sox_open_read() will call sox_warn for most errors.
       * Rely on that printing something. */
      exit(2);
    if (show_progress == SOX_OPTION_DEFAULT &&
        (files[j]->ft->handler->flags & SOX_FILE_DEVICE) != 0 &&
        (files[j]->ft->handler->flags & SOX_FILE_PHONY) == 0)
      show_progress = SOX_OPTION_YES;
    if (files[j]->ft->comment)
      set_replay_gain(files[j]->ft->comment, f);
  }
  signal(SIGINT, SIG_DFL);

  /* Loop through the rest of the arguments looking for effects */
  parse_effects(argc, argv);

  /* Not the greatest way for users to do this perhaps, but they're used
   * to it, so it ought to stay until we replace it with something better. */
  if (!nuser_effects && ofile->filetype && !strcmp(ofile->filetype, "null")) {
    for (i = 0; i < input_count; i++)
      report_file_info(files[i]);
    exit(0);
  }

  for (i = 0; i < input_count; i++) {
    unsigned j;
    for (j =0; j < nuser_effects && !files[i]->ft->signal.channels; ++j)
      files[i]->ft->signal.channels = user_efftab[j].ininfo.channels;
    if (!files[i]->ft->signal.channels)
      ++files[i]->ft->signal.channels;
  }

  if (repeatable_random)
    sox_debug("Not reseeding PRNG; randomness is repeatable");
  else {
    time_t t;

    time(&t);
    srand((unsigned)t);
  }

  ofile_signal = ofile->signal;
  if (combine_method == sox_sequence) do {
    if (ofile->ft)
      sox_close(ofile->ft);
    free(ofile->ft);
  } while (process() != SOX_EOF && !user_abort && current_input < input_count);
  else process();

  while (nuser_effects--)
    user_efftab[nuser_effects].handler.kill(&user_efftab[nuser_effects]);

  for (i = 0; i < file_count; ++i)
    if (files[i]->ft->clips != 0)
      sox_warn(i < input_count?"%s: input clipped %u samples" :
                              "%s: output clipped %u samples; decrease volume?",
          (files[i]->ft->handler->flags & SOX_FILE_DEVICE)?
                       files[i]->ft->handler->names[0] : files[i]->ft->filename,
          files[i]->ft->clips);

  if (mixing_clips > 0)
    sox_warn("mix-combining clipped %u samples; decrease volume?", mixing_clips);

  for (i = 0; i < file_count; i++)
    if (files[i]->volume_clips > 0)
      sox_warn("%s: balancing clipped %u samples; decrease volume?", files[i]->filename,
              files[i]->volume_clips);

  if (show_progress) {
    if (user_abort)
      fprintf(stderr, "Aborted.\n");
    else if (user_skip && !rec)
      fprintf(stderr, "Skipped.\n");
    else
      fprintf(stderr, "Done.\n");
  }

  success = 1; /* Signal success to cleanup so the output file isn't removed. */
  return 0;
}

static char * read_comment_file(char const * const filename)
{
  sox_bool file_error;
  int file_length = 0;
  char * result;
  FILE * file = fopen(filename, "rt");

  if (file == NULL) {
    sox_fail("Cannot open comment file %s", filename);
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
    sox_fail("Error reading comment file %s", filename);
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
    {"buffer"          , required_argument, NULL, 0},
    {"combine"         , required_argument, NULL, 0},
    {"comment-file"    , required_argument, NULL, 0},
    {"comment"         , required_argument, NULL, 0},
    {"endian"          , required_argument, NULL, 0},
    {"interactive"     ,       no_argument, NULL, 0},
    {"help-effect"     , required_argument, NULL, 0},
    {"plot"            , required_argument, NULL, 0},
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
  ENUM_ITEM(sox_,sequence)
  ENUM_ITEM(sox_,concatenate)
  ENUM_ITEM(sox_,mix)
  ENUM_ITEM(sox_,merge)
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

static enum_item const plot_methods[] = {
  ENUM_ITEM(sox_plot_,off)
  ENUM_ITEM(sox_plot_,octave)
  ENUM_ITEM(sox_plot_,gnuplot)
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
    sox_fail("--%s: '%s' is not one of: %s.",
        long_options[option_index].name, optarg, set + 2);
    free(set);
    exit(1);
  }
  return p->value;
}

static sox_bool parse_gopts_and_fopts(file_t f, int argc, char **argv)
{
  while (sox_true) {
    int option_index;
    int i; /* sscanf silently accepts negative numbers for %u :( */
    char dummy;     /* To check for extraneous chars in optarg. */

    switch (getopt_long(argc, argv, getoptstr, long_options, &option_index)) {
    case -1:        /* @ one of: file-name, effect name, end of arg-list. */
      return sox_false; /* i.e. not null file. */

    case 0:         /* Long options with no short equivalent. */
      switch (option_index) {
      case 0:
#define SOX_BUFMIN 16
        if (sscanf(optarg, "%i %c", &i, &dummy) != 1 || i <= SOX_BUFMIN) {
        sox_fail("Buffer size `%s' must be > %d", optarg, SOX_BUFMIN);
        exit(1);
        }
        sox_globals.bufsiz = i;
        break;

      case 1:
        combine_method = enum_option(option_index, combine_methods);
        break;

      case 2:
        f->comment = read_comment_file(optarg);
        break;

      case 3:
        f->comment = xstrdup(optarg);
        break;

      case 4:
        switch (enum_option(option_index, endian_options)) {
          case ENDIAN_little: f->signal.reverse_bytes = SOX_IS_BIGENDIAN; break;
          case ENDIAN_big: f->signal.reverse_bytes = SOX_IS_LITTLEENDIAN; break;
          case ENDIAN_swap: f->signal.reverse_bytes = sox_true; break;
        }
        break;

      case 5:
        interactive = sox_true;
        break;

      case 6:
        usage_effect(optarg);
        break;

      case 7:
        ofile_effects_chain.global_info.plot = enum_option(option_index, plot_methods);
        break;

      case 8:
        replay_gain_mode = enum_option(option_index, rg_modes);
        break;

      case 9:
        display_SoX_version(stdout);
        exit(0);
        break;
      }
      break;

    case 'm':
      combine_method = sox_mix;
      break;

    case 'M':
      combine_method = sox_merge;
      break;

    case 'R':                   /* Useful for regression testing. */
      repeatable_random = sox_true;
      break;

    case 'e': case 'n':
      return sox_true;          /* i.e. is null file. */
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
      if (sscanf(optarg, "%lf %c", &f->signal.rate, &dummy) != 1 || f->signal.rate <= 0) {
        sox_fail("Rate value `%s' is not a positive number", optarg);
        exit(1);
      }
      break;

    case 'v':
      if (sscanf(optarg, "%lf %c", &f->volume, &dummy) != 1) {
        sox_fail("Volume value `%s' is not a number", optarg);
        exit(1);
      }
      uservolume = sox_true;
      if (f->volume < 0.0)
        sox_report("Volume adjustment is negative; "
                  "this will result in a phase change");
      break;

    case 'c':
      if (sscanf(optarg, "%i %c", &i, &dummy) != 1 || i <= 0) {
        sox_fail("Channels value `%s' is not a positive integer", optarg);
        exit(1);
      }
      f->signal.channels = i;
      break;

    case 'C':
      if (sscanf(optarg, "%lf %c", &f->signal.compression, &dummy) != 1) {
        sox_fail("Compression value `%s' is not a number", optarg);
        exit(1);
      }
      break;

    case '1': case 'b': f->signal.size = SOX_SIZE_BYTE;  break;
    case '2': case 'w': f->signal.size = SOX_SIZE_16BIT; break;
    case '3':           f->signal.size = SOX_SIZE_24BIT; break;
    case '4': case 'l': f->signal.size = SOX_SIZE_32BIT; break;
    case '8': case 'd': f->signal.size = SOX_SIZE_64BIT; break;

    case 's': f->signal.encoding = SOX_ENCODING_SIGN2;     break;
    case 'u': f->signal.encoding = SOX_ENCODING_UNSIGNED;  break;
    case 'f': f->signal.encoding = SOX_ENCODING_FLOAT;     break;
    case 'a': f->signal.encoding = SOX_ENCODING_ADPCM;     break;
    case 'D': f->signal.encoding = SOX_ENCODING_MS_ADPCM;  break;
    case 'i': f->signal.encoding = SOX_ENCODING_IMA_ADPCM; break;
    case 'o': f->signal.encoding = SOX_ENCODING_OKI_ADPCM; break;
    case 'g': f->signal.encoding = SOX_ENCODING_GSM;       break;

    case 'U': f->signal.encoding = SOX_ENCODING_ULAW;
      if (f->signal.size == -1)
        f->signal.size = SOX_SIZE_BYTE;
      break;

    case 'A': f->signal.encoding = SOX_ENCODING_ALAW;
      if (f->signal.size == -1)
        f->signal.size = SOX_SIZE_BYTE;
      break;

    case 'L': f->signal.reverse_bytes   = SOX_IS_BIGENDIAN;    break;
    case 'B': f->signal.reverse_bytes   = SOX_IS_LITTLEENDIAN; break;
    case 'x': f->signal.reverse_bytes   = SOX_OPTION_YES;      break;
    case 'X': f->signal.reverse_bits    = SOX_OPTION_YES;      break;
    case 'N': f->signal.reverse_nibbles = SOX_OPTION_YES;      break;

    case 'S': show_progress = SOX_OPTION_YES; break;
    case 'q': show_progress = SOX_OPTION_NO;  break;

    case 'V':
      if (optarg == NULL)
        ++sox_globals.verbosity;
      else {
        if (sscanf(optarg, "%i %c", &i, &dummy) != 1 || i < 0) {
          sox_globals.verbosity = 2;
          sox_fail("Verbosity value `%s' is not a non-negative integer", optarg);
          exit(1);
        }
        sox_globals.verbosity = (unsigned)i;
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

static void display_file_info(file_t f, sox_bool full)
{
  static char const * const no_yes[] = {"no", "yes"};

  fprintf(stderr, "\n%s: '%s'",
    f->ft->mode == 'r'? "Input File     " : "Output File    ", f->ft->filename);
  if (strcmp(f->ft->filename, "-") == 0 || (f->ft->handler->flags & SOX_FILE_DEVICE))
    fprintf(stderr, " (%s)", f->ft->handler->names[0]);
  fprintf(stderr, "\n");

  if (f->ft->signal.size)
    fprintf(stderr, "Sample Size    : %s (%s)\n",
        sox_size_bits_str[f->ft->signal.size],
        sox_sizes_str[f->ft->signal.size]);

  if (f->ft->signal.encoding)
    fprintf(stderr, "Sample Encoding: %s\n",
        sox_encodings_str[f->ft->signal.encoding]);

  fprintf(stderr,
    "Channels       : %u\n"
    "Sample Rate    : %g\n",
    f->ft->signal.channels,
    f->ft->signal.rate);

  if (full) {
    if (f->ft->length && f->ft->signal.channels && f->ft->signal.rate) {
      sox_size_t ws = f->ft->length / f->ft->signal.channels;
      fprintf(stderr,
        "Duration       : %s = %u samples %c %g CDDA sectors\n",
        str_time((double)ws / f->ft->signal.rate),
        ws, "~="[f->ft->signal.rate == 44100],
        (double)ws/ f->ft->signal.rate * 44100 / 588);
    }
    if (f->ft->signal.size > 1)
      fprintf(stderr, "Endian Type    : %s\n",
          f->ft->signal.reverse_bytes != SOX_IS_BIGENDIAN ? "big" : "little");
    if (f->ft->signal.size)
      fprintf(stderr,
        "Reverse Nibbles: %s\n"
        "Reverse Bits   : %s\n",
        no_yes[f->ft->signal.reverse_nibbles],
        no_yes[f->ft->signal.reverse_bits]);
  }

  if (f->replay_gain != HUGE_VAL)
    fprintf(stderr, "Replay gain    : %+g dB\n" , f->replay_gain);
  if (f->volume != HUGE_VAL)
    fprintf(stderr, "Level adjust   : %g (linear gain)\n" , f->volume);

  if (!(f->ft->handler->flags & SOX_FILE_DEVICE) && f->ft->comment) {
    if (strchr(f->ft->comment, '\n'))
      fprintf(stderr, "Comments       : \n%s\n", f->ft->comment);
    else
      fprintf(stderr, "Comment        : '%s'\n", f->ft->comment);
  }
  fprintf(stderr, "\n");
}

static void report_file_info(file_t f)
{
  if (sox_globals.verbosity > 2)
    display_file_info(f, sox_true);
}

static void progress_to_file(file_t f)
{
  if (user_skip) {
    user_skip = sox_false;
    fprintf(stderr, "Skipped.\n");
  }
  read_wide_samples = 0;
  input_wide_samples = f->ft->length / f->ft->signal.channels;
  if (show_progress && (sox_globals.verbosity < 3 ||
                        (combine_method <= sox_concatenate && input_count > 1)))
    display_file_info(f, sox_false);
  if (f->volume == HUGE_VAL)
    f->volume = 1;
  if (f->replay_gain != HUGE_VAL)
    f->volume *= pow(10.0, f->replay_gain / 20);
  f->ft->sox_errno = errno = 0;
}

static sox_bool since(struct timeval * then, double secs, sox_bool always_reset)
{
  sox_bool ret;
  struct timeval now;
  time_t d;
  gettimeofday(&now, NULL);
  d = now.tv_sec - then->tv_sec;
  ret = d > ceil(secs) || now.tv_usec - then->tv_usec + d * TIME_FRAC >= secs * TIME_FRAC;
  if (ret || always_reset)
    *then = now;
  return ret;
}

static void sigint(int s)
{
  static struct timeval then;
  if (input_count > 1 && show_progress && s == SIGINT && combine_method <= sox_concatenate &&
             since(&then, 1.0, sox_true))
    user_skip = sox_true;
  else user_abort = sox_true;
}

static sox_bool can_segue(sox_size_t i)
{
  return
    files[i]->ft->signal.channels == files[i - 1]->ft->signal.channels &&
    files[i]->ft->signal.rate     == files[i - 1]->ft->signal.rate;
}

static sox_size_t sox_read_wide(sox_format_t * ft, sox_ssample_t * buf, sox_size_t max)
{
  sox_size_t len = max / combiner.channels;
  len = sox_read(ft, buf, len * ft->signal.channels) / ft->signal.channels;
  if (!len && ft->sox_errno)
    sox_fail("%s: %s (%s)", ft->filename, ft->sox_errstr, strerror(ft->sox_errno));
  return len;
}

static void balance_input(sox_ssample_t * buf, sox_size_t ws, file_t f)
{
  sox_size_t s = ws * f->ft->signal.channels;

  if (f->volume != 1)
    while (s--) {
      double d = f->volume * *buf;
      *buf++ = SOX_ROUND_CLIP_COUNT(d, f->volume_clips);
    }
}

typedef struct input_combiner
{
  sox_ssample_t *ibuf[MAX_INPUT_FILES];
} * input_combiner_t;

assert_static(sizeof(struct input_combiner) <= SOX_MAX_EFFECT_PRIVSIZE,
              /* else */ input_combiner_PRIVSIZE_too_big);

static int combiner_start(sox_effect_t *effp)
{
  input_combiner_t z = (input_combiner_t) effp->priv;
  sox_size_t ws, i;

  if (combine_method <= sox_concatenate)
    progress_to_file(files[current_input]);
  else {
    ws = 0;
    for (i = 0; i < input_count; i++) {
      z->ibuf[i] = (sox_ssample_t *)xmalloc(sox_globals.bufsiz * sizeof(sox_ssample_t));
      progress_to_file(files[i]);
      ws = max(ws, input_wide_samples);
    }
    input_wide_samples = ws; /* Output length is that of longest input file. */
  }
  return SOX_SUCCESS;
}

static int combiner_drain(sox_effect_t *effp, sox_ssample_t * obuf, sox_size_t * osamp)
{
  input_combiner_t z = (input_combiner_t) effp->priv;
  sox_size_t ws, s, i;
  sox_size_t ilen[MAX_INPUT_FILES];
  sox_size_t olen = 0;

  if (combine_method <= sox_concatenate) while (sox_true) {
    if (!user_skip)
      olen = sox_read_wide(files[current_input]->ft, obuf, *osamp);
    if (olen == 0) {   /* If EOF, go to the next input file. */
      if (++current_input < input_count) {
        if (combine_method == sox_sequence && !can_segue(current_input))
          break;
        progress_to_file(files[current_input]);
        continue;
      }
    }
    balance_input(obuf, olen, files[current_input]);
    break;
  } else {
    sox_ssample_t * p = obuf;
    for (i = 0; i < input_count; ++i) {
      ilen[i] = sox_read_wide(files[i]->ft, z->ibuf[i], *osamp);
      balance_input(z->ibuf[i], ilen[i], files[i]);
      olen = max(olen, ilen[i]);
    }
    for (ws = 0; ws < olen; ++ws) /* wide samples */
      if (combine_method == sox_mix) {          /* sum samples together */
        for (s = 0; s < effp->ininfo.channels; ++s, ++p) {
          *p = 0;
          for (i = 0; i < input_count; ++i)
            if (ws < ilen[i] && s < files[i]->ft->signal.channels) {
              /* Cast to double prevents integer overflow */
              double sample = *p + (double)z->ibuf[i][ws * files[i]->ft->signal.channels + s];
              *p = SOX_ROUND_CLIP_COUNT(sample, mixing_clips);
          }
        }
      } else { /* sox_merge: like a multi-track recorder */
        for (i = 0; i < input_count; ++i)
          for (s = 0; s < files[i]->ft->signal.channels; ++s)
            *p++ = (ws < ilen[i]) * z->ibuf[i][ws * files[i]->ft->signal.channels + s];
    }
  }
  read_wide_samples += olen;
  olen *= effp->ininfo.channels;
  *osamp = olen;
  return olen? SOX_SUCCESS : SOX_EOF;
}

static int combiner_stop(sox_effect_t *effp)
{
  input_combiner_t z = (input_combiner_t) effp->priv;
  sox_size_t i;

  if (combine_method > sox_concatenate)
    /* Free input buffers now that they are not used */
    for (i = 0; i < input_count; i++)
      free(z->ibuf[i]);

  return SOX_SUCCESS;
}

static sox_effect_handler_t const * input_combiner_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "input", 0, SOX_EFF_MCHAN,
    0, combiner_start, 0, combiner_drain, combiner_stop, 0
  };
  return &handler;
}

static int output_flow(sox_effect_t *effp UNUSED, sox_ssample_t const * ibuf,
    sox_ssample_t * obuf UNUSED, sox_size_t * isamp, sox_size_t * osamp)
{
  size_t len;
  for (*osamp = *isamp; *osamp; ibuf += len, *osamp -= len) {
    len = sox_write(ofile->ft, ibuf, *osamp);
    if (len == 0) {
      sox_warn("Error writing: %s", ofile->ft->sox_errstr);
      return SOX_EOF;
    }
    if (user_abort) /* Don't get stuck in this loop. */
      return SOX_EOF;
  }
  output_samples += *isamp / ofile->ft->signal.channels;
  return SOX_SUCCESS;
}

static sox_effect_handler_t const * output_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "output", 0, SOX_EFF_MCHAN, NULL, NULL, output_flow, NULL, NULL, NULL
  };
  return &handler;
}

static void add_auto_effect(sox_effects_chain_t * chain, char const * name, sox_signalinfo_t * signal)
{
  sox_effect_t eff;

  /* Auto effect should always succeed here */
  sox_create_effect(&eff, sox_find_effect(name));
  eff.handler.getopts(&eff, 0, NULL);          /* Set up with default opts */

  /* But could fail here */
  if (sox_add_effect(chain, &eff, signal, &ofile->ft->signal) != SOX_SUCCESS)
    exit(2);
}

/* If needed effects are not given, auto-add at (performance) optimal point. */
static void add_effects(sox_effects_chain_t * chain)
{
  sox_signalinfo_t signal = combiner;
  unsigned i, min_chan = 0, min_rate = 0;
  sox_effect_t eff;

  /* Find points after which we might add effects to change rate/chans */
  for (i = 0; i < nuser_effects; i++) {
    if (user_efftab[i].handler.flags & (SOX_EFF_CHAN|SOX_EFF_MCHAN))
      min_chan = i + 1;
    if (user_efftab[i].handler.flags & SOX_EFF_RATE)
      min_rate = i + 1;
  }
  /* 1st `effect' in the chain is the input combiner */
  sox_create_effect(&eff, input_combiner_effect_fn());
  sox_add_effect(chain, &eff, &signal, &ofile->ft->signal);

  /* Add auto effects if appropriate; add user specified effects */
  for (i = 0; i <= nuser_effects; i++) {
    /* If reducing channels, it's faster to do so before all other effects: */
    if (signal.channels > ofile->ft->signal.channels && i >= min_chan)
      add_auto_effect(chain, "mixer", &signal);

    /* If reducing rate, it's faster to do so before all other effects
     * (except reducing channels): */
    if (signal.rate > ofile->ft->signal.rate && i >= min_rate)
      add_auto_effect(chain, "resample", &signal);

    if (i < nuser_effects)
      if (sox_add_effect(chain, &user_efftab[i], &signal, &ofile->ft->signal) != SOX_SUCCESS)
        exit(2);
  }
  /* Add auto effects if still needed at this point */
  if (signal.rate != ofile->ft->signal.rate)
    add_auto_effect(chain, "resample", &signal);  /* Must be up-sampling */
  if (signal.channels != ofile->ft->signal.channels)
    add_auto_effect(chain, "mixer", &signal);     /* Must be increasing channels */

  /* Last `effect' in the chain is the output file */
  sox_create_effect(&eff, output_effect_fn());
  if (sox_add_effect(chain, &eff, &signal, &ofile->ft->signal) != SOX_SUCCESS)
    exit(2);

  for (i = 0; i < chain->length; ++i) {
    sox_effect_t const * effp = &chain->effects[i][0];
    sox_report("effects chain: %-10s %gHz %u channels %u bits %s",
        effp->handler.name, effp->ininfo.rate, effp->ininfo.channels, effp->ininfo.size * 8,
        (effp->handler.flags & SOX_EFF_MCHAN)? "(multi)" : "");
  }
}

static void optimize_trim(void)          
{
  /* Speed hack.  If the "trim" effect is the first effect then
   * peek inside its "effect descriptor" and see what the
   * start location is.  This has to be done after its start()
   * is called to have the correct location.
   * Also, only do this when only working with one input file.
   * This is because the logic to do it for multiple files is
   * complex and problably never used.
   * This hack is a huge time savings when trimming
   * gigs of audio data into managable chunks
   */ 
  if (input_count == 1 && ofile_effects_chain.length > 1 && strcmp(ofile_effects_chain.effects[1][0].handler.name, "trim") == 0) {
    if ((files[0]->ft->handler->flags & SOX_FILE_SEEK) && files[0]->ft->seekable){
      sox_size_t offset = sox_trim_get_start(&ofile_effects_chain.effects[1][0]);
      if (offset && sox_seek(files[0]->ft, offset, SOX_SEEK_SET) == SOX_SUCCESS) { 
        read_wide_samples = offset / files[0]->ft->signal.channels;
        /* Assuming a failed seek stayed where it was.  If the 
         * seek worked then reset the start location of 
         * trim so that it thinks user didn't request a skip.
         */ 
        sox_trim_clear_start(&ofile_effects_chain.effects[1][0]);
      }    
    }        
  }    
}

static void open_output_file(sox_size_t olen)
{
  sox_loopinfo_t loops[SOX_MAX_NLOOPS];
  double factor;
  int i;
  char const *comment = NULL;

  if (ofile->comment == NULL)
    comment = files[0]->ft->comment ? files[0]->ft->comment : "Processed by SoX";
  else if (*ofile->comment != '\0')
      comment = ofile->comment;

  /*
   * copy loop info, resizing appropriately
   * it's in samples, so # channels don't matter
   * FIXME: This doesn't work for multi-file processing or
   * effects that change file length.
   */
  factor = (double) ofile->signal.rate / combiner.rate;
  for (i = 0; i < SOX_MAX_NLOOPS; i++) {
    loops[i].start = files[0]->ft->loops[i].start * factor;
    loops[i].length = files[0]->ft->loops[i].length * factor;
    loops[i].count = files[0]->ft->loops[i].count;
    loops[i].type = files[0]->ft->loops[i].type;
  }

  ofile->ft = sox_open_write(overwrite_permitted,
                        ofile->filename,
                        &ofile->signal,
                        ofile->filetype,
                        comment,
                        olen,
                        &files[0]->ft->instr,
                        loops);

  if (!ofile->ft)
    /* sox_open_write() will call sox_warn for most errors.
     * Rely on that printing something. */
    exit(2);

  /* When writing to an audio device, auto turn on the
   * progress display to match behavior of ogg123,
   * unless the user requested us not to display anything. */
  if (show_progress == SOX_OPTION_DEFAULT)
    show_progress = (ofile->ft->handler->flags & SOX_FILE_DEVICE) != 0 &&
                    (ofile->ft->handler->flags & SOX_FILE_PHONY) == 0;

  report_file_info(ofile);
}

static int update_status(sox_bool all_done)
{
  display_status(all_done || user_abort);
  return user_abort? SOX_EOF : SOX_SUCCESS;
}

static void sox_stop_effects(sox_effects_chain_t * chain)
{
  sox_size_t e, clips;

  for (e = 0; e < chain->length; ++e)
    if ((clips = sox_stop_effect(chain, e)) != 0)
      sox_warn("%s clipped %u samples; decrease volume?",
          chain->effects[e][0].handler.name, clips);
}

/*
 * Process:   Input(s) -> Balancing -> Combiner -> Effects -> Output
 */

static int process(void) {
  int flowstatus = 0;
  sox_size_t i;
  sox_bool known_length = combine_method != sox_sequence;
  sox_size_t olen = 0;

  combiner = files[current_input]->ft->signal;
  if (combine_method == sox_sequence) {
    if (!current_input) for (i = 0; i < input_count; i++)
      report_file_info(files[i]);
  } else {
    sox_size_t total_channels = 0;
    sox_size_t min_channels = SOX_SIZE_MAX;
    sox_size_t max_channels = 0;
    sox_size_t min_rate = SOX_SIZE_MAX;
    sox_size_t max_rate = 0;

    for (i = 0; i < input_count; i++) { /* Report all inputs, then check */
      report_file_info(files[i]);
      total_channels += files[i]->ft->signal.channels;
      min_channels = min(min_channels, files[i]->ft->signal.channels);
      max_channels = max(max_channels, files[i]->ft->signal.channels);
      min_rate = min(min_rate, files[i]->ft->signal.rate);
      max_rate = max(max_rate, files[i]->ft->signal.rate);
      known_length = known_length && files[i]->ft->length != 0;
      if (combine_method == sox_concatenate)
        olen += files[i]->ft->length;
      else
        olen = max(olen, files[i]->ft->length);
    }
    if (min_rate != max_rate)
      sox_fail("Input files must have the same sample-rate");
    if (min_channels != max_channels) {
      if (combine_method == sox_concatenate) {
        sox_fail("Input files must have the same # channels");
        exit(1);
      } else if (combine_method == sox_mix)
        sox_warn("Input files don't have the same # channels");
    }
    if (min_rate != max_rate)
      exit(1);

    combiner.channels = 
      combine_method == sox_merge? total_channels : max_channels;
  }

  ofile->signal = ofile_signal;
  if (ofile->signal.rate == 0)
    ofile->signal.rate = combiner.rate;
  if (ofile->signal.size == -1)
    ofile->signal.size = combiner.size;
  if (ofile->signal.encoding == SOX_ENCODING_UNKNOWN)
    ofile->signal.encoding = combiner.encoding;
  if (ofile->signal.channels == 0)
    ofile->signal.channels = combiner.channels;

  combiner.rate *= sox_effects_globals.speed;

  for (i = 0; i < nuser_effects; i++)
    known_length = known_length && !(user_efftab[i].handler.flags & SOX_EFF_LENGTH);

  if (!known_length)
    olen = 0;

  open_output_file(olen);

  ofile_effects_chain.global_info = sox_effects_globals;
  add_effects(&ofile_effects_chain);

  optimize_trim();

  signal(SIGINT, sigint);
  /* FIXME: For SIGTERM at least we really should guarantee to stop quickly */
  signal(SIGTERM, sigint); /* Stop gracefully even in extremis */
  
  flowstatus = sox_flow_effects(&ofile_effects_chain, update_status);

  sox_stop_effects(&ofile_effects_chain);
  sox_delete_effects(&ofile_effects_chain);
  return flowstatus;
}

static sox_size_t total_clips(void)
{
  unsigned i;
  sox_size_t clips = 0;
  for (i = 0; i < file_count; ++i)
    clips += files[i]->ft->clips + files[i]->volume_clips;
  return clips + mixing_clips + sox_effects_clips(&ofile_effects_chain);
}

static char const * sigfigs3(sox_size_t number)
{
  static char string[16][10];
  static unsigned n;
  unsigned a, b, c = 2;
  sprintf(string[n = (n+1) & 15], "%#.3g", (double)number);
  if (sscanf(string[n], "%u.%ue%u", &a, &b, &c) == 3)
    a = 100*a + b;
  switch (c%3) {
    case 0: sprintf(string[n], "%u.%02u%c", a/100,a%100, " kMGTPE"[c/3]); break;
    case 1: sprintf(string[n], "%u.%u%c"  , a/10 ,a%10 , " kMGTPE"[c/3]); break;
    case 2: sprintf(string[n], "%u%c"     , a          , " kMGTPE"[c/3]); break;
  }
  return string[n];
}

static char const * sigfigs3p(double percentage)
{
  static char string[16][10];
  static unsigned n;
  sprintf(string[n = (n+1) & 15], "%.1f%%", percentage);
  if (strlen(string[n]) < 5)
    sprintf(string[n], "%.2f%%", percentage);
  else if (strlen(string[n]) > 5)
    sprintf(string[n], "%.0f%%", percentage);
  return string[n];
}

static void display_status(sox_bool all_done)
{
  static struct timeval then;
  if (!show_progress)
    return;
  if (all_done || since(&then, .15, sox_false)) {
    double read_time = (double)read_wide_samples / combiner.rate;
    double left_time = 0, in_time = 0, percentage = 0;

    if (input_wide_samples) {
      in_time = (double)input_wide_samples / combiner.rate;
      left_time = max(in_time - read_time, 0);
      percentage = max(100. * read_wide_samples / input_wide_samples, 0);
    }
    fprintf(stderr, "\rTime: %s [%s] of %s (%-5s) Samples out: %-6sClips: %-5s",
      str_time(read_time), str_time(left_time), str_time(in_time),
      sigfigs3p(percentage), sigfigs3(output_samples), sigfigs3(total_clips()));
  }
  if (all_done)
    fputc('\n', stderr);
}
