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

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "st_i.h"

typedef struct repeatstuff {
        FILE *fp;
        int first_drain;
        st_size_t total;
        st_size_t remaining;
        int repeats;
} *repeat_t;

int st_repeat_getopts(eff_t effp, int n, char **argv) 
{
        repeat_t repeat = (repeat_t)effp->priv;

        if (n != 1) {
                st_fail("Usage: repeat count]");
                return (ST_EOF);
        }

        if (!(sscanf(argv[0], "%i", &repeat->repeats))) {
                st_fail("repeat: could not parse repeat parameter");
                return (ST_EOF);
        }

        if (repeat->repeats < 0) {
                st_fail("repeat: repeat parameter must be positive");
                return (ST_EOF);
        }

        return (ST_SUCCESS);
}

int st_repeat_start(eff_t effp)
{
        repeat_t repeat = (repeat_t)effp->priv;

        if ((repeat->fp = tmpfile()) == NULL) {
                st_fail("repeat: could not create temporary file");
                return (ST_EOF);
        }

        repeat->first_drain = 1;
        
        return (ST_SUCCESS);
}

int st_repeat_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                st_size_t *isamp, st_size_t *osamp)
{
        repeat_t repeat = (repeat_t)effp->priv;

        if (fwrite((char *)ibuf, sizeof(st_sample_t), *isamp, repeat->fp) !=
                        *isamp) {
                st_fail("repeat: write error on temporary file\n");
                return (ST_EOF);
        }

        *osamp = 0;

        return (ST_SUCCESS);
}

int st_repeat_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
        size_t read = 0;
        st_sample_t *buf;
        st_size_t samp;
        st_size_t done;

        repeat_t repeat = (repeat_t)effp->priv;

        if (repeat->first_drain == 1) {
                repeat->first_drain = 0;

                fseek(repeat->fp, 0L, SEEK_END);
                repeat->total = ftell(repeat->fp);

                if ((repeat->total % sizeof(st_sample_t)) != 0) {
                        st_fail("repeat: corrupted temporary file\n");
                        return (ST_EOF);
                }
        
                repeat->total /= sizeof(st_sample_t);
                repeat->remaining = repeat->total;
                
                fseek(repeat->fp, 0L, SEEK_SET);
        }

        if (repeat->remaining == 0) {
                if (repeat->repeats == 0) {
                        *osamp = 0;
                        return (ST_SUCCESS);
                }
                else {
                        repeat->repeats--;
                        fseek(repeat->fp, 0L, SEEK_SET);
                        repeat->remaining = repeat->total;
                }
        }
        if (*osamp > repeat->remaining) {
                buf = obuf;
                samp = repeat->remaining;

                read = fread((char *)buf, sizeof(st_sample_t), samp,
                                repeat->fp);
                if (read != samp) {
                        perror(strerror(errno));
                        st_fail("repeat1: read error on temporary file\n");
                        return(ST_EOF);
                }

                done = samp;
                buf = &obuf[samp];
                repeat->remaining = 0;

                while (repeat->repeats > 0) {
                        repeat->repeats--;
                        fseek(repeat->fp, 0L, SEEK_SET);

                        if (repeat->total >= *osamp - done) {
                                samp = *osamp - done;
                        }
                        else {
                                samp = repeat->total;
                                if (samp > *osamp - done) {
                                        samp = *osamp - done;
                                }
                        }

                        repeat->remaining = repeat->total - samp;
                        
                        read = fread((char *)buf, sizeof(st_sample_t), samp,
                                        repeat->fp);
                        if (read != samp) {
                                perror(strerror(errno));
                                st_fail("repeat2: read error on temporary "
                                                "file\n");
                                return(ST_EOF);
                        }

                        done += samp;
                        if (done == *osamp) {
                                break;
                        }
                }
                *osamp = done;
        }
        else {
                read = fread((char *)obuf, sizeof(st_sample_t), *osamp,
                                repeat->fp);
                if (read != *osamp) {
                        perror(strerror(errno));
                        st_fail("repeat3: read error on temporary file\n");
                        return(ST_EOF);
                }
                repeat->remaining -= *osamp;
        }

        return (ST_SUCCESS);
}

int st_repeat_stop(eff_t effp)
{
        repeat_t repeat = (repeat_t)effp->priv;

        fclose(repeat->fp);
        
        return (ST_SUCCESS);
}

