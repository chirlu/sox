/* libSoX synth - Synthesizer Effect.
 *
 * Copyright (c) Jan 2001  Carsten Borchardt
 * Copyright (c) 2001-2008 SoX contributors
 *
 * This source code is freely redistributable and may be used for any purpose.
 * This copyright notice must be maintained.  The authors are not responsible
 * for the consequences of using this software.
 */

#include "sox_i.h"

#include <string.h>
#include <ctype.h>

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

static lsx_enum_item const synth_type[] = {
  LSX_ENUM_ITEM(synth_, sine)
  LSX_ENUM_ITEM(synth_, square)
  LSX_ENUM_ITEM(synth_, sawtooth)
  LSX_ENUM_ITEM(synth_, triangle)
  LSX_ENUM_ITEM(synth_, trapezium)
  LSX_ENUM_ITEM(synth_, trapetz)
  LSX_ENUM_ITEM(synth_, exp)
  LSX_ENUM_ITEM(synth_, whitenoise)
  LSX_ENUM_ITEM(synth_, noise)
  LSX_ENUM_ITEM(synth_, pinknoise)
  LSX_ENUM_ITEM(synth_, brownnoise)
  {0, 0}
};

typedef enum {synth_create, synth_mix, synth_amod, synth_fmod} combine_t;

static lsx_enum_item const combine_type[] = {
  LSX_ENUM_ITEM(synth_, create)
  LSX_ENUM_ITEM(synth_, mix)
  LSX_ENUM_ITEM(synth_, amod)
  LSX_ENUM_ITEM(synth_, fmod)
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
static void InitializePinkNoise(PinkNoise * pink, size_t numRows)
{
  size_t i;
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



typedef enum {Linear, Square, Exp, Exp_cycle} sweep_t;

typedef struct {
  /* options */
  type_t type;
  combine_t combine;
  double freq, freq2, mult;
  sweep_t sweep;
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
  size_t    getopts_nchannels;
  sox_sample_t  max;
  size_t    samples_done;
  size_t    samples_to_do;
  channel_t     channels;
  size_t    number_of_channels;
} priv_t;



static void create_channel(channel_t chan)
{
  memset(chan, 0, sizeof(*chan));
  chan->freq2 = chan->freq = 440;
  chan->p3 = chan->p2 = chan->p1 = -1;
}



static void set_default_parameters(channel_t chan, size_t c)
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
  priv_t * synth = (priv_t *) effp->priv;
  int argn = 0;

  /* Get duration if given (if first arg starts with digit) */
  if (argc && (isdigit((int)argv[argn][0]) || argv[argn][0] == '.')) {
    synth->length_str = lsx_malloc(strlen(argv[argn]) + 1);
    strcpy(synth->length_str, argv[argn]);
    /* Do a dummy parse of to see if it will fail */
    if (lsx_parsesamples(9e9, synth->length_str, &synth->samples_to_do, 't') == NULL || !synth->samples_to_do)
      return lsx_usage(effp);
    argn++;
  }

  while (argn < argc) { /* type [combine] [f1[-f2] [p1 [p2 [p3 [p3 [p4]]]]]] */
    channel_t chan;
    char * end_ptr;
    lsx_enum_item const *p = lsx_find_enum_text(argv[argn], synth_type);

    if (p == NULL) {
      lsx_fail("no type given");
      return SOX_EOF;
    }
    synth->getopts_channels = lsx_realloc(synth->getopts_channels, sizeof(*synth->getopts_channels) * (synth->getopts_nchannels + 1));
    chan = &synth->getopts_channels[synth->getopts_nchannels++];
    create_channel(chan);
    chan->type = p->value;
    if (++argn == argc)
      break;

    /* maybe there is a combine-type in next arg */
    p = lsx_find_enum_text(argv[argn], combine_type);
    if (p != NULL) {
      chan->combine = p->value;
      if (++argn == argc)
        break;
    }

    /* read frequencies if given */
    if (isdigit((int) argv[argn][0]) ||
        argv[argn][0] == '.' || argv[argn][0] == '%') {
      static const char sweeps[] = ":+/-";

      chan->freq2 = chan->freq = lsx_parse_frequency(argv[argn], &end_ptr);
      if (chan->freq < 0) {
        lsx_fail("invalid freq");
        return SOX_EOF;
      }
      if (*end_ptr && strchr(sweeps, *end_ptr)) {         /* freq2 given? */
        chan->sweep = strchr(sweeps, *end_ptr) - sweeps;
        chan->freq2 = lsx_parse_frequency(end_ptr + 1, &end_ptr);
        if (chan->freq2 < 0) {
          lsx_fail("invalid freq2");
          return SOX_EOF;
        }
        if (synth->length_str == NULL) {
          lsx_fail("duration must be given when using freq2");
          return SOX_EOF;
        }
      }
      if (*end_ptr) {
        lsx_fail("frequency: invalid trailing character");
        return SOX_EOF;
      }
      if (chan->sweep >= Exp && chan->freq * chan->freq2 == 0) {
        lsx_fail("invalid frequency for exponential sweep");
        return SOX_EOF;
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
        lsx_fail("parameter error"); \
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
    synth->getopts_channels = lsx_malloc(sizeof(*synth->getopts_channels));
    create_channel(&synth->getopts_channels[synth->getopts_nchannels++]);
  }

  if (!effp->in_signal.channels)
    effp->in_signal.channels = synth->getopts_nchannels;

  return SOX_SUCCESS;
}



static int start(sox_effect_t * effp)
{
  priv_t * synth = (priv_t *) effp->priv;
  size_t i;

  synth->max = lsx_sample_max(effp->out_encoding);
  synth->samples_done = 0;

  if (synth->length_str)
    if (lsx_parsesamples(effp->in_signal.rate, synth->length_str, &synth->samples_to_do, 't') == NULL || !synth->samples_to_do)
      return lsx_usage(effp);

  synth->number_of_channels = effp->in_signal.channels;
  synth->channels = lsx_calloc(synth->number_of_channels, sizeof(*synth->channels));
  for (i = 0; i < synth->number_of_channels; ++i) {
    channel_t chan = &synth->channels[i];
    *chan = synth->getopts_channels[i % synth->getopts_nchannels];
    set_default_parameters(chan, i);
    switch (chan->sweep) {
      case Linear: chan->mult = synth->samples_to_do?
          (chan->freq2 - chan->freq) / synth->samples_to_do / 2 : 0;
        break;
      case Square: chan->mult = synth->samples_to_do?
           sqrt(fabs(chan->freq2 - chan->freq)) / synth->samples_to_do / sqrt(3.) : 0;
        if (chan->freq > chan->freq2)
          chan->mult = -chan->mult;
        break;
      case Exp: chan->mult = synth->samples_to_do?
          log(chan->freq2 / chan->freq) / synth->samples_to_do * effp->in_signal.rate : 1;
        chan->freq /= chan->mult;
        break;
      case Exp_cycle: chan->mult = synth->samples_to_do?
          (log(chan->freq2) - log(chan->freq)) / synth->samples_to_do : 1;
        break;
    }
    lsx_debug("type=%s, combine=%s, samples_to_do=%lu, f1=%g, f2=%g, "
              "offset=%g, phase=%g, p1=%g, p2=%g, p3=%g mult=%g",
        lsx_find_enum_value(chan->type, synth_type)->text,
        lsx_find_enum_value(chan->combine, combine_type)->text,
        (unsigned long)synth->samples_to_do, chan->freq, chan->freq2,
        chan->offset, chan->phase, chan->p1, chan->p2, chan->p3, chan->mult);
  }
  return SOX_SUCCESS;
}



#define sign(d) ((d) < 0? -1. : 1.)
#define elapsed_time_s synth->samples_done / effp->in_signal.rate

static int flow(sox_effect_t * effp, const sox_sample_t * ibuf, sox_sample_t * obuf,
    size_t * isamp, size_t * osamp)
{
  priv_t * synth = (priv_t *) effp->priv;
  unsigned len = min(*isamp, *osamp) / effp->in_signal.channels;
  unsigned c, done;
  int result = SOX_SUCCESS;

  for (done = 0; done < len && result == SOX_SUCCESS; ++done) {
    for (c = 0; c < effp->in_signal.channels; c++) {
      sox_sample_t synth_input = *ibuf++;
      channel_t chan = &synth->channels[c];
      double synth_out;              /* [-1, 1] */

      if (chan->type < synth_noise) { /* Need to calculate phase: */
        double phase;            /* [0, 1) */
        switch (chan->sweep) {
          case Linear:
            phase = (chan->freq + synth->samples_done * chan->mult) *
                elapsed_time_s;
            break;
          case Square:
            phase = (chan->freq + sign(chan->mult) * 
                sqr(synth->samples_done * chan->mult)) * elapsed_time_s;
            break;
          case Exp:
            phase = chan->freq * exp(chan->mult * elapsed_time_s);
            break;
          case Exp_cycle: default: {
            double f = chan->freq * exp(synth->samples_done * chan->mult);
            double cycle_elapsed_time_s = elapsed_time_s - chan->cycle_start_time_s;
            if (f * cycle_elapsed_time_s >= 1) {  /* move to next cycle */
              chan->cycle_start_time_s += 1 / f;
              cycle_elapsed_time_s = elapsed_time_s - chan->cycle_start_time_s;
            }
            phase = f * cycle_elapsed_time_s;
            break;
          }
        }
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
        case synth_create: *obuf++ =  synth_out * synth->max; break;
        case synth_mix   : *obuf++ = (synth_out * synth->max + synth_input) * 0.5; break;
        case synth_amod  : *obuf++ = (synth_out + 1) * synth_input * 0.5; break;
        case synth_fmod  : *obuf++ =  synth_out * synth_input; break;
      }
    }
    if (++synth->samples_done == synth->samples_to_do)
      result = SOX_EOF;
  }
  *isamp = *osamp = done * effp->in_signal.channels;
  return result;
}



static int stop(sox_effect_t * effp)
{
  priv_t * synth = (priv_t *) effp->priv;
  free(synth->channels);
  return SOX_SUCCESS;
}



static int kill(sox_effect_t * effp)
{
  priv_t * synth = (priv_t *) effp->priv;
  free(synth->getopts_channels);
  free(synth->length_str);
  return SOX_SUCCESS;
}



const sox_effect_handler_t *sox_synth_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "synth", "[len] {type [combine] [[%]freq[k][:|+|/|-[%]freq2[k]] [off [ph [p1 [p2 [p3]]]]]]}",
    SOX_EFF_MCHAN | SOX_EFF_PREC |SOX_EFF_LENGTH,
    getopts, start, flow, 0, stop, kill, sizeof(priv_t)
  };
  return &handler;
}
