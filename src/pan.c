/* (c) 20/03/2000 Fabien COELHO <fabien@coelho.net> for sox.
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
 *
 * Change panorama of sound file with basic linear volume interpolation.
 * The human ear is not sensible to phases? What about delay? too short?
 *
 * Volume is kept constant (?).
 * Beware of saturations!
 * Operations are carried out on doubles.
 * Can handle different number of channels.
 * Cannot handle rate change.
 *
 * Initially based on avg effect.
 * pan 0.0 basically behaves as avg.
 */

#include "sox_i.h"
#include <string.h>

/* structure to hold pan parameter */

typedef struct {double direction;} priv_t; /* from left (-1.0) to right (1.0) */

/*
 * Process options
 */
static int sox_pan_getopts(sox_effect_t * effp, int n, char **argv)
{
    priv_t * pan = (priv_t *) effp->priv;

    pan->direction = 0.0; /* default is no change */

    if (n && (!sscanf(argv[0], "%lf", &pan->direction) ||
              pan->direction < -1.0 || pan->direction > 1.0))
      return lsx_usage(effp);

    return SOX_SUCCESS;
}

/*
 * Start processing
 */
static int sox_pan_start(sox_effect_t * effp)
{
    if (effp->out_signal.channels==1)
        lsx_warn("PAN onto a mono channel...");
    return SOX_SUCCESS;
}


#define UNEXPECTED_CHANNELS \
    lsx_fail("unexpected number of channels (in=%d, out=%d)", ich, och); \
    free(ibuf_copy); \
    return SOX_EOF

/*
 * Process either isamp or osamp samples, whichever is smaller.
 */
static int sox_pan_flow(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf,
                size_t *isamp, size_t *osamp)
{
    priv_t * pan = (priv_t *) effp->priv;
    size_t len, done;
    sox_sample_t *ibuf_copy;
    char ich, och;
    double left, right, direction, hdir;

    ibuf_copy = lsx_malloc(*isamp * sizeof(sox_sample_t));
    memcpy(ibuf_copy, ibuf, *isamp * sizeof(sox_sample_t));

    direction   = pan->direction;    /* -1   <=  direction  <= 1   */
    hdir  = 0.5 * direction;  /* -0.5 <=  hdir <= 0.5 */
    left  = 0.5 - hdir; /*  0   <=  left <= 1   */
    right = 0.5 + hdir; /*  0   <= right <= 1   */

    ich = effp->in_signal.channels;
    och = effp->out_signal.channels;

    len = min(*osamp/och,*isamp/ich);

    /* report back how much is processed. */
    *isamp = len*ich;
    *osamp = len*och;

    /* 9 different cases to handle: (1,2,4) X (1,2,4) */
    switch (och) {
    case 1: /* pan on mono channel... not much sense. just avg. */
        switch (ich) {
        case 1: /* simple copy */
            for (done=0; done<len; done++)
                *obuf++ = *ibuf_copy++;
            break;
        case 2: /* average 2 */
            for (done=0; done<len; done++)
            {
                double f;
                f = 0.5*ibuf_copy[0] + 0.5*ibuf_copy[1];
                SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                *obuf++ = f;
                ibuf_copy += 2;
            }
            break;
        case 4: /* average 4 */
            for (done=0; done<len; done++)
            {
                double f;
                f = 0.25*ibuf_copy[0] + 0.25*ibuf_copy[1] +
                        0.25*ibuf_copy[2] + 0.25*ibuf_copy[3];
                SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                *obuf++ = f;
                ibuf_copy += 4;
            }
            break;
        default:
            UNEXPECTED_CHANNELS;
            break;
        } /* end first switch in channel */
        break;
    case 2:
        switch (ich) {
        case 1: /* linear */
            for (done=0; done<len; done++)
            {
                double f;

                f = left * ibuf_copy[0];
                SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                obuf[0] = f;
                f = right * ibuf_copy[0];
                SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                obuf[1] = f;
                obuf += 2;
                ibuf_copy++;
            }
            break;
        case 2: /* linear panorama.
                 * I'm not sure this is the right way to do it.
                 */
            if (direction <= 0.0) /* to the left */
            {
                register double volume, cll, clr, cr;

                volume = 1.0 - 0.5*direction;
                cll = volume*(1.5-left);
                clr = volume*(left-0.5);
                cr  = volume*(1.0+direction);

                for (done=0; done<len; done++)
                {
                    double f;

                    f = cll * ibuf_copy[0] + clr * ibuf_copy[1];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[0] = f;
                    f = cr * ibuf_copy[1];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[1] = f;
                    obuf += 2;
                    ibuf_copy += 2;
                }
            }
            else /* to the right */
            {
                register double volume, cl, crl, crr;

                volume = 1.0 + 0.5*direction;
                cl  = volume*(1.0-direction);
                crl = volume*(right-0.5);
                crr = volume*(1.5-right);

                for (done=0; done<len; done++)
                {
                    double f;

                    f = cl * ibuf_copy[0];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[0] = f;
                    f = crl * ibuf_copy[0] + crr * ibuf_copy[1];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[1] = f;
                    obuf += 2;
                    ibuf_copy += 2;
                }
            }
            break;
        case 4:
            if (direction <= 0.0) /* to the left */
            {
                register double volume, cll, clr, cr;

                volume = 1.0 - 0.5*direction;
                cll = volume*(1.5-left);
                clr = volume*(left-0.5);
                cr  = volume*(1.0+direction);

                for (done=0; done<len; done++)
                {
                    register double ibuf0, ibuf1, f;

                    /* build stereo signal */
                    ibuf0 = 0.5*ibuf_copy[0] + 0.5*ibuf_copy[2];
                    ibuf1 = 0.5*ibuf_copy[1] + 0.5*ibuf_copy[3];

                    /* pan it */
                    f = cll * ibuf0 + clr * ibuf1;
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[0] = f;
                    f = cr * ibuf1;
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[1] = f;
                    obuf += 2;
                    ibuf_copy += 4;
                }
            }
            else /* to the right */
            {
                register double volume, cl, crl, crr;

                volume = 1.0 + 0.5*direction;
                cl  = volume*(1.0-direction);
                crl = volume*(right-0.5);
                crr = volume*(1.5-right);

                for (done=0; done<len; done++)
                {
                    register double ibuf0, ibuf1, f;

                    ibuf0 = 0.5*ibuf_copy[0] + 0.5*ibuf_copy[2];
                    ibuf1 = 0.5*ibuf_copy[1] + 0.5*ibuf_copy[3];

                    f = cl * ibuf0;
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[0] = f;
                    f = crl * ibuf0 + crr * ibuf1;
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[1] = f;
                    obuf += 2;
                    ibuf_copy += 4;
                }
            }
            break;
        default:
            UNEXPECTED_CHANNELS;
            break;
        } /* end second switch in channel */
        break;
    case 4:
        switch (ich) {
        case 1: /* linear */
            {
                register double cr, cl;

                cl = 0.5*left;
                cr = 0.5*right;

                for (done=0; done<len; done++)
                {
                    double f;

                    f = cl * ibuf_copy[0];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[2] = obuf[0] = f;
                    f = cr * ibuf_copy[0];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    ibuf_copy[3] = obuf[1] = f;
                    obuf += 4;
                    ibuf_copy++;
                }
            }
            break;
        case 2: /* simple linear panorama */
            if (direction <= 0.0) /* to the left */
            {
                register double volume, cll, clr, cr;

                volume = 0.5 - 0.25*direction;
                cll = volume * (1.5-left);
                clr = volume * (left-0.5);
                cr  = volume * (1.0+direction);

                for (done=0; done<len; done++)
                {
                    double f;

                    f = cll * ibuf_copy[0] + clr * ibuf_copy[1];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[2] = obuf[0] = f;
                    f = cr * ibuf_copy[1];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    ibuf_copy[3] = obuf[1] = f;
                    obuf += 4;
                    ibuf_copy += 2;
                }
            }
            else /* to the right */
            {
                register double volume, cl, crl, crr;

                volume = 0.5 + 0.25*direction;
                cl  = volume * (1.0-direction);
                crl = volume * (right-0.5);
                crr = volume * (1.5-right);

                for (done=0; done<len; done++)
                {
                    double f;

                    f = cl * ibuf_copy[0];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[2] = obuf[0] =f ;
                    f = crl * ibuf_copy[0] + crr * ibuf_copy[1];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    ibuf_copy[3] = obuf[1] = f;
                    obuf += 4;
                    ibuf_copy += 2;
                }
            }
            break;
        case 4:
            /* maybe I could improve the formula to reverse...
               also, turn only by quarters.
             */
            if (direction <= 0.0) /* to the left */
            {
                register double cown, cright;

                cright = -direction;
                cown = 1.0 + direction;

                for (done=0; done<len; done++)
                {
                    double f;

                    f = cown*ibuf_copy[0] + cright*ibuf_copy[1];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[0] = f;
                    f = cown*ibuf_copy[1] + cright*ibuf_copy[3];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[1] = f;
                    f = cown*ibuf_copy[2] + cright*ibuf_copy[0];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[2] = f;
                    f = cown*ibuf_copy[3] + cright*ibuf_copy[2];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[3] = f;
                    obuf += 4;
                    ibuf_copy += 4;
                }
            }
            else /* to the right */
            {
                register double cleft, cown;

                cleft = direction;
                cown = 1.0 - direction;

                for (done=0; done<len; done++)
                {
                    double f;

                    f = cleft*ibuf_copy[2] + cown*ibuf_copy[0];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[0] = f;
                    f = cleft*ibuf_copy[0] + cown*ibuf_copy[1];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[1] = f;
                    f = cleft*ibuf_copy[3] + cown*ibuf_copy[2];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[2] = f;
                    f = cleft*ibuf_copy[1] + cown*ibuf_copy[3];
                    SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
                    obuf[3] = f;
                    obuf += 4;
                    ibuf_copy += 4;
                }
            }
            break;
        default:
            UNEXPECTED_CHANNELS;
            break;
        } /* end third switch in channel */
        break;
    default:
        UNEXPECTED_CHANNELS;
        break;
    } /* end switch out channel */

    free(ibuf_copy - len * ich);

    return SOX_SUCCESS;
}

/*
 * FIXME: Add a stop function with statistics on right, left, and output amplitudes.
 */

static sox_effect_handler_t sox_pan_effect = {
  "pan",
  "direction (in [-1.0 .. 1.0])",
  SOX_EFF_MCHAN | SOX_EFF_CHAN | SOX_EFF_DEPRECATED,
  sox_pan_getopts,
  sox_pan_start,
  sox_pan_flow,
  NULL,
  NULL,
  NULL, sizeof(priv_t)
};

const sox_effect_handler_t *sox_pan_effect_fn(void)
{
    return &sox_pan_effect;
}
