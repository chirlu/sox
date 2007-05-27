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

sox_effect_t * sox_effects[SOX_MAX_EFFECTS];
unsigned sox_neffects;


int sox_add_effect(sox_effect_t * e, sox_signalinfo_t * in, sox_signalinfo_t * out, int * effects_mask)
{
  unsigned f, flows;

  if (sox_neffects == SOX_MAX_EFFECTS)
    return SOX_EOF;

  *effects_mask = sox_update_effect(e, in, out, *effects_mask);

  flows = (e->handler.flags & SOX_EFF_MCHAN)? 1 : e->ininfo.channels;

  sox_effects[sox_neffects] = xcalloc(flows, sizeof(sox_effects[sox_neffects][0]));
  sox_effects[sox_neffects][0] = *e;
  sox_effects[sox_neffects][0].flows = flows;

  for (f = 1; f < flows; ++f)
    sox_effects[sox_neffects][f] = sox_effects[sox_neffects][0];

  ++sox_neffects;
  return SOX_SUCCESS;
}

static void stop_effect(unsigned e)
{
  unsigned i;

  sox_size_t clips = 0;

  for (i = 0; i < sox_effects[e][0].flows; ++i) {
    sox_effects[e][0].handler.stop(&sox_effects[e][i]);
    clips += sox_effects[e][i].clips;
  }
  if (clips != 0)
    sox_warn("'%s' clipped %u samples; decrease volume?",sox_effects[e][0].handler.name,clips);
}

void sox_stop_effects(void)
{
  unsigned e;
  for (e = 0; e < sox_neffects; ++e)
    stop_effect(e);
}

static void kill_effect(unsigned e)
{
  sox_effects[e][0].handler.kill(&sox_effects[e][0]);/* One kill for all flows */
}

void sox_kill_effects(void)
{
  unsigned e;
  for (e = 0; e < sox_neffects; ++e)
    kill_effect(e);
}

int sox_start_effects(void)
{
  unsigned i, j;
  int ret = SOX_SUCCESS;

  for (i = 0; i < sox_neffects; ++i) {
    sox_effect_t * e = &sox_effects[i][0];
    sox_bool is_always_null = (e->handler.flags & SOX_EFF_NULL) != 0;
    int (*start)(sox_effect_t * effp) = e->handler.start;

    if (is_always_null)
      sox_report("'%s' has no effect (is a proxy effect)", e->handler.name);
    else {
      e->clips = 0;
      ret = start(e);
      if (ret == SOX_EFF_NULL)
        sox_warn("'%s' has no effect in this configuration", e->handler.name);
      else if (ret != SOX_SUCCESS)
        return SOX_EOF;
    }
    if (is_always_null || ret == SOX_EFF_NULL) { /* remove from the chain */
      kill_effect(i);
      free(sox_effects[i]);
      --sox_neffects;
      for (j = i--; j < sox_neffects; ++j)
        sox_effects[j] = sox_effects[j + 1];
    }
    else for (j = 1; j < sox_effects[i][0].flows; ++j) {
      sox_effects[i][j].clips = 0;
      if (start(&sox_effects[i][j]) != SOX_SUCCESS)
        return SOX_EOF;
    }
  }
  for (i = 0; i < sox_neffects; ++i) {
    sox_effect_t * e = &sox_effects[i][0];
    sox_report("Effects chain: %-10s %uHz %u channels %s",
        e->handler.name, e->ininfo.rate, e->ininfo.channels,
        (e->handler.flags & SOX_EFF_MCHAN)? "(multi)" : "");
  }
  return SOX_SUCCESS;
}

static sox_ssample_t **ibufc, **obufc; /* Channel interleave buffers */

static int flow_effect(unsigned e)
{
  int effstatus = SOX_SUCCESS;
  sox_size_t i, f;
  const sox_ssample_t *ibuf;
  sox_size_t idone = sox_effects[e - 1][0].olen - sox_effects[e - 1][0].odone;
  sox_size_t odone = sox_bufsiz - sox_effects[e][0].olen;

  if (sox_effects[e][0].flows == 1)   /* Run effect on all channels at once */
    effstatus = sox_effects[e][0].handler.flow(&sox_effects[e][0],
      &sox_effects[e - 1][0].obuf[sox_effects[e - 1][0].odone],
      &sox_effects[e][0].obuf[sox_effects[e][0].olen], &idone, &odone);
  else {                         /* Run effect on each channel individually */
    sox_ssample_t *obuf = &sox_effects[e][0].obuf[sox_effects[e][0].olen];
    sox_size_t idone_last, odone_last;

    ibuf = &sox_effects[e - 1][0].obuf[sox_effects[e - 1][0].odone];
    for (i = 0; i < idone; i += sox_effects[e][0].flows)
      for (f = 0; f < sox_effects[e][0].flows; ++f)
        ibufc[f][i / sox_effects[e][0].flows] = *ibuf++;

    for (f = 0; f < sox_effects[e][0].flows; ++f) {
      sox_size_t idonec = idone / sox_effects[e][0].flows;
      sox_size_t odonec = odone / sox_effects[e][0].flows;
      int eff_status_c = sox_effects[e][0].handler.flow(&sox_effects[e][f],
          ibufc[f], obufc[f], &idonec, &odonec);
      if (f && (idonec != idone_last || odonec != odone_last)) {
        sox_fail("'%s' flowed asymmetrically!", sox_effects[e][0].handler.name);
        effstatus = SOX_EOF;
      }
      idone_last = idonec;
      odone_last = odonec;

      if (eff_status_c != SOX_SUCCESS)
        effstatus = SOX_EOF;
    }

    for (i = 0; i < odone_last; ++i)
      for (f = 0; f < sox_effects[e][0].flows; ++f)
        *obuf++ = obufc[f][i];

    idone = f * idone_last;
    odone = f * odone_last;
  }
  sox_effects[e - 1][0].odone += idone;
  if (sox_effects[e - 1][0].odone == sox_effects[e - 1][0].olen) /* Can reuse this buffer? */
    sox_effects[e - 1][0].odone = sox_effects[e - 1][0].olen = 0;

  sox_effects[e][0].olen += odone;

  return effstatus == SOX_SUCCESS? SOX_SUCCESS : SOX_EOF;
}

/* The same as flow_effect but with no input */
static int drain_effect(unsigned e)
{
  int effstatus = SOX_SUCCESS;
  sox_size_t i, f;
  sox_size_t odone = sox_bufsiz - sox_effects[e][0].olen;

  if (sox_effects[e][0].flows == 1)   /* Run effect on all channels at once */
    effstatus = sox_effects[e][0].handler.drain(&sox_effects[e][0],
      &sox_effects[e][0].obuf[sox_effects[e][0].olen], &odone);
  else {                         /* Run effect on each channel individually */
    sox_ssample_t *obuf = &sox_effects[e][0].obuf[sox_effects[e][0].olen];
    sox_size_t odone_last;

    for (f = 0; f < sox_effects[e][0].flows; ++f) {
      sox_size_t odonec = odone / sox_effects[e][0].flows;
      int eff_status_c =
        sox_effects[e][0].handler.drain(&sox_effects[e][f], obufc[f], &odonec);
      if (f && (odonec != odone_last)) {
        sox_fail("'%s' drained asymmetrically!", sox_effects[e][0].handler.name);
        effstatus = SOX_EOF;
      }
      odone_last = odonec;

      if (eff_status_c != SOX_SUCCESS)
        effstatus = SOX_EOF;
    }

    for (i = 0; i < odone_last; ++i)
      for (f = 0; f < sox_effects[e][0].flows; ++f)
        *obuf++ = obufc[f][i];
    odone = f * odone_last;
  }
  if (!odone)   /* This is the only thing that drain has and flow hasn't */
    effstatus = SOX_EOF;

  sox_effects[e][0].olen += odone;

  return effstatus == SOX_SUCCESS? SOX_SUCCESS : SOX_EOF;
}

int sox_flow_effects(void (* update_status)(sox_bool), sox_bool * user_abort)
{
  int e, source_e = 0;               /* effect indices */
  int flow_status = SOX_SUCCESS;
  sox_bool draining = sox_true;
  sox_size_t f, max_flows = 0;

  for (e = 0; e < (int)sox_neffects; ++e) {
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

  --e;
  while (source_e < (int)sox_neffects) {
    if (e == source_e && (draining || sox_effects[e - 1][0].odone == sox_effects[e - 1][0].olen)) {
      if (drain_effect(e) == SOX_EOF) {
        ++source_e;
        draining = sox_false;
      }
    } else if (flow_effect(e) == SOX_EOF) {
      flow_status = SOX_EOF;
      source_e = e;
      draining = sox_true;
    }
    if (sox_effects[e][0].odone < sox_effects[e][0].olen)
      ++e;
    else if (--e < source_e)
      e = source_e;

    update_status(*user_abort || source_e == (int)sox_neffects);
    
    if (*user_abort) /* Don't get stuck in this loop. */
      return SOX_EOF;
  }

  for (f = 0; f < max_flows; ++f) {
    free(ibufc[f]);
    free(obufc[f]);
  }
  free(obufc);
  free(ibufc);

  for (e = 0; e < (int)sox_neffects; ++e)
    free(sox_effects[e][0].obuf);

  return flow_status;
}

void sox_delete_effects(void)
{
  while (sox_neffects)
    free(sox_effects[--sox_neffects]);
}
