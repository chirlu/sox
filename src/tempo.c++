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

/*
 * (c) 2007 robs@users.sourceforge.net
 *
 * libSoX tempo effect: adjust the audio tempo (but not key)
 *
 * Adjustment is given as the ratio of the new tempo to the old tempo.
 *
 * libSoX key effect: adjust the audio pitch (but not tempo)
 *
 * Adjustment is given as a number of cents (100ths of a semitone) to
 * change.  Implementation comprises a tempo change (performed by tempo)
 * and a speed change performed by whichever resampling effect is in effect.
 */

#include <soundtouch/SoundTouch.h>

extern "C" {

#include "sox_i.h"
#include <math.h>
#include <string.h>

typedef struct tempo
{
  soundtouch::SAMPLETYPE * buffer;
  soundtouch::SoundTouch * sound_touch;
  double factor;
} * tempo_t;

assert_static(sizeof(struct tempo) <= SOX_MAX_EFFECT_PRIVSIZE,
              /* else */ tempo_PRIVSIZE_too_big);

static int create(sox_effect_t * effp, int n, char * * argv)
{
  tempo_t p = (tempo_t) effp->priv;
  char dummy;

  if (n == 1 && sscanf(*argv, "%lf %c", &p->factor, &dummy) == 1 && p->factor >=0.05 && p->factor <= 20)
    return SOX_SUCCESS;
  return sox_usage(effp);
}

static int start(sox_effect_t * effp)
{
  tempo_t p = (tempo_t) effp->priv;

  if (!p->factor)
    return SOX_EFF_NULL;

  p->buffer = new soundtouch::SAMPLETYPE[effp->global_info->global_info->bufsiz];

  p->sound_touch = new soundtouch::SoundTouch;
  p->sound_touch->setSampleRate(static_cast<uint>(effp->ininfo.rate + 0.5));
  p->sound_touch->setTempoChange(100 / p->factor - 100);

  p->sound_touch->setChannels(1);
  p->sound_touch->setPitchSemiTones(0);
  p->sound_touch->setRateChange(0);
  p->sound_touch->setSetting(SETTING_USE_AA_FILTER, 0);
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_ssample_t * ibuf, sox_ssample_t * obuf,
                sox_size_t * isamp, sox_size_t * osamp)
{
  tempo_t p = (tempo_t) effp->priv;
  sox_size_t i;
  sox_size_t idone = 0;
  sox_size_t odone = p->sound_touch->receiveSamples(p->buffer, *osamp);

  for (i = 0; i < odone; ++i)
    obuf[i] = SOX_FLOAT_32BIT_TO_SAMPLE(p->buffer[i], effp->clips);

  if (odone < *osamp)
  if (*isamp && odone < *osamp) {
    for (i = 0; i < *isamp; ++i)
      p->buffer[i] = SOX_SAMPLE_TO_FLOAT_32BIT(ibuf[i], effp->clips);
    p->sound_touch->putSamples(p->buffer, idone = *isamp);
  }

  *isamp = idone;
  *osamp = odone;
  return SOX_SUCCESS;
}

static int drain(sox_effect_t * effp, sox_ssample_t * obuf, sox_size_t * osamp)
{
  static sox_size_t isamp = 0;
  tempo_t p = (tempo_t) effp->priv;
  p->sound_touch->flush();
  return flow(effp, 0, obuf, &isamp, osamp);
}

static int stop(sox_effect_t * effp)
{
  tempo_t p = (tempo_t) effp->priv;
  delete p->sound_touch;
  delete[] p->buffer;
  return SOX_SUCCESS;
}

sox_effect_handler_t const *sox_tempo_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "tempo", "factor", SOX_EFF_LENGTH,
    create, start, flow, drain, stop, 0};
  return &handler;
}

static int key_create(sox_effect_t * effp, int argc, char * * argv)
{
  double d;
  char dummy, arg[100];
  char * args[10];
  sox_size_t nargs = 0;

  if (!argc || sscanf(*argv, "%lf %c", &d, &dummy) != 1)
    return sox_usage(effp);

  d = pow(2., d/1200);
  effp->global_info->speed *= d;
  sprintf(arg, "%g", d);
  args[nargs++] = arg;
  ++argv, --argc;

  return argc ? sox_usage(effp) :
    sox_tempo_effect_fn()->getopts(effp, nargs, args);
}

sox_effect_handler_t const * sox_key_effect_fn(void)
{
  static sox_effect_handler_t handler;
  handler = *sox_tempo_effect_fn();
  handler.name = "key";
  handler.usage = "shift-in-cents";
  handler.getopts = key_create;
  return &handler;
}

} // extern "C"
