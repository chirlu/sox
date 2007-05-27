/*
 * LADSPA effect support for sox
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

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <ltdl.h>
#include <ladspa.h>

static sox_effect_handler_t sox_ladspa_effect;

/* Private data for resampling */
typedef struct {
  char *name;                   /* plugin name */
  lt_dlhandle lth;              /* dynamic object handle */
  const LADSPA_Descriptor *desc; /* plugin descriptor */
  LADSPA_Handle handle;         /* instantiated plugin handle */
  LADSPA_Data *control;         /* control ports */
  unsigned long input_port, output_port;
} *ladspa_t;

/*
 * Process options
 */
static int sox_ladspa_getopts(sox_effect_t *effp, int n, char **argv)
{
  ladspa_t l_st = (ladspa_t)effp->priv;
  char *path;
  LADSPA_Descriptor_Function l_fn;
  unsigned long index = 0, i;
  unsigned long audio_ports = 0;
  double arg;

  l_st->input_port = ULONG_MAX;
  l_st->output_port = ULONG_MAX;

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
  if ((l_fn = lt_dlsym(l_st->lth, "ladspa_descriptor")) == NULL) {
    sox_fail("could not find ladspa_descriptor");
    return SOX_EOF;
  }

  /* If no plugins in this module, complain */
  if (l_fn(0) == NULL) {
    sox_fail("no plugins found");
    return SOX_EOF;
  }

  /* Get first plugin descriptor */
  l_st->desc = l_fn(0);
  assert(l_st->desc);           /* We already know this will work */

  /* If more than one plugin, or first argument is not a number, try
     to use first argument as plugin label. */
  if (l_fn(1) != NULL || !sscanf(argv[0], "%lf", &arg)) {
    while (l_st->desc && strcmp(l_st->desc->Label, argv[0]) != 0)
      l_st->desc = l_fn(++index);
    if (l_st->desc == NULL) {
      sox_fail("no plugin called `%s' found", argv[0]);
      return SOX_EOF;
    } else
      n--; argv++;
  }


  /* Instantiate the plugin */
  l_st->handle = l_st->desc->instantiate(l_st->desc, effp->ininfo.rate);
  if (l_st->handle == NULL) {
    sox_fail("could not instantiate plugin");
    return SOX_EOF;
  }

  /* Scan the ports to check there's one input and one output */
  l_st->control = xcalloc(l_st->desc->PortCount, sizeof(LADSPA_Data));
  for (i = 0; i < l_st->desc->PortCount; i++) {
    const LADSPA_PortDescriptor port = l_st->desc->PortDescriptors[i];

    /* Check port is well specified. All control ports should be
       inputs, but don't bother checking, as we never rely on this. */
    if (LADSPA_IS_PORT_INPUT(port) && LADSPA_IS_PORT_OUTPUT(port)) {
      sox_fail("port %d is both input and output", i);
      return SOX_EOF;
    } else if (LADSPA_IS_PORT_CONTROL(port) && LADSPA_IS_PORT_AUDIO(port)) {
      sox_fail("port %d is both audio and control", i);
      return SOX_EOF;
    }
               
    if (LADSPA_IS_PORT_AUDIO(port)) {
      audio_ports++;
      if (audio_ports > 2) {
        sox_fail("can't use a plugin with more than two audio ports");
        return SOX_EOF;
      }

      /* Don't bother counting input and output ports, as if we have
         too many or not enough we'll find out anyway */
      if (LADSPA_IS_PORT_INPUT(port))
        l_st->input_port = i;
      else if (LADSPA_IS_PORT_OUTPUT(port))
        l_st->output_port = i;
    } else {                    /* Control port */
      if (n == 0) {
        sox_fail("not enough arguments for control ports");
        return SOX_EOF;
      }
      if (!sscanf(argv[0], "%lf", &arg)) {
        sox_fail(effp->handler.usage);
        return SOX_EOF;
      }
      l_st->control[i] = (LADSPA_Data)arg;
      sox_debug("argument for port %d is %f", i, l_st->control[i]);
      n--; argv++;
      l_st->desc->connect_port(l_st->handle, i, &(l_st->control[i]));
    }
  }

  /* FIXME: allow use of source and sink plugins */
  if (l_st->input_port == ULONG_MAX) {
    sox_fail("no input port");
    return SOX_EOF;
  } else if (l_st->output_port == ULONG_MAX) {
    sox_fail("no output port");
    return SOX_EOF;
  }
  
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

  /* If needed, activate the plugin */
  if (l_st->desc->activate)
    l_st->desc->activate(l_st->handle);

  return SOX_SUCCESS;
}

/*
 * Process one bufferful of data.
 */
static int sox_ladspa_flow(sox_effect_t * effp, const sox_ssample_t *ibuf, sox_ssample_t *obuf,
                           sox_size_t *isamp, sox_size_t *osamp)
{
  ladspa_t l_st = (ladspa_t)effp->priv;
  LADSPA_Data *buf = xmalloc(sizeof(LADSPA_Data) * *isamp);
  sox_sample_t i;

  /* Copy the input; FIXME: Assume LADSPA_Data == float! */
  for (i = 0; i < *isamp; i++)
    buf[i] = SOX_SAMPLE_TO_FLOAT_32BIT(ibuf[i], effp->clips);

  /* Connect the I/O ports */
  l_st->desc->connect_port(l_st->handle, l_st->input_port, buf);
  l_st->desc->connect_port(l_st->handle, l_st->output_port, buf);

  /* Run the plugin */
  l_st->desc->run(l_st->handle, *isamp);

  /* Copy the output; FIXME: Assume LADSPA_Data == float! */
  *osamp = *isamp;
  for (i = 0; i < *osamp; i++)
    obuf[i] = SOX_FLOAT_32BIT_TO_SAMPLE(buf[i], effp->clips);

  free(buf);

  return SOX_SUCCESS;
}

/*
 * Nothing to do.
 */
static int sox_ladspa_drain(sox_effect_t * effp UNUSED, sox_ssample_t *obuf UNUSED, sox_size_t *osamp)
{
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

  /* If needed, deactivate the plugin */
  if (l_st->desc->deactivate)
    l_st->desc->deactivate(l_st->handle);

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
