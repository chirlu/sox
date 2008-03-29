/* Implements a libSoX internal interface for implementing effects.
 * All public functions & data are prefixed with lsx_ .
 *
 * (c) 2005-8 Chris Bagwell and SoX contributors
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "sox_i.h"
#include <string.h>

#undef sox_fail
#define sox_fail sox_globals.subsystem=effp->handler.name,sox_fail

int lsx_usage(sox_effect_t * effp)
{
  if (effp->handler.usage)
    sox_fail("usage: %s", effp->handler.usage);
  else
    sox_fail("this effect takes no parameters");
  return SOX_EOF;
}

/* here for linear interp.  might be useful for other things */
sox_sample_t lsx_gcd(sox_sample_t a, sox_sample_t b)
{
  if (b == 0)
    return a;
  else
    return lsx_gcd(b, a % b);
}

sox_sample_t lsx_lcm(sox_sample_t a, sox_sample_t b)
{
  /* parenthesize this way to avoid sox_sample_t overflow in product term */
  return a * (b / lsx_gcd(a, b));
}

enum_item const lsx_wave_enum[] = {
  ENUM_ITEM(SOX_WAVE_,SINE)
  ENUM_ITEM(SOX_WAVE_,TRIANGLE)
  {0, 0}};

void lsx_generate_wave_table(
    lsx_wave_t wave_type,
    sox_data_t data_type,
    void *table,
    uint32_t table_size,
    double min,
    double max,
    double phase)
{
  uint32_t t;
  uint32_t phase_offset = phase / M_PI / 2 * table_size + 0.5;

  for (t = 0; t < table_size; t++)
  {
    uint32_t point = (t + phase_offset) % table_size;
    double d;
    switch (wave_type)
    {
      case SOX_WAVE_SINE:
      d = (sin((double)point / table_size * 2 * M_PI) + 1) / 2;
      break;

      case SOX_WAVE_TRIANGLE:
      d = (double)point * 2 / table_size;
      switch (4 * point / table_size)
      {
        case 0:         d = d + 0.5; break;
        case 1: case 2: d = 1.5 - d; break;
        case 3:         d = d - 1.5; break;
      }
      break;

      default: /* Oops! FIXME */
        d = 0.0; /* Make sure we have a value */
      break;
    }
    d  = d * (max - min) + min;
    switch (data_type)
    {
      case SOX_FLOAT:
        {
          float *fp = (float *)table;
          *fp++ = (float)d;
          table = fp;
          continue;
        }
      case SOX_DOUBLE:
        {
          double *dp = (double *)table;
          *dp++ = d;
          table = dp;
          continue;
        }
      default: break;
    }
    d += d < 0? -0.5 : +0.5;
    switch (data_type)
    {
      case SOX_SHORT:
        {
          short *sp = table;
          *sp++ = (short)d;
          table = sp;
          continue;
        }
      case SOX_INT:
        {
          int *ip = table;
          *ip++ = (int)d;
          table = ip;
          continue;
        }
      default: break;
    }
  }
}

/*
 * lsx_parsesamples
 *
 * Parse a string for # of samples.  If string ends with a 's'
 * then the string is interpreted as a user calculated # of samples.
 * If string contains ':' or '.' or if it ends with a 't' then its
 * treated as an amount of time.  This is converted into seconds and
 * fraction of seconds and then use the sample rate to calculate
 * # of samples.
 * Returns NULL on error, pointer to next char to parse otherwise.
 */
char const * lsx_parsesamples(sox_rate_t rate, const char *str, sox_size_t *samples, int def)
{
    int found_samples = 0, found_time = 0;
    int time = 0;
    long long_samples;
    float frac = 0;
    char const * end;
    char const * pos;
    sox_bool found_colon, found_dot;

    for (end = str; *end && strchr("0123456789:.ts", *end); ++end);
    if (end == str)
      return NULL;

    pos = strchr(str, ':');
    found_colon = pos && pos < end;

    pos = strchr(str, '.');
    found_dot = pos && pos < end;

    if (found_colon || found_dot || *(end-1) == 't')
        found_time = 1;
    else if (*(end-1) == 's')
        found_samples = 1;

    if (found_time || (def == 't' && !found_samples))
    {
        *samples = 0;

        while(1)
        {
            if (str[0] != '.' && sscanf(str, "%d", &time) != 1)
                return NULL;
            *samples += time;

            while (*str != ':' && *str != '.' && *str != 0)
                str++;

            if (*str == '.' || *str == 0)
                break;

            /* Skip past ':' */
            str++;
            *samples *= 60;
        }

        if (*str == '.')
        {
            if (sscanf(str, "%f", &frac) != 1)
                return NULL;
        }

        *samples *= rate;
        *samples += (rate * frac) + 0.5;
        return end;
    }
    if (found_samples || (def == 's' && !found_time))
    {
        if (sscanf(str, "%ld", &long_samples) != 1)
            return NULL;
        *samples = long_samples;
        return end;
    }
    return NULL;
}
