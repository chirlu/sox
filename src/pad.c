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

/* Effect: Pad With Silence   (c) 2006 robs@users.sourceforge.net */

#include "st_i.h"

typedef struct pad
{
  int npads;         /* Number of pads requested */
  struct {
    char *str;       /* Command-line argument to parse for this pad */
    st_size_t start; /* Start padding when in_pos equals this */
    st_size_t pad;   /* Number of samples to pad */
  } * pads;

  st_size_t in_pos;  /* Number of samples read from the input stream */
  int pads_pos;      /* Number of pads completed so far */
  st_size_t pad_pos; /* Number of samples through the current pad */
} * pad_t;

assert_static(sizeof(struct pad) <= ST_MAX_EFFECT_PRIVSIZE,
              /* else */ pad_PRIVSIZE_too_big);

static int parse(eff_t effp, char * * argv, st_rate_t rate)
{
  pad_t p = (pad_t) effp->priv;
  char const * next;
  int i;

  for (i = 0; i < p->npads; ++i) {
    if (argv) /* 1st parse only */
      p->pads[i].str = xstrdup(argv[i]);
    next = st_parsesamples(rate, p->pads[i].str, &p->pads[i].pad, 't');
    if (next == NULL) break;
    if (*next == '\0')
      p->pads[i].start = i? ST_SIZE_MAX : 0;
    else {
      if (*next != '@') break;
      next = st_parsesamples(rate, next+1, &p->pads[i].start, 't');
      if (next == NULL || *next != '\0') break;
    }
    if (i > 0 && p->pads[i].start <= p->pads[i-1].start) break;
  }
  if (i < p->npads) {
    st_fail(effp->h->usage);
    return ST_EOF;
  }
  return ST_SUCCESS;
}

static int create(eff_t effp, int n, char * * argv)
{
  pad_t p = (pad_t) effp->priv;
  p->pads = xcalloc(p->npads = n, sizeof(*p->pads));
  return parse(effp, argv, ST_MAXRATE); /* No rate yet; parse with dummy */
}

static int start(eff_t effp)
{
  pad_t p = (pad_t) effp->priv;
  int i;

  parse(effp, 0, effp->ininfo.rate); /* Re-parse now rate is known */
  p->in_pos = p->pad_pos = p->pads_pos = 0;
  for (i = 0; i < p->npads; ++i)
    if (p->pads[i].pad)
      return ST_SUCCESS;
  return ST_EFF_NULL;
}

static int flow(eff_t effp, const st_sample_t * ibuf, st_sample_t * obuf,
                st_size_t * isamp, st_size_t * osamp)
{
  pad_t p = (pad_t) effp->priv;
  st_size_t c, idone = 0, odone = 0;
  *isamp /= effp->ininfo.channels;
  *osamp /= effp->ininfo.channels;

  do {
    /* Copying: */
    for (; idone < *isamp && odone < *osamp && !(p->pads_pos != p->npads && p->in_pos == p->pads[p->pads_pos].start); ++idone, ++odone, ++p->in_pos)
      for (c = 0; c < effp->ininfo.channels; ++c) *obuf++ = *ibuf++;

    /* Padding: */
    if (p->pads_pos != p->npads && p->in_pos == p->pads[p->pads_pos].start) {
      for (; odone < *osamp && p->pad_pos < p->pads[p->pads_pos].pad; ++odone, ++p->pad_pos)
        for (c = 0; c < effp->ininfo.channels; ++c) *obuf++ = 0;
      if (p->pad_pos == p->pads[p->pads_pos].pad) { /* Move to next pad? */
        ++p->pads_pos;
        p->pad_pos = 0;
      }
    }
  } while (idone < *isamp && odone < *osamp);

  *isamp = idone * effp->ininfo.channels;
  *osamp = odone * effp->ininfo.channels;
  return ST_SUCCESS;
}

static int drain(eff_t effp, st_sample_t * obuf, st_size_t * osamp)
{
  static st_size_t isamp = 0;
  pad_t p = (pad_t) effp->priv;
  if (p->pads_pos != p->npads && p->in_pos != p->pads[p->pads_pos].start)
    p->in_pos = ST_SIZE_MAX;  /* Invoke the final pad (with no given start) */
  return flow(effp, 0, obuf, &isamp, osamp);
}

static int stop(eff_t effp)
{
  pad_t p = (pad_t) effp->priv;
  if (p->pads_pos != p->npads)
    st_warn("Input audio too short; pads not applied: %i",p->npads-p->pads_pos);
  return ST_SUCCESS;
}

static int delete(eff_t effp)
{
  pad_t p = (pad_t) effp->priv;
  int i;
  for (i = 0; i < p->npads; ++i)
    free(p->pads[i].str);
  free(p->pads);
  return ST_SUCCESS;
}

st_effect_t const * st_pad_effect_fn(void)
{
  static st_effect_t driver = {
    "pad", "Usage: pad {length[@position]}", ST_EFF_MCHAN,
    create, start, flow, drain, stop, delete
  };
  return &driver;
}
