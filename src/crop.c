/* libSoX effect: Crop ends of audio   (c) 2009 robs@users.sourceforge.net
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

/* This is W.I.P. hence marked SOX_EFF_ALPHA for now.
 * Need to change size_t/INT32_MAX to uint64_t/INT64_MAX (for LFS) and do
 * proper length change tracking through the effects chain.
 */

#include "sox_i.h"
#include <string.h>

typedef struct {
  int argc;
  struct {int flag; char * str; size_t at;} pos[2];
} priv_t;

static int parse(sox_effect_t * effp, char * * argv, sox_rate_t rate)
{
  priv_t * p = (priv_t *)effp->priv;
  char const * s, * q;
  int i;

  for (i = p->argc - 1; i == 0 || i == 1; --i) {
    if (argv) /* 1st parse only */
      p->pos[i].str = lsx_strdup(argv[i]);
    s = p->pos[i].str;
    if (strchr("+-" + 1 - i, *s))
      p->pos[i].flag = *s++;
    if (!(q = lsx_parsesamples(rate, s, &p->pos[i].at, 't')) || *q)
      break;
  }
  return i >= 0 ? lsx_usage(effp) : SOX_SUCCESS;
}

static int create(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *)effp->priv;
  --argc, ++argv;
  p->argc = argc;
  return parse(effp, argv, 1e5); /* No rate yet; parse with dummy */
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  int i;

  p->pos[1].at = SIZE_MAX / 2 / effp->in_signal.channels;
  parse(effp, 0, effp->in_signal.rate); /* Re-parse now rate is known */
  for (i = 0; i < 2; ++i) {
    p->pos[i].at *= effp->in_signal.channels;
    if (p->pos[i].flag == '-') {
      if (effp->in_signal.length == SOX_UNSPEC) {
        lsx_fail("cannot crop from end: audio length is not known");
        return SOX_EOF;
      }
      if (p->pos[i].at > effp->in_signal.length) {
        lsx_fail("cannot crop that much from end: audio is too short");
        return SOX_EOF;
      }
      p->pos[i].at = effp->in_signal.length - p->pos[i].at;
    }
  }
  if (p->pos[1].flag != '+') {
    if (p->pos[0].at > p->pos[1].at) {
      lsx_fail("start position must be less than stop position");
      return SOX_EOF;
    }
    if (!(p->pos[1].at -= p->pos[0].at))
      p->pos[0].at = 0;
  }
  if (effp->in_signal.length) {
    if (!p->pos[0].at && p->pos[1].at == effp->in_signal.length)
      return SOX_EFF_NULL;
    if (p->pos[0].at > effp->in_signal.length ||
        (p->argc > 1 && p->pos[0].at + p->pos[1].at > effp->in_signal.length)) {
      lsx_fail("audio is too short");
      return SOX_EOF;
    }
    effp->out_signal.length = p->argc == 2 ?
      p->pos[1].at : effp->in_signal.length - p->pos[0].at;
  }
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, sox_sample_t const * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t skipped;

  p->pos[0].at -= skipped = min(p->pos[0].at, *isamp);
  *osamp = !p->pos[0].at * min(p->pos[1].at, min(*isamp - skipped, *osamp));
  memcpy(obuf, ibuf + skipped, *osamp * sizeof(*obuf));
  *isamp = skipped + *osamp;
  return (p->pos[1].at -= *osamp) ? SOX_SUCCESS : SOX_EOF;
}

static int stop(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  if (p->pos[0].at || (p->pos[1].at && p->argc == 2))
    lsx_warn("input audio was too short to crop as requested");
  return SOX_SUCCESS;
}

static int lsx_kill(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  free(p->pos[0].str);
  free(p->pos[1].str);
  return SOX_SUCCESS;
}

sox_effect_handler_t const * lsx_crop_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "crop", "[-]before [[+|-]from]\n"
      "  n\tposition relative to start\n"
      "  -n\tposition relative to end\n"
      "  +n\tposition relative to previous",
    SOX_EFF_MCHAN | /* SOX_EFF_LENGTH | */ SOX_EFF_MODIFY | SOX_EFF_ALPHA,

    create, start, flow, NULL, stop, lsx_kill, sizeof(priv_t)
  };
  return &handler;
}

size_t sox_crop_get_start(sox_effect_t * effp)
{
  return ((priv_t *)effp->priv)->pos[0].at;
}

void sox_crop_clear_start(sox_effect_t * effp)
{
  ((priv_t *)effp->priv)->pos[0].at = 0;
}

#if 0
/*---------------------- emulation of the `trim' effect ----------------------*/

static int trim_getopts(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *)effp->priv;
  p->pos[1].flag = '+';
  return lsx_crop_effect_fn()->getopts(effp, argc, argv);
}

sox_effect_handler_t const * lsx_trim_effect_fn(void)
{
  static sox_effect_handler_t handler;
  handler = *lsx_crop_effect_fn();
  handler.name = "trim";
  handler.usage = "start [length]";
  handler.getopts = trim_getopts;
  return &handler;
}
#endif
