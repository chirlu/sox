/* LADSPA effect support for sox
 * (c) Reuben Thomas <rrt@sc3d.org> 2007
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

#ifdef HAVE_LADSPA_H

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <ltdl.h>
#include "ladspa.h"

static sox_effect_handler_t sox_ladspa_effect;

/* Private data for resampling */
typedef struct {
  char *name;                   /* plugin name */
  lt_dlhandle lth;              /* dynamic object handle */
  const LADSPA_Descriptor *desc; /* plugin descriptor */
  LADSPA_Handle handle;         /* instantiated plugin handle */
  LADSPA_Data *control;         /* control ports */
  unsigned long input_port, output_port;
} priv_t;

static LADSPA_Data ladspa_default(const LADSPA_PortRangeHint *p)
{
  LADSPA_Data d;

  if (LADSPA_IS_HINT_DEFAULT_0(p->HintDescriptor))
    d = 0.0;
  else if (LADSPA_IS_HINT_DEFAULT_1(p->HintDescriptor))
    d = 1.0;
  else if (LADSPA_IS_HINT_DEFAULT_100(p->HintDescriptor))
    d = 100.0;
  else if (LADSPA_IS_HINT_DEFAULT_440(p->HintDescriptor))
    d = 440.0;
  else if (LADSPA_IS_HINT_DEFAULT_MINIMUM(p->HintDescriptor))
    d = p->LowerBound;
  else if (LADSPA_IS_HINT_DEFAULT_MAXIMUM(p->HintDescriptor))
    d = p->UpperBound;
  else if (LADSPA_IS_HINT_DEFAULT_LOW(p->HintDescriptor)) {
    if (LADSPA_IS_HINT_LOGARITHMIC(p->HintDescriptor))
      d = exp(log(p->LowerBound) * 0.75 + log(p->UpperBound) * 0.25);
    else
      d = p->LowerBound * 0.75 + p->UpperBound * 0.25;
  } else if (LADSPA_IS_HINT_DEFAULT_MIDDLE(p->HintDescriptor)) {
    if (LADSPA_IS_HINT_LOGARITHMIC(p->HintDescriptor))
      d = exp(log(p->LowerBound) * 0.5 + log(p->UpperBound) * 0.5);
    else
      d = p->LowerBound * 0.5 + p->UpperBound * 0.5;
  } else if (LADSPA_IS_HINT_DEFAULT_HIGH(p->HintDescriptor)) {
    if (LADSPA_IS_HINT_LOGARITHMIC(p->HintDescriptor))
      d = exp(log(p->LowerBound) * 0.25 + log(p->UpperBound) * 0.75);
    else
      d = p->LowerBound * 0.25 + p->UpperBound * 0.75;
  } else { /* shouldn't happen */
    /* FIXME: Deal with this at a higher level */
    lsx_fail("non-existent default value; using 0.1");
    d = 0.1; /* Should at least avoid divide by 0 */
  }

  return d;
}

/*
 * Process options
 */
static int sox_ladspa_getopts(sox_effect_t *effp, int argc, char **argv)
{
  priv_t * l_st = (priv_t *)effp->priv;
  char *path;
  union {LADSPA_Descriptor_Function fn; lt_ptr ptr;} ltptr;
  unsigned long index = 0, i;
  double arg;
  --argc, ++argv;

  l_st->input_port = ULONG_MAX;
  l_st->output_port = ULONG_MAX;

  /* Get module name */
  if (argc >= 1) {
    l_st->name = argv[0];
    argc--; argv++;
  }

  /* Load module */
  path = getenv("LADSPA_PATH");
  if (path == NULL)
    path = LADSPA_PATH;

  if(lt_dlinit() || lt_dlsetsearchpath(path)
      || (l_st->lth = lt_dlopenext(l_st->name)) == NULL) {
    lsx_fail("could not open LADSPA plugin %s", l_st->name);
    return SOX_EOF;
  }

  /* Get descriptor function */
  if ((ltptr.ptr = lt_dlsym(l_st->lth, "ladspa_descriptor")) == NULL) {
    lsx_fail("could not find ladspa_descriptor");
    return SOX_EOF;
  }

  /* If no plugins in this module, complain */
  if (ltptr.fn(0UL) == NULL) {
    lsx_fail("no plugins found");
    return SOX_EOF;
  }

  /* Get first plugin descriptor */
  l_st->desc = ltptr.fn(0UL);
  assert(l_st->desc);           /* We already know this will work */

  /* If more than one plugin, or first argument is not a number, try
     to use first argument as plugin label. */
  if (argc > 0 && (ltptr.fn(1UL) != NULL || !sscanf(argv[0], "%lf", &arg))) {
    while (l_st->desc && strcmp(l_st->desc->Label, argv[0]) != 0)
      l_st->desc = ltptr.fn(++index);
    if (l_st->desc == NULL) {
      lsx_fail("no plugin called `%s' found", argv[0]);
      return SOX_EOF;
    } else
      argc--; argv++;
  }

  /* Scan the ports to check there's one input and one output */
  l_st->control = lsx_calloc(l_st->desc->PortCount, sizeof(LADSPA_Data));
  for (i = 0; i < l_st->desc->PortCount; i++) {
    const LADSPA_PortDescriptor port = l_st->desc->PortDescriptors[i];

    /* Check port is well specified. All control ports should be
       inputs, but don't bother checking, as we never rely on this. */
    if (LADSPA_IS_PORT_INPUT(port) && LADSPA_IS_PORT_OUTPUT(port)) {
      lsx_fail("port %lu is both input and output", i);
      return SOX_EOF;
    } else if (LADSPA_IS_PORT_CONTROL(port) && LADSPA_IS_PORT_AUDIO(port)) {
      lsx_fail("port %lu is both audio and control", i);
      return SOX_EOF;
    }

    if (LADSPA_IS_PORT_AUDIO(port)) {
      if (LADSPA_IS_PORT_INPUT(port)) {
        if (l_st->input_port != ULONG_MAX) {
          lsx_fail("can't use a plugin with more than one audio input port");
          return SOX_EOF;
        }
        l_st->input_port = i;
      } else if (LADSPA_IS_PORT_OUTPUT(port)) {
        if (l_st->output_port != ULONG_MAX) {
          lsx_fail("can't use a plugin with more than one audio output port");
          return SOX_EOF;
        }
        l_st->output_port = i;
      }
    } else {                    /* Control port */
      if (argc == 0) {
        if (!LADSPA_IS_HINT_HAS_DEFAULT(l_st->desc->PortRangeHints[i].HintDescriptor)) {
          lsx_fail("not enough arguments for control ports");
          return SOX_EOF;
        }
        l_st->control[i] = ladspa_default(&(l_st->desc->PortRangeHints[i]));
        lsx_debug("default argument for port %lu is %f", i, l_st->control[i]);
      } else {
        if (!sscanf(argv[0], "%lf", &arg))
          return lsx_usage(effp);
        l_st->control[i] = (LADSPA_Data)arg;
        lsx_debug("argument for port %lu is %f", i, l_st->control[i]);
        argc--; argv++;
      }
    }
  }

  /* Stop if we have any unused arguments */
  return argc? lsx_usage(effp) : SOX_SUCCESS;
}

/*
 * Prepare processing.
 */
static int sox_ladspa_start(sox_effect_t * effp)
{
  priv_t * l_st = (priv_t *)effp->priv;
  unsigned long i;

  /* Instantiate the plugin */
  lsx_debug("rate for plugin is %g", effp->in_signal.rate);
  l_st->handle = l_st->desc->instantiate(l_st->desc, (unsigned long)effp->in_signal.rate);
  if (l_st->handle == NULL) {
    lsx_fail("could not instantiate plugin");
    return SOX_EOF;
  }

  for (i = 0; i < l_st->desc->PortCount; i++) {
    const LADSPA_PortDescriptor port = l_st->desc->PortDescriptors[i];

    if (LADSPA_IS_PORT_CONTROL(port))
      l_st->desc->connect_port(l_st->handle, i, &(l_st->control[i]));
  }

  /* If needed, activate the plugin */
  if (l_st->desc->activate)
    l_st->desc->activate(l_st->handle);

  return SOX_SUCCESS;
}

/*
 * Process one bufferful of data.
 */
static int sox_ladspa_flow(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf,
                           size_t *isamp, size_t *osamp)
{
  priv_t * l_st = (priv_t *)effp->priv;
  size_t i, len = min(*isamp, *osamp);

  *osamp = *isamp = len;

  if (len) {
    LADSPA_Data *buf = lsx_malloc(sizeof(LADSPA_Data) * len);
    SOX_SAMPLE_LOCALS;

    /* Insert input if effect takes it */
    if (l_st->input_port != ULONG_MAX) {
      /* Copy the input; FIXME: Assume LADSPA_Data == float! */
      for (i = 0; i < len; i++)
        buf[i] = SOX_SAMPLE_TO_FLOAT_32BIT(ibuf[i], effp->clips);

      /* Connect the input port */
      l_st->desc->connect_port(l_st->handle, l_st->input_port, buf);
    }

    /* Connect the output port if used */
    if (l_st->output_port != ULONG_MAX)
      l_st->desc->connect_port(l_st->handle, l_st->output_port, buf);

    /* Run the plugin */
    l_st->desc->run(l_st->handle, len);

    /* Grab output if effect produces it */
    if (l_st->output_port != ULONG_MAX)
      /* FIXME: Assume LADSPA_Data == float! */
      for (i = 0; i < len; i++) {
        obuf[i] = SOX_FLOAT_32BIT_TO_SAMPLE(buf[i], effp->clips);
      }

    free(buf);
  }

  return SOX_SUCCESS;
}

/*
 * Nothing to do.
 */
static int sox_ladspa_drain(sox_effect_t * effp UNUSED, sox_sample_t *obuf UNUSED, size_t *osamp)
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
  priv_t * l_st = (priv_t *)effp->priv;

  /* If needed, deactivate the plugin */
  if (l_st->desc->deactivate)
    l_st->desc->deactivate(l_st->handle);
  /* If needed, cleanup memory used by the plugin */
  if (l_st->desc->cleanup)
    l_st->desc->cleanup(l_st->handle);

  return SOX_SUCCESS;
}

static int sox_ladspa_kill(sox_effect_t * effp)
{
  priv_t * l_st = (priv_t *)effp->priv;

  free(l_st->control);

  return SOX_SUCCESS;
}

static sox_effect_handler_t sox_ladspa_effect = {
  "ladspa",
  "MODULE [PLUGIN] [ARGUMENT...]",
  SOX_EFF_GAIN,
  sox_ladspa_getopts,
  sox_ladspa_start,
  sox_ladspa_flow,
  sox_ladspa_drain,
  sox_ladspa_stop,
  sox_ladspa_kill,
  sizeof(priv_t)
};

const sox_effect_handler_t *lsx_ladspa_effect_fn(void)
{
  return &sox_ladspa_effect;
}

#endif /* HAVE_LADSPA */
