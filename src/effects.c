/*
 * SoX Effects chain
 * (c) 2007 robs@users.sourceforge.net
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, write to the Free Software Foundation,
 * Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301, USA.
 */

#include "sox_i.h"
#include <assert.h>
#include <string.h>
#include <strings.h>

#undef sox_fail
#undef sox_warn
#undef sox_report
#define sox_fail sox_message_filename=effp->handler.name,sox_fail
#define sox_warn sox_message_filename=effp->handler.name,sox_warn
#define sox_report sox_message_filename=effp->handler.name,sox_report

/* dummy effect routine for do-nothing functions */
static int effect_nothing(sox_effect_t * effp UNUSED)
{
  return SOX_SUCCESS;
}

static int effect_nothing_flow(sox_effect_t * effp UNUSED, const sox_ssample_t *ibuf UNUSED, sox_ssample_t *obuf UNUSED, sox_size_t *isamp, sox_size_t *osamp)
{
  /* Pass through samples verbatim */
  *isamp = *osamp = min(*isamp, *osamp);
  memcpy(obuf, ibuf, *isamp * sizeof(*obuf));
  return SOX_SUCCESS;
}

static int effect_nothing_drain(sox_effect_t * effp UNUSED, sox_ssample_t *obuf UNUSED, sox_size_t *osamp)
{
  /* Inform no more samples to drain */
  *osamp = 0;
  return SOX_EOF;
}

static int effect_nothing_getopts(sox_effect_t * effp, int argc, char **argv UNUSED)
{
  if (argc) {
    sox_fail(effp->handler.usage);
    return SOX_EOF;
  }
  return SOX_SUCCESS;
}


/* Effect chain routines */

sox_effect_handler_t const * sox_find_effect(char const * name)
{
  int e;

  for (e = 0; sox_effect_fns[e]; ++e) {
    const sox_effect_handler_t *effp = sox_effect_fns[e] ();
    if (effp && effp->name && strcasecmp(effp->name, name) == 0)
      return effp;                 /* Found it. */
  }
  return NULL;
}

void sox_create_effect(sox_effect_t * effp, sox_effect_handler_t const * eh)
{
  assert(eh);
  memset(effp, 0, sizeof(*effp));
  effp->global_info = &effects_global_info;
  effp->handler = *eh;
  if (!effp->handler.getopts) effp->handler.getopts = effect_nothing_getopts;
  if (!effp->handler.start  ) effp->handler.start   = effect_nothing;
  if (!effp->handler.flow   ) effp->handler.flow    = effect_nothing_flow;
  if (!effp->handler.drain  ) effp->handler.drain   = effect_nothing_drain;
  if (!effp->handler.stop   ) effp->handler.stop    = effect_nothing;
  if (!effp->handler.kill   ) effp->handler.kill    = effect_nothing;
}

/*
 * Copy input and output signal info into effect structures.
 * Must pass in a bitmask containing info on whether SOX_EFF_CHAN
 * or SOX_EFF_RATE has been used previously on this effect stream.
 * If not running multiple effects then just pass in a value of 0.
 *
 * Return value is the same mask plus addition of SOX_EFF_CHAN or
 * SOX_EFF_RATE if it was used in this effect.  That make this
 * return value can be passed back into this function in future
 * calls.
 */

int sox_update_effect(sox_effect_t * effp, const sox_signalinfo_t * in,
                      const sox_signalinfo_t * out, int effect_mask)
{
  effp->ininfo = *in;
  effp->outinfo = *out;

  if (in->channels != out->channels) {
    /* Only effects with SOX_EFF_CHAN flag can actually handle
     * outputing a different number of channels then the input.
     */
    if (!(effp->handler.flags & SOX_EFF_CHAN)) {
      /* If this effect is being run before a SOX_EFF_CHAN effect
       * then its output is the same as the input file; otherwise,
       * its input contains the same number of channels as the
       * output file. */
      if (effect_mask & SOX_EFF_CHAN)
        effp->ininfo.channels = out->channels;
      else
        effp->outinfo.channels = in->channels;
    }
  }

  if (in->rate != out->rate) {
    /* Only SOX_EFF_RATE effects can handle an input that
     * has a different sample rate from the output. */
    if (!(effp->handler.flags & SOX_EFF_RATE)) {
      if (effect_mask & SOX_EFF_RATE)
        effp->ininfo.rate = out->rate;
      else
        effp->outinfo.rate = in->rate;
    }
  }

  if (effp->handler.flags & SOX_EFF_CHAN)
    effect_mask |= SOX_EFF_CHAN;
  if (effp->handler.flags & SOX_EFF_RATE)
    effect_mask |= SOX_EFF_RATE;

  return effect_mask;
}

sox_effect_t * sox_effects[SOX_MAX_EFFECTS];
unsigned sox_neffects;

int sox_effect_set_imin(sox_effect_t * effp, sox_size_t imin)
{
  if (imin > sox_bufsiz / effp->flows) {
    sox_fail("sox_bufsiz not big enough");
    return SOX_EOF;
  }

  effp->imin = imin;
  return SOX_SUCCESS;
}

int sox_add_effect(sox_effect_t * effp, sox_signalinfo_t * in, sox_signalinfo_t * out, int * effects_mask)
{
  unsigned f, flows;

  if (sox_neffects == SOX_MAX_EFFECTS)
    return SOX_EOF;

  *effects_mask = sox_update_effect(effp, in, out, *effects_mask);

  flows = (effp->handler.flags & SOX_EFF_MCHAN)? 1 : effp->ininfo.channels;

  sox_effects[sox_neffects] = xcalloc(flows, sizeof(sox_effects[sox_neffects][0]));
  sox_effects[sox_neffects][0] = *effp;
  sox_effects[sox_neffects][0].flows = flows;

  for (f = 1; f < flows; ++f)
    sox_effects[sox_neffects][f] = sox_effects[sox_neffects][0];

  ++sox_neffects;
  return SOX_SUCCESS;
}

static void stop_effect(unsigned e)
{
  sox_effect_t * effp = &sox_effects[e][0];
  unsigned f;

  sox_size_t clips = 0;

  for (f = 0; f < effp->flows; ++f) {
    effp->handler.stop(&sox_effects[e][f]);
    clips += sox_effects[e][f].clips;
  }
  if (clips != 0)
    sox_warn("clipped %u samples; decrease volume?", clips);
}

void sox_stop_effects(void)
{
  unsigned e;
  for (e = 0; e < sox_neffects; ++e)
    stop_effect(e);
}

int sox_start_effects(void)
{
  unsigned e, f, i;
  int ret = SOX_SUCCESS;

  for (e = 0; e < sox_neffects; ++e) {
    sox_effect_t * effp = &sox_effects[e][0];
    sox_bool is_always_null = (effp->handler.flags & SOX_EFF_NULL) != 0;
    int (*start)(sox_effect_t * effp) = effp->handler.start;

    if (is_always_null)
      sox_report("has no effect (is a proxy effect)");
    else {
      effp->clips = 0;
      effp->imin = 0;
      ret = start(effp);
      if (ret == SOX_EFF_NULL)
        sox_report("has no effect in this configuration");
      else if (ret != SOX_SUCCESS)
        return SOX_EOF;
    }
    if (is_always_null || ret == SOX_EFF_NULL) { /* remove from the chain */
      free(sox_effects[e]);
      --sox_neffects;
      for (i = e--; i < sox_neffects; ++i)
        sox_effects[i] = sox_effects[i + 1];
    }
    else for (f = 1; f < sox_effects[e][0].flows; ++f) {
      sox_effects[e][f].clips = 0;
      if (start(&sox_effects[e][f]) != SOX_SUCCESS)
        return SOX_EOF;
    }
  }
  for (e = 0; e < sox_neffects; ++e) {
    sox_effect_t * effp = &sox_effects[e][0];
    #undef sox_report
    #define sox_report     sox_message_filename="effects chain",sox_report
    sox_report("%-10s %uHz %u channels %s",
        effp->handler.name, effp->ininfo.rate, effp->ininfo.channels,
        (effp->handler.flags & SOX_EFF_MCHAN)? "(multi)" : "");
  }
  return SOX_SUCCESS;
}

static sox_ssample_t **ibufc, **obufc; /* Channel interleave buffers */

static int flow_effect(unsigned n)
{
  sox_effect_t * effp1 = &sox_effects[n - 1][0];
  sox_effect_t * effp = &sox_effects[n][0];
  int effstatus = SOX_SUCCESS;
  sox_size_t i, f;
  const sox_ssample_t *ibuf;
  sox_size_t idone = effp1->olen - effp1->odone;
  sox_size_t odone = sox_bufsiz - effp->olen;

  if (effp->flows == 1)   /* Run effect on all channels at once */
    effstatus = effp->handler.flow(effp,
        &effp1->obuf[effp1->odone], &effp->obuf[effp->olen], &idone, &odone);
  else {                         /* Run effect on each channel individually */
    sox_ssample_t *obuf = &effp->obuf[effp->olen];
    sox_size_t idone_last, odone_last;

    ibuf = &effp1->obuf[effp1->odone];
    for (i = 0; i < idone; i += effp->flows)
      for (f = 0; f < effp->flows; ++f)
        ibufc[f][i / effp->flows] = *ibuf++;

    for (f = 0; f < effp->flows; ++f) {
      sox_size_t idonec = idone / effp->flows;
      sox_size_t odonec = odone / effp->flows;
      int eff_status_c = effp->handler.flow(&sox_effects[n][f],
          ibufc[f], obufc[f], &idonec, &odonec);
      if (f && (idonec != idone_last || odonec != odone_last)) {
        sox_fail("flowed asymmetrically!");
        effstatus = SOX_EOF;
      }
      idone_last = idonec;
      odone_last = odonec;

      if (eff_status_c != SOX_SUCCESS)
        effstatus = SOX_EOF;
    }

    for (i = 0; i < odone_last; ++i)
      for (f = 0; f < effp->flows; ++f)
        *obuf++ = obufc[f][i];

    idone = f * idone_last;
    odone = f * odone_last;
  }
  effp1->odone += idone;
  if (effp1->odone == effp1->olen)
    effp1->odone = effp1->olen = 0;
  else if (effp1->olen - effp1->odone < effp->imin ) { /* Need to refill? */
    memmove(effp1->obuf, &effp1->obuf[effp1->odone], (effp1->olen - effp1->odone) * sizeof(*effp1->obuf));
    effp1->olen -= effp1->odone;
    effp1->odone = 0;
  }

  effp->olen += odone;

  return effstatus == SOX_SUCCESS? SOX_SUCCESS : SOX_EOF;
}

/* The same as flow_effect but with no input */
static int drain_effect(unsigned n)
{
  sox_effect_t * effp = &sox_effects[n][0];
  int effstatus = SOX_SUCCESS;
  sox_size_t i, f;
  sox_size_t odone = sox_bufsiz - effp->olen;

  if (effp->flows == 1)   /* Run effect on all channels at once */
    effstatus = effp->handler.drain(effp, &effp->obuf[effp->olen], &odone);
  else {                         /* Run effect on each channel individually */
    sox_ssample_t *obuf = &effp->obuf[effp->olen];
    sox_size_t odone_last;

    for (f = 0; f < effp->flows; ++f) {
      sox_size_t odonec = odone / effp->flows;
      int eff_status_c = effp->handler.drain(&sox_effects[n][f], obufc[f], &odonec);
      if (f && (odonec != odone_last)) {
        sox_fail("drained asymmetrically!");
        effstatus = SOX_EOF;
      }
      odone_last = odonec;

      if (eff_status_c != SOX_SUCCESS)
        effstatus = SOX_EOF;
    }

    for (i = 0; i < odone_last; ++i)
      for (f = 0; f < effp->flows; ++f)
        *obuf++ = obufc[f][i];
    odone = f * odone_last;
  }
  if (!odone)   /* This is the only thing that drain has and flow hasn't */
    effstatus = SOX_EOF;

  effp->olen += odone;

  return effstatus == SOX_SUCCESS? SOX_SUCCESS : SOX_EOF;
}

int sox_flow_effects(int (* callback)(sox_bool all_done))
{
  int flow_status = SOX_SUCCESS;
  sox_size_t e, source_e = 0;               /* effect indices */
  sox_size_t f, max_flows = 0;
  sox_bool draining = sox_true;

  for (e = 0; e < sox_neffects; ++e) {
    sox_effects[e][0].obuf = xmalloc(sox_bufsiz * sizeof(sox_effects[e][0].obuf[0]));
    sox_effects[e][0].odone = sox_effects[e][0].olen = 0;
    max_flows = max(max_flows, sox_effects[e][0].flows);
  }

  ibufc = xcalloc(max_flows, sizeof(*ibufc));
  obufc = xcalloc(max_flows, sizeof(*obufc));
  for (f = 0; f < max_flows; ++f) {
    ibufc[f] = xcalloc(sox_bufsiz / 2, sizeof(ibufc[f][0]));
    obufc[f] = xcalloc(sox_bufsiz / 2, sizeof(obufc[f][0]));
  }

  e = sox_neffects - 1;
  while (source_e < sox_neffects) {
#define have_imin (sox_effects[e - 1][0].olen - sox_effects[e - 1][0].odone >= sox_effects[e][0].imin)
    if (e == source_e && (draining || !have_imin)) {
      if (drain_effect(e) == SOX_EOF) {
        ++source_e;
        draining = sox_false;
      }
    } else if (have_imin && flow_effect(e) == SOX_EOF) {
      flow_status = SOX_EOF;
      source_e = e;
      draining = sox_true;
    }
    if (sox_effects[e][0].olen > sox_effects[e][0].odone) /* False for output */
      ++e;
    else if (e == source_e)
      draining = sox_true;
    else if ((int)--e < (int)source_e)
      e = source_e;

    if (callback && callback(source_e == sox_neffects) != SOX_SUCCESS) {
      flow_status = SOX_EOF; /* Client has requested to stop the flow. */
      break;
    }
  }

  for (f = 0; f < max_flows; ++f) {
    free(ibufc[f]);
    free(obufc[f]);
  }
  free(obufc);
  free(ibufc);

  for (e = 0; e < sox_neffects; ++e)
    free(sox_effects[e][0].obuf);

  return flow_status;
}

void sox_delete_effects(void)
{
  while (sox_neffects)
    free(sox_effects[--sox_neffects]);
}


/* Effects handlers. */

sox_effect_fn_t sox_effect_fns[] = {
#define EFFECT(f) sox_##f##_effect_fn,
#include "effects.h"
#undef EFFECT
  NULL
};
