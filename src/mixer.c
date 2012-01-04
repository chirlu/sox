/* July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 *
 * Channel duplication code by Graeme W. Gill - 93/5/18
 * General-purpose panning by Geoffrey H. Kuenning -- 2000/11/28
 */

/*
 * libSoX stereo/quad -> mono mixdown effect file.
 * and mono/stereo -> stereo/quad channel duplication.
 */

#include "sox_i.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
        /* How to generate each output channel.  sources[i][j] */
        /* represents the fraction of channel i that should be passed */
        /* through to channel j on output, and so forth.  Channel 0 is */
        /* left front, channel 1 is right front, and 2 and 3 are left */
        /* and right rear, respectively. (GHK) */
        double  sources[4][4];
        int     num_pans;
        int     mix;                    /* How are we mixing it? */
} priv_t;

/* MIX_CENTER is shorthand to mix channels together at 50% each */
#define MIX_CENTER      0
#define MIX_SPECIFIED   1


/*
 * Process options
 */
static int getopts(sox_effect_t * effp, int argc, char **argv)
{
    priv_t * mixer = (priv_t *) effp->priv;
    double* pans = &mixer->sources[0][0];
    int i;
  --argc, ++argv;

    for (i = 0;  i < 16;  i++)
        pans[i] = 0.0;
    mixer->mix = MIX_CENTER;
    mixer->num_pans = 0;

    /* Parse parameters.  Since we don't yet know the number of */
    /* input and output channels, we'll record the information for */
    /* later. */
    if (argc == 1) {
        if      (!strcmp(argv[0], "-l")) mixer->mix = 'l';
        else if (!strcmp(argv[0], "-r")) mixer->mix = 'r';
        else if (!strcmp(argv[0], "-f")) mixer->mix = 'f';
        else if (!strcmp(argv[0], "-b")) mixer->mix = 'b';
        else if (!strcmp(argv[0], "-1")) mixer->mix = '1';
        else if (!strcmp(argv[0], "-2")) mixer->mix = '2';
        else if (!strcmp(argv[0], "-3")) mixer->mix = '3';
        else if (!strcmp(argv[0], "-4")) mixer->mix = '4';
        else if (argv[0][0] == '-' && !isdigit((int)argv[0][1])
                && argv[0][1] != '.')
          return lsx_usage(effp);
        else {
            int commas;
            char *s;
            mixer->mix = MIX_SPECIFIED;
            pans[0] = atof(argv[0]);
            for (s = argv[0], commas = 0; *s; ++s) {
                if (*s == ',') {
                    ++commas;
                    if (commas >= 16) {
                        lsx_fail("mixer can only take up to 16 pan values");
                        return (SOX_EOF);
                    }
                    pans[commas] = atof(s+1);
                }
            }
            mixer->num_pans = commas + 1;
        }
    }
    else if (argc == 0) {
        mixer->mix = MIX_CENTER;
    }
    else
      return lsx_usage(effp);

    return (SOX_SUCCESS);
}

/*
 * Start processing
 */
static int start(sox_effect_t * effp)
{
    /*
       Hmmm, this is tricky.  Lemme think:
       channel orders are [0][0],[0][1], etc.
       i.e., 0->0, 0->1, 0->2, 0->3, 1->0, 1->1, ...
       trailing zeros are omitted
       L/R balance is x= -1 for left only, 1 for right only
       1->1 channel effects:
       changing volume by x is x,0,0,0
       1->2 channel effects:
       duplicating everywhere is 1,1,0,0
       1->4 channel effects:
       duplicating everywhere is 1,1,1,1
       2->1 channel effects:
       left only is 1,0,0,0 0,0,0,0
       right only is 0,0,0,0 1,0,0,0
       left+right is 0.5,0,0,0 0.5,0,0,0
       left-right is 1,0,0,0 -1,0,0,0
       2->2 channel effects:
       L/R balance can be done several ways.  The standard stereo
       way is both the easiest and the most sensible:
       min(1-x,1),0,0,0 0,min(1+x,1),0,0
       left to both is 1,1,0,0
       right to both is 0,0,0,0 1,1,0,0
       left+right to both is 0.5,0.5,0,0 0.5,0.5,0,0
       left-right to both is 1,1,0,0 -1,-1,0,0
       left-right to left, right-left to right is 1,-1,0,0 -1,1,0,0
       2->4 channel effects:
       front duplicated into rear is 1,0,1,0 0,1,0,1
       front swapped into rear (why?) is 1,0,0,1 0,1,1,0
       front put into rear as mono (why?) is 1,0,0.5,0.5 0,1,0.5,0.5
       4->1 channel effects:
       left front only is 1,0,0,0
       left only is 0.5,0,0,0 0,0,0,0 0.5,0,0,0
       etc.
       4->2 channel effects:
       merge front/back is 0.5,0,0,0 0,0.5,0,0 0.5,0,0,0 0,0.5,0,0
       selections similar to above
       4->4 channel effects:
       left front to all is 1,1,1,1 0,0,0,0
       right front to all is 0,0,0,0 1,1,1,1
       left f/r to all f/r is 1,1,0,0 0,0,0,0 0,0,1,1 0,0,0,0
       etc.

       The interesting cases from above (deserving of abbreviations of
       less than 16 numbers) are:

       0) n->n volume change (1 number)
       1) 1->n duplication (0 numbers)
       2) 2->1 mixdown (0 or 2 numbers)
       3) 2->2 balance (1 number)
       4) 2->2 fully general mix (4 numbers)
       5) 2->4 duplication (0 numbers)
       6) 4->1 mixdown (0 or 4 numbers)
       7) 4->2 mixdown (0, or 2 numbers)
       8) 4->4 balance (1 or 2 numbers)

       The above has one ambiguity: n->n volume change conflicts with
       n->n balance for n != 1.  In such a case, we'll prefer
       balance, since there is already a volume effect in vol.c.

       GHK 2000/11/28
     */
     priv_t * mixer = (priv_t *) effp->priv;
     double pans[16];
     int i, j;
     int ichan, ochan;

     for (i = 0;  i < 16;  i++)
         pans[i] = ((double*)&mixer->sources[0][0])[i];

     ichan = effp->in_signal.channels;
     ochan = effp->out_signal.channels;
     if (ochan == -1) {
         lsx_fail("Output must have known number of channels");
         return(SOX_EOF);
     }

     if ((ichan != 1 && ichan != 2 && ichan != 4 &&
          mixer->mix != MIX_CENTER && ochan != 1)
             ||  (ochan != 1 && ochan != 2 && ochan != 4)) {
         lsx_fail("Can't mix %d -> %d channels", ichan, ochan);
         return (SOX_EOF);
     }

     /* Handle the special-case flags */
     switch (mixer->mix) {
         case MIX_CENTER:
             if (ichan == ochan)
               return SOX_EFF_NULL;
             break;             /* Code below will handle this case */
         case 'l':
             if (ichan == 2 && ochan == 1)
             {
                 pans[0] = 1.0;
                 pans[1] = 0.0;
                 mixer->num_pans = 2;
             }
             else if (ichan == 4 && ochan == 1)
             {
                 pans[0] = 0.5;
                 pans[1] = 0.0;
                 pans[2] = 0.5;
                 pans[3] = 0.0;
                 mixer->num_pans = 4;
             }
             else
             {
                 lsx_fail("Can't mix -%c %d -> %d channels", mixer->mix, ichan, ochan);
                 return SOX_EOF;
             }
             break;
         case 'r':
             if (ichan == 2 && ochan == 1)
             {
                 pans[0] = 0.0;
                 pans[1] = 1.0;
                 mixer->num_pans = 2;
             }
             else if (ichan == 4 && ochan == 1)
             {
                 pans[0] = 0.0;
                 pans[1] = 0.5;
                 pans[2] = 0.0;
                 pans[3] = 0.5;
                 mixer->num_pans = 4;
             }
             else
             {
                 lsx_fail("Can't mix -%c %d -> %d channels", mixer->mix, ichan, ochan);
                 return SOX_EOF;
             }
             break;
         case 'f':
             if (ichan == 4 && ochan == 2)
             {
                 pans[0] = 1.0;
                 pans[1] = 0.0;
                 mixer->num_pans = 2;
             }
             else if (ichan == 4 && ochan == 1)
             {
                 pans[0] = 0.5;
                 pans[1] = 0.5;
                 pans[2] = 0.0;
                 pans[3] = 0.0;
                 mixer->num_pans = 4;
             }
             else
             {
                 lsx_fail("Can't mix -%c %d -> %d channels", mixer->mix, ichan, ochan);
                 return SOX_EOF;
             }
             break;
         case 'b':
             if (ichan == 4 && ochan == 2)
             {
                 pans[0] = 0.0;
                 pans[1] = 1.0;
                 mixer->num_pans = 2;
             }
             else if (ichan == 4 && ochan == 1)
             {
                 pans[0] = 0.0;
                 pans[1] = 0.0;
                 pans[2] = 0.5;
                 pans[3] = 0.5;
                 mixer->num_pans = 4;
             }
             else
             {
                 lsx_fail("Can't mix -%c %d -> %d channels", mixer->mix, ichan, ochan);
                 return SOX_EOF;
             }
             break;
         case '1':
             if (ichan == 2 && ochan == 1)
             {
                 pans[0] = 1.0;
                 pans[1] = 0.0;
                 mixer->num_pans = 2;
             }
             else if (ichan == 4 && ochan == 1)
             {
                 pans[0] = 1.0;
                 pans[1] = 0.0;
                 pans[2] = 0.0;
                 pans[3] = 0.0;
                 mixer->num_pans = 4;
             }
             else
             {
                 lsx_fail("Can't mix -%c %d -> %d channels", mixer->mix, ichan, ochan);
                 return SOX_EOF;
             }
             break;
         case '2':
             if (ichan == 2 && ochan == 1)
             {
                 pans[0] = 0.0;
                 pans[1] = 1.0;
                 mixer->num_pans = 2;
             }
             else if (ichan == 4 && ochan == 1)
             {
                 pans[0] = 0.0;
                 pans[1] = 1.0;
                 pans[2] = 0.0;
                 pans[3] = 0.0;
                 mixer->num_pans = 4;
             }
             else
             {
                 lsx_fail("Can't mix -%c %d -> %d channels", mixer->mix, ichan, ochan);
                 return SOX_EOF;
             }
             break;

         case '3':
             if (ichan == 4 && ochan == 1)
             {
                 pans[0] = 0.0;
                 pans[1] = 0.0;
                 pans[2] = 1.0;
                 pans[3] = 0.0;
                 mixer->num_pans = 4;
             }
             else
             {
                 lsx_fail("Can't mix -%c %d -> %d channels", mixer->mix, ichan, ochan);
                 return SOX_EOF;
             }
             break;

         case '4':
             if (ichan == 4 && ochan == 1)
             {
                 pans[0] = 0.0;
                 pans[1] = 0.0;
                 pans[2] = 0.0;
                 pans[3] = 1.0;
                 mixer->num_pans = 4;
             }
             else
             {
                 lsx_fail("Can't mix -%c %d -> %d channels", mixer->mix, ichan, ochan);
                 return SOX_EOF;
             }
             break;

         case MIX_SPECIFIED:
             break;
         default:
             lsx_fail("Unknown mix option");
             return SOX_EOF;
     }

     /* If number of pans is 4 or less then its a shorthand
      * representation.  If user specified it, then we have
      * garbage in our sources[][] array.  Need to clear that
      * now that all data is stored in pans[] array.
      */
     if (mixer->num_pans <= 4)
     {
         for (i = 0; i < ichan; i++)
         {
             for (j = 0; j < ochan; j++)
             {
                 mixer->sources[i][j] = 0;
             }
         }
     }

     /* If the number of pans given is 4 or fewer, handle the special */
     /* cases listed in the comments above.  The code is lengthy but */
     /* straightforward. */
     if (mixer->num_pans == 0) {
         /* CASE 1 */
         if (ichan == 1 && ochan > ichan) {
             mixer->sources[0][0] = 1.0;
             mixer->sources[0][1] = 1.0;
             mixer->sources[0][2] = 1.0;
             mixer->sources[0][3] = 1.0;
         }
         /* CASE 2, 6 */
         else if (ochan == 1) {
             mixer->sources[0][0] = 1.0 / ichan;
         }
         /* CASE 5 */
         else if (ichan == 2 && ochan == 4) {
             mixer->sources[0][0] = 1.0;
             mixer->sources[0][2] = 1.0;
             mixer->sources[1][1] = 1.0;
             mixer->sources[1][3] = 1.0;
         }
         /* CASE 7 */
         else if (ichan == 4 && ochan == 2) {
             mixer->sources[0][0] = 0.5;
             mixer->sources[1][1] = 0.5;
             mixer->sources[2][0] = 0.5;
             mixer->sources[3][1] = 0.5;
         }
         else {
             lsx_fail("You must specify at least one mix level when using mixer with an unusual number of channels.");
             return(SOX_EOF);
         }
     }
     else if (mixer->num_pans == 1) {
         /* Might be volume change or balance change */
         /* CASE 3 and CASE 8 */
         if ((ichan == 2 || ichan == 4) &&  ichan == ochan) {
             /* -1 is left only, 1 is right only */
             if (pans[0] <= 0.0) {
                 mixer->sources[1][1] = pans[0] + 1.0;
                 if (mixer->sources[1][1] < 0.0)
                     mixer->sources[1][1] = 0.0;
                 mixer->sources[0][0] = 1.0;
             }
             else {
                 mixer->sources[0][0] = 1.0 - pans[0];
                 if (mixer->sources[0][0] < 0.0)
                     mixer->sources[0][0] = 0.0;
                 mixer->sources[1][1] = 1.0;
             }
             if (ichan == 4) {
                 mixer->sources[2][2] = mixer->sources[0][0];
                 mixer->sources[3][3] = mixer->sources[1][1];
             }
         }
         else
         {
             lsx_fail("Invalid options while not mixing");
             return SOX_EOF;
         }
     }
     else if (mixer->num_pans == 2) {
         /* CASE 2 */
         if (ichan == 2 && ochan == 1) {
             mixer->sources[0][0] = pans[0];
             mixer->sources[1][0] = pans[1];
         }
         /* CASE 7 */
         else if (ichan == 4 && ochan == 2) {
             mixer->sources[0][0] = pans[0];
             mixer->sources[1][1] = pans[0];
             mixer->sources[2][0] = pans[1];
             mixer->sources[3][1] = pans[1];
         }
         /* CASE 8 */
         else if (ichan == 4 && ochan == 4) {
             /* pans[0] is front -> front, pans[1] is for back */
             mixer->sources[0][0] = pans[0];
             mixer->sources[1][1] = pans[0];
             mixer->sources[2][2] = pans[1];
             mixer->sources[3][3] = pans[1];
         }
         else
         {
             lsx_fail("Invalid options for this channel combination");
             return SOX_EOF;
         }
     }
     else if (mixer->num_pans == 3) {
       lsx_fail("Invalid options while not mixing");
       return SOX_EOF;
     }
     else if (mixer->num_pans == 4) {
         /* CASE 4 */
         if (ichan == 2 && ochan == 2) {
             /* Shorthand for 2-channel case */
             mixer->sources[0][0] = pans[0];
             mixer->sources[0][1] = pans[1];
             mixer->sources[1][0] = pans[2];
             mixer->sources[1][1] = pans[3];
         }
         /* CASE 6 */
         else if (ichan == 4 && ochan == 1) {
             mixer->sources[0][0] = pans[0];
             mixer->sources[1][0] = pans[1];
             mixer->sources[2][0] = pans[2];
             mixer->sources[3][0] = pans[3];
         }
         else
         {
             lsx_fail("Invalid options for this channel combination");
             return SOX_EOF;
         }
     }

     if (effp->in_signal.mult) {
       double max_sum = 0;

       for (j = 0; j < (int)effp->out_signal.channels; ++j) {
         double sum = 0;
         for (i = 0; i < (int)effp->in_signal.channels; ++i)
           sum += fabs(mixer->sources[mixer->mix == MIX_CENTER? 0 : i][j]);
         max_sum = max(max_sum, sum);
       }
       if (max_sum > 1)
         *effp->in_signal.mult /= max_sum;
     }

     if (effp->in_signal.channels != effp->out_signal.channels)
       return SOX_SUCCESS;

     for (i = 0; i < (int)effp->in_signal.channels; ++i)
       for (j = 0; j < (int)effp->out_signal.channels; ++j)
         if (mixer->sources[i][j] != (i == j))
           return SOX_SUCCESS;

     return SOX_EFF_NULL;
}

/*
 * Process either isamp or osamp samples, whichever is smaller.
 */

static int flow(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf,
                size_t *isamp, size_t *osamp)
{
    priv_t * mixer = (priv_t *) effp->priv;
    size_t len, done;
    int ichan, ochan;
    int i, j;
    double samp;

    ichan = effp->in_signal.channels;
    ochan = effp->out_signal.channels;
    len = *isamp / ichan;
    if (len > *osamp / ochan)
        len = *osamp / ochan;
    for (done = 0; done < len; done++, ibuf += ichan, obuf += ochan) {
        for (j = 0; j < ochan; j++) {
            samp = 0.0;
            for (i = 0; i < ichan; i++)
                samp += ibuf[i] * mixer->sources[mixer->mix == MIX_CENTER? 0 : i][j];
            SOX_SAMPLE_CLIP_COUNT(samp, effp->clips);
            obuf[j] = samp;
        }
    }
    *isamp = len * ichan;
    *osamp = len * ochan;
    return (SOX_SUCCESS);
}

sox_effect_handler_t const * lsx_mixer_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "mixer",
    "[ -l | -r | -f | -b | -1 | -2 | -3 | -4 | n,n,n...,n ]",
    SOX_EFF_MCHAN | SOX_EFF_CHAN | SOX_EFF_GAIN | SOX_EFF_DEPRECATED,
    getopts, start, flow, 0, 0, 0, sizeof(priv_t)
  };
  return &handler;
}
