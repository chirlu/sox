/* libSoX effect: Normalise   (c) 2008 robs@users.sourceforge.net
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

typedef struct {
  sox_bool      individual;
  double        norm0;    /* Multiplier to take to 0dB FSD */
  double        level;    /* Multiplier to take to 'level' */
  sox_sample_t  min, max;
  FILE          * tmp_file;
} priv_t;
#define p ((priv_t *)effp->priv)

static int create(sox_effect_t * effp, int argc, char * * argv)
{
  if (argc && !strcmp(*argv, "-i")) p->individual = sox_true, ++argv, --argc;
  do {NUMERIC_PARAMETER(level, -100, 0)} while (0);
  p->level = dB_to_linear(p->level);
  return argc?  lsx_usage(effp) : SOX_SUCCESS;
}

static int start(sox_effect_t * effp)
{
  if (!p->individual)
    effp->flows = 1;
  p->norm0 = p->max = p->min = 0;
  p->tmp_file = tmpfile();
  if (p->tmp_file == NULL) {
    sox_fail("can't create temporary file: %s", strerror(errno));
    return SOX_EOF;
  }
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, sox_size_t * isamp, sox_size_t * osamp)
{
  sox_size_t len;

  if (fwrite(ibuf, sizeof(*ibuf), *isamp, p->tmp_file) != *isamp) {
    sox_fail("error writing temporary file: %s", strerror(errno));
    return SOX_EOF;
  }
  for (len = *osamp; len; --len, ++ibuf) {
    p->max = max(p->max, *ibuf);
    p->min = min(p->min, *ibuf);
  }
  (void)obuf, *osamp = 0; /* samples not output until drain */
  return SOX_SUCCESS;
}

static int drain(sox_effect_t * effp, sox_sample_t * obuf, sox_size_t * osamp)
{
  sox_size_t len;
  int result = SOX_SUCCESS;

  if (!p->norm0) {
    double max = lsx_sample_max(effp->out_encoding);
    p->norm0 = p->level * min(max / p->max, (double)SOX_SAMPLE_MIN / p->min);
    rewind(p->tmp_file);
  }
  len = fread(obuf, sizeof(*obuf), *osamp, p->tmp_file);
  if (len != *osamp && !feof(p->tmp_file)) {
    sox_fail("error reading temporary file: %s", strerror(errno));
    result = SOX_EOF;
  }
  for (*osamp = len; len; --len, ++obuf)
    *obuf = floor(*obuf * p->norm0 + .5);
  return result;
}

static int stop(sox_effect_t * effp)
{
  fclose(p->tmp_file); /* auto-deleted by tmpfile */
  return SOX_SUCCESS;
}

sox_effect_handler_t const * sox_norm_effect_fn(void)
{
  static sox_effect_handler_t handler = {"norm", "[-i] [level]", 0,
    create, start, flow, drain, stop, NULL, sizeof(priv_t)};
  return &handler;
}
