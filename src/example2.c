/*
 * Simple example of using SoX libraries
 *
 * Copyright (c) 2008 robs@users.sourceforge.net
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
 * with this program. If not, write to the Free Software Foundation, Fifth
 * Floor, 51 Franklin Street, Boston, MA 02111-1301, USA.
 */

#include "sox.h"
#include "util.h"
#include <stdio.h>
#include <math.h>
#ifdef NDEBUG /* N.B. assert used with active statements so enable always */
#undef NDEBUG
#endif
#include <assert.h>


/* 
 * Reads input file and displays a few seconds of wave-form, starting from
 * a given time through the audio.   E.g. example2 song2.au 30.75 1
 */
int main(int argc, char * argv[])
{
  sox_format_t * in;
  sox_sample_t * buf;
  size_t blocks, block_size;
  static const double block_period = 0.025; /* seconds */
  double start_secs = 0, period = 2;
  char dummy;
  sox_size_t seek;

  assert(sox_format_init() == SOX_SUCCESS);

  assert(argc > 1);
  ++argv, --argc;

  assert(in = sox_open_read(*argv, NULL, NULL, NULL));
  ++argv, --argc;

  if (argc) {
    assert(sscanf(*argv, "%lf%c", &start_secs, &dummy) == 1);
    ++argv, --argc;
  }

  if (argc) {
    assert(sscanf(*argv, "%lf%c", &period, &dummy) == 1);
    ++argv, --argc;
  }

  seek = start_secs * in->signal.rate * in->signal.channels + .5;
  seek -= seek % in->signal.channels;
  assert(sox_seek(in, seek, SOX_SEEK_SET) == SOX_SUCCESS);

  block_size = block_period * in->signal.rate * in->signal.channels + .5;
  block_size -= block_size % in->signal.channels;
  assert(buf = malloc(sizeof(sox_sample_t) * block_size));

  assert(in->signal.channels == 2);

  for (blocks = 0; sox_read(in, buf, block_size) == block_size && blocks * block_period < period; ++blocks) {
    double left = 0, right = 0;
    size_t i, clips = 0;
    static const char line[] = "===================================";
    int l, r;

    for (i = 0; i < block_size; ++i) {
      double sample = SOX_SAMPLE_TO_FLOAT_64BIT(buf[i], clips);
      if (i & 1)
        right = max(right, fabs(sample));
      else
        left = max(left, fabs(sample));
    }
    l = (1 - left) * 35;
    r = (1 - right) * 35;
    printf("%8.3f%36s|%s\n", start_secs + blocks * block_period, line + l, line + r);
  }

  free(buf);
  sox_close(in);
  sox_format_quit();
  return 0;
}
