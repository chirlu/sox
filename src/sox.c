/* SoX - The Swiss Army Knife of Audio Manipulation.
 *
 * This is the main function for the command line sox program.
 *
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * Copyright 1998-2008 Chris Bagwell and SoX contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "soxconfig.h"
#include "sox.h"
#include "util.h"

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

#ifdef HAVE_SYS_UTSNAME_H
  #include <sys/utsname.h>
#endif

#ifdef HAVE_UNISTD_H
  #include <unistd.h>
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

/*#define INTERACTIVE 1 */
#ifdef INTERACTIVE
#include <unistd.h>
#include <fcntl.h>
#endif

/* We are playing games with getopt aliases so this needs to be included after
 * unistd.h to prevent aliasing OS's version of getopt.
 */
#include "getopt.h"

/* argv[0] options */

static char const * myname = NULL;
static enum {sox_sox, sox_play, sox_rec, sox_soxi} sox_mode;


/* gopts */

static enum {sox_sequence, sox_concatenate, sox_mix, sox_mix_power, sox_merge, sox_multiply}
    combine_method = sox_concatenate;
static enum { sox_single, sox_multiple } output_method = sox_single;
#define is_serial(m) ((m) <= sox_concatenate)
#define is_parallel(m) (!is_serial(m))
static sox_bool interactive = sox_false;
static sox_bool uservolume = sox_false;
typedef enum {RG_off, RG_track, RG_album} rg_mode;
static lsx_enum_item const rg_modes[] = {
  LSX_ENUM_ITEM(RG_,off)
  LSX_ENUM_ITEM(RG_,track)
  LSX_ENUM_ITEM(RG_,album)
  {0, 0}};
static rg_mode replay_gain_mode = RG_off;
static sox_option_t show_progress = SOX_OPTION_DEFAULT;


/* Input & output files */

typedef struct {
  char * filename;

  /* fopts */
  char const * filetype;
  sox_signalinfo_t signal;
  sox_encodinginfo_t encoding;
  double volume;
  double replay_gain;
  sox_oob_t oob;

  sox_format_t * ft;  /* libSoX file descriptor */
  size_t volume_clips;
  rg_mode replay_gain_mode;
} file_t;

static file_t * * files; /* Array tracking input and output files */
#define ofile files[file_count - 1]
static size_t file_count = 0;
static size_t input_count = 0;
static size_t output_count = 0;

/* Effects */

/* We parse effects into a temporary effects table and then place into
 * the real effects chain.  This allows scanning all effects to give
 * hints to what input effect options should be as well as determining
 * when mixer or resample effects need to be auto-inserted as well.
 *
 * User effects table must be 4 entries smaller then the real
 * effects table.  This is because at most we will need to add
 * the input effect, an optional resample effect, an optional mixer
 * effect, and the output effect.
 */
#define MAX_USER_EFF (SOX_MAX_EFFECTS - 4)
static sox_effect_t *user_efftab[MAX_USER_EFF];
static sox_effects_chain_t *effects_chain = NULL;
static sox_effect_t *save_output_eff = NULL;

static struct { char *name; int argc; char *argv[FILENAME_MAX]; } (*user_effargs)[MAX_USER_EFF] = NULL;
static unsigned *nuser_effects = NULL;
static int current_eff_chain = 0;
static int eff_chain_count = 0;
static char *effects_filename = NULL;

/* Flowing */

static sox_signalinfo_t combiner_signal, ofile_signal_options;
static sox_encodinginfo_t combiner_encoding, ofile_encoding_options;
static size_t mixing_clips = 0;
static size_t current_input = 0;
static unsigned long input_wide_samples = 0;
static unsigned long read_wide_samples = 0;
static unsigned long output_samples = 0;
static sox_bool input_eof = sox_false;
static sox_bool output_eof = sox_false;
static sox_bool user_abort = sox_false;
static sox_bool user_skip = sox_false;
static sox_bool user_restart_eff = sox_false;
static int success = 0;
static sox_sample_t omax[2], omin[2];


/* Cleanup atexit() function, hence always called. */
static void cleanup(void)
{
  size_t i;

  /* Close the input and output files before exiting. */
  for (i = 0; i < input_count; i++) {
    if (files[i]->ft) {
      sox_close(files[i]->ft);
    }
    free(files[i]);
  }

  if (file_count) {
    if (ofile->ft) {                  
      if (!success && ofile->ft->fp) {   /* If we failed part way through */
        struct stat st;                  /* writing a normal file, remove it. */
        fstat(fileno(ofile->ft->fp), &st);
        if ((st.st_mode & S_IFMT) == S_IFREG)
          unlink(ofile->ft->filename);
      }
      sox_close(ofile->ft); /* Assume we can unlink a file before closing it. */
    }
    free(ofile);
  }

  sox_format_quit();
}

static char const * str_time(double seconds)
{
  static char string[16][50];
  static int i;
  int hours, mins = seconds / 60;
  seconds -= mins * 60;
  hours = mins / 60;
  mins -= hours * 60;
  i = (i+1) & 15;
  sprintf(string[i], "%02i:%02i:%05.2f", hours, mins, seconds);
  return string[i];
}

static void play_file_info(sox_format_t * ft, file_t * f, sox_bool full)
{
  FILE * const output = sox_mode == sox_soxi? stdout : stderr;
  char const * text;
  char buffer[30];
  size_t ws = ft->signal.length / ft->signal.channels;
  (void)full;

  fprintf(output, "\n");
  if (ft->filename[0]) {
    fprintf(output, "%s:", ft->filename);
    if (strcmp(ft->filename, "-") == 0 || (ft->handler.flags & SOX_FILE_DEVICE))
      fprintf(output, " (%s)", ft->handler.names[0]);
    fprintf(output, "\n\n");
  }

  fprintf(output, "  Encoding: %-14s", sox_encodings_info[ft->encoding.encoding].name);
  text = sox_find_comment(f->ft->oob.comments, "Comment");
  if (!text)
    text = sox_find_comment(f->ft->oob.comments, "Description");
  if (!text)
    text = sox_find_comment(f->ft->oob.comments, "Year");
  if (text)
    fprintf(output, "Info: %s", text);
  fprintf(output, "\n");

  sprintf(buffer, "  Channels: %u @ %u-bit", ft->signal.channels, ft->signal.precision);
  fprintf(output, "%-25s", buffer);
  text = sox_find_comment(f->ft->oob.comments, "Tracknumber");
  if (text) {
    fprintf(output, "Track: %s", text);
    text = sox_find_comment(f->ft->oob.comments, "Tracktotal");
    if (text)
      fprintf(output, " of %s", text);
  }
  fprintf(output, "\n");

  sprintf(buffer, "Samplerate: %gHz", ft->signal.rate);
  fprintf(output, "%-25s", buffer);
  text = sox_find_comment(f->ft->oob.comments, "Album");
  if (text)
    fprintf(output, "Album: %s", text);
  fprintf(output, "\n");

  if (f && f->replay_gain != HUGE_VAL){
    sprintf(buffer, "%s gain: %+.1fdB", lsx_find_enum_value(f->replay_gain_mode, rg_modes)->text, f->replay_gain);
    buffer[0] += 'A' - 'a';
    fprintf(output, "%-24s", buffer);
  } else
    fprintf(output, "%-24s", "Replaygain: off");
  text = sox_find_comment(f->ft->oob.comments, "Artist");
  if (text)
    fprintf(output, "Artist: %s", text);
  fprintf(output, "\n");

  fprintf(output, "  Duration: %-13s", ft->signal.length? str_time((double)ws / ft->signal.rate) : "unknown");
  text = sox_find_comment(f->ft->oob.comments, "Title");
  if (text)
    fprintf(output, "Title: %s", text);
  fprintf(output, "\n\n");
}

static void display_file_info(sox_format_t * ft, file_t * f, sox_bool full)
{
  static char const * const no_yes[] = {"no", "yes"};
  FILE * const output = sox_mode == sox_soxi? stdout : stderr;
  char const * filetype = lsx_find_file_extension(ft->filename);
  sox_bool show_type = sox_true;
  size_t i;

  if (sox_mode == sox_play && sox_globals.verbosity < 3) {
    play_file_info(ft, f, full);
    return;
  }

  fprintf(output, "\n%s: '%s'",
    ft->mode == 'r'? "Input File     " : "Output File    ", ft->filename);
  if (filetype) for (i = 0; ft->handler.names[i] && show_type; ++i)
    if (!strcasecmp(filetype, ft->handler.names[i]))
      show_type = sox_false;
  if (show_type)
    fprintf(output, " (%s)", ft->handler.names[0]);
  fprintf(output, "\n");

  fprintf(output,
    "Channels       : %u\n"
    "Sample Rate    : %g\n"
    "Precision      : %u-bit\n",
    ft->signal.channels,
    ft->signal.rate,
    ft->signal.precision);

  if (ft->signal.length && ft->signal.channels && ft->signal.rate) {
    size_t ws = ft->signal.length / ft->signal.channels;
    fprintf(output,
      "Duration       : %s = %lu samples %c %g CDDA sectors\n",
      str_time((double)ws / ft->signal.rate),
      (unsigned long)ws, "~="[ft->signal.rate == 44100],
      (double)ws / ft->signal.rate * 44100 / 588);
  }
  if (ft->encoding.encoding) {
    char buffer[20] = {'\0'};
    if (ft->encoding.bits_per_sample)
      sprintf(buffer, "%u-bit ", ft->encoding.bits_per_sample);

    fprintf(output, "Sample Encoding: %s%s\n", buffer,
        sox_encodings_info[ft->encoding.encoding].desc);
  }

  if (full) {
    if (ft->encoding.bits_per_sample > 8 || (ft->handler.flags & SOX_FILE_ENDIAN))
      fprintf(output, "Endian Type    : %s\n",
          ft->encoding.reverse_bytes != MACHINE_IS_BIGENDIAN ? "big" : "little");
    if (ft->encoding.bits_per_sample)
      fprintf(output,
        "Reverse Nibbles: %s\n"
        "Reverse Bits   : %s\n",
        no_yes[ft->encoding.reverse_nibbles],
        no_yes[ft->encoding.reverse_bits]);
  }

  if (f && f->replay_gain != HUGE_VAL)
    fprintf(output, "Replay gain    : %+g dB (%s)\n" , f->replay_gain,
        lsx_find_enum_value(f->replay_gain_mode, rg_modes)->text);
  if (f && f->volume != HUGE_VAL)
    fprintf(output, "Level adjust   : %g (linear gain)\n" , f->volume);

  if (!(ft->handler.flags & SOX_FILE_DEVICE) && ft->oob.comments) {
    if (sox_num_comments(ft->oob.comments) > 1) {
      sox_comments_t p = ft->oob.comments;
      fprintf(output, "Comments       : \n");
      do fprintf(output, "%s\n", *p);
      while (*++p);
    }
    else fprintf(output, "Comment        : '%s'\n", ft->oob.comments[0]);
  }
  fprintf(output, "\n");
}

static void report_file_info(file_t * f)
{
  if (sox_globals.verbosity > 2)
    display_file_info(f->ft, f, sox_true);
}

static void display_error(sox_format_t * ft)
{
  static char const * const sox_strerror[] = {
    "Invalid Audio Header",
    "Unsupported data format",
    "Unsupported rate for format",
    "Can't alloc memory",
    "Operation not permitted",
    "Operation not supported",
    "Invalid argument",
    "Unsupported file format",
  };
  lsx_fail("%s: %s: %s", ft->filename, ft->sox_errstr,
      ft->sox_errno < SOX_EHDR?
      strerror(ft->sox_errno) : sox_strerror[ft->sox_errno - SOX_EHDR]);
}

static void progress_to_next_input_file(file_t * f)
{
  if (user_skip) {
    user_skip = sox_false;
    fprintf(stderr, "\nSkipped (Ctrl-C twice to quit).\n");
  }
  read_wide_samples = 0;
  input_wide_samples = f->ft->signal.length / f->ft->signal.channels;
  if (show_progress && (sox_globals.verbosity < 3 ||
                        (is_serial(combine_method) && input_count > 1)))
    display_file_info(f->ft, f, sox_false);
  if (f->volume == HUGE_VAL)
    f->volume = 1;
  if (f->replay_gain != HUGE_VAL)
    f->volume *= pow(10.0, f->replay_gain / 20);
  f->ft->sox_errno = errno = 0;
}

/* Read up to max `wide' samples.  A wide sample contains one sample per channel
            printf("effect %d obeg = %d oend = %d\n", e, effects_chain->effects[e]->obeg, effects_chain->effects[e]->oend);
 * from the input audio. */
static size_t sox_read_wide(sox_format_t * ft, sox_sample_t * buf, size_t max)
{
  size_t len = max / combiner_signal.channels;
  len = sox_read(ft, buf, len * ft->signal.channels) / ft->signal.channels;
  if (!len && ft->sox_errno)
    display_error(ft);
  return len;
}

static void balance_input(sox_sample_t * buf, size_t ws, file_t * f)
{
  size_t s = ws * f->ft->signal.channels;

  if (f->volume != 1)
    while (s--) {
      double d = f->volume * *buf;
      *buf++ = SOX_ROUND_CLIP_COUNT(d, f->volume_clips);
    }
}

/* The input combiner: contains one sample buffer per input file, but only
 * needed if is_parallel(combine_method) */
typedef struct {
  sox_sample_t * * ibuf;
  size_t *         ilen;
} input_combiner_t;

static int combiner_start(sox_effect_t *effp)
{
  input_combiner_t * z = (input_combiner_t *) effp->priv;
  size_t ws, i;

  if (is_serial(combine_method))
    progress_to_next_input_file(files[current_input]);
  else {
    ws = 0;
    z->ibuf = lsx_malloc(input_count * sizeof(*z->ibuf));
    for (i = 0; i < input_count; i++) {
      z->ibuf[i] = lsx_malloc(sox_globals.bufsiz * sizeof(sox_sample_t));
      progress_to_next_input_file(files[i]);
      ws = max(ws, input_wide_samples);
    }
    input_wide_samples = ws; /* Output length is that of longest input file. */
  }
  z->ilen = lsx_malloc(input_count * sizeof(*z->ilen));
  return SOX_SUCCESS;
}

static sox_bool can_segue(size_t i)
{
  return
    files[i]->ft->signal.channels == files[i - 1]->ft->signal.channels &&
    files[i]->ft->signal.rate     == files[i - 1]->ft->signal.rate;
}

static int combiner_drain(sox_effect_t *effp, sox_sample_t * obuf, size_t * osamp)
{
  input_combiner_t * z = (input_combiner_t *) effp->priv;
  size_t ws, s, i;
  size_t olen = 0;

  if (is_serial(combine_method)) {
    while (sox_true) {
      if (!user_skip)
        olen = sox_read_wide(files[current_input]->ft, obuf, *osamp);
      if (olen == 0) {   /* If EOF, go to the next input file. */
        if (++current_input < input_count) {
          if (combine_method == sox_sequence && !can_segue(current_input))
            break;
          progress_to_next_input_file(files[current_input]);
          continue;
        }
      }
      balance_input(obuf, olen, files[current_input]);
      break;
    } /* while */
  } /* is_serial */ else { /* else is_parallel() */
    sox_sample_t * p = obuf;
    for (i = 0; i < input_count; ++i) {
      z->ilen[i] = sox_read_wide(files[i]->ft, z->ibuf[i], *osamp);
      balance_input(z->ibuf[i], z->ilen[i], files[i]);
      olen = max(olen, z->ilen[i]);
    }
    for (ws = 0; ws < olen; ++ws) { /* wide samples */
      if (combine_method == sox_mix || combine_method == sox_mix_power) {
        for (s = 0; s < effp->in_signal.channels; ++s, ++p) { /* sum samples */
          *p = 0;
          for (i = 0; i < input_count; ++i)
            if (ws < z->ilen[i] && s < files[i]->ft->signal.channels) {
              /* Cast to double prevents integer overflow */
              double sample = *p + (double)z->ibuf[i][ws * files[i]->ft->signal.channels + s];
              *p = SOX_ROUND_CLIP_COUNT(sample, mixing_clips);
            }
        }
      } /* sox_mix */ else if (combine_method == sox_multiply)  {
        for (s = 0; s < effp->in_signal.channels; ++s, ++p) { /* multiple samples */
          i = 0;
          *p = ws < z->ilen[i] && s < files[i]->ft->signal.channels?
            z->ibuf[i][ws * files[i]->ft->signal.channels + s] : 0;
          for (++i; i < input_count; ++i) {
            double sample = *p * (-1. / SOX_SAMPLE_MIN) * (ws < z->ilen[i] && s < files[i]->ft->signal.channels? z->ibuf[i][ws * files[i]->ft->signal.channels + s] : 0);
            *p = SOX_ROUND_CLIP_COUNT(sample, mixing_clips);
          }
        }
      } /* sox_multiply */ else { /* sox_merge: like a multi-track recorder */
        for (i = 0; i < input_count; ++i)
          for (s = 0; s < files[i]->ft->signal.channels; ++s)
            *p++ = (ws < z->ilen[i]) * z->ibuf[i][ws * files[i]->ft->signal.channels + s];
      } /* sox_merge */
    } /* wide samples */
    current_input += input_count;
  } /* is_parallel */
  read_wide_samples += olen;
  olen *= effp->in_signal.channels;
  *osamp = olen;

  input_eof = olen ? sox_false : sox_true;

  return olen? SOX_SUCCESS : SOX_EOF;
}

static int combiner_stop(sox_effect_t *effp)
{
  input_combiner_t * z = (input_combiner_t *) effp->priv;
  size_t i;

  if (is_parallel(combine_method))
    /* Free input buffers now that they are not used */
    for (i = 0; i < input_count; i++)
      free(z->ibuf[i]);

  return SOX_SUCCESS;
}

static sox_effect_handler_t const * input_combiner_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "input", 0, SOX_EFF_MCHAN, 0, combiner_start, 0, combiner_drain,
    combiner_stop, 0, sizeof(input_combiner_t)
  };
  return &handler;
}

static int output_flow(sox_effect_t *effp, sox_sample_t const * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  size_t len;

  (void)effp, (void)obuf;
  if (show_progress) for (len = 0; len < *isamp; len += effp->in_signal.channels) {
    omax[0] = max(omax[0], ibuf[len]);
    omin[0] = min(omin[0], ibuf[len]);
    if (effp->in_signal.channels > 1) {
      omax[1] = max(omax[1], ibuf[len + 1]);
      omin[1] = min(omin[1], ibuf[len + 1]);
    }
    else {
      omax[1] = omax[0];
      omin[1] = omin[0];
    }
  }
  *osamp = 0;
  len = *isamp? sox_write(ofile->ft, ibuf, *isamp) : 0;
  output_samples += len / ofile->ft->signal.channels;
  output_eof = (len != *isamp) ? sox_true: sox_false;
  if (len != *isamp) {
    if (ofile->ft->sox_errno)
      display_error(ofile->ft);
    return SOX_EOF;
  }
  return SOX_SUCCESS;
}

static sox_effect_handler_t const * output_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "output", 0, SOX_EFF_MCHAN, NULL, NULL, output_flow, NULL, NULL, NULL, 0
  };
  return &handler;
}

static void add_effect(sox_effects_chain_t *chain, char const *name, 
                       int argc, char *argv[], sox_signalinfo_t *signal)
{
  sox_effect_t * effp;

  effp = sox_create_effect(sox_find_effect(name)); /* Should always succeed. */

  if (!effp)
    lsx_fail("Failed creating effect.  Out of Memory?\n");

  if (effp->handler.flags & SOX_EFF_DEPRECATED)
    lsx_warn("effect `%s' is deprecated; see sox(1) for an alternative", 
             effp->handler.name);

  if (sox_effect_options(effp, argc, argv) == SOX_EOF)
    exit(1); /* The failing effect should have displayed an error message */
  
  if (sox_add_effect(chain, effp, signal, &ofile->ft->signal) != SOX_SUCCESS)
    exit(2); /* The effects chain should have displayed an error message */
}

static void init_eff_chains(void)
{
  user_effargs = lsx_malloc(sizeof(user_effargs[0][0]) * MAX_USER_EFF);
  nuser_effects = lsx_malloc(sizeof(*nuser_effects));
  nuser_effects[0] = 0;
} /* init_eff_chains */

/* add_eff_chain() - NOTE: this only adds memory for one 
 * additional effects chain beyond value of eff_chain_count.  It
 * does not unconditionally increase size of effects chain.
 */
static void add_eff_chain(void)
{
  user_effargs = lsx_realloc(user_effargs, (eff_chain_count+1) *
                             sizeof(user_effargs[0][0]) * MAX_USER_EFF);
  nuser_effects = lsx_realloc(nuser_effects, (eff_chain_count+1) *
                              sizeof(*nuser_effects));
  nuser_effects[eff_chain_count] = 0;
} /* add_eff_chain */

static void delete_eff_chains(void)
{
  unsigned j;
  int i, k;

  for (i = 0; i < eff_chain_count; i++) 
  {
    for (j = 0; j < nuser_effects[i]; j++) 
    {
      if (user_effargs[i][j].name)
        free(user_effargs[i][j].name);
      user_effargs[i][j].name = NULL;
      for (k = 0; k < user_effargs[i][j].argc; k++)
      {
        if (user_effargs[i][j].argv[k])
          free(user_effargs[i][j].argv[k]);
        user_effargs[i][j].argv[k] = NULL;
      }
      user_effargs[i][j].argc = 0;
    }
    nuser_effects[i] = 0;
  }
  free(user_effargs);
  free(nuser_effects);
  user_effargs = NULL;
  nuser_effects = NULL;
  eff_chain_count = 0;
} /* delete_eff_chains */

static sox_bool is_pseudo_effect(char *s)
{
  if (strcmp("newfile", s) == 0 || 
      strcmp("restart", s) == 0 ||
      strcmp(":", s) == 0)
    return sox_true;
  else
    return sox_false;
} /* is_pseudo_effect */

static void parse_effects(int argc, char **argv)
{
  while (optind < argc) {
    unsigned eff_offset;
    int j;
    int newline_mode = 0;

    eff_offset = nuser_effects[eff_chain_count];
    if (eff_offset >= MAX_USER_EFF) {
      lsx_fail("too many effects specified (at most %i allowed)", MAX_USER_EFF);
      exit(1);
    }

    /* psuedo-effect ":" is used to create a new effects chain */
    if (strcmp(argv[optind], ":") == 0)
    {
      /* Only create a new chain if current one has effects.
       * Error checking will be done when loop is restarted.
       */
      if (nuser_effects[eff_chain_count] != 0)
      {
        eff_chain_count++;
        add_eff_chain();
      }
      optind++;
      continue;
    }
    
    if (strcmp(argv[optind], "newfile") == 0)
    {
      /* Start a new effect chain for newfile if user doesn't
       * manually do it.  Restart loop without advancing
       * optind to do error checking.
       */
      if (nuser_effects[eff_chain_count] != 0)
      {
        eff_chain_count++;
        add_eff_chain();
        continue;
      }
      newline_mode = 1;
    }
    else if (strcmp(argv[optind], "restart") == 0)
    {
      /* Start a new effect chain for restart if user doesn't
       * manually do it.  Restart loop without advancing
       * optind to do error checking.
       */
      if (nuser_effects[eff_chain_count] != 0)
      {
        eff_chain_count++;
        add_eff_chain();
        continue;
      }
      newline_mode = 1;
    }

    /* Name should always be correct! */
    user_effargs[eff_chain_count][eff_offset].name = strdup(argv[optind++]);
    for (j = 0; j < argc - optind && !sox_find_effect(argv[optind + j]) &&
         !is_pseudo_effect(argv[optind + j]); ++j)
      user_effargs[eff_chain_count][eff_offset].argv[j] = strdup(argv[optind + j]);
    user_effargs[eff_chain_count][eff_offset].argc = j;

    optind += j; /* Skip past the effect arguments */
    nuser_effects[eff_chain_count]++;
    if (newline_mode) 
    {
      output_method = sox_multiple;
      eff_chain_count++;
      add_eff_chain();
    }
  }
} /* parse_effects */

static int strtoargv(char *s, char *(*argv)[])
{
    int argc = 0;

    if (!s) return 0;

    while (*s)
    {
        int quote_mode;

        /* Skip past any white space */
        while (*s == ' ' || *s == '\t')
            s++;

        /* Stop processing if no more data */
        if (!*s)
            break;

        /* If starts with quote then start quote mode.  This
         * will ignore seperators until a final quote is seen.
         * Don't put quote into the argument.
         */
        if (*s == '"')
        {
            quote_mode = 1;
            s++;
        }
        else
            quote_mode = 0;

        (*argv)[argc] = s;

        /* Scan for seperator and when overwrite with 0 */
        while (*s)
        {
            /* Treat quote as final seperator; even if there is
             * more data right after it.
             * TODO: Process \" and \\ so that user can
             * have quotes inside quotes.  Tricky to do though.
             */
            if (quote_mode && *s == '"')
                break;

            if (!quote_mode && (*s == ' ' || *s == '\t'))
                break;
            s++;
        }
        *s = 0;
        s++;
        argc += 1;
    }

    return argc;
} /* strtoargv */

static void read_user_effects(char *filename)
{
    FILE *file = fopen(filename, "rt");
    char s[FILENAME_MAX];
    int argc;
    char *argv[FILENAME_MAX];
    int len;

    /* Free any command line options and then re-initialize to
     * starter user_effargs.
     */
    delete_eff_chains();
    current_eff_chain = 0;
    init_eff_chains();

    if (file == NULL)
    {
        lsx_fail("Cannot open effects file %s", filename);
        exit(1);
    }

    lsx_report("Reading effects from file %s", filename);

    while (fgets(s, FILENAME_MAX, file))
    {
      len = strlen(s);
      if (s[len-1] == '\n')
        s[len-1] = 0;

      argc = strtoargv(s, &argv);

      /* Make sure first option is an effect name. */
      if (!sox_find_effect(argv[0]) && !is_pseudo_effect(argv[0]))
      {
        printf("Cannot find an effect called `%s'.\n", argv[0]);
        exit(1);
      }

      /* parse_effects normally parses options from command line.
       * Reset opt index so it thinks its back at beginning of
       * main()'s argv[].
       */
      optind = 0;
      parse_effects(argc, argv);

      /* Advance to next effect but only if current chain has been
       * filled in.  This recovers from side affects of psuedo-effects.
       */
      if (nuser_effects[eff_chain_count] > 0)
      {
        eff_chain_count++;
        add_eff_chain();
      }
    }

    fclose(file);

} /* read_user_effects */

/* Creates users effects and passes in user specified options.
 * This is done without putting anything into the effects chain
 * because an effect may set the effp->in_format and we may want
 * to copy that back into the input/combiner before opening and 
 * inserting it.  
 * Similarly, we may want to use effp->out_format to override the 
 * default values of output file before we open it.
 * To keep things simple, we create all user effects.  Later, when
 * we add them, some may already be in the chain and we will need to free
 * them.
 */
static void create_user_effects(void)
{
  unsigned i;
  sox_effect_t *effp;

  for (i = 0; i < nuser_effects[current_eff_chain]; i++) 
  {
      effp = sox_create_effect(sox_find_effect(user_effargs[current_eff_chain][i].name));

      if (!effp)
        lsx_fail("Failed creating effect.  Out of Memory?\n");

      if (effp->handler.flags & SOX_EFF_DEPRECATED)
        lsx_warn("effect `%s' is deprecated; see sox(1) for an alternative", 
                 effp->handler.name);

      /* The failing effect should have displayed an error message */
      if (sox_effect_options(effp, user_effargs[current_eff_chain][i].argc, 
                             user_effargs[current_eff_chain][i].argv) == SOX_EOF)
        exit(1);

      user_efftab[i] = effp;
  }
}

/* Add all user effects to the chain.  If the output effect's rate or
 * channel count do not match the end of the effects chain then
 * insert effects to correct this.
 *
 * This can be called with the input effect already in the effects
 * chain from a previous run.  Also, it use a pre-existing
 * output effect if its been saved into save_output_eff.
 */
static void add_effects(sox_effects_chain_t *chain)
{
  sox_signalinfo_t signal = combiner_signal;
  unsigned i;
  sox_effect_t * effp;
  char * rate_arg = sox_mode != sox_play? NULL :
    (rate_arg = getenv("PLAY_RATE_ARG"))? rate_arg : "-l";

  /* 1st `effect' in the chain is the input combiner_signal.
   * add it only if its not there from a previous run.
   */
  if (chain->length == 0)
  {
    effp = sox_create_effect(input_combiner_effect_fn());
    sox_add_effect(chain, effp, &signal, &ofile->ft->signal);
  }

  /* Add auto effects if appropriate; add user specified effects */
  for (i = 0; i < nuser_effects[current_eff_chain]; i++) {
    /* Effects chain should have displayed an error message */
    if (sox_add_effect(chain, user_efftab[i], 
                       &signal, &ofile->ft->signal) != SOX_SUCCESS)
      exit(2);
  }

  /* Add auto effects if still needed at this point */
  if (signal.rate != ofile->ft->signal.rate)
  {
    add_effect(chain, "rate", rate_arg != NULL, &rate_arg, &signal); /* Must be up-sampling */
  }
  if (signal.channels != ofile->ft->signal.channels)
  {
    add_effect(chain, "mixer", 0, NULL, &signal); /* Must be increasing channels */
  }
    
  if (!save_output_eff)
  {
    /* Last `effect' in the chain is the output file */
    effp = sox_create_effect(output_effect_fn());
    if (sox_add_effect(chain, effp, &signal, &ofile->ft->signal) != SOX_SUCCESS)
      exit(2);
  }
  else
  {
    sox_push_effect_last(chain, save_output_eff);
    save_output_eff = NULL;
  }

  for (i = 0; i < chain->length; ++i) {
    sox_effect_t const * effp = &chain->effects[i][0];
    lsx_report("effects chain: %-10s %gHz %u channels %u bits %s",
        effp->handler.name, effp->in_signal.rate, effp->in_signal.channels, effp->in_signal.precision,
        (effp->handler.flags & SOX_EFF_MCHAN)? "(multi)" : "");
  }
}

static int advance_eff_chain(void)
{
  sox_bool reuse_output = sox_true;

  /* If input file reached EOF then delete all effects in current
   * chain and restart the current chain.
   *
   * This is only used with sox_sequence combine mode even though
   * we do not specifically check for that method.
   */
  if (input_eof)
    sox_delete_effects(effects_chain);
  else
  {
    /* If user requested to restart this effect chain then
     * do not advance to next.  Usually, this is because
     * an option to current effect was changed.
     */
    if (user_restart_eff)
      user_restart_eff = sox_false;
    /* Effect chain stopped so advance to next effect chain but
     * quite if no more chains exist.
     */
    else if (++current_eff_chain >= eff_chain_count) 
      return SOX_EOF;

    while (nuser_effects[current_eff_chain] == 1 && 
           is_pseudo_effect(user_effargs[current_eff_chain][0].name))
    {
      if (strcmp("newfile", user_effargs[current_eff_chain][0].name) == 0)
      {
        if (++current_eff_chain >= eff_chain_count) 
          return SOX_EOF;
        reuse_output = sox_false;
      }
      else if (strcmp("restart", user_effargs[current_eff_chain][0].name) == 0)
        current_eff_chain = 0;
    }

    if (reuse_output)
      save_output_eff = sox_pop_effect_last(effects_chain);

    /* TODO: Warn user when an effect is deleted that still
     * had unprocessed samples.
     */
    while (effects_chain->length > 1)
    {
      sox_delete_effect_last(effects_chain);
    }
  }
  return SOX_SUCCESS;
} /* advance_eff_chain */

static size_t total_clips(void)
{
  unsigned i;
  size_t clips = 0;
  for (i = 0; i < file_count; ++i)
    clips += files[i]->ft->clips + files[i]->volume_clips;
  return clips + mixing_clips + sox_effects_clips(effects_chain);
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

#define MIN_HEADROOM 6.
static double min_headroom = MIN_HEADROOM;

static char const * vu(unsigned channel)
{
  static struct timeval then;
  static char const * const text[][2] = {
    /* White: 2dB steps */
    {"", ""}, {"-", "-"}, {"=", "="}, {"-=", "=-"},
    {"==", "=="}, {"-==", "==-"}, {"===", "==="}, {"-===", "===-"},
    {"====", "===="}, {"-====", "====-"}, {"=====", "====="},
    {"-=====", "=====-"}, {"======", "======"},
    /* Red: 1dB steps */
    {"!=====", "=====!"},
  };
  int const red = 1, white = array_length(text) - red;
  double const MAX = SOX_SAMPLE_MAX, MIN = SOX_SAMPLE_MIN;
  double linear = max(omax[channel] / MAX, omin[channel] / MIN);
  double dB = linear_to_dB(linear);
  int vu_dB = linear? floor(2 * white + red + dB) : 0;
  int index = vu_dB < 2 * white? max(vu_dB / 2, 0) : min(vu_dB - white, red + white - 1);
  omax[channel] = omin[channel] = 0;
  if (-dB < min_headroom) {
    gettimeofday(&then, NULL);
    min_headroom = -dB;
  }
  else if (since(&then, 3., sox_false))
    min_headroom = -dB;

  return text[index][channel];
}

static char * headroom(void)
{
  static char buff[10];
  unsigned h = (unsigned)(min_headroom * 10);
  if (min_headroom >= MIN_HEADROOM) return "      ";
  sprintf(buff, "Hd:%u.%u", h /10, h % 10);
  return buff;
}

static void display_status(sox_bool all_done)
{
  static struct timeval then;
  if (!show_progress)
    return;
  if (all_done || since(&then, .1, sox_false)) {
    double read_time = (double)read_wide_samples / combiner_signal.rate;
    double left_time = 0, in_time = 0, percentage = 0;

    if (input_wide_samples) {
      in_time = (double)input_wide_samples / combiner_signal.rate;
      left_time = max(in_time - read_time, 0);
      percentage = max(100. * read_wide_samples / input_wide_samples, 0);
    }
    fprintf(stderr, "\rIn:%-5s %s [%s] Out:%-5s [%6s|%-6s] %s Clip:%-5s",
      lsx_sigfigs3p(percentage), str_time(read_time), str_time(left_time),
      lsx_sigfigs3(output_samples),
      vu(0), vu(1), headroom(), lsx_sigfigs3(total_clips()));
  }
  if (all_done)
    fputc('\n', stderr);
}

static int update_status(sox_bool all_done)
{
#ifdef INTERACTIVE
  char ch = fgetc(stdin);

  if (ch != -1)
  {
    if (files[current_input]->ft->handler.seek && 
        files[current_input]->ft->seekable)
    {
      if (ch == '>')
      {
        read_wide_samples += files[current_input]->ft->signal.rate*30;
        sox_seek(files[current_input]->ft, read_wide_samples,
                 SOX_SEEK_SET);
      }
      if (ch == '<')
      {
        read_wide_samples -= files[current_input]->ft->signal.rate*30;
        sox_seek(files[current_input]->ft, read_wide_samples,
                 SOX_SEEK_SET);
      }
      if (ch == 'R')
      {
        /* Not very useful, eh!  Sample though of the place you
         * could change the value to effects options
         * like vol or speed or mixer.
         * Modify values in user_effargs[current_eff_chain][xxx]
         * and then chain will be drain()ed and restarted whence
         * this function is existed.
         */
        user_restart_eff = sox_true;
      }
    }
  }
#endif

  display_status(all_done || user_abort);
  return (user_abort || user_restart_eff) ? SOX_EOF : SOX_SUCCESS;
}

static void optimize_trim(void)
{
  /* Speed hack.  If the "trim" effect is the first effect then peek inside its
   * "effect descriptor" and see what the start location is.  This has to be
   * done after its start() is called to have the correct location.  Also, only
   * do this when only working with one input file.  This is because the logic
   * to do it for multiple files is complex and problably never used.  This
   * hack is a huge time savings when trimming gigs of audio data into
   * managable chunks.  */
  if (input_count == 1 && effects_chain->length > 1 && strcmp(effects_chain->effects[1][0].handler.name, "trim") == 0) {
    if (files[0]->ft->handler.seek && files[0]->ft->seekable){
      uint64_t offset = sox_trim_get_start(&effects_chain->effects[1][0]);
      if (offset && sox_seek(files[0]->ft, offset, SOX_SEEK_SET) == SOX_SUCCESS) {
        read_wide_samples = offset / files[0]->ft->signal.channels;
        /* Assuming a failed seek stayed where it was.  If the seek worked then
         * reset the start location of trim so that it thinks user didn't
         * request a skip.  */
        sox_trim_clear_start(&effects_chain->effects[1][0]);
        lsx_debug("optimize_trim successful");
      }
    }
  }
}

static sox_bool overwrite_permitted(char const * filename)
{
  char c;

  if (!interactive) {
    lsx_report("Overwriting `%s'", filename);
    return sox_true;
  }
  lsx_warn("Output file `%s' already exists", filename);
  if (!isatty(fileno(stdin)))
    return sox_false;
  do fprintf(stderr, "%s sox: overwrite `%s' (y/n)? ", myname, filename);
  while (scanf(" %c%*[^\n]", &c) != 1 || !strchr("yYnN", c));
  return c == 'y' || c == 'Y';
}

static char *fndup_with_count(const char *filename, size_t count)
{
    char *expand_fn, *efn;
    const char *fn, *ext, *end;
    sox_bool found_marker = sox_false;

    fn = filename;

    efn = expand_fn = lsx_malloc((size_t)FILENAME_MAX);

    /* Find extension in case user didn't specify a substitution
     * marker.
     */
    end = ext = filename + strlen(filename);
    while (ext > filename && *ext != '.')
        ext--;

    /* In case extension not found, point back to end of string to do less
     * copying later.
     */
    if (*ext != '.')
        ext = end;

    while (fn < end)
    {
        /* Look for %n. If found, replace with count.  Can specify an
         * option width of 1-9.
         */
        if (*fn == '%')
        {
            int width = 0;
            fn++;
            if (*fn >= '1' || *fn <= '9')
            {
                width = *fn++;
            }
            if (*fn == 'n')
            {
                char num[10];
                char format[5];

                found_marker = sox_true;

                strcpy(format, "%");
                if (width)
                {
                    char tmps[2];
                    tmps[0] = width;
                    tmps[1] = 0;
                    strcat(format, "0");
                    strcat(format, tmps);
                }
                strcat(format, "d");
                strcpy(format, "%02d");
                sprintf(num, format, count);
                *efn = 0;
                strcat(efn, num);
                efn += strlen(num);
                fn++;
            }
            else
                *efn++ = *fn++;
        }
        else
            *efn++ = *fn++;
    }

    *efn = 0;

    /* If user didn't tell us what to do then default to putting
     * the count right before file extension.
     */
    if (!found_marker)
    {
        efn -= strlen (ext);

        sprintf(efn, "%03lu", (unsigned long)count);
        efn = efn + 3;
        strcat(efn, ext);
    }

    return expand_fn;
}

static void open_output_file(void)
{
  double factor;
  int i;
  sox_comments_t p = ofile->oob.comments;
  sox_oob_t oob = files[0]->ft->oob;
  char *expand_fn;

  /* Skip opening file if we are not recreating output effect */
  if (save_output_eff)
    return;

  oob.comments = sox_copy_comments(files[0]->ft->oob.comments);

  if (!oob.comments && !p)
    sox_append_comment(&oob.comments, "Processed by SoX");
  else if (p) {
    if (!(*p)[0]) {
      sox_delete_comments(&oob.comments);
      ++p;
    }
    while (*p)
      sox_append_comment(&oob.comments, *p++);
  }

  /* Copy loop info, resizing appropriately it's in samples, so # channels
   * don't matter FIXME: This doesn't work for multi-file processing or effects
   * that change file length.  */
  factor = (double) ofile->signal.rate / combiner_signal.rate;
  for (i = 0; i < SOX_MAX_NLOOPS; i++) {
    oob.loops[i].start = oob.loops[i].start * factor;
    oob.loops[i].length = oob.loops[i].length * factor;
  }

  if (output_method == sox_multiple)
    expand_fn = fndup_with_count(ofile->filename, ++output_count);
  else
    expand_fn = strdup(ofile->filename);
  ofile->ft = sox_open_write(expand_fn, &ofile->signal, &ofile->encoding,
      ofile->filetype, &oob, overwrite_permitted);
  sox_delete_comments(&oob.comments);
  free(expand_fn);

  if (!ofile->ft)
    /* sox_open_write() will call lsx_warn for most errors.
     * Rely on that printing something. */
    exit(2);

  /* If whether to enable the progress display (similar to that of ogg123) has
   * not been specified by the user, auto turn on when outputting to an audio
   * device: */
  if (show_progress == SOX_OPTION_DEFAULT)
    show_progress = (ofile->ft->handler.flags & SOX_FILE_DEVICE) != 0 &&
                    (ofile->ft->handler.flags & SOX_FILE_PHONY) == 0;

  report_file_info(ofile);
}

static void sigint(int s)
{
  static struct timeval then;
  if (input_count > 1 && show_progress && s == SIGINT &&
      is_serial(combine_method) && since(&then, 1.0, sox_true))
    user_skip = sox_true;
  else user_abort = sox_true;
}

static void calculate_combiner_signal_parameters(void)
{
  size_t i;

  /* If user didn't specify # of channels then see if an effect
   * is specifying them.  This is of most use currently with the
   * synth effect were user can use null input handler and specify
   * channel counts directly in effect.  Forcing to use -c with
   * -n isn't as convenient.
   */
  for (i = 0; i < input_count; i++) {
    unsigned j;
    for (j =0; j < nuser_effects[current_eff_chain] && 
               !files[i]->ft->signal.channels; ++j)
      files[i]->ft->signal.channels = user_efftab[j]->in_signal.channels;
    /* For historical reasons, default to one channel if not specified. */
    if (!files[i]->ft->signal.channels)
      files[i]->ft->signal.channels = 1;
  }

  /* Set the combiner output signal attributes to those of the 1st/next input
   * file.  If we are in sox_sequence mode then we don't need to check the
   * attributes of the other inputs, otherwise, it is mandatory that all input
   * files have the same sample rate, and for sox_concatenate, it is mandatory
   * that they have the same number of channels, otherwise, the number of
   * channels at the output of the combiner is calculated according to the
   * combiner mode. */
  combiner_signal = files[current_input]->ft->signal;
  if (combine_method == sox_sequence) {
    /* Report all input files; do this only the 1st time process() is called: */
    if (!current_input) for (i = 0; i < input_count; i++)
      report_file_info(files[i]);
  } else {
    size_t total_channels = 0;
    size_t min_channels = SOX_SIZE_MAX;
    size_t max_channels = 0;
    size_t min_rate = SOX_SIZE_MAX;
    size_t max_rate = 0;

    /* Report all input files and gather info on differing rates & numbers of
     * channels, and on the resulting output audio length: */
    for (i = 0; i < input_count; i++) {
      report_file_info(files[i]);
      total_channels += files[i]->ft->signal.channels;
      min_channels = min(min_channels, files[i]->ft->signal.channels);
      max_channels = max(max_channels, files[i]->ft->signal.channels);
      min_rate     = min(min_rate    , files[i]->ft->signal.rate);
      max_rate     = max(max_rate    , files[i]->ft->signal.rate);
    }

    /* Check for invalid/unusual rate or channel combinations: */
    if (min_rate != max_rate)
      lsx_fail("Input files must have the same sample-rate");
      /* Don't exit quite yet; give the user any other message 1st */
    if (min_channels != max_channels) {
      if (combine_method == sox_concatenate) {
        lsx_fail("Input files must have the same # channels");
        exit(1);
      } else if (combine_method != sox_merge)
        lsx_warn("Input files don't have the same # channels");
    }
    if (min_rate != max_rate)
      exit(1);

    /* Store the calculated # of combined channels: */
    combiner_signal.channels =
      combine_method == sox_merge? total_channels : max_channels;
  }
} /* calculate_combiner_signal_parameters */

static void calculate_output_signal_parameters(void)
{
  sox_bool known_length = combine_method != sox_sequence;
  size_t i, olen = 0;

  /* Report all input files and gather info on differing rates & numbers of
   * channels, and on the resulting output audio length: */
  for (i = 0; i < input_count; i++) {
    known_length = known_length && files[i]->ft->signal.length != 0;
    if (combine_method == sox_concatenate)
      olen += files[i]->ft->signal.length / files[i]->ft->signal.channels;
    else
      olen = max(olen, files[i]->ft->signal.length / files[i]->ft->signal.channels);
  }

  /* Determine the output file signal attributes; set from user options
   * if given: */
  ofile->signal = ofile_signal_options;

  /* If no user option for output rate or # of channels, set from the last
   * effect that sets these, or from the input combiner if there is none such */
  for (i = 0; i < nuser_effects[current_eff_chain] && !ofile->signal.rate; ++i)
    ofile->signal.rate = user_efftab[nuser_effects[current_eff_chain] - 1 - i]->out_signal.rate;
  for (i = 0; i < nuser_effects[current_eff_chain] && !ofile->signal.channels; ++i)
    ofile->signal.channels = user_efftab[nuser_effects[current_eff_chain] - 1 - i]->out_signal.channels;
  if (!ofile->signal.rate)
    ofile->signal.rate = combiner_signal.rate;
  if (!ofile->signal.channels)
    ofile->signal.channels = combiner_signal.channels;

  /* FIXME: comment this: */
  ofile->signal.precision = combiner_signal.precision;

  /* If any given user effect modifies the audio length, then we assume that
   * we don't know what the output length will be.  FIXME: in most cases,
   * an effect that modifies length will be able to determine by how much from
   * its getopts parameters, so olen should be calculable. */
  for (i = 0; i < nuser_effects[current_eff_chain]; i++)
    known_length = known_length && !(user_efftab[i]->handler.flags & SOX_EFF_LENGTH);

  if (!known_length)
    olen = 0;
  ofile->signal.length = (size_t)(olen * ofile->signal.channels * ofile->signal.rate / combiner_signal.rate + .5);
}

static void set_combiner_and_output_encoding_parameters(void)
{
  /* The input encoding parameters passed to the effects chain are those of
   * the first input file (for each segued block if sox_sequence):*/
  combiner_encoding = files[current_input]->ft->encoding;

  /* Determine the output file encoding attributes; set from user options
   * if given: */
  ofile->encoding = ofile_encoding_options;
 
  /* Get unspecified output file encoding attributes from the input file and
   * set the output file to the resultant encoding if this is supported by the
   * output file type; if not, the output file handler should select an
   * encoding suitable for the output signal and its precision. */
  {
    sox_encodinginfo_t t = ofile->encoding;
    if (!t.encoding)
      t.encoding = combiner_encoding.encoding;
    if (!t.bits_per_sample)
      t.bits_per_sample = combiner_encoding.bits_per_sample;
    if (sox_format_supports_encoding(ofile->filename, ofile->filetype, &t))
      ofile->encoding = t;
  }
}

static int process(void)
{         /* Input(s) -> Balancing -> Combiner -> Effects -> Output */
  int flow_status;

  create_user_effects();

  calculate_combiner_signal_parameters();
  set_combiner_and_output_encoding_parameters();
  calculate_output_signal_parameters();
  open_output_file();

  if (!effects_chain)
    effects_chain = sox_create_effects_chain(&combiner_encoding, 
                                             &ofile->ft->encoding);
  add_effects(effects_chain);

  optimize_trim();

  signal(SIGTERM, sigint); /* Stop gracefully, as soon as we possibly can. */
  signal(SIGINT , sigint); /* Either skip current input or behave as SIGTERM. */
  flow_status = sox_flow_effects(effects_chain, update_status);

  /* Don't return SOX_EOF if
   * 1) input reach EOF and there are more input files to process or
   * 2) output didn't return EOF (disk full?) there are more
   *    effect chains.
   * For case #2, something else must decide when to stop processing.
   */
  if ((input_eof && current_input < input_count) || 
      (!output_eof && current_eff_chain < eff_chain_count))
    flow_status = SOX_SUCCESS;

  return flow_status;
}

static void display_SoX_version(FILE * file)
{
#if HAVE_SYS_UTSNAME_H
  struct utsname uts;
#endif

  fprintf(file, "%s: SoX v%s\n", myname, PACKAGE_VERSION);

  if (sox_globals.verbosity > 3) {
    fprintf(file, "time:  %s %s\n", __DATE__, __TIME__);
#if HAVE_DISTRO
    fprintf(file, "issue: %s\n", DISTRO);
#endif
#if HAVE_SYS_UTSNAME_H
    if (!uname(&uts))
      fprintf(file, "uname: %s %s %s %s %s\n", uts.sysname, uts.nodename,
          uts.release, uts.version, uts.machine);
#endif
#if defined __GNUC__
    fprintf(file, "gcc:   %s\n", __VERSION__);
#elif defined _MSC_VER
    fprintf(file, "msc:   %u\n", _MSC_VER);
#elif defined __SUNPRO_C
    fprintf(file, "sun c: %x\n", __SUNPRO_C);
#endif
    fprintf(file, "arch:  %lu%lu%lu%lu %lu%lu %lu%lu %c\n",
        (unsigned long)sizeof(char), (unsigned long)sizeof(short),
        (unsigned long)sizeof(long), (unsigned long)sizeof(off_t),
        (unsigned long)sizeof(float), (unsigned long)sizeof(double),
        (unsigned long)sizeof(int *), (unsigned long)sizeof(int (*)(void)),
        "LB"[MACHINE_IS_BIGENDIAN]);
  }
}

static int strcmp_p(const void *p1, const void *p2)
{
  return strcmp(*(const char **)p1, *(const char **)p2);
}

static void display_supported_formats(void)
{
  size_t i, formats;
  char const * * format_list;
  char const * const * names;

  for (i = formats = 0; sox_format_fns[i].fn; ++i) {
    char const * const *names = sox_format_fns[i].fn()->names;
    while (*names++)
      formats++;
  }
  format_list = lsx_malloc(formats * sizeof(*format_list));

  printf("AUDIO FILE FORMATS:");
  for (i = formats = 0; sox_format_fns[i].fn; ++i) {
    sox_format_handler_t const * handler = sox_format_fns[i].fn();
    if (!(handler->flags & SOX_FILE_DEVICE))
      for (names = handler->names; *names; ++names)
        if (!strchr(*names, '/'))
          format_list[formats++] = *names;
  }
  qsort(format_list, formats, sizeof(*format_list), strcmp_p);
  for (i = 0; i < formats; i++)
    printf(" %s", format_list[i]);
  putchar('\n');

  printf("PLAYLIST FORMATS: m3u pls\nAUDIO DEVICE DRIVERS:");
  for (i = formats = 0; sox_format_fns[i].fn; ++i) {
    sox_format_handler_t const * handler = sox_format_fns[i].fn();
    if ((handler->flags & SOX_FILE_DEVICE) && !(handler->flags & SOX_FILE_PHONY))
      for (names = handler->names; *names; ++names)
        format_list[formats++] = *names;
  }
  qsort(format_list, formats, sizeof(*format_list), strcmp_p);
  for (i = 0; i < formats; i++)
    printf(" %s", format_list[i]);
  puts("\n");

  free(format_list);
}

static void display_supported_effects(void)
{
  size_t i;
  const sox_effect_handler_t *e;

  printf("EFFECTS:");
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
"SPECIAL FILENAMES (infile, outfile):",
"-                        Pipe/redirect input/output (stdin/stdout); use with -t",
"-d, --default-device     Use the default audio device (where available)",
"-n, --null               Use the `null' file handler; e.g. with synth effect",
"-p, --sox-pipe           Alias for `-t sox -'",
#ifdef HAVE_POPEN
"\nSPECIAL FILENAMES (infile only):",
"\"|program [options] ...\" Pipe input from external program (where supported)",
"http://server/file       Use the given URL as input file (where supported)",
#endif
"",
"GLOBAL OPTIONS (gopts) (can be specified at any point before the first effect):",
"--buffer BYTES           Set the size of all processing buffers (default 8192)",
"--combine concatenate    Concatenate multiple input files (default for sox, rec)",
"--combine sequence       Sequence multiple input files (default for play)",
"--effects-file FILENAME  File containing effects and options",
"-h, --help               Display version number and usage information",
"--help-effect NAME       Show usage of effect NAME, or NAME=all for all",
"--help-format NAME       Show info on format NAME, or NAME=all for all",
"--input-buffer BYTES     Override the input buffer size (default: as --buffer)",
"--interactive            Prompt to overwrite output file",
"-m, --combine mix        Mix multiple input files (instead of concatenating)",
"-M, --combine merge      Merge multiple input files (instead of concatenating)",
"--plot gnuplot|octave    Generate script to plot response of filter effect",
"-q, --no-show-progress   Run in quiet mode; opposite of -S",
"--replay-gain track|album|off  Default: off (sox, rec), track (play)",
"-R                       Use default random numbers (same on each run of SoX)",
"-S, --show-progress      Display progress while processing audio data",
"--version                Display version number of SoX and exit",
"-V[LEVEL]                Increment or set verbosity level (default 2); levels:",
"                           1: failure messages",
"                           2: warnings",
"                           3: details of processing",
"                           4-6: increasing levels of debug messages",
"FORMAT OPTIONS (fopts):",
"Input file format options need only be supplied for files that are headerless.",
"Output files will have the same format as the input file where possible and not",
"overriden by any of various means including providing output format options.",
"",
"-v|--volume FACTOR       Input file volume adjustment factor (real number)",
"-t|--type FILETYPE       File type of audio",
"-s/-u/-f/-U/-A/-i/-a/-g  Encoding type=signed-integer/unsigned-integer/floating-",
"                         point/mu-law/a-law/ima-adpcm/ms-adpcm/gsm-full-rate",
"-e|--encoding ENCODING   Set encoding (ENCODING in above list)",
"-b|--bits BITS           Encoded sample size in bits",
"-1/-2/-3/-4/-8           Encoded sample size in bytes",
"-N|--reverse-nibbles     Encoded nibble-order",
"-X|--reverse-bits        Encoded bit-order",
"--endian little|big|swap Encoded byte-order; swap means opposite to default",
"-L/-B/-x                 Short options for the above",
"-c|--channels CHANNELS   Number of channels of audio data; e.g. 2 = stereo",
"-r|--rate RATE           Sample rate of audio",
"-C|--compression FACTOR  Compression factor for output format",
"--add-comment TEXT       Append output file comment",
"--comment TEXT           Specify comment text for the output file",
"--comment-file FILENAME  File containing comment text for the output file",
""};

  if (!(sox_globals.verbosity > 2)) {
    display_SoX_version(stdout);
    putchar('\n');
  }

  if (message)
    fprintf(stderr, "Failed: %s\n\n", message);  /* N.B. stderr */

  printf("Usage summary: [gopts] [[fopts] infile]... [fopts]%s [effect [effopts]]...\n\n",
         sox_mode == sox_play? "" : " outfile");
  for (i = 0; i < array_length(lines); ++i)
    puts(lines[i]);
  display_supported_formats();
  display_supported_effects();
  printf("EFFECT OPTIONS (effopts): effect dependent; see --help-effect\n");
  exit(message != NULL);
}

static void usage_effect(char const * name)
{
  int i;

  display_SoX_version(stdout);
  putchar('\n');

  if (strcmp("all", name) && !sox_find_effect(name)) {
    printf("Cannot find an effect called `%s'.\n", name);
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

static void usage_format1(sox_format_handler_t const * f)
{
  char const * const * names;

  printf("\nFormat: %s\n", f->names[0]);
  printf("Description: %s\n", f->description);
  if (f->names[1]) {
    printf("Also handles:");
    for (names = f->names + 1; *names; ++names)
      printf(" %s", *names);
    putchar('\n');
  }
  if (f->flags & SOX_FILE_CHANS) {
    printf("Channels restricted to:");
    if (f->flags & SOX_FILE_MONO) printf(" mono");
    if (f->flags & SOX_FILE_STEREO) printf(" stereo");
    if (f->flags & SOX_FILE_QUAD) printf(" quad");
    putchar('\n');
  }
  if (f->write_rates) {
    sox_rate_t const * p = f->write_rates;
    printf("Sample-rate restricted to:");
    while (*p)
      printf(" %g", *p++);
    putchar('\n');
  }
  printf("Reads: %s\n", f->startread || f->read? "yes" : "no");
  if (f->startwrite || f->write) {
    if (f->write_formats) {
      sox_encoding_t e;
      unsigned i, s;
#define enc_arg(T) (T)f->write_formats[i++]
      i = 0;
      puts("Writes:");
      while ((e = enc_arg(sox_encoding_t)))
        do {
          s = enc_arg(unsigned);
          if (sox_precision(e, s)) {
            printf("  ");
            if (s)
              printf("%2u-bit ", s);
            printf("%s (%u-bit precision)\n", sox_encodings_info[e].desc, sox_precision(e, s));
          }
        } while (s);
      }
      else puts("Writes: yes");
    }
  else puts("Writes: no");
}

static void usage_format(char const * name)
{
  sox_format_handler_t const * f;
  unsigned i;

  display_SoX_version(stdout);

  if (strcmp("all", name)) {
    if (!(f = sox_find_format(name, sox_false))) {
      printf("Cannot find a format called `%s'.\n", name);
      display_supported_formats();
    }
    else usage_format1(f);
  }
  else {
    for (i = 0; sox_format_fns[i].fn; ++i) {
      sox_format_handler_t const * f = sox_format_fns[i].fn();
      if (!(f->flags & SOX_FILE_PHONY))
        usage_format1(f);
    }
  }
  exit(1);
}

static void read_comment_file(sox_comments_t * comments, char const * const filename)
{
  int c;
  size_t text_length = 100;
  char * text = lsx_malloc(text_length + 1);
  FILE * file = fopen(filename, "rt");

  if (file == NULL) {
    lsx_fail("Cannot open comment file %s", filename);
    exit(1);
  }
  do {
    size_t i = 0;

    while ((c = getc(file)) != EOF && !strchr("\r\n", c)) {
      if (i == text_length)
        text = lsx_realloc(text, (text_length <<= 1) + 1);
      text[i++] = c;
    }
    if (ferror(file)) {
      lsx_fail("Error reading comment file %s", filename);
      exit(1);
    }
    if (i) {
      text[i] = '\0';
      sox_append_comment(comments, text);
    }
  } while (c != EOF);

  fclose(file);
  free(text);
}

static char *getoptstr = "+ab:c:de:fghimnopqr:st:uv:xABC:LMNRSTUV::X12348";

static struct option long_options[] =
  {
    {"add-comment"     , required_argument, NULL, 0},
    {"buffer"          , required_argument, NULL, 0},
    {"combine"         , required_argument, NULL, 0},
    {"comment-file"    , required_argument, NULL, 0},
    {"comment"         , required_argument, NULL, 0},
    {"endian"          , required_argument, NULL, 0},
    {"input-buffer"    , required_argument, NULL, 0},
    {"interactive"     ,       no_argument, NULL, 0},
    {"help-effect"     , required_argument, NULL, 0},
    {"help-format"     , required_argument, NULL, 0},
    {"plot"            , required_argument, NULL, 0},
    {"replay-gain"     , required_argument, NULL, 0},
    {"version"         ,       no_argument, NULL, 0},
    {"output"          , required_argument, NULL, 0},
    {"effects-file"    , required_argument, NULL, 0},

    {"bits"            , required_argument, NULL, 'b'},
    {"channels"        , required_argument, NULL, 'c'},
    {"compression"     , required_argument, NULL, 'C'},
    {"default-device"  ,       no_argument, NULL, 'd'},
    {"encoding"        , required_argument, NULL, 'e'},
    {"help"            ,       no_argument, NULL, 'h'},
    {"null"            ,       no_argument, NULL, 'n'},
    {"no-show-progress",       no_argument, NULL, 'q'},
    {"pipe"            ,       no_argument, NULL, 'p'},
    {"rate"            , required_argument, NULL, 'r'},
    {"reverse-bits"    ,       no_argument, NULL, 'X'},
    {"reverse-nibbles" ,       no_argument, NULL, 'N'},
    {"show-progress"   ,       no_argument, NULL, 'S'},
    {"type"            , required_argument, NULL, 't'},
    {"volume"          , required_argument, NULL, 'v'},

    {NULL, 0, NULL, 0}
  };

static int opt_index(int val)
{
  int i;
  for (i = 0; long_options[i].name; ++i)
    if (long_options[i].val == val)
      return i;
  return -1;
}

static lsx_enum_item const combine_methods[] = {
  LSX_ENUM_ITEM(sox_,sequence)
  LSX_ENUM_ITEM(sox_,concatenate)
  LSX_ENUM_ITEM(sox_,mix)
  {"mix-power", sox_mix_power},
  LSX_ENUM_ITEM(sox_,merge)
  LSX_ENUM_ITEM(sox_,multiply)
  {0, 0}};

enum {ENDIAN_little, ENDIAN_big, ENDIAN_swap};
static lsx_enum_item const endian_options[] = {
  LSX_ENUM_ITEM(ENDIAN_,little)
  LSX_ENUM_ITEM(ENDIAN_,big)
  LSX_ENUM_ITEM(ENDIAN_,swap)
  {0, 0}};

static lsx_enum_item const plot_methods[] = {
  LSX_ENUM_ITEM(sox_plot_,off)
  LSX_ENUM_ITEM(sox_plot_,octave)
  LSX_ENUM_ITEM(sox_plot_,gnuplot)
  {0, 0}};

enum {
  encoding_signed_integer, encoding_unsigned_integer, encoding_floating_point,
  encoding_ms_adpcm, encoding_ima_adpcm, encoding_oki_adpcm,
  encoding_gsm_full_rate, encoding_u_law, encoding_a_law};

static lsx_enum_item const encodings[] = {
  {"signed-integer", encoding_signed_integer},
  {"unsigned-integer", encoding_unsigned_integer},
  {"floating-point", encoding_floating_point},
  {"ms-adpcm", encoding_ms_adpcm},
  {"ima-adpcm", encoding_ima_adpcm},
  {"oki-adpcm", encoding_oki_adpcm},
  {"gsm-full-rate", encoding_gsm_full_rate},
  {"u-law", encoding_u_law},
  {"mu-law", encoding_u_law},
  {"a-law", encoding_a_law},
  {0, 0}};

static int enum_option(int option_index, lsx_enum_item const * items)
{
  lsx_enum_item const * p = lsx_find_enum_text(optarg, items);
  if (p == NULL) {
    size_t len = 1;
    char * set = lsx_malloc(len);
    *set = 0;
    for (p = items; p->text; ++p) {
      set = lsx_realloc(set, len += 2 + strlen(p->text));
      strcat(set, ", "); strcat(set, p->text);
    }
    lsx_fail("--%s: '%s' is not one of: %s.",
        long_options[option_index].name, optarg, set + 2);
    free(set);
    exit(1);
  }
  return p->value;
}

static char parse_gopts_and_fopts(file_t * f, int argc, char **argv)
{
  while (sox_true) {
    int c, option_index;
    int i; /* sscanf silently accepts negative numbers for %u :( */
    char dummy;     /* To check for extraneous chars in optarg. */

    switch (c=getopt_long(argc, argv, getoptstr, long_options, &option_index)) {
    case -1:        /* @ one of: file-name, effect name, end of arg-list. */
      return '\0'; /* i.e. not device. */

    case 0:         /* Long options with no short equivalent. */
      switch (option_index) {
      case 0:
        if (optarg)
          sox_append_comment(&f->oob.comments, optarg);
        break;

      case 1:
#define SOX_BUFMIN 16
        if (sscanf(optarg, "%i %c", &i, &dummy) != 1 || i <= SOX_BUFMIN) {
          lsx_fail("Buffer size `%s' must be > %d", optarg, SOX_BUFMIN);
          exit(1);
        }
        sox_globals.bufsiz = i;
        break;

      case 2:
        combine_method = enum_option(option_index, combine_methods);
        break;

      case 3:
        sox_append_comment(&f->oob.comments, "");
        read_comment_file(&f->oob.comments, optarg);
        break;

      case 4:
        sox_append_comment(&f->oob.comments, "");
        if (optarg)
          sox_append_comment(&f->oob.comments, optarg);
        break;

      case 5:
        if (f->encoding.reverse_bytes != SOX_OPTION_DEFAULT || f->encoding.opposite_endian)
          usage("only one endian option per file is allowed");
        switch (enum_option(option_index, endian_options)) {
          case ENDIAN_little: f->encoding.reverse_bytes = MACHINE_IS_BIGENDIAN; break;
          case ENDIAN_big: f->encoding.reverse_bytes = MACHINE_IS_LITTLEENDIAN; break;
          case ENDIAN_swap: f->encoding.opposite_endian = sox_true; break;
        }
        break;

      case 6:
        if (sscanf(optarg, "%i %c", &i, &dummy) != 1 || i <= SOX_BUFMIN) {
          lsx_fail("Buffer size `%s' must be > %d", optarg, SOX_BUFMIN);
          exit(1);
        }
        sox_globals.input_bufsiz = i;
        break;

      case 7:
        interactive = sox_true;
        break;

      case 8:
        usage_effect(optarg);
        break;

      case 9:
        usage_format(optarg);
        break;

      case 10:
        sox_effects_globals.plot = enum_option(option_index, plot_methods);
        break;

      case 11:
        replay_gain_mode = enum_option(option_index, rg_modes);
        break;

      case 12:
        display_SoX_version(stdout);
        exit(0);
        break;

      case 14:
        effects_filename = strdup(optarg);
        break;

      }
      break;

    case 'm': combine_method = sox_mix; break;
    case 'M': combine_method = sox_merge; break;
    case 'T': combine_method = sox_multiply; break;

    case 'R':                   /* Useful for regression testing. */
      sox_globals.repeatable = sox_true;
      break;

    case 'd': case 'n': case 'p':
      return c;
      break;

    case 'h': 
      usage(NULL);
      break;

    case '?':
      usage("invalid option");              /* No return */
      break;

    case 't':
      f->filetype = optarg;
      if (f->filetype[0] == '.')
        f->filetype++;
      break;

    case 'r': {
      char k = 0;
      size_t n = sscanf(optarg, "%lf %c %c", &f->signal.rate, &k, &dummy);
      if (n < 1 || f->signal.rate <= 0 || (n > 1 && k != 'k') || n > 2) {
        lsx_fail("Rate value `%s' is not a positive number", optarg);
        exit(1);
      }
      f->signal.rate *= k == 'k'? 1000. : 1.;
      break;
    }

    case 'v':
      if (sscanf(optarg, "%lf %c", &f->volume, &dummy) != 1) {
        lsx_fail("Volume value `%s' is not a number", optarg);
        exit(1);
      }
      uservolume = sox_true;
      if (f->volume < 0.0)
        lsx_report("Volume adjustment is negative; "
                  "this will result in a phase change");
      break;

    case 'c':
      if (sscanf(optarg, "%i %c", &i, &dummy) != 1 || i <= 0) {
        lsx_fail("Channels value `%s' is not a positive integer", optarg);
        exit(1);
      }
      f->signal.channels = i;
      break;

    case 'C':
      if (sscanf(optarg, "%lf %c", &f->encoding.compression, &dummy) != 1) {
        lsx_fail("Compression value `%s' is not a number", optarg);
        exit(1);
      }
      break;

    case 'b':
      if (sscanf(optarg, "%i %c", &i, &dummy) != 1 || i <= 0) {
        lsx_fail("Bits value `%s' is not a positive integer", optarg);
        exit(1);
      }
      f->encoding.bits_per_sample = i;
      break;

    case 'e': switch (enum_option(opt_index('e'), encodings)) {
      case encoding_signed_integer:   f->encoding.encoding = SOX_ENCODING_SIGN2;     break;
      case encoding_unsigned_integer: f->encoding.encoding = SOX_ENCODING_UNSIGNED;  break;
      case encoding_floating_point:   f->encoding.encoding = SOX_ENCODING_FLOAT;     break;
      case encoding_ms_adpcm:         f->encoding.encoding = SOX_ENCODING_MS_ADPCM;  break;
      case encoding_ima_adpcm:        f->encoding.encoding = SOX_ENCODING_IMA_ADPCM; break;
      case encoding_oki_adpcm:        f->encoding.encoding = SOX_ENCODING_OKI_ADPCM; break;
      case encoding_gsm_full_rate:           f->encoding.encoding = SOX_ENCODING_GSM;       break;
      case encoding_u_law: f->encoding.encoding = SOX_ENCODING_ULAW;
        if (f->encoding.bits_per_sample == 0)
          f->encoding.bits_per_sample = 8;
        break;
      case encoding_a_law: f->encoding.encoding = SOX_ENCODING_ALAW;
        if (f->encoding.bits_per_sample == 0)
          f->encoding.bits_per_sample = 8;
        break;
      }
      break;

    case '1': f->encoding.bits_per_sample = 8;  break;
    case '2': f->encoding.bits_per_sample = 16; break;
    case '3': f->encoding.bits_per_sample = 24; break;
    case '4': f->encoding.bits_per_sample = 32; break;
    case '8': f->encoding.bits_per_sample = 64; break;

    case 's': f->encoding.encoding = SOX_ENCODING_SIGN2;     break;
    case 'u': f->encoding.encoding = SOX_ENCODING_UNSIGNED;  break;
    case 'f': f->encoding.encoding = SOX_ENCODING_FLOAT;     break;
    case 'a': f->encoding.encoding = SOX_ENCODING_MS_ADPCM;  break;
    case 'i': f->encoding.encoding = SOX_ENCODING_IMA_ADPCM; break;
    case 'o': f->encoding.encoding = SOX_ENCODING_OKI_ADPCM; break;
    case 'g': f->encoding.encoding = SOX_ENCODING_GSM;       break;

    case 'U': f->encoding.encoding = SOX_ENCODING_ULAW;
      if (f->encoding.bits_per_sample == 0)
        f->encoding.bits_per_sample = 8;
      break;

    case 'A': f->encoding.encoding = SOX_ENCODING_ALAW;
      if (f->encoding.bits_per_sample == 0)
        f->encoding.bits_per_sample = 8;
      break;

    case 'L': case 'B': case 'x':
      if (f->encoding.reverse_bytes != SOX_OPTION_DEFAULT || f->encoding.opposite_endian)
        usage("only one endian option per file is allowed");
      switch (c) {
        case 'L': f->encoding.reverse_bytes   = MACHINE_IS_BIGENDIAN;    break;
        case 'B': f->encoding.reverse_bytes   = MACHINE_IS_LITTLEENDIAN; break;
        case 'x': f->encoding.opposite_endian = sox_true;            break;
      }
      break;
    case 'X': f->encoding.reverse_bits    = SOX_OPTION_YES;      break;
    case 'N': f->encoding.reverse_nibbles = SOX_OPTION_YES;      break;

    case 'S': show_progress = SOX_OPTION_YES; break;
    case 'q': show_progress = SOX_OPTION_NO;  break;

    case 'V':
      if (optarg == NULL)
        ++sox_globals.verbosity;
      else {
        if (sscanf(optarg, "%i %c", &i, &dummy) != 1 || i < 0) {
          sox_globals.verbosity = 2;
          lsx_fail("Verbosity value `%s' is not a non-negative integer", optarg);
          exit(1);
        }
        sox_globals.verbosity = (unsigned)i;
      }
      break;
    }
  }
}

static char const * device_name(char const * const type)
{
  char * name = NULL, * from_env = getenv("AUDIODEV");

  if (!type)
    return NULL;
  if (!strcmp(type, "sunau")) name = "/dev/audio";
  else if (!strcmp(type, "oss" ) || !strcmp(type, "ossdsp")) name = "/dev/dsp";
  else if (!strcmp(type, "alsa") || !strcmp(type, "ao") || !strcmp(type, "coreaudio")) name = "default";
  return name? from_env? from_env : name : NULL;
}

static char const * set_default_device(file_t * f)
{
  /* Default audio driver type in order of preference: */
  if (!f->filetype) f->filetype = getenv("AUDIODRIVER");
  if (!f->filetype && sox_find_format("coreaudio", sox_false)) f->filetype = "coreaudio";
  if (!f->filetype && sox_find_format("alsa", sox_false)) f->filetype = "alsa";
  if (!f->filetype && sox_find_format("oss" , sox_false)) f->filetype = "oss";
  if (!f->filetype && sox_find_format("sunau",sox_false)) f->filetype = "sunau";
  if (!f->filetype && sox_find_format("ao"  , sox_false) && file_count) /*!rec*/
    f->filetype = "ao";

  if (!f->filetype) {
    lsx_fail("Sorry, there is no default audio device configured");
    exit(1);
  }
  return device_name(f->filetype);
}

static int add_file(file_t const * const opts, char const * const filename)
{
  file_t * f = lsx_malloc(sizeof(*f));

  *f = *opts;
  if (!filename)
    usage("missing filename"); /* No return */
  f->filename = lsx_strdup(filename);
  files = lsx_realloc(files, (file_count + 1) * sizeof(*files));
  files[file_count++] = f;
  return 0;
}

static void init_file(file_t * f)
{
  memset(f, 0, sizeof(*f));
  sox_init_encodinginfo(&f->encoding);
  f->volume = HUGE_VAL;
  f->replay_gain = HUGE_VAL;
}

static void parse_options_and_filenames(int argc, char **argv)
{
  file_t opts, opts_none;
  init_file(&opts), init_file(&opts_none);

  if (sox_mode == sox_rec)
    add_file(&opts, set_default_device(&opts)), init_file(&opts);

  for (; optind < argc && !sox_find_effect(argv[optind]); init_file(&opts)) {
    char c = parse_gopts_and_fopts(&opts, argc, argv);
    if (c == 'n') { /* is null file? */
      if (opts.filetype != NULL && strcmp(opts.filetype, "null") != 0)
        lsx_warn("ignoring `-t %s'.", opts.filetype);
      opts.filetype = "null";
      add_file(&opts, "");
    }
    else if (c == 'd') /* is default device? */
      add_file(&opts, set_default_device(&opts));
    else if (c == 'p') { /* is sox pipe? */
      if (opts.filetype != NULL && strcmp(opts.filetype, "sox") != 0)
        lsx_warn("ignoring `-t %s'.", opts.filetype);
      opts.filetype = "sox";
      add_file(&opts, "-");
    }
    else if (optind >= argc || sox_find_effect(argv[optind]))
      break;
    else if (!sox_is_playlist(argv[optind]))
      add_file(&opts, argv[optind++]);
    else if (sox_parse_playlist((sox_playlist_callback_t)add_file, &opts, argv[optind++]) != SOX_SUCCESS)
      exit(1);
  }
  if (sox_mode == sox_play)
    add_file(&opts, set_default_device(&opts));
  else if (memcmp(&opts, &opts_none, sizeof(opts))) /* fopts but no file */
    add_file(&opts, device_name(opts.filetype));
}

typedef enum {Full,
  Type, Rate, Channels, Samples, Duration, Bits, Encoding, Annotation} soxi_t;

static int soxi1(soxi_t * type, char * filename)
{
  size_t ws;
  sox_format_t * ft = sox_open_read(filename, NULL, NULL, NULL);

  if (!ft)
    return 1;
  ws = ft->signal.length / max(ft->signal.channels, 1);
  switch (*type) {
    case Type: printf("%s\n", ft->filetype); break;
    case Rate: printf("%g\n", ft->signal.rate); break;
    case Channels: printf("%u\n", ft->signal.channels); break;
    case Samples: printf("%lu\n", (unsigned long)ws); break;
    case Duration: printf("%s\n", str_time((double)ws / max(ft->signal.rate, 1))); break;
    case Bits: printf("%u\n", ft->encoding.bits_per_sample); break;
    case Encoding: printf("%s\n", sox_encodings_info[ft->encoding.encoding].desc); break;
    case Annotation: if (ft->oob.comments) {
      sox_comments_t p = ft->oob.comments;
      do printf("%s\n", *p); while (*++p);
    }
    break;
    case Full: display_file_info(ft, NULL, sox_false); break;
  }
  return !!sox_close(ft);
}

static int soxi(int argc, char * const * argv)
{
  static char const opts[] = "trcsdbea?V::";
  soxi_t type = Full;
  int opt, num_errors = 0;

  while ((opt = getopt(argc, argv, opts)) > 0) /* act only on last option */
    if (opt == 'V') {
      int i; /* sscanf silently accepts negative numbers for %u :( */
      char dummy;     /* To check for extraneous chars in optarg. */
      if (optarg == NULL)
        ++sox_globals.verbosity;
      else {
        if (sscanf(optarg, "%i %c", &i, &dummy) != 1 || i < 0) {
          sox_globals.verbosity = 2;
          lsx_fail("Verbosity value `%s' is not a non-negative integer", optarg);
          exit(1);
        }
        sox_globals.verbosity = (unsigned)i;
      }
    } else type = 1 + (strchr(opts, opt) - opts);
  if (type > Annotation)
    printf("Usage: soxi [-V[level]] [-t|-r|-c|-s|-d|-b|-e|-a] infile1 ...\n");
  else for (; optind < argc; ++optind) {
    if (sox_is_playlist(argv[optind]))
      num_errors += (sox_parse_playlist((sox_playlist_callback_t)soxi1, &type, argv[optind]) != SOX_SUCCESS);
    else num_errors += soxi1(&type, argv[optind]);
  }
  return num_errors;
}

static void set_replay_gain(sox_comments_t comments, file_t * f)
{
  rg_mode rg = replay_gain_mode;
  int try = 2; /* Will try to find the other GAIN if preferred one not found */
  size_t i, n = sox_num_comments(comments);

  if (rg != RG_off) while (try--) {
    char const * target =
      rg == RG_track? "REPLAYGAIN_TRACK_GAIN=" : "REPLAYGAIN_ALBUM_GAIN=";
    for (i = 0; i < n; ++i) {
      if (strncasecmp(comments[i], target, strlen(target)) == 0) {
        f->replay_gain = atof(comments[i] + strlen(target));
        f->replay_gain_mode = rg;
        return;
      }
    }
    rg ^= RG_track ^ RG_album;
  }
}

static void output_message(unsigned level, const char *filename, const char *fmt, va_list ap)
{
  if (sox_globals.verbosity >= level) {
    fprintf(stderr, "%s ", myname);
    sox_output_message(stderr, filename, fmt, ap);
    fprintf(stderr, "\n");
  }
}

static sox_bool cmp_comment_text(char const * c1, char const * c2)
{
  return c1 && c2 && !strcasecmp(c1, c2);
}

static int strends(char const * str, char const * end)
{
  size_t str_len = strlen(str), end_len = strlen(end);
  return str_len >= end_len && !strcmp(str + str_len - end_len, end);
}

int main(int argc, char **argv)
{
  size_t i;
#ifdef INTERACTIVE
  int flags;
#endif

  myname = argv[0];
  atexit(cleanup);
  sox_globals.output_message_handler = output_message;

  if (strends(myname, "play")) {
    sox_mode = sox_play;
    replay_gain_mode = RG_track;
    combine_method = sox_sequence;
  }
  else if (strends(myname, "rec"))
    sox_mode = sox_rec;
  else if (strends(myname, "soxi"))
    sox_mode = sox_soxi;

  if (sox_format_init() != SOX_SUCCESS)
    exit(1);

  if (sox_mode == sox_soxi)
    exit(soxi(argc, argv));

#ifdef INTERACTIVE
  /* Turn off blocking on stdin so we can be fully interactive. */
  flags = fcntl(STDIN_FILENO, F_GETFL);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
#endif

  parse_options_and_filenames(argc, argv);

  if (sox_globals.verbosity > 2)
    display_SoX_version(stderr);

  input_count = file_count ? file_count - 1 : 0;

  /* Allow e.g. known length processing in this case */
  if (combine_method == sox_sequence && input_count == 1)
    combine_method = sox_concatenate;

  /* Make sure we got at least the required # of input filenames */
  if (input_count < (is_serial(combine_method) ? 1 : 2))
    usage("Not enough input filenames specified");

  /* Check for misplaced input/output-specific options */
  for (i = 0; i < input_count; ++i) {
    if (files[i]->encoding.compression != HUGE_VAL)
      usage("A compression factor can be given only for an output file");
    if (files[i]->oob.comments != NULL)
      usage("Comments can be given only for an output file");
  }
  if (ofile->volume != HUGE_VAL)
    usage("-v can be given only for an input file;\n"
            "\tuse `vol' to set the output file volume");

  signal(SIGINT, SIG_IGN); /* So child pipes aren't killed by track skip */
  for (i = 0; i < input_count; i++) {
    int j = input_count - 1 - i; /* Open in reverse order 'cos of rec (below) */
    file_t * f = files[j];

    /* When mixing audio, default to input side volume adjustments that will
     * make sure no clipping will occur.  Users probably won't be happy with
     * this, and will override it, possibly causing clipping to occur. */
    if (combine_method == sox_mix && !uservolume)
      f->volume = 1.0 / input_count;
    else if (combine_method == sox_mix_power && !uservolume)
      f->volume = 1.0 / sqrt((double)input_count);

    if (sox_mode == sox_rec && !j) {       /* Set the recording parameters: */
      if (input_count > 1)                 /* from the (just openned) next */
        f->signal = files[1]->ft->signal;  /* input file, or from the output */
      else f->signal = files[1]->signal;   /* file (which is not open yet). */
    }
    files[j]->ft = sox_open_read(f->filename, &f->signal, &f->encoding, f->filetype);
    if (!files[j]->ft)
      /* sox_open_read() will call lsx_warn for most errors.
       * Rely on that printing something. */
      exit(2);
    if (show_progress == SOX_OPTION_DEFAULT &&
        (files[j]->ft->handler.flags & SOX_FILE_DEVICE) != 0 &&
        (files[j]->ft->handler.flags & SOX_FILE_PHONY) == 0)
      show_progress = SOX_OPTION_YES;
  }
  /* Simple heuristic to determine if replay-gain should be in album mode */
  if (sox_mode == sox_play && replay_gain_mode == RG_track && input_count > 1 &&
      cmp_comment_text(
        sox_find_comment(files[0]->ft->oob.comments, "artist"),
        sox_find_comment(files[1]->ft->oob.comments, "artist")) &&
      cmp_comment_text(
        sox_find_comment(files[0]->ft->oob.comments, "album"),
        sox_find_comment(files[1]->ft->oob.comments, "album")))
    replay_gain_mode = RG_album;

  for (i = 0; i < input_count; i++)
    set_replay_gain(files[i]->ft->oob.comments, files[i]);
  signal(SIGINT, SIG_DFL);

  init_eff_chains();

  /* Loop through the rest of the arguments looking for effects */
  parse_effects(argc, argv);
  if (eff_chain_count == 0 || nuser_effects[eff_chain_count] > 0)
  {
    eff_chain_count++;
    /* Note: Purposely not calling add_eff_chain() to save some
     * memory although it would be more consistent to do so.
     */
  }

  /* Not the best way for users to do this; now deprecated in favour of soxi. */
  if (!show_progress && !nuser_effects[current_eff_chain] && 
      ofile->filetype && !strcmp(ofile->filetype, "null")) {
    for (i = 0; i < input_count; i++)
      report_file_info(files[i]);
    exit(0);
  }

  if (sox_globals.repeatable)
    lsx_debug("Not reseeding PRNG; randomness is repeatable");
  else {
    time_t t;

    time(&t); 
    srand((unsigned)t);
  }

  /* Save things that sox_sequence needs to be reinitialised for each segued
   * block of input files.*/
  ofile_signal_options = ofile->signal;
  ofile_encoding_options = ofile->encoding;

  /* If user specified an effects filename then use that file
   * to load user effects.  Free any previously specified options
   * from the command line.  
   */
  if (effects_filename)
  {
    read_user_effects(effects_filename);
  }

  while (process() != SOX_EOF && !user_abort && current_input < input_count)
  {
    if (advance_eff_chain() == SOX_EOF)
      break;

    if (!save_output_eff)
    {
      sox_close(ofile->ft);
      ofile->ft = NULL;
    }
  }

  sox_delete_effects_chain(effects_chain);
  delete_eff_chains();

  for (i = 0; i < file_count; ++i)
    if (files[i]->ft->clips != 0)
      lsx_warn(i < input_count?"%s: input clipped %lu samples" :
                              "%s: output clipped %lu samples; decrease volume?",
          (files[i]->ft->handler.flags & SOX_FILE_DEVICE)?
                       files[i]->ft->handler.names[0] : files[i]->ft->filename,
          (unsigned long)files[i]->ft->clips);

  if (mixing_clips > 0)
    lsx_warn("mix-combining clipped %lu samples; decrease volume?", (unsigned long)mixing_clips);

  for (i = 0; i < file_count; i++)
    if (files[i]->volume_clips > 0)
      lsx_warn("%s: balancing clipped %lu samples; decrease volume?",
          files[i]->filename, (unsigned long)files[i]->volume_clips);

  if (show_progress) {
    if (user_abort)
      fprintf(stderr, "Aborted.\n");
    else if (user_skip && sox_mode != sox_rec)
      fprintf(stderr, "Skipped.\n");
    else
      fprintf(stderr, "Done.\n");
  }

  success = 1; /* Signal success to cleanup so the output file isn't removed. */
  return 0;
}
