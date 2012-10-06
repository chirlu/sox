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
#include "ladspa.h"

/*
 * Assuming LADSPA_Data == float.  This is the case in 2012 and has been
 * the case for many years now.
 */
#define SOX_SAMPLE_TO_LADSPA_DATA(d,clips) \
        SOX_SAMPLE_TO_FLOAT_32BIT((d),(clips))
#define LADSPA_DATA_TO_SOX_SAMPLE(d,clips) \
        SOX_FLOAT_32BIT_TO_SAMPLE((d),(clips))

static sox_effect_handler_t sox_ladspa_effect;

/* Private data for resampling */
typedef struct {
  char *name;                   /* plugin name */
  lt_dlhandle lth;              /* dynamic object handle */
  sox_bool clone;
  const LADSPA_Descriptor *desc; /* plugin descriptor */
  LADSPA_Handle *handles;        /* instantiated plugin handles */
  size_t handle_count;
  LADSPA_Data *control;         /* control ports */
  unsigned long *inputs;
  size_t input_count;
  unsigned long *outputs;
  size_t output_count;
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

  if (argc >= 1 && strcmp(argv[0], "-r") == 0) {
    l_st->clone = sox_true;
    argc--; argv++;
  }

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

  /* Scan the ports for inputs and outputs */
  l_st->control = lsx_calloc(l_st->desc->PortCount, sizeof(LADSPA_Data));
  l_st->inputs = lsx_malloc(l_st->desc->PortCount * sizeof(unsigned long));
  l_st->outputs = lsx_malloc(l_st->desc->PortCount * sizeof(unsigned long));

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
        l_st->inputs[l_st->input_count++] = i;
      } else if (LADSPA_IS_PORT_OUTPUT(port)) {
        l_st->outputs[l_st->output_count++] = i;
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
  size_t h;
  unsigned long rate = (unsigned long)effp->in_signal.rate;

  /* Instantiate the plugin */
  lsx_debug("rate for plugin is %g", effp->in_signal.rate);

  if (l_st->input_count == 1 && l_st->output_count == 1 &&
      effp->in_signal.channels == effp->out_signal.channels) {
    /* for mono plugins, they are common */

    if (!l_st->clone && effp->in_signal.channels > 1) {
      lsx_fail("expected 1 input channel(s), found %u; consider using -r",
               effp->in_signal.channels);
      return SOX_EOF;
    }

    /*
     * create one handle per channel for mono plugins. ecasound does this, too.
     * mono LADSPA plugins are common and SoX supported mono LADSPA plugins
     * exclusively for a while.
     */
    l_st->handles = lsx_malloc(effp->in_signal.channels *
                               sizeof(LADSPA_Handle *));

    while (l_st->handle_count < effp->in_signal.channels)
      l_st->handles[l_st->handle_count++] = l_st->desc->instantiate(l_st->desc, rate);

  } else {
    /*
     * assume the plugin is multi-channel capable with one instance,
     * Some LADSPA plugins are stereo (e.g. bs2b-ladspa)
     */

    if (l_st->input_count < effp->in_signal.channels) {
      lsx_fail("fewer plugin input ports than input channels (%u < %u)",
               (unsigned)l_st->input_count, effp->in_signal.channels);
      return SOX_EOF;
    }

    /* warn if LADSPA audio ports are unused.  ecasound does this, too */
    if (l_st->input_count > effp->in_signal.channels)
      lsx_warn("more plugin input ports than input channels (%u > %u)",
               (unsigned)l_st->input_count, effp->in_signal.channels);

    /*
     * some LADSPA plugins increase/decrease the channel count
     * (e.g. "mixer" in cmt or vocoder):
     */
    if (l_st->output_count != effp->out_signal.channels) {
      lsx_debug("changing output channels to match plugin output ports (%u => %u)",
               effp->out_signal.channels, (unsigned)l_st->output_count);
      effp->out_signal.channels = l_st->output_count;
    }

    l_st->handle_count = 1;
    l_st->handles = lsx_malloc(sizeof(LADSPA_Handle *));
    l_st->handles[0] = l_st->desc->instantiate(l_st->desc, rate);
  }

  /* abandon everything completely on any failed handle instantiation */
  for (h = 0; h < l_st->handle_count; h++) {
    if (l_st->handles[h] == NULL) {
      /* cleanup the handles that did instantiate successfully */
      for (h = 0; l_st->desc->cleanup && h < l_st->handle_count; h++) {
        if (l_st->handles[h])
          l_st->desc->cleanup(l_st->handles[h]);
      }

      free(l_st->handles);
      l_st->handle_count = 0;
      lsx_fail("could not instantiate plugin");
      return SOX_EOF;
    }
  }

  for (i = 0; i < l_st->desc->PortCount; i++) {
    const LADSPA_PortDescriptor port = l_st->desc->PortDescriptors[i];

    if (LADSPA_IS_PORT_CONTROL(port)) {
      for (h = 0; h < l_st->handle_count; h++)
        l_st->desc->connect_port(l_st->handles[h], i, &(l_st->control[i]));
    }
  }

  /* If needed, activate the plugin instances */
  if (l_st->desc->activate) {
    for (h = 0; h < l_st->handle_count; h++)
      l_st->desc->activate(l_st->handles[h]);
  }

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
  size_t j;
  size_t h;
  const size_t total_input_count = l_st->input_count * l_st->handle_count;
  const size_t total_output_count = l_st->output_count * l_st->handle_count;
  const size_t input_len = len / total_input_count;
  size_t output_len = len / total_output_count;

  if (total_output_count < total_input_count)
    output_len = input_len;

  *isamp = len;
  *osamp = 0;

  if (len) {
    LADSPA_Data *buf = lsx_calloc(len, sizeof(LADSPA_Data));
    LADSPA_Data *outbuf = lsx_calloc(len, sizeof(LADSPA_Data));
    LADSPA_Handle handle;
    unsigned long port;
    SOX_SAMPLE_LOCALS;

    /*
     * prepare buffer for LADSPA input
     * deinterleave sox samples and write non-interleaved data to
     * input_port-specific buffer locations
     */
    for (i = 0; i < input_len; i++) {
      for (j = 0; j < total_input_count; j++) {
        const sox_sample_t s = *ibuf++;
        buf[j * input_len + i] = SOX_SAMPLE_TO_LADSPA_DATA(s, effp->clips);
      }
    }

    /* Connect the LADSPA input port(s) to the prepared buffers */
    for (j = 0; j < total_input_count; j++) {
      handle = l_st->handles[j / l_st->input_count];
      port = l_st->inputs[j / l_st->handle_count];
      l_st->desc->connect_port(handle, port, buf + j * input_len);
    }

    /* Connect the LADSPA output port(s) if used */
    for (j = 0; j < total_output_count; j++) {
      handle = l_st->handles[j / l_st->output_count];
      port = l_st->outputs[j / l_st->handle_count];
      l_st->desc->connect_port(handle, port, outbuf + j * output_len);
    }

    /* Run the plugin for each handle */
    for (h = 0; h < l_st->handle_count; h++)
      l_st->desc->run(l_st->handles[h], input_len);

    /* Grab output if effect produces it, re-interleaving it */
    for (i = 0; i < output_len; i++) {
      for (j = 0; j < total_output_count; j++) {
        LADSPA_Data d = outbuf[j * output_len + i];
        *obuf++ = LADSPA_DATA_TO_SOX_SAMPLE(d, effp->clips);
        (*osamp)++;
      }
    }

    free(outbuf);
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
  size_t h;

  for (h = 0; h < l_st->handle_count; h++) {
    /* If needed, deactivate and cleanup the plugin */
    if (l_st->desc->deactivate)
      l_st->desc->deactivate(l_st->handles[h]);
    if (l_st->desc->cleanup)
      l_st->desc->cleanup(l_st->handles[h]);
  }
  free(l_st->handles);
  l_st->handle_count = 0;

  return SOX_SUCCESS;
}

static int sox_ladspa_kill(sox_effect_t * effp)
{
  priv_t * l_st = (priv_t *)effp->priv;

  free(l_st->control);
  free(l_st->inputs);
  free(l_st->outputs);

  return SOX_SUCCESS;
}

static sox_effect_handler_t sox_ladspa_effect = {
  "ladspa",
  "MODULE [PLUGIN] [ARGUMENT...]",
  SOX_EFF_MCHAN | SOX_EFF_CHAN | SOX_EFF_GAIN,
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
