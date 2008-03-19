/*
 * Simple example of using SoX libraries
 *
 * Copyright (c) 2007 robs@users.sourceforge.net
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
#include <stdio.h>
#ifdef NDEBUG /* N.B. assert used with active statements so enable always */
#undef NDEBUG
#endif
#include <assert.h>

static sox_format_t * in, * out;

static int input_drain(sox_effect_t *effp, sox_sample_t * obuf, sox_size_t * osamp)
{
  *osamp -= *osamp % effp->out_signal.channels;
  *osamp = sox_read(in, obuf, *osamp);
  *osamp -= *osamp % effp->out_signal.channels;
  if (!*osamp && in->sox_errno)
    fprintf(stderr, "%s: %s\n", in->filename, in->sox_errstr);
  return *osamp? SOX_SUCCESS : SOX_EOF;
}

static int output_flow(sox_effect_t *effp UNUSED, sox_sample_t const * ibuf,
    sox_sample_t * obuf UNUSED, sox_size_t * isamp, sox_size_t * osamp)
{
  size_t len = sox_write(out, ibuf, *isamp);

  *osamp = 0;
  if (len != *isamp) {
    fprintf(stderr, "%s: %s\n", out->filename, out->sox_errstr);
    return SOX_EOF;
  }
  return SOX_SUCCESS;
}

static sox_effect_handler_t const * input_handler(void)
{
  static sox_effect_handler_t handler = {
    "input", NULL, SOX_EFF_MCHAN, NULL, NULL, NULL, input_drain, NULL, NULL
  };
  return &handler;
}

static sox_effect_handler_t const * output_handler(void)
{
  static sox_effect_handler_t handler = {
    "output", NULL, SOX_EFF_MCHAN, NULL, NULL, output_flow, NULL, NULL, NULL
  };
  return &handler;
}

/* 
 * Reads input file, applies vol & flanger effects, stores in output file.
 * E.g. example1 monkey.au monkey.aiff
 */
int main(int argc, char * argv[])
{
  sox_effects_chain_t * chain;
  sox_effect_t e;
  char * vol[] = {"3dB"};

  assert(argc == 3);
  assert(sox_format_init() == SOX_SUCCESS);

  assert(in = sox_open_read(argv[1], NULL, NULL, NULL));
  assert(out = sox_open_write(NULL, argv[2], &in->signal, NULL, NULL, NULL, 0, NULL, 0));

  chain = sox_create_effects_chain(&in->encoding, &out->encoding);

  sox_create_effect(&e, input_handler());
  assert(sox_add_effect(chain, &e, &in->signal, &in->signal) == SOX_SUCCESS);

  sox_create_effect(&e, sox_find_effect("vol"));
  assert(e.handler.getopts(&e, 1, vol) == SOX_SUCCESS);
  assert(sox_add_effect(chain, &e, &in->signal, &in->signal) == SOX_SUCCESS);

  sox_create_effect(&e, sox_find_effect("flanger"));
  assert(e.handler.getopts(&e, 0, NULL) == SOX_SUCCESS);
  assert(sox_add_effect(chain, &e, &in->signal, &in->signal) == SOX_SUCCESS);

  sox_create_effect(&e, output_handler());
  assert(sox_add_effect(chain, &e, &in->signal, &in->signal) == SOX_SUCCESS);

  sox_flow_effects(chain, NULL);

  sox_delete_effects(chain);
  sox_close(out);
  sox_close(in);
  sox_format_quit();
  free(chain);
  return 0;
}
