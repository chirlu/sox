/* Simple example of using SoX libraries
 *
 * Copyright (c) 2007-8 robs@users.sourceforge.net
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

#include "sox.h"
#include <stdio.h>
#ifdef NDEBUG /* N.B. assert used with active statements so enable always */
#undef NDEBUG
#endif
#include <assert.h>

/*
 * Reads input file, applies vol & flanger effects, stores in output file.
 * E.g. example1 monkey.au monkey.aiff
 */
int main(int argc, char * argv[])
{
  static sox_format_t * in, * out; /* input and output files */
  sox_effects_chain_t * chain;
  sox_effect_t * e;
  char * args[10];

  assert(argc == 3);

  /* All libSoX applications must start by initialising the SoX library */
  assert(sox_format_init() == SOX_SUCCESS);

  /* Open the input file (with default parameters) */
  assert(in = sox_open_read(argv[1], NULL, NULL, NULL));

  /* Open the output file; we must specify the output signal characteristics.
   * Since we are using only simple effects, they are the same as the input
   * file characteristics */
  assert(out = sox_open_write(argv[2], &in->signal, NULL, NULL, NULL, NULL));

  /* Create an effects chain; some effects need to know about the input
   * or output file encoding so we provide that information here */
  chain = sox_create_effects_chain(&in->encoding, &out->encoding);

  /* The first effect in the effect chain must be something that can source
   * samples; in this case, we use the built-in handler that inputs
   * data from an audio file */
  e = sox_create_effect(sox_find_effect("input"));
  args[0] = (char *)in, assert(e->handler.getopts(e, 1, args) == SOX_SUCCESS);
  /* This becomes the first `effect' in the chain */
  assert(sox_add_effect(chain, e, &in->signal, &in->signal) == SOX_SUCCESS);

  /* Create the `vol' effect, and initialise it with the desired parameters: */
  e = sox_create_effect(sox_find_effect("vol"));
  args[0] = "3dB", assert(e->handler.getopts(e, 1, args) == SOX_SUCCESS);
  /* Add the effect to the end of the effects processing chain: */
  assert(sox_add_effect(chain, e, &in->signal, &in->signal) == SOX_SUCCESS);

  /* Create the `flanger' effect, and initialise it with default parameters: */
  e = sox_create_effect(sox_find_effect("flanger"));
  assert(e->handler.getopts(e, 0, NULL) == SOX_SUCCESS);
  /* Add the effect to the end of the effects processing chain: */
  assert(sox_add_effect(chain, e, &in->signal, &in->signal) == SOX_SUCCESS);

  /* The last effect in the effect chain must be something that only consumes
   * samples; in this case, we use the built-in handler that outputs
   * data to an audio file */
  e = sox_create_effect(sox_find_effect("output"));
  args[0] = (char *)out, assert(e->handler.getopts(e, 1, args) == SOX_SUCCESS);
  assert(sox_add_effect(chain, e, &in->signal, &in->signal) == SOX_SUCCESS);

  /* Flow samples through the effects processing chain until EOF is reached */
  sox_flow_effects(chain, NULL);

  /* All done; tidy up: */
  sox_delete_effects_chain(chain);
  sox_close(out);
  sox_close(in);
  sox_format_quit();
  return 0;
}
