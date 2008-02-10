/*
 * synth - Synthesizer Effect.
 *
 * Copyright (c) Jan 2001  Carsten Borchardt
 * Copyright (c) 2001-2007 SoX contributors
 *
 * This source code is freely redistributable and may be used for any purpose.
 * This copyright notice must be maintained.  The authors are not responsible
 * for the consequences of using this software.
 */

#include <string.h>
#include <math.h>
#include <ctype.h>
#include "sox_i.h"

typedef enum {
  synth_sine,
  synth_square,
  synth_sawtooth,
  synth_triangle,
  synth_trapezium,
  synth_trapetz  = synth_trapezium,   /* Deprecated name for trapezium */
  synth_exp,
                                      /* Tones above, noises below */
  synth_whitenoise,
  synth_noise = synth_whitenoise,     /* Just a handy alias */
  synth_pinknoise,
  synth_brownnoise
} type_t;

enum_item const synth_type[] = {
  ENUM_ITEM(synth_, sine)
  ENUM_ITEM(synth_, square)
  ENUM_ITEM(synth_, sawtooth)
  ENUM_ITEM(synth_, triangle)
  ENUM_ITEM(synth_, trapezium)
  ENUM_ITEM(synth_, trapetz)
  ENUM_ITEM(synth_, exp)
  ENUM_ITEM(synth_, whitenoise)
  ENUM_ITEM(synth_, noise)
  ENUM_ITEM(synth_, pinknoise)
  ENUM_ITEM(synth_, brownnoise)
  {0, 0}
};

typedef enum {synth_create, synth_mix, synth_amod, synth_fmod} combine_t;

enum_item const combine_type[] = {
  ENUM_ITEM(synth_, create)
  ENUM_ITEM(synth_, mix)
  ENUM_ITEM(synth_, amod)
  ENUM_ITEM(synth_, fmod)
  {0, 0}
};



/******************************************************************************
 * start of pink noise generator stuff
 * algorithm stolen from:
 * Author: Phil Burk, http://www.softsynth.com
 */

/* Calculate pseudo-random 32 bit number based on linear congruential method. */
static unsigned long GenerateRandomNumber(void)
{
  static unsigned long randSeed = 22222;        /* Change this for different random sequences. */

  randSeed = (randSeed * 196314165) + 907633515;
  return randSeed;
}

#define PINK_MAX_RANDOM_ROWS   (30)
#define PINK_RANDOM_BITS       (24)
#define PINK_RANDOM_SHIFT      ((sizeof(long)*8)-PINK_RANDOM_BITS)

typedef struct {
  long pink_Rows[PINK_MAX_RANDOM_ROWS];
  long pink_RunningSum;         /* Used to optimize summing of generators. */
  int pink_Index;               /* Incremented each sample. */
  int pink_IndexMask;           /* Index wrapped by ANDing with this mask. */
  float pink_Scalar;            /* Used to scale within range of -1 to +1 */
} PinkNoise;

/* Setup PinkNoise structure for N rows of generators. */
static void InitializePinkNoise(PinkNoise * pink, int numRows)
{
  int i;
  long pmax;

  pink->pink_Index = 0;
  pink->pink_IndexMask = (1 << numRows) - 1;
  /* Calculate maximum possible signed random value. Extra 1 for white noise always added. */
  pmax = (numRows + 1) * (1 << (PINK_RANDOM_BITS - 1));
  pink->pink_Scalar = 1.0f / pmax;
  /* Initialize rows. */
  for (i = 0; i < numRows; i++)
    pink->pink_Rows[i] = 0;
  pink->pink_RunningSum = 0;
}

/* Generate Pink noise values between -1 and +1 */
static float GeneratePinkNoise(PinkNoise * pink)
{
  long newRandom;
  long sum;
  float output;

  /* Increment and mask index. */
  pink->pink_Index = (pink->pink_Index + 1) & pink->pink_IndexMask;

  /* If index is zero, don't update any random values. */
  if (pink->pink_Index != 0) {
    /* Determine how many trailing zeros in PinkIndex. */
    /* This algorithm will hang if n==0 so test first. */
    int numZeros = 0;
    int n = pink->pink_Index;

    while ((n & 1) == 0) {
      n = n >> 1;
      numZeros++;
    }

    /* Replace the indexed ROWS random value.
     * Subtract and add back to RunningSum instead of adding all the random
     * values together. Only one changes each time.
     */
    pink->pink_RunningSum -= pink->pink_Rows[numZeros];
    newRandom = ((long) GenerateRandomNumber()) >> PINK_RANDOM_SHIFT;
    pink->pink_RunningSum += newRandom;
    pink->pink_Rows[numZeros] = newRandom;
  }

  /* Add extra white noise value. */
  newRandom = ((long) GenerateRandomNumber()) >> PINK_RANDOM_SHIFT;
  sum = pink->pink_RunningSum + newRandom;

  /* Scale to range of -1 to 0.9999. */
  output = pink->pink_Scalar * sum;

  return output;
}

/**************** end of pink noise stuff */



typedef struct {
  /* options */
  type_t type;
  combine_t combine;
  double freq, freq2;
  double offset, phase;
  double p1, p2, p3; /* Use depends on synth type */

  /* internal stuff */
  double cycle_start_time_s;
  double brown_noise;
  PinkNoise pink_noise;
} * channel_t;



/* Private data for the synthesizer */
typedef struct {
  char *        length_str;
  channel_t     getopts_channels;
  sox_size_t    getopts_nchannels;
  sox_sample_t max;
  sox_size_t    samples_done;
  sox_size_t    samples_to_do;
  channel_t     channels;
  sox_size_t    number_of_channels;
} * synth_t;



/* a note is given as an int,
 * 0   => 440 Hz = A
 * >0  => number of half notes 'up',
 * <0  => number of half notes down,
 * example 12 => A of next octave, 880Hz
 *
 * calculated by freq = 440Hz * 2**(note/12)
 */
static double calc_note_freq(double note)
{
  return 440.0 * pow(2.0, note / 12.0);
}



/* Read string 's' and convert to frequency.
 * 's' can be a positive number which is the frequency in Hz.
 * If 's' starts with a hash '%' and a following number the corresponding
 * note is calculated.
 * Return -1 on error.
 */
static double StringToFreq(char *s, char **h)
{
  double f;

  if (*s == '%') {
    f = strtod(s + 1, h);
    if (*h == s + 1)
      return -1;
    f = calc_note_freq(f);
  } else {
    f = strtod(s, h);
    if (*h == s)
      return -1;
  }
  if (f < 0)
    return -1;
  return f;
}



static void create_channel(channel_t chan)
{
  memset(chan, 0, sizeof(*chan));
  chan->freq2 = chan->freq = 440;
  chan->p3 = chan->p2 = chan->p1 = -1;
}



static void set_default_parameters(channel_t chan, int c)
{
  switch (chan->type) {
    case synth_square:    /* p1 is pulse width */
      if (chan->p1 < 0)
        chan->p1 = 0.5;   /* default to 50% duty cycle */
      break;

    case synth_triangle:  /* p1 is position of maximum */
      if (chan->p1 < 0)
        chan->p1 = 0.5;
      break;

    case synth_trapezium:
      /* p1 is length of rising slope,
       * p2 position where falling slope begins
       * p3 position of end of falling slope
       */
      if (chan->p1 < 0) {
        chan->p1 = 0.1;
        chan->p2 = 0.5;
        chan->p3 = 0.6;
      } else if (chan->p2 < 0) { /* try a symetric waveform */
        if (chan->p1 <= 0.5) {
          chan->p2 = (1 - 2 * chan->p1) / 2;
          chan->p3 = chan->p2 + chan->p1;
        } else {
          /* symetric is not possible, fall back to asymetrical triangle */
          chan->p2 = chan->p1;
          chan->p3 = 1;
        }
      } else if (chan->p3 < 0)
        chan->p3 = 1;     /* simple falling slope to the end */
      break;

    case synth_pinknoise:
      /* Initialize pink noise signals with different numbers of rows. */
      InitializePinkNoise(&(chan->pink_noise), 10 + 2 * c);
      break;

    case synth_exp:
      if (chan->p1 < 0) /* p1 is position of maximum */
        chan->p1 = 0.5;
      if (chan->p2 < 0) /* p2 is amplitude */
        chan->p2 = 1;
      break;

    default: break;
  }
}



static int getopts(sox_effect_t * effp, int argc, char **argv)
{
  synth_t synth = (synth_t) effp->priv;
  int argn = 0;

  /* Get duration if given (if first arg starts with digit) */
  if (argc && (isdigit(argv[argn][0]) || argv[argn][0] == '.')) {
    synth->length_str = xmalloc(strlen(argv[argn]) + 1);
    strcpy(synth->length_str, argv[argn]);
    /* Do a dummy parse of to see if it will fail */
    if (sox_parsesamples(0., synth->length_str, &synth->samples_to_do, 't') == NULL)
      return sox_usage(effp);
    argn++;
  }

  while (argn < argc) { /* type [combine] [f1[-f2] [p1 [p2 [p3 [p3 [p4]]]]]] */
    channel_t chan;
    char * char_ptr;
    enum_item const *p = find_enum_text(argv[argn], synth_type);

    if (p == NULL) {
      sox_fail("no type given");
      return SOX_EOF;
    }
    synth->getopts_channels = xrealloc(synth->getopts_channels, sizeof(*synth->getopts_channels) * (synth->getopts_nchannels + 1));
    chan = &synth->getopts_channels[synth->getopts_nchannels++];
    create_channel(chan);
    chan->type = p->value;
    if (++argn == argc)
      break;

    /* maybe there is a combine-type in next arg */
    p = find_enum_text(argv[argn], combine_type);
    if (p != NULL) {
      chan->combine = p->value;
      if (++argn == argc)
        break;
    }

    /* read frequencies if given */
    if (isdigit((int) argv[argn][0]) || argv[argn][0] == '%') {
      chan->freq2 = chan->freq = StringToFreq(argv[argn], &char_ptr);
      if (chan->freq < 0) {
        sox_fail("invalid freq");
        return SOX_EOF;
      }
      if (*char_ptr == '-') {        /* freq2 given? */
        char *hlp2;

        chan->freq2 = StringToFreq(char_ptr + 1, &hlp2);
        if (chan->freq2 < 0) {
          sox_fail("invalid freq2");
          return SOX_EOF;
        }
        if (synth->length_str == NULL) {
          sox_fail("duration must be given when using freq2");
          return SOX_EOF;
        }
      }
      if (++argn == argc)
        break;
    }

    /* read rest of parameters */
#undef NUMERIC_PARAMETER
#define NUMERIC_PARAMETER(p, min, max) { \
      char * end_ptr; \
      double d = strtod(argv[argn], &end_ptr); \
      if (end_ptr == argv[argn]) \
        break; \
      if (d < min || d > max || *end_ptr != '\0') { \
        sox_fail("parameter error"); \
        return SOX_EOF; \
      } \
      chan->p = d / 100; /* adjust so abs(parameter) <= 1 */\
      if (++argn == argc) \
        break; \
    }
    do { /* break-able block */
      NUMERIC_PARAMETER(offset,-100, 100)
      NUMERIC_PARAMETER(phase ,   0, 100)
      NUMERIC_PARAMETER(p1,   0, 100)
      NUMERIC_PARAMETER(p2,   0, 100)
      NUMERIC_PARAMETER(p1,   0, 100)
    } while (0);
  }

  /* If no channel parameters were given, create one default channel: */
  if (!synth->getopts_nchannels) {
    synth->getopts_channels = xmalloc(sizeof(*synth->getopts_channels));
    create_channel(&synth->getopts_channels[synth->getopts_nchannels++]);
  }

  if (!effp->ininfo.channels)
    effp->ininfo.channels = synth->getopts_nchannels;

  return SOX_SUCCESS;
}



static int start(sox_effect_t * effp)
{
  synth_t synth = (synth_t) effp->priv;
  unsigned i;
  int shift_for_max = (4 - min(effp->outinfo.size, 4)) << 3;

  synth->max = (SOX_SAMPLE_MAX >> shift_for_max) << shift_for_max;
  synth->samples_done = 0;

  if (synth->length_str)
    if (sox_parsesamples(effp->ininfo.rate, synth->length_str, &synth->samples_to_do, 't') == NULL)
      return sox_usage(effp);

  synth->number_of_channels = effp->ininfo.channels;
  synth->channels = xcalloc(synth->number_of_channels, sizeof(*synth->channels));
  for (i = 0; i < synth->number_of_channels; ++i) {
    channel_t chan = &synth->channels[i];
    *chan = synth->getopts_channels[i % synth->getopts_nchannels];
    set_default_parameters(chan, i);
    sox_debug("type=%s, combine=%s, samples_to_do=%u, f1=%g, f2=%g, "
              "offset=%g, phase=%g, p1=%g, p2=%g, p3=%g",
        find_enum_value(chan->type, synth_type)->text,
        find_enum_value(chan->combine, combine_type)->text,
        synth->samples_to_do, chan->freq, chan->freq2,
        chan->offset, chan->phase, chan->p1, chan->p2, chan->p3);
  }
  return SOX_SUCCESS;
}



static sox_sample_t do_synth(sox_sample_t synth_input, synth_t synth, int c, double rate)
{
  channel_t chan = &synth->channels[c];
  double synth_out;              /* [-1, 1] */

  if (chan->type < synth_noise) { /* Need to calculate phase: */
    double f;              /* Current frequency; variable if sweeping */
    double cycle_period_s; /* Current period in seconds */
    double total_elapsed_time_s, cycle_elapsed_time_s;
    double phase;            /* [0, 1) */

    if (synth->samples_to_do <= 0)
      f = chan->freq;      /* Can't sweep if synth duration is unknown */
    else
      f = chan->freq * exp((log(chan->freq2) - log(chan->freq)) *
          synth->samples_done / synth->samples_to_do);
    cycle_period_s = 1 / f;
    total_elapsed_time_s = synth->samples_done / rate;
    cycle_elapsed_time_s = total_elapsed_time_s - chan->cycle_start_time_s;
    if (cycle_elapsed_time_s >= cycle_period_s) {  /* move to next cycle */
      chan->cycle_start_time_s += cycle_period_s;
      cycle_elapsed_time_s = total_elapsed_time_s - chan->cycle_start_time_s;
    }
    phase = cycle_elapsed_time_s / cycle_period_s;
    phase = fmod(phase + chan->phase, 1.0);

    switch (chan->type) {
      case synth_sine:
        synth_out = sin(2 * M_PI * phase);
        break;

      case synth_square:
        /* |_______           | +1
         * |       |          |
         * |_______|__________|  0
         * |       |          |
         * |       |__________| -1
         * |                  |
         * 0       p1          1
         */
        synth_out = -1 + 2 * (phase < chan->p1);
        break;

      case synth_sawtooth:
        /* |           __| +1
         * |        __/  |
         * |_______/_____|  0
         * |  __/        |
         * |_/           | -1
         * |             |
         * 0             1
         */
        synth_out = -1 + 2 * phase;
        break;

      case synth_triangle:
        /* |    .    | +1
         * |   / \   |
         * |__/___\__|  0
         * | /     \ |
         * |/       \| -1
         * |         |
         * 0   p1    1
         */

        if (phase < chan->p1)
          synth_out = -1 + 2 * phase / chan->p1;          /* In rising part of period */
        else
          synth_out = 1 - 2 * (phase - chan->p1) / (1 - chan->p1); /* In falling part */
        break;

      case synth_trapezium:
        /* |    ______             |+1
         * |   /      \            |
         * |__/________\___________| 0
         * | /          \          |
         * |/            \_________|-1
         * |                       |
         * 0   p1    p2   p3       1
         */
        if (phase < chan->p1)       /* In rising part of period */
          synth_out = -1 + 2 * phase / chan->p1;
        else if (phase < chan->p2)  /* In high part of period */
          synth_out = 1;
        else if (phase < chan->p3)  /* In falling part */
          synth_out = 1 - 2 * (phase - chan->p2) / (chan->p3 - chan->p2);
        else                        /* In low part of period */
          synth_out = -1;
        break;

      case synth_exp:
        /* |             |              | +1
         * |            | |             |
         * |          _|   |_           | 0
         * |       __-       -__        |
         * |____---             ---____ | f(p2)
         * |                            |
         * 0             p1             1
         */
        synth_out = dB_to_linear(chan->p2 * -100);  /* 0 ..  1 */
        if (phase < chan->p1)
          synth_out = synth_out * exp(phase * log(1 / synth_out) / chan->p1);
        else
          synth_out = synth_out * exp((1 - phase) * log(1 / synth_out) / (1 - chan->p1));
        synth_out = synth_out * 2 - 1;      /* map 0 .. 1 to -1 .. +1 */
        break;

      default: synth_out = 0;
    }
  } else switch (chan->type) {
#define RAND (2. * rand() * (1. / RAND_MAX) - 1)
    case synth_whitenoise:
      synth_out = RAND;
      break;

    case synth_pinknoise:
      synth_out = GeneratePinkNoise(&(chan->pink_noise));
      break;

    case synth_brownnoise:
      do synth_out = chan->brown_noise + RAND * (1. / 16);
      while (fabs(synth_out) > 1);
      chan->brown_noise = synth_out;
      break;

    default: synth_out = 0;
  }

  /* Add offset, but prevent clipping: */
  synth_out = synth_out * (1 - fabs(chan->offset)) + chan->offset;

  switch (chan->combine) {
    case synth_create: return  synth_out * synth->max;
    case synth_mix   : return (synth_out * synth->max + synth_input) * 0.5;
    case synth_amod  : return (synth_out + 1) * synth_input * 0.5;
    case synth_fmod  : return  synth_out * synth_input;
  }
  return 0;
}



static int flow(sox_effect_t * effp, const sox_sample_t * ibuf, sox_sample_t * obuf,
    sox_size_t * isamp, sox_size_t * osamp)
{
  synth_t synth = (synth_t) effp->priv;
  unsigned len = min(*isamp, *osamp) / effp->ininfo.channels;
  unsigned c, done, result = SOX_SUCCESS;

  for (done = 0; done < len && result == SOX_SUCCESS; ++done) {
    for (c = 0; c < effp->ininfo.channels; c++)
      *obuf++ = do_synth(*ibuf++, synth, c, effp->ininfo.rate);
    if (++synth->samples_done == synth->samples_to_do)
      result = SOX_EOF;
  }
  *isamp = *osamp = done * effp->ininfo.channels;
  return result;
}



static int stop(sox_effect_t * effp)
{
  synth_t synth = (synth_t) effp->priv;
  free(synth->channels);
  return SOX_SUCCESS;
}



static int kill(sox_effect_t * effp)
{
  synth_t synth = (synth_t) effp->priv;
  free(synth->getopts_channels);
  free(synth->length_str);
  return SOX_SUCCESS;
}



const sox_effect_handler_t *sox_synth_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "synth",
    "[len] {type [combine] [freq[-freq2] [off [ph [p1 [p2 [p3]]]]]]}",
    SOX_EFF_MCHAN | SOX_EFF_PREC, getopts, start, flow, 0, stop, kill
  };
  return &handler;
}
