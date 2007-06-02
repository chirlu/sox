/*
 * libao player support for sox
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

#include <stdlib.h>
#include <stdio.h>
#include <ao/ao.h>

typedef struct ao_priv
{
  int driver_id;
  ao_device *device;
  ao_sample_format format;
} *ao_priv_t;

static int startread(UNUSED sox_format_t * ft)
{
  sox_fail("Cannot read from libao driver");
  return SOX_EOF;
}

static int startwrite(sox_format_t * ft)
{
  ao_priv_t ao = (ao_priv_t)ft->priv;

  ao_initialize();
  if ((ao->driver_id = ao_default_driver_id()) < 0) {
    sox_fail("Could not find a default driver");
    return SOX_EOF;
  }

  ao->format.bits = SOX_SAMPLE_BITS;
  ao->format.rate = ft->signal.rate;
  ao->format.channels = ft->signal.channels;
  ao->format.byte_format = AO_FMT_NATIVE;
  if ((ao->device = ao_open_live(ao->driver_id, &ao->format, NULL)) == NULL) {
    sox_fail("Could not open default device: error %d", errno);
    return SOX_EOF;
  }

  return SOX_SUCCESS;
}

static sox_size_t write(sox_format_t * ft, const sox_ssample_t *buf, sox_size_t len)
{
  ao_priv_t ao = (ao_priv_t)ft->priv;

  if (ao_play(ao->device, (void *)buf, len * sizeof(sox_ssample_t)) == 0)
    return 0;

  return len;
}

static int stopwrite(sox_format_t * ft)
{
  ao_priv_t ao = (ao_priv_t)ft->priv;

  if (ao_close(ao->device) == 0) {
    sox_fail("Error closing libao output");
    return SOX_EOF;
  }
  ao_shutdown();

  return SOX_SUCCESS;
}

/* libao player */
static const char *aonames[] = {
  "ao",
  NULL
};

static sox_format_handler_t sox_ao_format = {
  aonames,
  SOX_FILE_DEVICE | SOX_FILE_NOSTDIO,
  startread,
  sox_format_nothing_read,
  sox_format_nothing,
  startwrite,
  write,
  stopwrite,
  sox_format_nothing_seek
};

const sox_format_handler_t *sox_ao_format_fn(void);

const sox_format_handler_t *sox_ao_format_fn(void)
{
    return &sox_ao_format;
}
