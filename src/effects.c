/* SoX Effects chain     (c) 2007 robs@users.sourceforge.net
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
#include <assert.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
  #include <strings.h>
#endif

#define DEBUG_EFFECTS_CHAIN 0

/* Default effect handler functions for do-nothing situations: */

static int default_function(sox_effect_t * effp UNUSED)
{
  return SOX_SUCCESS;
}

/* Pass through samples verbatim */
int lsx_flow_copy(sox_effect_t * effp UNUSED, const sox_sample_t * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  *isamp = *osamp = min(*isamp, *osamp);
  memcpy(obuf, ibuf, *isamp * sizeof(*obuf));
  return SOX_SUCCESS;
}

/* Inform no more samples to drain */
static int default_drain(sox_effect_t * effp UNUSED, sox_sample_t *obuf UNUSED, size_t *osamp)
{
  *osamp = 0;
  return SOX_EOF;
}

/* Check that no parameters have been given */
static int default_getopts(sox_effect_t * effp, int argc, char **argv UNUSED)
{
  return --argc? lsx_usage(effp) : SOX_SUCCESS;
}

/* Partially initialise the effect structure; signal info will come later */
sox_effect_t * sox_create_effect(sox_effect_handler_t const * eh)
{
  sox_effect_t * effp = lsx_calloc(1, sizeof(*effp));
  effp->obuf = NULL;

  effp->global_info = sox_get_effects_globals();
  effp->handler = *eh;
  if (!effp->handler.getopts) effp->handler.getopts = default_getopts;
  if (!effp->handler.start  ) effp->handler.start   = default_function;
  if (!effp->handler.flow   ) effp->handler.flow    = lsx_flow_copy;
  if (!effp->handler.drain  ) effp->handler.drain   = default_drain;
  if (!effp->handler.stop   ) effp->handler.stop    = default_function;
  if (!effp->handler.kill   ) effp->handler.kill    = default_function;

  effp->priv = lsx_calloc(1, effp->handler.priv_size);

  return effp;
} /* sox_create_effect */

int sox_effect_options(sox_effect_t *effp, int argc, char * const argv[])
{
  int result;

  char * * argv2 = lsx_malloc((argc + 1) * sizeof(*argv2));
  argv2[0] = (char *)effp->handler.name;
  memcpy(argv2 + 1, argv, argc * sizeof(*argv2));
  result = effp->handler.getopts(effp, argc + 1, argv2);
  free(argv2);
  return result;
} /* sox_effect_options */

/* Effects chain: */

sox_effects_chain_t * sox_create_effects_chain(
    sox_encodinginfo_t const * in_enc, sox_encodinginfo_t const * out_enc)
{
  sox_effects_chain_t * result = lsx_calloc(1, sizeof(sox_effects_chain_t));
  result->global_info = *sox_get_effects_globals();
  result->in_enc = in_enc;
  result->out_enc = out_enc;
  return result;
} /* sox_create_effects_chain */

void sox_delete_effects_chain(sox_effects_chain_t *ecp)
{
    if (ecp && ecp->length)
        sox_delete_effects(ecp);
    free(ecp->effects);
    free(ecp);
} /* sox_delete_effects_chain */

/* Effect can call in start() or flow() to set minimum input size to flow() */
int lsx_effect_set_imin(sox_effect_t * effp, size_t imin)
{
  if (imin > sox_globals.bufsiz / effp->flows) {
    lsx_fail("sox_bufsiz not big enough");
    return SOX_EOF;
  }

  effp->imin = imin;
  return SOX_SUCCESS;
}

/* Effects table to be extended in steps of EFF_TABLE_STEP */
#define EFF_TABLE_STEP 8

/* Add an effect to the chain. *in is the input signal for this effect. *out is
 * a suggestion as to what the output signal should be, but depending on its
 * given options and *in, the effect can choose to do differently.  Whatever
 * output rate and channels the effect does produce are written back to *in,
 * ready for the next effect in the chain.
 */
int sox_add_effect(sox_effects_chain_t * chain, sox_effect_t * effp, sox_signalinfo_t * in, sox_signalinfo_t const * out)
{
  int ret, (*start)(sox_effect_t * effp) = effp->handler.start;
  unsigned f;
  sox_effect_t eff0;  /* Copy of effect for flow 0 before calling start */

  effp->global_info = &chain->global_info;
  effp->in_signal = *in;
  effp->out_signal = *out;
  effp->in_encoding = chain->in_enc;
  effp->out_encoding = chain->out_enc;
  if (!(effp->handler.flags & SOX_EFF_CHAN))
    effp->out_signal.channels = in->channels;
  if (!(effp->handler.flags & SOX_EFF_RATE))
    effp->out_signal.rate = in->rate;
  if (!(effp->handler.flags & SOX_EFF_PREC))
    effp->out_signal.precision = (effp->handler.flags & SOX_EFF_MODIFY)?
        in->precision : SOX_SAMPLE_PRECISION;
  if (!(effp->handler.flags & SOX_EFF_GAIN))
    effp->out_signal.mult = in->mult;

  effp->flows =
    (effp->handler.flags & SOX_EFF_MCHAN)? 1 : effp->in_signal.channels;
  effp->clips = 0;
  effp->imin = 0;
  eff0 = *effp, eff0.priv = lsx_memdup(eff0.priv, eff0.handler.priv_size);
  eff0.in_signal.mult = NULL; /* Only used in channel 0 */
  ret = start(effp);
  if (ret == SOX_EFF_NULL) {
    lsx_report("has no effect in this configuration");
    free(eff0.priv);
    free(effp->priv);
    effp->priv = NULL;
    return SOX_SUCCESS;
  }
  if (ret != SOX_SUCCESS) {
    free(eff0.priv);
    return SOX_EOF;
  }
  if (in->mult)
    lsx_debug("mult=%g", *in->mult);

  if (!(effp->handler.flags & SOX_EFF_LENGTH)) {
    effp->out_signal.length = in->length;
    if (effp->out_signal.length != SOX_UNKNOWN_LEN) {
      if (effp->handler.flags & SOX_EFF_CHAN)
        effp->out_signal.length =
          effp->out_signal.length / in->channels * effp->out_signal.channels;
      if (effp->handler.flags & SOX_EFF_RATE)
        effp->out_signal.length =
          effp->out_signal.length / in->rate * effp->out_signal.rate + .5;
    }
  }

  *in = effp->out_signal;

  if (chain->length == chain->table_size) {
    chain->table_size += EFF_TABLE_STEP;
    lsx_debug_more("sox_add_effect: extending effects table, "
      "new size = %lu", (unsigned long)chain->table_size);
    lsx_revalloc(chain->effects, chain->table_size);
  }

  chain->effects[chain->length] =
    lsx_calloc(effp->flows, sizeof(chain->effects[chain->length][0]));
  chain->effects[chain->length][0] = *effp;

  for (f = 1; f < effp->flows; ++f) {
    chain->effects[chain->length][f] = eff0;
    chain->effects[chain->length][f].flow = f;
    chain->effects[chain->length][f].priv = lsx_memdup(eff0.priv, eff0.handler.priv_size);
    if (start(&chain->effects[chain->length][f]) != SOX_SUCCESS) {
      free(eff0.priv);
      return SOX_EOF;
    }
  }

  ++chain->length;
  free(eff0.priv);
  return SOX_SUCCESS;
}

static int flow_effect(sox_effects_chain_t * chain, size_t n)
{
  sox_effect_t * effp1 = &chain->effects[n - 1][0];
  sox_effect_t * effp = &chain->effects[n][0];
  int effstatus = SOX_SUCCESS, f = 0;
  size_t i;
  const sox_sample_t *ibuf;
  size_t idone = effp1->oend - effp1->obeg;
  size_t obeg = sox_globals.bufsiz - effp->oend;
#if DEBUG_EFFECTS_CHAIN
  size_t pre_idone = idone;
  size_t pre_odone = obeg;
#endif

  if (effp->flows == 1) {     /* Run effect on all channels at once */
    idone -= idone % effp->in_signal.channels;
    effstatus = effp->handler.flow(effp, &effp1->obuf[effp1->obeg],
                                   &effp->obuf[effp->oend], &idone, &obeg);
    if (obeg % effp->out_signal.channels != 0) {
      lsx_fail("multi-channel effect flowed asymmetrically!");
      effstatus = SOX_EOF;
    }
  } else {               /* Run effect on each channel individually */
    sox_sample_t *obuf = &effp->obuf[effp->oend];
    size_t idone_last = 0, odone_last = 0; /* Initialised to prevent warning */

    ibuf = &effp1->obuf[effp1->obeg];
    for (i = 0; i < idone; i += effp->flows)
      for (f = 0; f < (int)effp->flows; ++f)
        chain->ibufc[f][i / effp->flows] = *ibuf++;

#ifdef HAVE_OPENMP
    if (sox_globals.use_threads && effp->flows > 1)
    {
      #pragma omp parallel for
      for (f = 0; f < (int)effp->flows; ++f) {
        size_t idonec = idone / effp->flows;
        size_t odonec = obeg / effp->flows;
        int eff_status_c = effp->handler.flow(&chain->effects[n][f],
            chain->ibufc[f], chain->obufc[f], &idonec, &odonec);
        if (!f) {
          idone_last = idonec;
          odone_last = odonec;
        }

        if (eff_status_c != SOX_SUCCESS)
          effstatus = SOX_EOF;
      }
    }
    else /* sox_globals.use_threads */
#endif
    {
      for (f = 0; f < (int)effp->flows; ++f) {
        size_t idonec = idone / effp->flows;
        size_t odonec = obeg / effp->flows;
        int eff_status_c = effp->handler.flow(&chain->effects[n][f],
            chain->ibufc[f], chain->obufc[f], &idonec, &odonec);
        if (f && (idonec != idone_last || odonec != odone_last)) {
          lsx_fail("flowed asymmetrically!");
          effstatus = SOX_EOF;
        }
        idone_last = idonec;
        odone_last = odonec;

        if (eff_status_c != SOX_SUCCESS)
          effstatus = SOX_EOF;
      }
    }

    for (i = 0; i < odone_last; ++i)
      for (f = 0; f < (int)effp->flows; ++f)
        *obuf++ = chain->obufc[f][i];

    idone = effp->flows * idone_last;
    obeg = effp->flows * odone_last;
  }
#if DEBUG_EFFECTS_CHAIN
  lsx_report("flow:  %5" PRIuPTR " %5" PRIuPTR " %5" PRIuPTR " %5" PRIuPTR,
      pre_idone, pre_odone, idone, obeg);
#endif
  effp1->obeg += idone;
  if (effp1->obeg == effp1->oend)
    effp1->obeg = effp1->oend = 0;
  else if (effp1->oend - effp1->obeg < effp->imin ) { /* Need to refill? */
    memmove(effp1->obuf, &effp1->obuf[effp1->obeg], (effp1->oend - effp1->obeg) * sizeof(*effp1->obuf));
    effp1->oend -= effp1->obeg;
    effp1->obeg = 0;
  }

  effp->oend += obeg;

  return effstatus == SOX_SUCCESS? SOX_SUCCESS : SOX_EOF;
}

/* The same as flow_effect but with no input */
static int drain_effect(sox_effects_chain_t * chain, size_t n)
{
  sox_effect_t * effp = &chain->effects[n][0];
  int effstatus = SOX_SUCCESS;
  size_t i, f;
  size_t obeg = sox_globals.bufsiz - effp->oend;
#if DEBUG_EFFECTS_CHAIN
  size_t pre_odone = obeg;
#endif

  if (effp->flows == 1) { /* Run effect on all channels at once */
    effstatus = effp->handler.drain(effp, &effp->obuf[effp->oend], &obeg);
    if (obeg % effp->out_signal.channels != 0) {
      lsx_fail("multi-channel effect drained asymmetrically!");
      effstatus = SOX_EOF;
    }
  } else {                       /* Run effect on each channel individually */
    sox_sample_t *obuf = &effp->obuf[effp->oend];
    size_t odone_last = 0; /* Initialised to prevent warning */

    for (f = 0; f < effp->flows; ++f) {
      size_t odonec = obeg / effp->flows;
      int eff_status_c = effp->handler.drain(&chain->effects[n][f], chain->obufc[f], &odonec);
      if (f && (odonec != odone_last)) {
        lsx_fail("drained asymmetrically!");
        effstatus = SOX_EOF;
      }
      odone_last = odonec;

      if (eff_status_c != SOX_SUCCESS)
        effstatus = SOX_EOF;
    }

    for (i = 0; i < odone_last; ++i)
      for (f = 0; f < effp->flows; ++f)
        *obuf++ = chain->obufc[f][i];
    obeg = f * odone_last;
  }
#if DEBUG_EFFECTS_CHAIN
  lsx_report("drain: %5" PRIuPTR " %5" PRIuPTR " %5" PRIuPTR " %5" PRIuPTR,
      (size_t)0, pre_odone, (size_t)0, obeg);
#endif
  if (!obeg)   /* This is the only thing that drain has and flow hasn't */
    effstatus = SOX_EOF;

  effp->oend += obeg;

  return effstatus == SOX_SUCCESS? SOX_SUCCESS : SOX_EOF;
}

/* Flow data through the effects chain until an effect or callback gives EOF */
int sox_flow_effects(sox_effects_chain_t * chain, int (* callback)(sox_bool all_done, void * client_data), void * client_data)
{
  int flow_status = SOX_SUCCESS;
  size_t e, source_e = 0;               /* effect indices */
  size_t f, max_flows = 0;
  sox_bool draining = sox_true;

  for (e = 0; e < chain->length; ++e) {
    chain->effects[e][0].obuf = lsx_realloc(chain->effects[e][0].obuf,
        sox_globals.bufsiz * sizeof(chain->effects[e][0].obuf[0]));
      /* Possibly there is already a buffer, if this is a used effect;
         it may still contain samples in that case. */
      /* Memory will be freed by sox_delete_effect() later. */
    max_flows = max(max_flows, chain->effects[e][0].flows);
  }
  if (max_flows == 1) /* don't need interleave buffers */
    max_flows = 0;
  chain->ibufc = lsx_calloc(max_flows, sizeof(*chain->ibufc));
  chain->obufc = lsx_calloc(max_flows, sizeof(*chain->obufc));
  for (f = 0; f < max_flows; ++f) {
    chain->ibufc[f] = lsx_calloc(sox_globals.bufsiz / 2, sizeof(chain->ibufc[f][0]));
    chain->obufc[f] = lsx_calloc(sox_globals.bufsiz / 2, sizeof(chain->obufc[f][0]));
  }

  e = chain->length - 1;
  while (source_e < chain->length) {
#define have_imin (e > 0 && e < chain->length && chain->effects[e - 1][0].oend - chain->effects[e - 1][0].obeg >= chain->effects[e][0].imin)
    size_t osize = chain->effects[e][0].oend - chain->effects[e][0].obeg;
    if (e == source_e && (draining || !have_imin)) {
      if (drain_effect(chain, e) == SOX_EOF) {
        ++source_e;
        draining = sox_false;
      }
    } else if (have_imin && flow_effect(chain, e) == SOX_EOF) {
      flow_status = SOX_EOF;
      if (e == chain->length - 1)
        break;
      source_e = e;
      draining = sox_true;
    }
    if (e < chain->length && chain->effects[e][0].oend - chain->effects[e][0].obeg > osize) /* False for output */
      ++e;
    else if (e == source_e)
      draining = sox_true;
    else if ((int)--e < (int)source_e)
      e = source_e;

    if (callback && callback(source_e == chain->length, client_data) != SOX_SUCCESS) {
      flow_status = SOX_EOF; /* Client has requested to stop the flow. */
      break;
    }
  }

  for (f = 0; f < max_flows; ++f) {
    free(chain->ibufc[f]);
    free(chain->obufc[f]);
  }
  free(chain->obufc);
  free(chain->ibufc);

  return flow_status;
}

sox_uint64_t sox_effects_clips(sox_effects_chain_t * chain)
{
  unsigned i, f;
  uint64_t clips = 0;
  for (i = 1; i < chain->length - 1; ++i)
    for (f = 0; f < chain->effects[i][0].flows; ++f)
      clips += chain->effects[i][f].clips;
  return clips;
}

sox_uint64_t sox_stop_effect(sox_effect_t *effp)
{
  unsigned f;
  uint64_t clips = 0;

  for (f = 0; f < effp->flows; ++f) {
    effp[f].handler.stop(&effp[f]);
    clips += effp[f].clips;
  }
  return clips;
}

void sox_push_effect_last(sox_effects_chain_t *chain, sox_effect_t *effp)
{
  if (chain->length == chain->table_size) {
    chain->table_size += EFF_TABLE_STEP;
    lsx_debug_more("sox_push_effect_last: extending effects table, "
        "new size = %lu", (unsigned long)chain->table_size);
    lsx_revalloc(chain->effects, chain->table_size);
  }

  chain->effects[chain->length++] = effp;
} /* sox_push_effect_last */

sox_effect_t *sox_pop_effect_last(sox_effects_chain_t *chain)
{
  if (chain->length > 0)
  {
    sox_effect_t *effp;
    chain->length--;
    effp = chain->effects[chain->length];
    chain->effects[chain->length] = NULL;
    return effp;
  }
  else
    return NULL;
} /* sox_pop_effect_last */

/* Free resources related to effect.
 * Note: This currently closes down the effect which might
 * not be obvious from name.
 */
void sox_delete_effect(sox_effect_t *effp)
{
  uint64_t clips;
  unsigned f;

  if ((clips = sox_stop_effect(effp)) != 0)
    lsx_warn("%s clipped %" PRIu64 " samples; decrease volume?",
        effp->handler.name, clips);
  if (effp->obeg != effp->oend)
    lsx_debug("output buffer still held %" PRIuPTR " samples; dropped.",
        (effp->oend - effp->obeg)/effp->out_signal.channels);
      /* May or may not indicate a problem; it is normal if the user aborted
         processing, or if an effect like "trim" stopped early. */
  effp->handler.kill(effp); /* N.B. only one kill; not one per flow */
  for (f = 0; f < effp->flows; ++f)
    free(effp[f].priv);
  free(effp->obuf);
  free(effp);
}

void sox_delete_effect_last(sox_effects_chain_t *chain)
{
  if (chain->length > 0)
  {
    chain->length--;
    sox_delete_effect(chain->effects[chain->length]);
    chain->effects[chain->length] = NULL;
  }
} /* sox_delete_effect_last */

/* Remove all effects from the chain.
 * Note: This currently closes down the effect which might
 * not be obvious from name.
 */
void sox_delete_effects(sox_effects_chain_t * chain)
{
  size_t e;

  for (e = 0; e < chain->length; ++e) {
    sox_delete_effect(chain->effects[e]);
    chain->effects[e] = NULL;
  }
  chain->length = 0;
}

/*----------------------------- Effects library ------------------------------*/

static sox_effect_fn_t s_sox_effect_fns[] = {
#define EFFECT(f) lsx_##f##_effect_fn,
#include "effects.h"
#undef EFFECT
  NULL
};

const sox_effect_fn_t*
sox_get_effect_fns(void)
{
    return s_sox_effect_fns;
}

/* Find a named effect in the effects library */
sox_effect_handler_t const * sox_find_effect(char const * name)
{
  int e;
  sox_effect_fn_t const * fns = sox_get_effect_fns();
  for (e = 0; fns[e]; ++e) {
    const sox_effect_handler_t *eh = fns[e] ();
    if (eh && eh->name && strcasecmp(eh->name, name) == 0)
      return eh;                 /* Found it. */
  }
  return NULL;
}
