/*
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

/* SoX Effects chain   (c) 2007 robs@users.sourceforge.net */

#include "sox_i.h"

struct sox_effect * effects[MAX_EFFECTS];
unsigned neffects;


void add_effect(struct sox_effect * e, sox_signalinfo_t * in, sox_signalinfo_t * out, int * effects_mask)
{
  unsigned f, flows;

  *effects_mask = sox_updateeffect(e, in, out, *effects_mask);

  flows = (e->h->flags & SOX_EFF_MCHAN)? 1 : e->ininfo.channels;

  effects[neffects] = xcalloc(flows, sizeof(effects[neffects][0]));
  effects[neffects][0] = *e;
  effects[neffects][0].flows = flows;

  for (f = 1; f < flows; ++f)
    effects[neffects][f] = effects[neffects][0];

  ++neffects;
}

static void stop_effect(unsigned e)
{
  unsigned i;

  sox_size_t clips = 0;
  int (*stop)(eff_t effp) =
     effects[e][0].h->stop? effects[e][0].h->stop : sox_effect_nothing;

  for (i = 0; i < effects[e][0].flows; ++i) {
    stop(&effects[e][i]);
    clips += effects[e][i].clips;
  }
  if (clips != 0)
    sox_warn("'%s' clipped %u samples; decrease volume?",effects[e][0].name,clips);
}

void stop_effects(void)
{
  unsigned e;
  for (e = 0; e < neffects; ++e)
    stop_effect(e);
}

static void kill_effect(unsigned e)
{
  int (*kill)(eff_t effp) =
     effects[e][0].h->kill? effects[e][0].h->kill : sox_effect_nothing;

  kill(&effects[e][0]);  /* One kill for all flows */
}

void kill_effects(void)
{
  unsigned e;
  for (e = 0; e < neffects; ++e)
    kill_effect(e);
}

int start_effects(void)
{
  unsigned i, j;
  int ret = SOX_SUCCESS;

  for (i = 0; i < neffects; ++i) {
    struct sox_effect * e = &effects[i][0];
    sox_bool is_always_null = (e->h->flags & SOX_EFF_NULL) != 0;
    int (*start)(eff_t effp) = e->h->start? e->h->start : sox_effect_nothing;

    if (is_always_null)
      sox_report("'%s' has no effect (is a proxy effect)", e->name);
    else {
      e->clips = 0;
      ret = start(e);
      if (ret == SOX_EFF_NULL)
        sox_warn("'%s' has no effect in this configuration", e->name);
      else if (ret != SOX_SUCCESS)
        return SOX_EOF;
    }
    if (is_always_null || ret == SOX_EFF_NULL) { /* remove from the chain */
      kill_effect(i);
      free(effects[i]);
      --neffects;
      for (j = i--; j < neffects; ++j)
        effects[j] = effects[j + 1];
    }
    else for (j = 1; j < effects[i][0].flows; ++j) {
      effects[i][j].clips = 0;
      if (start(&effects[i][j]) != SOX_SUCCESS)
        return SOX_EOF;
    }
  }
  for (i = 0; i < neffects; ++i) {
    struct sox_effect * e = &effects[i][0];
    sox_report("Effects chain: %-10s %uHz %u channels %s",
        e->name, e->ininfo.rate, e->ininfo.channels,
        (e->h->flags & SOX_EFF_MCHAN)? "(multi)" : "");
  }
  return SOX_SUCCESS;
}

static sox_ssample_t **ibufc, **obufc; /* Channel interleave buffers */

static int flow_effect(unsigned e)
{
  sox_size_t i, f, idone, odone;
  const sox_ssample_t *ibuf;
  int effstatus = SOX_SUCCESS;
  int (*flow)(eff_t, sox_ssample_t const*, sox_ssample_t*, sox_size_t*, sox_size_t*) =
    effects[e][0].h->flow? effects[e][0].h->flow : sox_effect_nothing_flow;

  idone = effects[e - 1][0].olen - effects[e - 1][0].odone;
  odone = sox_bufsiz - effects[e][0].olen;

  if (effects[e][0].flows == 1)   /* Run effect on all channels at once */
    effstatus = flow(&effects[e][0],
      &effects[e - 1][0].obuf[effects[e - 1][0].odone],
      &effects[e][0].obuf[effects[e][0].olen], &idone, &odone);
  else {                         /* Run effect on each channel individually */
    sox_ssample_t *obuf = &effects[e][0].obuf[effects[e][0].olen];
    sox_size_t idone_last, odone_last;

    ibuf = &effects[e - 1][0].obuf[effects[e - 1][0].odone];
    for (i = 0; i < idone; i += effects[e][0].flows)
      for (f = 0; f < effects[e][0].flows; ++f)
        ibufc[f][i / effects[e][0].flows] = *ibuf++;

    for (f = 0; f < effects[e][0].flows; ++f) {
      sox_size_t idonec = idone / effects[e][0].flows;
      sox_size_t odonec = odone / effects[e][0].flows;
      int eff_status_c =
        flow(&effects[e][f], ibufc[f], obufc[f], &idonec, &odonec);
      if (f && (idonec != idone_last || odonec != odone_last)) {
        sox_fail("'%s' flowed asymmetrically!", effects[e][0].name);
        effstatus = SOX_EOF;
      }
      idone_last = idonec;
      odone_last = odonec;

      if (eff_status_c != SOX_SUCCESS)
        effstatus = SOX_EOF;
    }

    for (i = 0; i < odone_last; ++i)
      for (f = 0; f < effects[e][0].flows; ++f)
        *obuf++ = obufc[f][i];

    idone = f * idone_last;
    odone = f * odone_last;
  }
  effects[e - 1][0].odone += idone;
  if (effects[e - 1][0].odone == effects[e - 1][0].olen) /* Can reuse this buffer? */
    effects[e - 1][0].odone = effects[e - 1][0].olen = 0;

  effects[e][0].olen += odone;

  return effstatus == SOX_SUCCESS? SOX_SUCCESS : SOX_EOF;
}

static int drain_effect(unsigned e)
{
  sox_size_t i, f, odone;
  int effstatus = SOX_SUCCESS;
  int (*drain)(eff_t, sox_ssample_t*, sox_size_t*) =
    effects[e][0].h->drain? effects[e][0].h->drain : sox_effect_nothing_drain;

  odone = sox_bufsiz - effects[e][0].olen;

  if (effects[e][0].flows == 1)   /* Run effect on all channels at once */
    effstatus = drain(&effects[e][0],
      &effects[e][0].obuf[effects[e][0].olen], &odone);
  else {                         /* Run effect on each channel individually */
    sox_ssample_t *obuf = &effects[e][0].obuf[effects[e][0].olen];
    sox_size_t odone_last;

    for (f = 0; f < effects[e][0].flows; ++f) {
      sox_size_t odonec = odone / effects[e][0].flows;
      int eff_status_c =
        drain(&effects[e][f], obufc[f], &odonec);
      if (f && (odonec != odone_last)) {
        sox_fail("'%s' drained asymmetrically!", effects[e][0].name);
        effstatus = SOX_EOF;
      }
      odone_last = odonec;

      if (eff_status_c != SOX_SUCCESS)
        effstatus = SOX_EOF;
    }

    for (i = 0; i < odone_last; ++i)
      for (f = 0; f < effects[e][0].flows; ++f)
        *obuf++ = obufc[f][i];
    odone = f * odone_last;
  }
  if (!odone)
    effstatus = SOX_EOF;

  effects[e][0].olen += odone;

  return effstatus == SOX_SUCCESS? SOX_SUCCESS : SOX_EOF;
}

int flow_effects(void (* update_status)(sox_bool), sox_bool * user_abort)
{
  int e, source_e = 0;               /* effect indices */
  int flow_status = SOX_SUCCESS;
  sox_bool draining = sox_true;
  sox_size_t f, max_flows = 0;

  for (e = 0; e < (int)neffects; ++e) {
    effects[e][0].obuf = xmalloc(sox_bufsiz * sizeof(effects[e][0].obuf[0]));
    effects[e][0].odone = effects[e][0].olen = 0;
    max_flows = max(max_flows, effects[e][0].flows);
  }

  ibufc = xcalloc(max_flows, sizeof(*ibufc));
  obufc = xcalloc(max_flows, sizeof(*obufc));
  for (f = 0; f < max_flows; ++f) {
    ibufc[f] = xcalloc(sox_bufsiz / 2, sizeof(ibufc[f][0]));
    obufc[f] = xcalloc(sox_bufsiz / 2, sizeof(obufc[f][0]));
  }

  --e;
  while (source_e < (int)neffects) {
    if (e == source_e && (draining || effects[e - 1][0].odone == effects[e - 1][0].olen)) {
      if (drain_effect(e) == SOX_EOF) {
        ++source_e;
        draining = sox_false;
      }
    } else if (flow_effect(e) == SOX_EOF) {
      flow_status = SOX_EOF;
      source_e = e;
      draining = sox_true;
    }
    if (effects[e][0].odone < effects[e][0].olen)
      ++e;
    else if (--e < source_e)
      e = source_e;

    update_status(*user_abort || source_e == (int)neffects);
    
    if (*user_abort) /* Don't get stuck in this loop. */
      return SOX_EOF;
  }

  for (f = 0; f < max_flows; ++f) {
    free(ibufc[f]);
    free(obufc[f]);
  }
  free(obufc);
  free(ibufc);

  for (e = 0; e < (int)neffects; ++e)
    free(effects[e][0].obuf);

  return flow_status;
}

void delete_effects(void)
{
  while (neffects)
    free(effects[--neffects]);
}
