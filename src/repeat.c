/*
    Repeat effect file for SoX
    Copyright (C) 2004 Jan Paul Schmidt <jps@fundament.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sox_i.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>

static sox_effect_t sox_repeat_effect;

typedef struct repeatstuff {
        FILE *fp;
        int first_drain;
        sox_size_t total;
        sox_size_t remaining;
        int repeats;
} *repeat_t;

static int sox_repeat_getopts(eff_t effp, int n, char **argv)
{
        repeat_t repeat = (repeat_t)effp->priv;

        if (n != 1) {
                sox_fail(sox_repeat_effect.usage);
                return (SOX_EOF);
        }

        if (!(sscanf(argv[0], "%i", &repeat->repeats))) {
                sox_fail("repeat: could not parse repeat parameter");
                return (SOX_EOF);
        }

        if (repeat->repeats < 0) {
                sox_fail("repeat: repeat parameter must be positive");
                return (SOX_EOF);
        }

        return (SOX_SUCCESS);
}

static int sox_repeat_start(eff_t effp)
{
        repeat_t repeat = (repeat_t)effp->priv;

        if (repeat->repeats == 0)
          return SOX_EFF_NULL;

        if ((repeat->fp = tmpfile()) == NULL) {
                sox_fail("repeat: could not create temporary file");
                return (SOX_EOF);
        }

        repeat->first_drain = 1;

        return (SOX_SUCCESS);
}

static int sox_repeat_flow(eff_t effp, const sox_sample_t *ibuf, sox_sample_t *obuf UNUSED,
                sox_size_t *isamp, sox_size_t *osamp)
{
        repeat_t repeat = (repeat_t)effp->priv;

        if (fwrite((char *)ibuf, sizeof(sox_sample_t), *isamp, repeat->fp) !=
                        *isamp) {
                sox_fail("repeat: write error on temporary file");
                return (SOX_EOF);
        }

        *osamp = 0;

        return (SOX_SUCCESS);
}

static int sox_repeat_drain(eff_t effp, sox_sample_t *obuf, sox_size_t *osamp)
{
        size_t read = 0;
        sox_sample_t *buf;
        sox_size_t samp;
        sox_size_t done;

        repeat_t repeat = (repeat_t)effp->priv;

        if (repeat->first_drain == 1) {
                repeat->first_drain = 0;

                fseeko(repeat->fp, (off_t)0, SEEK_END);
                repeat->total = ftello(repeat->fp);

                if ((repeat->total % sizeof(sox_sample_t)) != 0) {
                        sox_fail("repeat: corrupted temporary file");
                        return (SOX_EOF);
                }

                repeat->total /= sizeof(sox_sample_t);
                repeat->remaining = repeat->total;

                fseeko(repeat->fp, (off_t)0, SEEK_SET);
        }

        if (repeat->remaining == 0) {
                if (repeat->repeats == 0) {
                        *osamp = 0;
                        return (SOX_EOF);
                } else {
                        repeat->repeats--;
                        fseeko(repeat->fp, (off_t)0, SEEK_SET);
                        repeat->remaining = repeat->total;
                }
        }
        if (*osamp > repeat->remaining) {
                buf = obuf;
                samp = repeat->remaining;

                read = fread((char *)buf, sizeof(sox_sample_t), samp,
                                repeat->fp);
                if (read != samp) {
                        perror(strerror(errno));
                        sox_fail("repeat: read error on temporary file");
                        return(SOX_EOF);
                }

                done = samp;
                buf = &obuf[samp];
                repeat->remaining = 0;

                while (repeat->repeats > 0) {
                        repeat->repeats--;
                        fseeko(repeat->fp, (off_t)0, SEEK_SET);

                        if (repeat->total >= *osamp - done) {
                                samp = *osamp - done;
                        } else {
                                samp = repeat->total;
                                if (samp > *osamp - done)
                                        samp = *osamp - done;
                        }

                        repeat->remaining = repeat->total - samp;

                        read = fread((char *)buf, sizeof(sox_sample_t), samp,
                                        repeat->fp);
                        if (read != samp) {
                                perror(strerror(errno));
                                sox_fail("repeat2: read error on temporary "
                                                "file\n");
                                return(SOX_EOF);
                        }

                        done += samp;
                        if (done == *osamp)
                                break;
                }
                *osamp = done;
        }
        else {
                read = fread((char *)obuf, sizeof(sox_sample_t), *osamp,
                                repeat->fp);
                if (read != *osamp) {
                        perror(strerror(errno));
                        sox_fail("repeat3: read error on temporary file");
                        return(SOX_EOF);
                }
                repeat->remaining -= *osamp;
        }

        if (repeat->remaining == 0)
            return SOX_EOF;
        else
            return SOX_SUCCESS;
}

static int sox_repeat_stop(eff_t effp)
{
        repeat_t repeat = (repeat_t)effp->priv;

        fclose(repeat->fp);

        return (SOX_SUCCESS);
}

static sox_effect_t sox_repeat_effect = {
  "repeat",
  "Usage: repeat count",
  0,
  sox_repeat_getopts,
  sox_repeat_start,
  sox_repeat_flow,
  sox_repeat_drain,
  sox_repeat_stop,
  sox_effect_nothing
};

const sox_effect_t *sox_repeat_effect_fn(void)
{
    return &sox_repeat_effect;
}
