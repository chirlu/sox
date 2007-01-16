/*
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, write to the Free Software Foundation,
 * Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301, USA.
 */

/* Effect: Stereo Flanger   (c) 2006 robs@users.sourceforge.net */

#define st_flanger_usage \
  "Usage: flanger [delay depth regen width speed shape phase interp]\n"
/*
  "                  .\n" \
  "                 /|regen\n" \
  "                / |\n" \
  "            +--(  |------------+\n" \
  "            |   \\ |            |   .\n" \
  "           _V_   \\|  _______   |   |\\ width   ___\n" \
  "          |   |   ' |       |  |   | \\       |   |\n" \
  "      +-->| + |---->| DELAY |--+-->|  )----->|   |\n" \
  "      |   |___|     |_______|      | /       |   |\n" \
  "      |           delay : depth    |/        |   |\n" \
  "  In  |                 : interp   '         |   | Out\n" \
  "  --->+               __:__                  | + |--->\n" \
  "      |              |     |speed            |   |\n" \
  "      |              |  ~  |shape            |   |\n" \
  "      |              |_____|phase            |   |\n" \
  "      +------------------------------------->|   |\n" \
  "                                             |___|\n" \
  "\n" \
  "       RANGE DEFAULT DESCRIPTION\n" \
  "delay   0 10    0    base delay in milliseconds\n" \
  "depth   0 10    2    added swept delay in milliseconds\n" \
  "regen -95 +95   0    percentage regeneration (delayed signal feedback)\n" \
  "width   0 100   71   percentage of delayed signal mixed with original\n" \
  "speed  0.1 10  0.5   sweeps per second (Hz) \n" \
  "shape    --    sin   swept wave shape: sine|triangle\n" \
  "phase   0 100   25   swept wave percentage phase-shift for multi-channel\n" \
  "                     (e.g. stereo) flange; 0 = 100 = same phase on each channel\n" \
  "interp   --    lin   delay-line interpolation: linear|quadratic"
*/

/* TODO: Slide in the delay at the start? */



#include "st_i.h"
#include <math.h>
#include <string.h>



typedef enum {INTERP_LINEAR, INTERP_QUADRATIC} interp_t;

#define MAX_CHANNELS 4



typedef struct flanger {
  /* Parameters */
  double     delay_min;
  double     delay_depth;
  double     feedback_gain;
  double     delay_gain;
  double     speed;
  st_wave_t  wave_shape;
  double     channel_phase;
  interp_t   interpolation;
            
  /* Delay buffers */
  double *   delay_bufs[MAX_CHANNELS];
  st_size_t  delay_buf_length;
  st_size_t  delay_buf_pos;
  double     delay_last[MAX_CHANNELS];
            
  /* Low Frequency Oscillator */
  float *    lfo;
  st_size_t  lfo_length;
  st_size_t  lfo_pos;
            
  /* Balancing */
  double     in_gain;
} * flanger_t;

assert_static(sizeof(struct flanger) <= ST_MAX_EFFECT_PRIVSIZE,
              /* else */ flanger_PRIVSIZE_too_big);



static enum_item const interp_enum[] = {
  ENUM_ITEM(INTERP_,LINEAR)
  ENUM_ITEM(INTERP_,QUADRATIC)
  {0, 0}};



#define NUMERIC_PARAMETER(p, min, max) { \
  char * end_ptr; \
  double d; \
  if (argc == 0) break; \
  d = strtod(*argv, &end_ptr); \
  if (end_ptr != *argv) { \
    if (d < min || d > max || *end_ptr != '\0') { \
      st_fail(effp->h->usage); \
      return ST_EOF; \
    } \
    f->p = d; \
    --argc, ++argv; \
  } \
}



#define TEXTUAL_PARAMETER(p, enum_table) { \
  enum_item const * e; \
  if (argc == 0) break; \
  e = find_enum_text(*argv, enum_table); \
  if (e != NULL) { \
    f->p = e->value; \
    --argc, ++argv; \
  } \
}



static int st_flanger_getopts(eff_t effp, int argc, char *argv[])
{
  flanger_t f = (flanger_t) effp->priv;

  /* Set non-zero defaults: */
  f->delay_depth  = 2;
  f->delay_gain   = 71;
  f->speed        = 0.5;
  f->channel_phase= 25;

  do { /* break-able block */
    NUMERIC_PARAMETER(delay_min    , 0  , 10 )
    NUMERIC_PARAMETER(delay_depth  , 0  , 10 )
    NUMERIC_PARAMETER(feedback_gain,-95 , 95 )
    NUMERIC_PARAMETER(delay_gain   , 0  , 100)
    NUMERIC_PARAMETER(speed        , 0.1, 10 )
    TEXTUAL_PARAMETER(wave_shape, st_wave_enum)
    NUMERIC_PARAMETER(channel_phase, 0  , 100)
    TEXTUAL_PARAMETER(interpolation, interp_enum)
  } while (0);

  if (argc != 0) {
    st_fail(effp->h->usage);
    return ST_EOF;
  }

  st_report("parameters:\n"
      "delay = %gms\n"
      "depth = %gms\n"
      "regen = %g%%\n"
      "width = %g%%\n"
      "speed = %gHz\n"
      "shape = %s\n"
      "phase = %g%%\n"
      "interp= %s",
      f->delay_min,
      f->delay_depth,
      f->feedback_gain,
      f->delay_gain,
      f->speed,
      st_wave_enum[f->wave_shape].text,
      f->channel_phase,
      interp_enum[f->interpolation].text);

  return ST_SUCCESS;
}



static int st_flanger_start(eff_t effp)
{
  flanger_t f = (flanger_t) effp->priv;
  int c, channels = effp->ininfo.channels;

  if (channels > MAX_CHANNELS) {
    st_fail("Can not operate with more than %i channels", MAX_CHANNELS);
    return ST_EOF;
  }

  /* Scale percentages to unity: */
  f->feedback_gain /= 100;
  f->delay_gain    /= 100;
  f->channel_phase /= 100;

  /* Balance output: */
  f->in_gain = 1 / (1 + f->delay_gain);
  f->delay_gain  /= 1 + f->delay_gain;

  /* Balance feedback loop: */
  f->delay_gain *= 1 - fabs(f->feedback_gain);

  st_debug("in_gain=%g feedback_gain=%g delay_gain=%g\n",
      f->in_gain, f->feedback_gain, f->delay_gain);

  /* Create the delay buffers, one for each channel: */
  f->delay_buf_length =
    (f->delay_min + f->delay_depth) / 1000 * effp->ininfo.rate + 0.5;
  ++f->delay_buf_length;  /* Need 0 to n, i.e. n + 1. */
  ++f->delay_buf_length;  /* Quadratic interpolator needs one more. */
  for (c = 0; c < channels; ++c)
    f->delay_bufs[c] = xcalloc(f->delay_buf_length, sizeof(*f->delay_bufs[0]));

  /* Create the LFO lookup table: */
  f->lfo_length = effp->ininfo.rate / f->speed;
  f->lfo = xcalloc(f->lfo_length, sizeof(*f->lfo));
  st_generate_wave_table(
      f->wave_shape,
      ST_FLOAT,
      f->lfo,
      f->lfo_length,
      (st_size_t)(f->delay_min / 1000 * effp->ininfo.rate + .5),
      f->delay_buf_length - 2,
      3 * M_PI_2);  /* Start the sweep at minimum delay (for mono at least) */

  st_debug("delay_buf_length=%u lfo_length=%u\n",
      f->delay_buf_length, f->lfo_length);

  return ST_SUCCESS;
}



static int st_flanger_flow(eff_t effp, st_sample_t const * ibuf,
    st_sample_t * obuf, st_size_t * isamp, st_size_t * osamp)
{
  flanger_t f = (flanger_t) effp->priv;
  int c, channels = effp->ininfo.channels;
  st_size_t len = (*isamp > *osamp ? *osamp : *isamp) / channels;

  *isamp = *osamp = len * channels;

  while (len--) {
    f->delay_buf_pos =
      (f->delay_buf_pos + f->delay_buf_length - 1) % f->delay_buf_length;
    for (c = 0; c < channels; ++c) {
      double delayed_0, delayed_1;
      double delayed;
      double in, out;
      st_size_t channel_phase = c * f->lfo_length * f->channel_phase + .5;
      double delay = f->lfo[(f->lfo_pos + channel_phase) % f->lfo_length];
      double frac_delay = modf(delay, &delay);
      st_size_t int_delay = (size_t)delay;

      in = *ibuf++;
      f->delay_bufs[c][f->delay_buf_pos] = in + f->delay_last[c] * f->feedback_gain;

      delayed_0 = f->delay_bufs[c]
        [(f->delay_buf_pos + int_delay++) % f->delay_buf_length];
      delayed_1 = f->delay_bufs[c]
        [(f->delay_buf_pos + int_delay++) % f->delay_buf_length];

      if (f->interpolation == INTERP_LINEAR)
        delayed = delayed_0 + (delayed_1 - delayed_0) * frac_delay;
      else /* if (f->interpolation == INTERP_QUADRATIC) */
      {
        double a, b;
        double delayed_2 = f->delay_bufs[c]
          [(f->delay_buf_pos + int_delay++) % f->delay_buf_length];
        delayed_2 -= delayed_0;
        delayed_1 -= delayed_0;
        a = delayed_2 *.5 - delayed_1;
        b = delayed_1 * 2 - delayed_2 *.5;
        delayed = delayed_0 + (a * frac_delay + b) * frac_delay;
      }

      f->delay_last[c] = delayed;
      out = in * f->in_gain + delayed * f->delay_gain;
      *obuf++ = ST_ROUND_CLIP_COUNT(out, effp->clips);
    }
    f->lfo_pos = (f->lfo_pos + 1) % f->lfo_length;
  }

  return ST_SUCCESS;
}



static int st_flanger_stop(eff_t effp)
{
  flanger_t f = (flanger_t) effp->priv;
  int c, channels = effp->ininfo.channels;

  for (c = 0; c < channels; ++c)
    free(f->delay_bufs[c]);

  free(f->lfo);

  memset(f, 0, sizeof(*f));

  return ST_SUCCESS;
}



static st_effect_t st_flanger_effect = {
  "flanger",
  st_flanger_usage,
  ST_EFF_MCHAN,
  st_flanger_getopts,
  st_flanger_start,
  st_flanger_flow,
  st_effect_nothing_drain,
  st_flanger_stop,
  st_effect_nothing
};



st_effect_t const * st_flanger_effect_fn(void)
{
  return &st_flanger_effect;
}
