/* Implements a libSoX internal interface for implementing effects.
 * All public functions & data are prefixed with lsx_ .
 *
 * Copyright (c) 2005-8 Chris Bagwell and SoX contributors
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

#define LSX_EFF_ALIAS
#include "sox_i.h"
#include <string.h>
#include <ctype.h>

int lsx_usage(sox_effect_t * effp)
{
  if (effp->handler.usage)
    lsx_fail("usage: %s", effp->handler.usage);
  else
    lsx_fail("this effect takes no parameters");
  return SOX_EOF;
}

char * lsx_usage_lines(char * * usage, char const * const * lines, size_t n)
{
  if (!*usage) {
    size_t i, len;
    for (len = i = 0; i < n; len += strlen(lines[i++]) + 1);
    *usage = lsx_malloc(len); /* FIXME: this memory will never be freed */
    strcpy(*usage, lines[0]);
    for (i = 1; i < n; ++i) {
      strcat(*usage, "\n");
      strcat(*usage, lines[i]);
    }
  }
  return *usage;
}

static lsx_enum_item const s_lsx_wave_enum[] = {
  LSX_ENUM_ITEM(SOX_WAVE_,SINE)
  LSX_ENUM_ITEM(SOX_WAVE_,TRIANGLE)
  {0, 0}};

lsx_enum_item const * lsx_get_wave_enum(void)
{
  return s_lsx_wave_enum;
}

void lsx_generate_wave_table(
    lsx_wave_t wave_type,
    sox_data_t data_type,
    void *table,
    size_t table_size,
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
char const * lsx_parsesamples(sox_rate_t rate, const char *str0, uint64_t *samples, int def)
{
  int i, found_samples = 0, found_time = 0;
  char const * end;
  char const * pos;
  sox_bool found_colon, found_dot;
  char * str = (char *)str0;

  for (;*str == ' '; ++str);
  for (end = str; *end && strchr("0123456789:.ets", *end); ++end);
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

  if (found_time || (def == 't' && !found_samples)) {
    for (*samples = 0, i = 0; *str != '.' && i < 3; ++i) {
      char * last_str = str;
      long part = strtol(str, &str, 10);
      if (!i && str == last_str)
        return NULL;
      *samples += rate * part;
      if (i < 2) {
        if (*str != ':')
          break;
        ++str;
        *samples *= 60;
      }
    }
    if (*str == '.') {
      char * last_str = str;
      double part = strtod(str, &str);
      if (str == last_str)
        return NULL;
      *samples += rate * part + .5;
    }
    return *str == 't'? str + 1 : str;
  }
  {
    char * last_str = str;
    double part = strtod(str, &str);
    if (str == last_str)
      return NULL;
    *samples = part + .5;
    return *str == 's'? str + 1 : str;
  }
}

#if 0

#include <assert.h>

#define TEST(st, samp, len) \
  str = st; \
  next = lsx_parsesamples(10000, str, &samples, 't'); \
  assert(samples == samp && next == str + len);

int main(int argc, char * * argv)
{
  char const * str, * next;
  uint64_t samples;

  TEST("0"  , 0, 1)
  TEST("1" , 10000, 1)

  TEST("0s" , 0, 2)
  TEST("0s,", 0, 2)
  TEST("0s/", 0, 2)
  TEST("0s@", 0, 2)

  TEST("0t" , 0, 2)
  TEST("0t,", 0, 2)
  TEST("0t/", 0, 2)
  TEST("0t@", 0, 2)

  TEST("1s" , 1, 2)
  TEST("1s,", 1, 2)
  TEST("1s/", 1, 2)
  TEST("1s@", 1, 2)
  TEST(" 01s" , 1, 4)
  TEST("1e6s" , 1000000, 4)

  TEST("1t" , 10000, 2)
  TEST("1t,", 10000, 2)
  TEST("1t/", 10000, 2)
  TEST("1t@", 10000, 2)
  TEST("1.1t" , 11000, 4)
  TEST("1.1t,", 11000, 4)
  TEST("1.1t/", 11000, 4)
  TEST("1.1t@", 11000, 4)
  TEST("1e6t" , 10000, 1)

  TEST(".0", 0, 2)
  TEST("0.0", 0, 3)
  TEST("0:0.0", 0, 5)
  TEST("0:0:0.0", 0, 7)

  TEST(".1", 1000, 2)
  TEST(".10", 1000, 3)
  TEST("0.1", 1000, 3)
  TEST("1.1", 11000, 3)
  TEST("1:1.1", 611000, 5)
  TEST("1:1:1.1", 36611000, 7)
  TEST("1:1", 610000, 3)
  TEST("1:01", 610000, 4)
  TEST("1:1:1", 36610000, 5)
  TEST("1:", 600000, 2)
  TEST("1::", 36000000, 3)

  TEST("0.444444", 4444, 8)
  TEST("0.555555", 5556, 8)

  assert(!lsx_parsesamples(10000, "x", &samples, 't'));
  return 0;
}
#endif 

/* a note is given as an int,
 * 0   => 440 Hz = A
 * >0  => number of half notes 'up',
 * <0  => number of half notes down,
 * example 12 => A of next octave, 880Hz
 *
 * calculated by freq = 440Hz * 2**(note/12)
 */
static double calc_note_freq(double note, int key)
{
  if (key != INT_MAX) {                         /* Just intonation. */
    static const int n[] = {16, 9, 6, 5, 4, 7}; /* Numerator. */
    static const int d[] = {15, 8, 5, 4, 3, 5}; /* Denominator. */
    static double j[13];                        /* Just semitones */
    int i, m = floor(note);

    if (!j[1]) for (i = 1; i <= 12; ++i)
      j[i] = i <= 6? log((double)n[i - 1] / d[i - 1]) / log(2.) : 1 - j[12 - i];
    note -= m;
    m -= key = m - ((INT_MAX / 2 - ((INT_MAX / 2) % 12) + m - key) % 12);
    return 440 * pow(2., key / 12. + j[m] + (j[m + 1] - j[m]) * note);
  }
  return 440 * pow(2., note / 12);
}

int lsx_parse_note(char const * text, char * * end_ptr)
{
  int result = INT_MAX;

  if (*text >= 'A' && *text <= 'G') {
    result = (int)(5/3. * (*text++ - 'A') + 9.5) % 12 - 9;
    if (*text == 'b') {--result; ++text;}
    else if (*text == '#') {++result; ++text;}
    if (isdigit((unsigned char)*text))
      result += 12 * (*text++ - '4'); 
  }
  *end_ptr = (char *)text;
  return result;
}

/* Read string 'text' and convert to frequency.
 * 'text' can be a positive number which is the frequency in Hz.
 * If 'text' starts with a '%' and a following number the corresponding
 * note is calculated.
 * Return -1 on error.
 */
double lsx_parse_frequency_k(char const * text, char * * end_ptr, int key)
{
  double result;

  if (*text == '%') {
    result = strtod(text + 1, end_ptr);
    if (*end_ptr == text + 1)
      return -1;
    return calc_note_freq(result, key);
  }
  if (*text >= 'A' && *text <= 'G') {
    int result2 = lsx_parse_note(text, end_ptr);
    return result2 == INT_MAX? - 1 : calc_note_freq((double)result2, key);
  }
  result = strtod(text, end_ptr);
  if (end_ptr) {
    if (*end_ptr == text)
      return -1;
    if (**end_ptr == 'k') {
      result *= 1000;
      ++*end_ptr;
    }
  }
  return result < 0 ? -1 : result;
}

FILE * lsx_open_input_file(sox_effect_t * effp, char const * filename)
{
  FILE * file;

  if (!filename || !strcmp(filename, "-")) {
    if (effp->global_info->global_info->stdin_in_use_by) {
      lsx_fail("stdin already in use by `%s'", effp->global_info->global_info->stdin_in_use_by);
      return NULL;
    }
    effp->global_info->global_info->stdin_in_use_by = effp->handler.name;
    file = stdin;
  }
  else if (!(file = fopen(filename, "r"))) {
    lsx_fail("couldn't open file %s: %s", filename, strerror(errno));
    return NULL;
  }
  return file;
}

int lsx_effects_init(void)
{
  init_fft_cache();
  return SOX_SUCCESS;
}

int lsx_effects_quit(void)
{
  clear_fft_cache();
  return SOX_SUCCESS;
}
