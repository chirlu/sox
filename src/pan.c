/*
 * (c) 20/03/2000 Fabien COELHO <fabien@coelho.net> for sox.
 *
 * Same copyright as SOX. See Sox (and Sun?;-)
 *
 * Change panorama of sound file with basic linear volume interpolation.
 * The human ear is not sensible to phases? What about delay? too short?
 * 
 * Volume is kept constant (?). 
 * Beware of saturations!
 * Operations are carried out on floats.
 * Can handle different number of channels.
 * Cannot handle rate change.
 *
 * Initially based on avg effect. 
 * pan 0.0 basically behaves as avg.
 */

#include "st_i.h"

/* type for computations.
   float mantissa is okay for 8 or 16 bits signals.
   maybe a little bit short for 24 bits.
   switch to double if you're not happy.
 */
#ifndef PAN_FLOAT
#define PAN_FLOAT float /* float or double */
#define PAN_FLOAT_SCAN "%f" /* "%f" or "%lf" */
#endif

/* constants */

#define TWO      ((PAN_FLOAT)(2.0e0))
#define ONEHALF  ((PAN_FLOAT)(1.5e0))
#define ONE      ((PAN_FLOAT)(1.0e0))
#define MONE     ((PAN_FLOAT)(-1.0e0))
#define HALF     ((PAN_FLOAT)(0.5e0))
#define QUARTER  ((PAN_FLOAT)(0.25e0))
#define ZERO     ((PAN_FLOAT)(0.0e0))

/* error message */

#define PAN_USAGE "Usage: pan direction (in [-1.0 .. 1.0])"

/* structure to hold pan parameter */

typedef struct {
    /* pan option
     */
    PAN_FLOAT dir; /* direction, from left (-1.0) to right (1.0) */

    /* local data
     */
    int clipped;   /* number of clipped values */
} * pan_t;

/*
 * Process options
 */
int st_pan_getopts(eff_t effp, int n, char **argv) 
{
    pan_t pan = (pan_t) effp->priv; 
    
    pan->dir = ZERO; /* default is no change */
    pan->clipped = 0;
    
    if (n && (!sscanf(argv[0], PAN_FLOAT_SCAN, &pan->dir) || 
              pan->dir < MONE || pan->dir > ONE))
    {
        st_fail(PAN_USAGE);
        return ST_EOF;
    }

    return ST_SUCCESS;
}

/*
 * Start processing
 */
int st_pan_start(eff_t effp)
{
    if (effp->outinfo.channels==1)
        st_warn("PAN onto a mono channel...");

    if (effp->outinfo.rate != effp->ininfo.rate)
    {
        st_fail("PAN cannot handle different rates (in=%ld, out=%ld)"
             " use resample or rate", effp->ininfo.rate, effp->outinfo.rate);
        return ST_EOF;
    }

    return ST_SUCCESS;
}


/* clip value if necessary. see comments about such a function in vol.c
 * Okay, it might be quite slow to have such a function for so small
 * a task. Hopefully the function can be inlined by the compiler?
 */
static st_sample_t clip(pan_t pan, PAN_FLOAT value)
{
    if (value < ST_SAMPLE_MIN) 
    {
        pan->clipped++;
        return ST_SAMPLE_MIN;
    }
    else if (value > ST_SAMPLE_MAX) 
    {
        pan->clipped++;
        return ST_SAMPLE_MAX;
    } /* else */

    return (st_sample_t) value;
}

#ifndef MIN
#define MIN(s1,s2) ((s1)<(s2)?(s1):(s2))
#endif

#define UNEXPECTED_CHANNELS \
    st_fail("unexpected number of channels (in=%d, out=%d)", ich, och); \
    return ST_EOF

/*
 * Process either isamp or osamp samples, whichever is smaller.
 */
int st_pan_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                st_size_t *isamp, st_size_t *osamp)
{
    pan_t pan = (pan_t) effp->priv;
    register st_size_t len;
    st_size_t done;
    char ich, och;
    register PAN_FLOAT left, right, dir, hdir;
    
    dir   = pan->dir;    /* -1   <=  dir  <= 1   */
    hdir  = HALF * dir;  /* -0.5 <=  hdir <= 0.5 */
    left  = HALF - hdir; /*  0   <=  left <= 1   */
    right = HALF + hdir; /*  0   <= right <= 1   */

    ich = effp->ininfo.channels;
    och = effp->outinfo.channels;

    len = MIN(*osamp/och,*isamp/ich);

    /* report back how much is processed. */
    *isamp = len*ich;
    *osamp = len*och;
    
    /* 9 different cases to handle: (1,2,4) X (1,2,4) */
    switch (och) {
    case 1: /* pan on mono channel... not much sense. just avg. */
        switch (ich) {
        case 1: /* simple copy */
            for (done=0; done<len; done++)
                *obuf++ = *ibuf++;
            break;
        case 2: /* average 2 */
            for (done=0; done<len; done++)
                *obuf++ = clip(pan, HALF*ibuf[0] + HALF*ibuf[1]),
                    ibuf += 2;
            break;
        case 4: /* average 4 */
            for (done=0; done<len; done++)
                *obuf++ = clip(pan, 
                               QUARTER*ibuf[0] + QUARTER*ibuf[1] + 
                               QUARTER*ibuf[2] + QUARTER*ibuf[3]),
                    ibuf += 4;
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
                obuf[0] = clip(pan, left * ibuf[0]);
                obuf[1] = clip(pan, right * ibuf[0]);
                obuf += 2;
                ibuf++;
            }
            break;
        case 2: /* linear panorama. 
                 * I'm not sure this is the right way to do it.
                 */
            if (dir <= ZERO) /* to the left */
            {
                register PAN_FLOAT volume, cll, clr, cr;

                volume = ONE - HALF*dir;
                cll = volume*(ONEHALF-left);
                clr = volume*(left-HALF);
                cr  = volume*(ONE+dir);

                for (done=0; done<len; done++)
                {
                    obuf[0] = clip(pan, cll * ibuf[0] + clr * ibuf[1]);
                    obuf[1] = clip(pan, cr * ibuf[1]);
                    obuf += 2;
                    ibuf += 2;
                }
            }
            else /* to the right */
            {
                register PAN_FLOAT volume, cl, crl, crr;

                volume = ONE + HALF*dir;
                cl  = volume*(ONE-dir);
                crl = volume*(right-HALF);
                crr = volume*(ONEHALF-right);

                for (done=0; done<len; done++)
                {
                    obuf[0] = clip(pan, cl * ibuf[0]);
                    obuf[1] = clip(pan, crl * ibuf[0] + crr * ibuf[1]);
                    obuf += 2;
                    ibuf += 2;
                }
            }
            break;
        case 4:
            if (dir <= ZERO) /* to the left */
            {
                register PAN_FLOAT volume, cll, clr, cr;

                volume = ONE - HALF*dir;
                cll = volume*(ONEHALF-left);
                clr = volume*(left-HALF);
                cr  = volume*(ONE+dir);

                for (done=0; done<len; done++)
                {
                    register PAN_FLOAT ibuf0, ibuf1;

                    /* build stereo signal */
                    ibuf0 = HALF*ibuf[0] + HALF*ibuf[2];
                    ibuf1 = HALF*ibuf[1] + HALF*ibuf[3];

                    /* pan it */
                    obuf[0] = clip(pan, cll * ibuf0 + clr * ibuf1);
                    obuf[1] = clip(pan, cr * ibuf1);
                    obuf += 2;
                    ibuf += 4;
                }
            }
            else /* to the right */
            {
                register PAN_FLOAT volume, cl, crl, crr;

                volume = ONE + HALF*dir;
                cl  = volume*(ONE-dir);
                crl = volume*(right-HALF);
                crr = volume*(ONEHALF-right);

                for (done=0; done<len; done++)
                {
                    register PAN_FLOAT ibuf0, ibuf1;

                    ibuf0 = HALF*ibuf[0] + HALF*ibuf[2];
                    ibuf1 = HALF*ibuf[1] + HALF*ibuf[3];

                    obuf[0] = clip(pan, cl * ibuf0);
                    obuf[1] = clip(pan, crl * ibuf0 + crr * ibuf1);
                    obuf += 2;
                    ibuf += 4;
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
                register PAN_FLOAT cr, cl;

                cl = HALF*left;
                cr = HALF*right;

                for (done=0; done<len; done++)
                {
                    obuf[0] = clip(pan, cl * ibuf[0]);
                    obuf[2] = obuf[0];
                    obuf[1] = clip(pan, cr * ibuf[0]);
                    ibuf[3] = obuf[1];
                    obuf += 4;
                    ibuf++;
                }
            }
            break;
        case 2: /* simple linear panorama */
            if (dir <= ZERO) /* to the left */
            {
                register PAN_FLOAT volume, cll, clr, cr;

                volume = HALF - QUARTER*dir;
                cll = volume * (ONEHALF-left);
                clr = volume * (left-HALF);
                cr  = volume * (ONE+dir);

                for (done=0; done<len; done++)
                {
                    obuf[0] = clip(pan, cll * ibuf[0] + clr * ibuf[1]);
                    obuf[2] = obuf[0];
                    obuf[1] = clip(pan, cr * ibuf[1]);
                    ibuf[3] = obuf[1];
                    obuf += 4;
                    ibuf += 2;
                }
            }
            else /* to the right */
            {
                register PAN_FLOAT volume, cl, crl, crr;

                volume = HALF + QUARTER*dir;
                cl  = volume * (ONE-dir);
                crl = volume * (right-HALF);
                crr = volume * (ONEHALF-right);

                for (done=0; done<len; done++)
                {
                    obuf[0] = clip(pan, cl * ibuf[0]);
                    obuf[2] = obuf[0];
                    obuf[1] = clip(pan, crl * ibuf[0] + crr * ibuf[1]);
                    ibuf[3] = obuf[1];
                    obuf += 4;
                    ibuf += 2;
                }
            }
            break;
        case 4:
            /* maybe I could improve the formula to reverse...
               also, turn only by quarters.
             */
            if (dir <= ZERO) /* to the left */
            {
                register PAN_FLOAT cown, cright;

                cright = -dir;
                cown = ONE + dir;

                for (done=0; done<len; done++)
                {
                    obuf[0] = clip(pan, cown*ibuf[0] + cright*ibuf[1]);
                    obuf[1] = clip(pan, cown*ibuf[1] + cright*ibuf[3]);
                    obuf[2] = clip(pan, cown*ibuf[2] + cright*ibuf[0]);
                    obuf[3] = clip(pan, cown*ibuf[3] + cright*ibuf[2]);
                    obuf += 4;
                    ibuf += 4;              
                }
            }
            else /* to the right */
            {
                register PAN_FLOAT cleft, cown;

                cleft = dir;
                cown = ONE - dir;

                for (done=0; done<len; done++)
                {
                    obuf[0] = clip(pan, cleft*ibuf[2] + cown*ibuf[0]);
                    obuf[1] = clip(pan, cleft*ibuf[0] + cown*ibuf[1]);
                    obuf[2] = clip(pan, cleft*ibuf[3] + cown*ibuf[2]);
                    obuf[3] = clip(pan, cleft*ibuf[1] + cown*ibuf[3]);
                    obuf += 4;
                    ibuf += 4;
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

    return ST_SUCCESS;
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 *
 * Should have statistics on right, left, and output amplitudes.
 */
int st_pan_stop(eff_t effp)
{
    pan_t pan = (pan_t) effp->priv;
    if (pan->clipped) {
        st_warn("PAN clipped %d values, maybe adjust volume?",
             pan->clipped);
    }
    return ST_SUCCESS;
}
