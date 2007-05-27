/*
 * LADSPA effectsupport for sox
 * (c) Reuben Thomas <rrt@sc3d.org> 2007
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, write to the Free Software
 * Foundation, Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301,
 * USA.  */

#include "sox_i.h"

#ifdef HAVE_LADSPA_H

#include <ltdl.h>
#include <ladspa.h>

static sox_effect_handler_t sox_ladspa_effect;

/* Private data for resampling */
typedef struct {
  char *name;                   /* Plugin name */
  lt_dlhandle lth;
} *ladspa_t;

/*
 * Process options
 */
static int sox_ladspa_getopts(sox_effect_t * effp, int n, char **argv)
{
  ladspa_t l_st = (ladspa_t)effp->priv;
  char *path;
  LADSPA_Descriptor *l_desc;
  LADSPA_Descriptor_Function l_fn;

  /* Get module name */
  if (n >= 1) {
    l_st->name = argv[0];
    n--; argv++;
  }

  /* Load module */
  path = getenv("LADSPA_PATH");
  if (path == NULL)
    path = LADSPA_PATH;
  lt_dlsetsearchpath(path);
  if ((l_st->lth = lt_dlopenext(l_st->name)) == NULL) {
    sox_fail("could not open LADSPA plugin %s", l_st->name);
    return SOX_EOF;
  }

  /* Get descriptor function */
  if ((l_fn = lt_dlsym(l_st->lth, "ladspa_descriptor") == NULL)) {
    sox_fail("could not find ladspa_descriptor");
    return SOX_EOF;
  }
  
  /* If more than one plugin, next argument is plugin name */
//   if (ladspa_descriptor
  
  /* Stop if we have any unused arguments */
  if (n > 0) {
    sox_fail(sox_ladspa_effect.usage);
    return SOX_EOF;
  }

  return SOX_SUCCESS;
}

/*
 * Prepare processing.
 */
static int sox_ladspa_start(sox_effect_t * effp)
{
  ladspa_t l_st = (ladspa_t)effp->priv;

  return SOX_SUCCESS;
}

/*
 * Process one bufferful of data.
 */
static int sox_ladspa_flow(sox_effect_t * effp, const sox_ssample_t *ibuf, sox_ssample_t *obuf UNUSED,
                           sox_size_t *isamp, sox_size_t *osamp)
{
  ladspa_t l_st = (ladspa_t)effp->priv;

  return SOX_SUCCESS;
}

/*
 * Close down the effect.
 */
static int sox_ladspa_drain(sox_effect_t * effp, sox_ssample_t *obuf, sox_size_t *osamp)
{
  ladspa_t l_st = (ladspa_t)effp->priv;

  *osamp = 0;

  return SOX_SUCCESS;
}

/*
 * Do anything required when you stop reading samples.
 * Don't close input file!
 */
static int sox_ladspa_stop(sox_effect_t * effp)
{
  ladspa_t l_st = (ladspa_t)effp->priv;

  return SOX_SUCCESS;
}

static sox_effect_handler_t sox_ladspa_effect = {
  "ladspa",
  "Usage: ladspa MODULE [PLUGIN] [ARGUMENT...]",
  SOX_EFF_RATE | SOX_EFF_MCHAN,
  sox_ladspa_getopts,
  sox_ladspa_start,
  sox_ladspa_flow,
  sox_ladspa_drain,
  sox_ladspa_stop,
  NULL
};

const sox_effect_handler_t *sox_ladspa_effect_fn(void)
{
  return &sox_ladspa_effect;
}

#endif /* HAVE_LADSPA */
