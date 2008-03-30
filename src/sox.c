/* SoX - The Swiss Army Knife of Audio Manipulation.
 *
 * This is the main function for the command line sox program.
 *
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * Copyright 1998-2007 Chris Bagwell and SoX contributors
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
#include "getopt.h"

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
static enum {sox_sox, sox_play, sox_rec, sox_soxi} sox_mode;


/* gopts */

static enum {sox_sequence, sox_concatenate, sox_mix, sox_merge, sox_multiply}
    combine_method = sox_concatenate;
static sox_bool interactive = sox_false;
static sox_bool uservolume = sox_false;
typedef enum {RG_off, RG_track, RG_album} rg_mode;
static enum_item const rg_modes[] = {
  ENUM_ITEM(RG_,off)
  ENUM_ITEM(RG_,track)
  ENUM_ITEM(RG_,album)
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
  sox_size_t volume_clips;
  rg_mode replay_gain_mode;
} file_t;

#define MAX_INPUT_FILES 32
#define MAX_FILES MAX_INPUT_FILES + 2 /* 1 output file plus record input */
static file_t * files[MAX_FILES]; /* Array tracking input and output files */
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
static sox_effect_t * user_efftab[MAX_USER_EFF];
static unsigned nuser_effects;
static sox_effects_chain_t ofile_effects_chain;


/* Flowing */

static sox_signalinfo_t combiner_signal, ofile_signal;
static sox_encodinginfo_t combiner_encoding, ofile_encoding;
static sox_size_t mixing_clips = 0;
static size_t current_input = 0;
static unsigned long input_wide_samples = 0;
static unsigned long read_wide_samples = 0;
static unsigned long output_samples = 0;
static sox_bool user_abort = sox_false;
static sox_bool user_skip = sox_false;
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

static char const * str_time(double duration)
{
  static char string[16][50];
  static int i;
  int mins = duration / 60;
  i = (i+1) & 15;
  sprintf(string[i], "%02i:%05.2f", mins, duration - mins * 60);
  return string[i];
}

static void play_file_info(sox_format_t * ft, file_t * f, sox_bool full)
{
  FILE * const output = sox_mode == sox_soxi? stdout : stderr;
  char const * text;
  char buffer[30];
  sox_size_t ws = ft->signal.length / ft->signal.channels;
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
    sprintf(buffer, "%s gain: %+.1fdB", find_enum_value(f->replay_gain_mode, rg_modes)->text, f->replay_gain);
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

  if (sox_mode == sox_play && sox_globals.verbosity < 3) {
    play_file_info(ft, f, full);
    return;
  }

  fprintf(output, "\n%s: '%s'",
    ft->mode == 'r'? "Input File     " : "Output File    ", ft->filename);
  if (strcmp(ft->filename, "-") == 0 || (ft->handler.flags & SOX_FILE_DEVICE))
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
    sox_size_t ws = ft->signal.length / ft->signal.channels;
    fprintf(output,
      "Duration       : %s = %u samples %c %g CDDA sectors\n",
      str_time((double)ws / ft->signal.rate),
      ws, "~="[ft->signal.rate == 44100],
      (double)ws/ ft->signal.rate * 44100 / 588);
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
        find_enum_value(f->replay_gain_mode, rg_modes)->text);
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
  sox_fail("%s: %s: %s", ft->filename, ft->sox_errstr,
      ft->sox_errno < SOX_EHDR?
      strerror(ft->sox_errno) : sox_strerror[ft->sox_errno - SOX_EHDR]);
}

static void progress_to_file(file_t * f)
{
  if (user_skip) {
    user_skip = sox_false;
    fprintf(stderr, "\nSkipped (Ctrl-C twice to quit).\n");
  }
  read_wide_samples = 0;
  input_wide_samples = f->ft->signal.length / f->ft->signal.channels;
  if (show_progress && (sox_globals.verbosity < 3 ||
                        (combine_method <= sox_concatenate && input_count > 1)))
    display_file_info(f->ft, f, sox_false);
  if (f->volume == HUGE_VAL)
    f->volume = 1;
  if (f->replay_gain != HUGE_VAL)
    f->volume *= pow(10.0, f->replay_gain / 20);
  f->ft->sox_errno = errno = 0;
}

static sox_size_t sox_read_wide(sox_format_t * ft, sox_sample_t * buf, sox_size_t max)
{
  sox_size_t len = max / combiner_signal.channels;
  len = sox_read(ft, buf, len * ft->signal.channels) / ft->signal.channels;
  if (!len && ft->sox_errno)
    display_error(ft);
  return len;
}

static void balance_input(sox_sample_t * buf, sox_size_t ws, file_t * f)
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
  sox_sample_t *ibuf[MAX_INPUT_FILES];
} * input_combiner_t;

static int combiner_start(sox_effect_t *effp)
{
  input_combiner_t z = (input_combiner_t) effp->priv;
  sox_size_t ws, i;

  if (combine_method <= sox_concatenate)
    progress_to_file(files[current_input]);
  else {
    ws = 0;
    for (i = 0; i < input_count; i++) {
      z->ibuf[i] = (sox_sample_t *)lsx_malloc(sox_globals.bufsiz * sizeof(sox_sample_t));
      progress_to_file(files[i]);
      ws = max(ws, input_wide_samples);
    }
    input_wide_samples = ws; /* Output length is that of longest input file. */
  }
  return SOX_SUCCESS;
}

static sox_bool can_segue(sox_size_t i)
{
  return
    files[i]->ft->signal.channels == files[i - 1]->ft->signal.channels &&
    files[i]->ft->signal.rate     == files[i - 1]->ft->signal.rate;
}

static int combiner_drain(sox_effect_t *effp, sox_sample_t * obuf, sox_size_t * osamp)
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
    sox_sample_t * p = obuf;
    for (i = 0; i < input_count; ++i) {
      ilen[i] = sox_read_wide(files[i]->ft, z->ibuf[i], *osamp);
      balance_input(z->ibuf[i], ilen[i], files[i]);
      olen = max(olen, ilen[i]);
    }
    for (ws = 0; ws < olen; ++ws) /* wide samples */
      if (combine_method == sox_mix) {          /* sum samples together */
        for (s = 0; s < effp->in_signal.channels; ++s, ++p) {
          *p = 0;
          for (i = 0; i < input_count; ++i)
            if (ws < ilen[i] && s < files[i]->ft->signal.channels) {
              /* Cast to double prevents integer overflow */
              double sample = *p + (double)z->ibuf[i][ws * files[i]->ft->signal.channels + s];
              *p = SOX_ROUND_CLIP_COUNT(sample, mixing_clips);
          }
        }
      } else if (combine_method == sox_multiply) {    /* multiply samples */
        for (s = 0; s < effp->in_signal.channels; ++s, ++p) {
          i = 0;
          *p = ws < ilen[i] && s < files[i]->ft->signal.channels?
            z->ibuf[i][ws * files[i]->ft->signal.channels + s] : 0;
          for (++i; i < input_count; ++i) {
            double sample = *p * (-1. / SOX_SAMPLE_MIN) * (ws < ilen[i] && s < files[i]->ft->signal.channels? z->ibuf[i][ws * files[i]->ft->signal.channels + s] : 0);
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
  olen *= effp->in_signal.channels;
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
    0, combiner_start, 0, combiner_drain, combiner_stop, 0, 0
  };
  return &handler;
}

static int output_flow(sox_effect_t *effp, sox_sample_t const * ibuf,
    sox_sample_t * obuf, sox_size_t * isamp, sox_size_t * osamp)
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

static void add_auto_effect(sox_effects_chain_t * chain, char const * name, sox_signalinfo_t * signal)
{
  sox_effect_t * effp;

  /* Auto effect should always succeed here */
  effp = sox_create_effect(sox_find_effect(name));
  effp->handler.getopts(effp, 0, NULL);          /* Set up with default opts */

  /* But could fail here */
  if (sox_add_effect(chain, effp, signal, &ofile->ft->signal) != SOX_SUCCESS)
    exit(2);
}

/* If needed effects are not given, auto-add at (performance) optimal point. */
static void add_effects(sox_effects_chain_t * chain)
{
  sox_signalinfo_t signal = combiner_signal;
  unsigned i, min_chan = 0, min_rate = 0;
  sox_effect_t * effp;

  /* Find points after which we might add effects to change rate/chans */
  for (i = 0; i < nuser_effects; i++) {
    if (user_efftab[i]->handler.flags & (SOX_EFF_CHAN | SOX_EFF_MCHAN))
      min_chan = i + 1;
    if (user_efftab[i]->handler.flags & SOX_EFF_RATE)
      min_rate = i + 1;
  }
  /* 1st `effect' in the chain is the input combiner_signal */
  effp = sox_create_effect(input_combiner_effect_fn());
  sox_add_effect(chain, effp, &signal, &ofile->ft->signal);

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
      if (sox_add_effect(chain, user_efftab[i], &signal, &ofile->ft->signal) != SOX_SUCCESS)
        exit(2);
  }
  /* Add auto effects if still needed at this point */
  if (signal.rate != ofile->ft->signal.rate)
    add_auto_effect(chain, "resample", &signal);  /* Must be up-sampling */
  if (signal.channels != ofile->ft->signal.channels)
    add_auto_effect(chain, "mixer", &signal);     /* Must be increasing channels */

  /* Last `effect' in the chain is the output file */
  effp = sox_create_effect(output_effect_fn());
  if (sox_add_effect(chain, effp, &signal, &ofile->ft->signal) != SOX_SUCCESS)
    exit(2);

  for (i = 0; i < chain->length; ++i) {
    sox_effect_t const * effp = &chain->effects[i][0];
    sox_report("effects chain: %-10s %gHz %u channels %u bits %s",
        effp->handler.name, effp->in_signal.rate, effp->in_signal.channels, effp->in_signal.precision,
        (effp->handler.flags & SOX_EFF_MCHAN)? "(multi)" : "");
  }
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
    fprintf(stderr, "\r%-5s %s [%s] of %s Out:%-5s [%6s|%-6s]%s Clip:%-5s",
      sigfigs3p(percentage), str_time(read_time), str_time(left_time),
      str_time(in_time), sigfigs3(output_samples),
      vu(0), vu(1), headroom(), sigfigs3(total_clips()));
  }
  if (all_done)
    fputc('\n', stderr);
}

static int update_status(sox_bool all_done)
{
  display_status(all_done || user_abort);
  return user_abort? SOX_EOF : SOX_SUCCESS;
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
    if (files[0]->ft->handler.seek && files[0]->ft->seekable){
      sox_size_t offset = sox_trim_get_start(&ofile_effects_chain.effects[1][0]);
      if (offset && sox_seek(files[0]->ft, offset, SOX_SEEK_SET) == SOX_SUCCESS) {
        read_wide_samples = offset / files[0]->ft->signal.channels;
        /* Assuming a failed seek stayed where it was.  If the
         * seek worked then reset the start location of
         * trim so that it thinks user didn't request a skip.
         */
        sox_trim_clear_start(&ofile_effects_chain.effects[1][0]);
        sox_debug("optimize_trim successful");
      }
    }
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

static void open_output_file(void)
{
  double factor;
  int i;
  sox_comments_t p = ofile->oob.comments;
  sox_oob_t oob = files[0]->ft->oob;
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

  /*
   * copy loop info, resizing appropriately
   * it's in samples, so # channels don't matter
   * FIXME: This doesn't work for multi-file processing or
   * effects that change file length.
   */
  factor = (double) ofile->signal.rate / combiner_signal.rate;
  for (i = 0; i < SOX_MAX_NLOOPS; i++) {
    oob.loops[i].start = oob.loops[i].start * factor;
    oob.loops[i].length = oob.loops[i].length * factor;
  }

  ofile->ft = sox_open_write(ofile->filename, &ofile->signal, &ofile->encoding,
      ofile->filetype, &oob, overwrite_permitted);
  sox_delete_comments(&oob.comments);

  if (!ofile->ft)
    /* sox_open_write() will call sox_warn for most errors.
     * Rely on that printing something. */
    exit(2);

  /* When writing to an audio device, auto turn on the
   * progress display to match behavior of ogg123,
   * unless the user requested us not to display anything. */
  if (show_progress == SOX_OPTION_DEFAULT)
    show_progress = (ofile->ft->handler.flags & SOX_FILE_DEVICE) != 0 &&
                    (ofile->ft->handler.flags & SOX_FILE_PHONY) == 0;

  report_file_info(ofile);
}

static void sigint(int s)
{
  static struct timeval then;
  if (input_count > 1 && show_progress && s == SIGINT &&
      combine_method <= sox_concatenate && since(&then, 1.0, sox_true))
    user_skip = sox_true;
  else user_abort = sox_true;
}

/*
 * Process:   Input(s) -> Balancing -> Combiner -> Effects -> Output
 */

static int process(void) {
  int flowstatus = 0;
  sox_size_t i;
  sox_bool known_length = combine_method != sox_sequence;
  sox_size_t olen = 0;

  combiner_signal = files[current_input]->ft->signal;
  combiner_encoding = files[current_input]->ft->encoding;
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
      known_length = known_length && files[i]->ft->signal.length != 0;
      if (combine_method == sox_concatenate)
        olen += files[i]->ft->signal.length / files[i]->ft->signal.channels;
      else
        olen = max(olen, files[i]->ft->signal.length / files[i]->ft->signal.channels);
    }
    if (min_rate != max_rate)
      sox_fail("Input files must have the same sample-rate");
    if (min_channels != max_channels) {
      if (combine_method == sox_concatenate) {
        sox_fail("Input files must have the same # channels");
        exit(1);
      } else if (combine_method == sox_mix || combine_method == sox_multiply)
        sox_warn("Input files don't have the same # channels");
    }
    if (min_rate != max_rate)
      exit(1);

    combiner_signal.channels =
      combine_method == sox_merge? total_channels : max_channels;
  }

  ofile->signal = ofile_signal;
  if (ofile->signal.rate == 0)
    ofile->signal.rate = combiner_signal.rate;
  if (ofile->signal.channels == 0) {
    unsigned j;
    for (j = 0; j < nuser_effects && !ofile->signal.channels; ++j)
      ofile->signal.channels = user_efftab[nuser_effects - 1 - j]->out_signal.channels;
    if (ofile->signal.channels == 0)
      ofile->signal.channels = combiner_signal.channels;
  }
  ofile->signal.precision = combiner_signal.precision;

  combiner_signal.rate *= sox_effects_globals.speed;

  ofile->encoding = ofile_encoding; {
    sox_encodinginfo_t t = ofile->encoding;
    if (!t.encoding)
      t.encoding = combiner_encoding.encoding;
    if (!t.bits_per_sample)
      t.bits_per_sample = combiner_encoding.bits_per_sample;
    if (sox_format_supports_encoding(ofile->filename, ofile->filetype, &t))
      ofile->encoding = t;
  }

  for (i = 0; i < nuser_effects; i++)
    known_length = known_length && !(user_efftab[i]->handler.flags & SOX_EFF_LENGTH);

  if (!known_length)
    olen = 0;
  ofile->signal.length = (sox_size_t)(olen * ofile->signal.channels * ofile->signal.rate / combiner_signal.rate + .5);
  open_output_file();

  ofile_effects_chain.global_info = sox_effects_globals;
  ofile_effects_chain.in_enc = &combiner_encoding;
  ofile_effects_chain.out_enc = &ofile->ft->encoding;
  add_effects(&ofile_effects_chain);

  optimize_trim();

  signal(SIGINT, sigint);
  /* FIXME: For SIGTERM at least we really should guarantee to stop quickly */
  signal(SIGTERM, sigint); /* Stop gracefully even in extremis */

  flowstatus = sox_flow_effects(&ofile_effects_chain, update_status);

  sox_delete_effects(&ofile_effects_chain);
  return flowstatus;
}

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
  char const * * format_list;
  char const * const * names;

  for (i = formats = 0; sox_format_fns[i].fn; ++i) {
    char const * const *names = sox_format_fns[i].fn()->names;
    while (*names++)
      formats++;
  }
  format_list = (const char **)lsx_malloc(formats * sizeof(char *));

  printf("AUDIO FILE FORMATS:");
  for (i = formats = 0; sox_format_fns[i].fn; ++i) {
    sox_format_handler_t const * handler = sox_format_fns[i].fn();
    if (!(handler->flags & SOX_FILE_DEVICE))
      for (names = handler->names; *names; ++names)
        format_list[formats++] = *names;
  }
  qsort(format_list, formats, sizeof(char *), strcmp_p);
  for (i = 0; i < formats; i++)
    printf(" %s", format_list[i]);
  putchar('\n');

  printf("PLAYLIST FORMATS: m3u pls\nAUDIO DEVICES:");
  for (i = formats = 0; sox_format_fns[i].fn; ++i) {
    sox_format_handler_t const * handler = sox_format_fns[i].fn();
    if ((handler->flags & SOX_FILE_DEVICE) && !(handler->flags & SOX_FILE_PHONY))
      for (names = handler->names; *names; ++names)
        format_list[formats++] = *names;
  }
  qsort(format_list, formats, sizeof(char *), strcmp_p);
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
"--help-format NAME  display info on specified format; use `all' to display all",
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
"--add-comment TEXT  Append output file comment",
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
"  -a/-i/-g/-f   ADPCM/IMA ADPCM/GSM/floating point",
"-1/-2/-3/-4/-8  sample size in bytes",
"-v, --volume FACTOR  volume input file volume adjustment factor (real number)",
""};

  display_SoX_version(stdout);
  putchar('\n');

  if (message)
    fprintf(stderr, "Failed: %s\n\n", message);  /* N.B. stderr */

  printf("Usage summary: [gopts] [[fopts] infile]... [fopts]%s [effect [effopts]]...\n\n",
         sox_mode == sox_play? "" : " outfile");
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
    sox_fail("Cannot open comment file %s", filename);
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
      sox_fail("Error reading comment file %s", filename);
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

static char *getoptstr = "+ac:fghimnoqr:st:uv:xABC:LMNRSTUV::X12348";

static struct option long_options[] =
  {
    {"add-comment"     , required_argument, NULL, 0},
    {"buffer"          , required_argument, NULL, 0},
    {"combine"         , required_argument, NULL, 0},
    {"comment-file"    , required_argument, NULL, 0},
    {"comment"         , required_argument, NULL, 0},
    {"endian"          , required_argument, NULL, 0},
    {"interactive"     ,       no_argument, NULL, 0},
    {"help-effect"     , required_argument, NULL, 0},
    {"help-format"     , required_argument, NULL, 0},
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
  ENUM_ITEM(sox_,multiply)
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
    char * set = lsx_malloc(len);
    *set = 0;
    for (p = items; p->text; ++p) {
      set = lsx_realloc(set, len += 2 + strlen(p->text));
      strcat(set, ", "); strcat(set, p->text);
    }
    sox_fail("--%s: '%s' is not one of: %s.",
        long_options[option_index].name, optarg, set + 2);
    free(set);
    exit(1);
  }
  return p->value;
}

static sox_bool parse_gopts_and_fopts(file_t * f, int argc, char **argv)
{
  while (sox_true) {
    int c, option_index;
    int i; /* sscanf silently accepts negative numbers for %u :( */
    char dummy;     /* To check for extraneous chars in optarg. */

    switch (c=getopt_long(argc, argv, getoptstr, long_options, &option_index)) {
    case -1:        /* @ one of: file-name, effect name, end of arg-list. */
      return sox_false; /* i.e. not null file. */

    case 0:         /* Long options with no short equivalent. */
      switch (option_index) {
      case 0:
        if (optarg)
          sox_append_comment(&f->oob.comments, optarg);
        break;

      case 1:
#define SOX_BUFMIN 16
        if (sscanf(optarg, "%i %c", &i, &dummy) != 1 || i <= SOX_BUFMIN) {
        sox_fail("Buffer size `%s' must be > %d", optarg, SOX_BUFMIN);
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
        interactive = sox_true;
        break;

      case 7:
        usage_effect(optarg);
        break;

      case 8:
        usage_format(optarg);
        break;

      case 9:
        sox_effects_globals.plot = enum_option(option_index, plot_methods);
        break;

      case 10:
        replay_gain_mode = enum_option(option_index, rg_modes);
        break;

      case 11:
        display_SoX_version(stdout);
        exit(0);
        break;
      }
      break;

    case 'm': combine_method = sox_mix; break;
    case 'M': combine_method = sox_merge; break;
    case 'T': combine_method = sox_multiply; break;

    case 'R':                   /* Useful for regression testing. */
      sox_globals.repeatable = sox_true;
      break;

    case 'n':
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

    case 'r': {
      char k = 0;
      size_t n = sscanf(optarg, "%lf %c %c", &f->signal.rate, &k, &dummy);
      if (n < 1 || f->signal.rate <= 0 || (n > 1 && k != 'k') || n > 2) {
        sox_fail("Rate value `%s' is not a positive number", optarg);
        exit(1);
      }
      f->signal.rate *= k == 'k'? 1000. : 1.;
      break;
    }

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
      if (sscanf(optarg, "%lf %c", &f->encoding.compression, &dummy) != 1) {
        sox_fail("Compression value `%s' is not a number", optarg);
        exit(1);
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
          sox_fail("Verbosity value `%s' is not a non-negative integer", optarg);
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
  else if (!strcmp(type, "alsa") || !strcmp(type, "ao"))     name = "default";
  return name? from_env? from_env : name : NULL;
}

static char const * set_default_device(file_t * f)
{
  /* Default audio driver type in order of preference: */
  if (!f->filetype) f->filetype = getenv("AUDIODRIVER");
  if (!f->filetype && sox_find_format("alsa", sox_false)) f->filetype = "alsa";
  if (!f->filetype && sox_find_format("oss" , sox_false)) f->filetype = "oss";
  if (!f->filetype && sox_find_format("sunau",sox_false)) f->filetype = "sunau";
  if (!f->filetype && sox_find_format("ao"  , sox_false) && file_count) /*!rec*/
    f->filetype = "ao";

  if (!f->filetype) {
    sox_fail("Sorry, there is no default audio device configured");
    exit(1);
  }
  return device_name(f->filetype);
}

static int add_file(file_t const * const opts, char const * const filename)
{
  file_t * f = lsx_malloc(sizeof(*f));

  if (file_count >= MAX_FILES) {
    sox_fail("too many files; maximum is %d input files (and 1 output file)", MAX_INPUT_FILES);
    exit(1);
  }
  *f = *opts;
  if (!filename)
    usage("missing filename"); /* No return */
  f->filename = lsx_strdup(filename);
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
    if (parse_gopts_and_fopts(&opts, argc, argv)) { /* is null file? */
      if (opts.filetype != NULL && strcmp(opts.filetype, "null") != 0)
        sox_warn("ignoring `-t %s'.", opts.filetype);
      opts.filetype = "null";
      add_file(&opts, "");
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

static void parse_effects(int argc, char **argv)
{
  for (nuser_effects = 0; optind < argc; ++nuser_effects) {
    sox_effect_t * e;
    int i;

    if (nuser_effects >= MAX_USER_EFF) {
      sox_fail("too many effects specified (at most %i allowed)", MAX_USER_EFF);
      exit(1);
    }

    /* Name should always be correct! */
    e = sox_create_effect(sox_find_effect(argv[optind++]));

    for (i = 0; i < argc - optind && !sox_find_effect(argv[optind + i]); ++i);
    if (e->handler.getopts(e, i, &argv[optind]) == SOX_EOF)
      exit(1); /* The failing effect should have displayed an error message */

    optind += i; /* Skip past the effect arguments */

    if (e->handler.flags & SOX_EFF_DEPRECATED)
      sox_warn("effect `%s' is deprecated; see sox(1) for an alternative", e->handler.name);
    user_efftab[nuser_effects] = e;
  }
}

typedef enum {
  full, rate, channels, samples, duration, bits, encoding, annotation} soxi_t;

static int soxi1(soxi_t * type, char * filename)
{
  sox_size_t ws;
  sox_format_t * ft = sox_open_read(filename, NULL, NULL, NULL);

  if (!ft)
    return 1;
  ws = ft->signal.length / max(ft->signal.channels, 1);
  switch (*type) {
    case rate: printf("%g\n", ft->signal.rate); break;
    case channels: printf("%u\n", ft->signal.channels); break;
    case samples: printf("%u\n", ws); break;
    case duration: printf("%s\n", str_time((double)ws / max(ft->signal.rate, 1))); break;
    case bits: printf("%u\n", ft->encoding.bits_per_sample); break;
    case encoding: printf("%s\n", sox_encodings_info[ft->encoding.encoding].desc); break;
    case annotation: if (ft->oob.comments) {
      sox_comments_t p = ft->oob.comments;
      do printf("%s\n", *p); while (*++p);
    }
    break;
    case full: display_file_info(ft, NULL, sox_false); break;
  }
  return !!sox_close(ft);
}

static int soxi(int argc, char * const * argv)
{
  static char const opts[] = "rcsdbea?V::";
  soxi_t type = full;
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
          sox_fail("Verbosity value `%s' is not a non-negative integer", optarg);
          exit(1);
        }
        sox_globals.verbosity = (unsigned)i;
      }
    } else type = 1 + (strchr(opts, opt) - opts);
  if (type > annotation)
    printf("Usage: soxi [-V] [-r|-c|-s|-d|-b|-e|-a] infile1 ...\n");
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

  parse_options_and_filenames(argc, argv);

  if (sox_globals.verbosity > 2)
    display_SoX_version(stderr);

  /* Make sure we got at least the required # of input filenames */
  input_count = file_count ? file_count - 1 : 0;
  if (input_count < (combine_method <= sox_concatenate ? 1 : 2))
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

    if (sox_mode == sox_rec && !j) {       /* Set the recording parameters: */
      if (input_count > 1)                 /* from the (just openned) next */
        f->signal = files[1]->ft->signal;  /* input file, or from the output */
      else f->signal = files[1]->signal;   /* file (which is not open yet). */
    }
    files[j]->ft = sox_open_read(f->filename, &f->signal, &f->encoding, f->filetype);
    if (!files[j]->ft)
      /* sox_open_read() will call sox_warn for most errors.
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

  /* Loop through the rest of the arguments looking for effects */
  parse_effects(argc, argv);

  /* Not the best way for users to do this; now deprecated in favour of soxi. */
  if (!show_progress && !nuser_effects && ofile->filetype && !strcmp(ofile->filetype, "null")) {
    for (i = 0; i < input_count; i++)
      report_file_info(files[i]);
    exit(0);
  }

  /* Bit of a hack: input files can get # of chans from an effect */
  for (i = 0; i < input_count; i++) {
    unsigned j;
    for (j =0; j < nuser_effects && !files[i]->ft->signal.channels; ++j)
      files[i]->ft->signal.channels = user_efftab[j]->in_signal.channels;
    if (!files[i]->ft->signal.channels)
      ++files[i]->ft->signal.channels;
  }

  if (sox_globals.repeatable)
    sox_debug("Not reseeding PRNG; randomness is repeatable");
  else {
    time_t t;

    time(&t);
    srand((unsigned)t);
  }

  ofile_signal = ofile->signal;
  ofile_encoding = ofile->encoding;
  if (combine_method == sox_sequence) do {
    if (ofile->ft)
      sox_close(ofile->ft);
  } while (process() != SOX_EOF && !user_abort && current_input < input_count);
  else process();

  for (i = 0; i < file_count; ++i)
    if (files[i]->ft->clips != 0)
      sox_warn(i < input_count?"%s: input clipped %u samples" :
                              "%s: output clipped %u samples; decrease volume?",
          (files[i]->ft->handler.flags & SOX_FILE_DEVICE)?
                       files[i]->ft->handler.names[0] : files[i]->ft->filename,
          files[i]->ft->clips);

  if (mixing_clips > 0)
    sox_warn("mix-combining clipped %u samples; decrease volume?", mixing_clips);

  for (i = 0; i < file_count; i++)
    if (files[i]->volume_clips > 0)
      sox_warn("%s: balancing clipped %u samples; decrease volume?",
          files[i]->filename, files[i]->volume_clips);

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
