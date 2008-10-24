/* libSoX repeat effect  Copyright (c) 2004 Jan Paul Schmidt <jps@fundament.org>
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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>  /* for off_t on OS/2 and possibly others */

typedef struct {
  FILE * tmp_file;
  int first_drain;
  size_t total;
  size_t remaining;
  int repeats;
} priv_t;

static int getopts(sox_effect_t * effp, int argc, char **argv)
{
  priv_t * p = (priv_t *)effp->priv;
  do {NUMERIC_PARAMETER(repeats, 0, 1e6)} while (0);
  return argc? lsx_usage(effp) : SOX_SUCCESS;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  if (p->repeats == 0)
    return SOX_EFF_NULL;

  if ((p->tmp_file = tmpfile()) == NULL) {
    lsx_fail("can't create temporary file: %s", strerror(errno));
    return SOX_EOF;
  }
  p->first_drain = 1;
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  if (fwrite(ibuf, sizeof(*ibuf), *isamp, p->tmp_file) != *isamp) {
    lsx_fail("error writing temporary file: %s", strerror(errno));
    return SOX_EOF;
  }
  (void)obuf, *osamp = 0; /* samples not output until drain */
  return SOX_SUCCESS;
}

static int drain(sox_effect_t * effp, sox_sample_t * obuf, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t read = 0;
  sox_sample_t *buf;
  size_t samp;
  size_t done;

  if (p->first_drain == 1) {
    p->first_drain = 0;

    fseeko(p->tmp_file, (off_t) 0, SEEK_END);
    p->total = ftello(p->tmp_file);

    if ((p->total % sizeof(sox_sample_t)) != 0) {
      lsx_fail("corrupted temporary file");
      return (SOX_EOF);
    }

    p->total /= sizeof(sox_sample_t);
    p->remaining = p->total;

    fseeko(p->tmp_file, (off_t) 0, SEEK_SET);
  }

  if (p->remaining == 0) {
    if (p->repeats == 0) {
      *osamp = 0;
      return (SOX_EOF);
    } else {
      p->repeats--;
      fseeko(p->tmp_file, (off_t) 0, SEEK_SET);
      p->remaining = p->total;
    }
  }
  if (*osamp > p->remaining) {
    buf = obuf;
    samp = p->remaining;

    read = fread(buf, sizeof(sox_sample_t), samp, p->tmp_file);
    if (read != samp) {
      perror(strerror(errno));
      lsx_fail("read error on temporary file");
      return (SOX_EOF);
    }

    done = samp;
    buf = &obuf[samp];
    p->remaining = 0;

    while (p->repeats > 0) {
      p->repeats--;
      fseeko(p->tmp_file, (off_t) 0, SEEK_SET);

      if (p->total >= *osamp - done) {
        samp = *osamp - done;
      } else {
        samp = p->total;
        if (samp > *osamp - done)
          samp = *osamp - done;
      }

      p->remaining = p->total - samp;

      read = fread(buf, sizeof(sox_sample_t), samp, p->tmp_file);
      if (read != samp) {
        perror(strerror(errno));
        lsx_fail("repeat2: read error on temporary " "file\n");
        return (SOX_EOF);
      }

      done += samp;
      if (done == *osamp)
        break;
    }
    *osamp = done;
  } else {
    read = fread(obuf, sizeof(sox_sample_t), *osamp, p->tmp_file);
    if (read != *osamp) {
      perror(strerror(errno));
      lsx_fail("repeat3: read error on temporary file");
      return (SOX_EOF);
    }
    p->remaining -= *osamp;
  }

  if (p->remaining == 0)
    return SOX_EOF;
  else
    return SOX_SUCCESS;
}

static int stop(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  fclose(p->tmp_file);
  return SOX_SUCCESS;
}

const sox_effect_handler_t *sox_repeat_effect_fn(void)
{
  static sox_effect_handler_t effect = {"repeat", "count", SOX_EFF_MCHAN |
    SOX_EFF_LENGTH, getopts, start, flow, drain, stop, NULL, sizeof(priv_t)};
  return &effect;
}
