
/*
 * July 5, 1991
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
 * Sound Tools stereo/quad -> mono mixdown effect file.
 * and mono/stereo -> stereo/quad channel duplication.
 *
 * What's in a center channel?
 */

#include "st_i.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/* Private data for SKEL file */
typedef struct avgstuff {
	/* How to generate each output channel.  sources[i][j] */
	/* represents the fraction of channel i that should be passed */
	/* through to channel j on output, and so forth.  Channel 0 is */
	/* left front, channel 1 is right front, and 2 and 3 are left */
	/* and right rear, respectively. (GHK) */
	double	sources[4][4];
	int	num_pans;
	int	mix;			/* How are we mixing it? */
} *avg_t;

#define MIX_CENTER	0
#define MIX_LEFT	1
#define MIX_RIGHT	2
#define MIX_FRONT	3
#define MIX_BACK	4
#define MIX_SPECIFIED	5

#define CLIP_LEVEL	((double)(((unsigned)1 << 31) - 1))

/*
 * Process options
 */
int st_avg_getopts(eff_t effp, int n, char **argv) 
{
    avg_t avg = (avg_t) effp->priv;
    double* pans = &avg->sources[0][0];
    int i;

    for (i = 0;  i < 16;  i++)
	pans[i] = 0.0;
    avg->mix = MIX_CENTER;
    avg->num_pans = 0;

    /* Parse parameters.  Since we don't yet know the number of */
    /* input and output channels, we'll record the information for */
    /* later. */
    if (n) {
	if(!strcmp(argv[0], "-l"))
	    avg->mix = MIX_LEFT;
	else if (!strcmp(argv[0], "-r"))
	    avg->mix = MIX_RIGHT;
	else if (!strcmp(argv[0], "-f"))
	    avg->mix = MIX_FRONT;
	else if (!strcmp(argv[0], "-b"))
	    avg->mix = MIX_BACK;
	else if (argv[0][0] == '-' && !isdigit((int)argv[0][1])
		&& argv[0][1] != '.') {
	    st_fail("Usage: avg [ -l | -r | -f | -b | n,n,n...,n ]");
	    return (ST_EOF);
	}
	else {
	    int commas;
	    char *s;
	    avg->mix = MIX_SPECIFIED;
	    pans[0] = atof(argv[0]);
	    for (s = argv[0], commas = 0; *s; ++s) {
		if (*s == ',') {
		    ++commas;
		    if (commas >= 16) {
			st_fail("avg can only take up to 16 pan values");
			return (ST_EOF);
		    }
		    pans[commas] = atof(s+1);
		}
	    }
	    avg->num_pans = commas + 1;
	}
    }
    else {
	pans[0] = 0.5;
	pans[1] = 0.5;
	avg->num_pans = 2;
    }
    return (ST_SUCCESS);
}

/*
 * Start processing
 */
int st_avg_start(eff_t effp)
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

       The interesting ones from above (deserving of abbreviations of
       less than 16 numbers) are:

       n->n volume change (1 number)
       1->n duplication (0 numbers)
       2->1 mixdown (0 or 2 numbers)
       2->2 balance (1 number)
       2->2 fully general mix (4 numbers)
       2->4 duplication (0 numbers)
       4->1 mixdown (0 or 4 numbers)
       4->2 mixdown (0 or 2 numbers)
       4->4 balance (1 or 2 numbers)

       The above has one ambiguity: n->n volume change conflicts with
       n->n balance for n != 1.  In such a case, we'll prefer
       balance, since there is already a volume effect in vol.c.

       GHK 2000/11/28
     */
     avg_t avg = (avg_t) effp->priv;
     double pans[16];
     int i;
     int ichan, ochan;

     for (i = 0;  i < 16;  i++)
         pans[i] = ((double*)&avg->sources[0][0])[i];

     ichan = effp->ininfo.channels;
     ochan = effp->outinfo.channels;
     if (ochan == -1) {
         st_fail("Output must have known number of channels to use avg effect");
         return(ST_EOF);
     }

     if ((ichan != 1 && ichan != 2 && ichan != 4)
             ||  (ochan != 1 && ochan != 2 && ochan != 4)) {
         st_fail("Can't average %d channels into %d channels",
                 ichan, ochan);
         return (ST_EOF);
     }

     /* Handle the special-case flags */
     switch (avg->mix) {
         case MIX_CENTER:
             if (ichan == ochan) {
                 st_fail("Output must have different number of channels to use avg effect");
                 return(ST_EOF);
             }
             break;		/* Code below will handle this case */
         case MIX_LEFT:
             if (ichan < 2) {
                 st_fail("Input must have at least two channels to use avg -l");
                 return(ST_EOF);
             }
             pans[0] = 1.0;
             pans[1] = 0.0;
             avg->num_pans = 2;
             break;
         case MIX_RIGHT:
             if (ichan < 2) {
                 st_fail("Input must have at least two channels to use avg -r");
                 return(ST_EOF);
             }
             pans[0] = 0.0;
             pans[1] = 1.0;
             avg->num_pans = 2;
             break;
         case MIX_FRONT:
             if (ichan < 4) {
                 st_fail("Input must have at four channels to use avg -f");
                 return(ST_EOF);
             }
             pans[0] = 1.0;
             pans[1] = 0.0;
             avg->num_pans = 2;
             break;
         case MIX_BACK:
             if (ichan < 4) {
                 st_fail("Input must have at four channels to use avg -b");
                 return(ST_EOF);
             }
             pans[0] = 0.0;
             pans[1] = 1.0;
             avg->num_pans = 2;
             break;
         default:
             break;
     }

     /* If the number of pans given is 4 or fewer, handle the special */
     /* cases listed in the comments above.  The code is lengthy but */
     /* straightforward. */
     if (avg->num_pans == 0) {
         if (ichan == 1) {
             avg->sources[0][0] = 1.0;
             avg->sources[0][1] = 1.0;
             avg->sources[0][2] = 1.0;
             avg->sources[0][3] = 1.0;
         }
         else if (ichan == 2 && ochan == 1) {
             avg->sources[0][0] = 0.5;
             avg->sources[1][0] = 0.5;
         }
         else if (ichan == 2 && ochan == 4) {
             avg->sources[0][0] = 1.0;
             avg->sources[0][2] = 1.0;
             avg->sources[1][1] = 1.0;
             avg->sources[1][3] = 1.0;
         }
         else if (ichan == 4 && ochan == 1) {
             avg->sources[0][0] = 0.25;
             avg->sources[1][0] = 0.25;
             avg->sources[2][0] = 0.25;
             avg->sources[3][0] = 0.25;
         }
         else if (ichan == 4 && ochan == 2) {
             avg->sources[0][0] = 0.5;
             avg->sources[1][1] = 0.5;
             avg->sources[2][0] = 0.5;
             avg->sources[3][1] = 0.5;
         }
         else {
             st_fail("You must specify at least one mix level when using avg with an unusual number of channels.");
             return(ST_EOF);
         }
     }
     else if (avg->num_pans == 1) {
         /* Might be volume change or balance change */
         if ((ichan == 2 || ichan == 4) &&  ichan == ochan) {
             /* -1 is left only, 1 is right only */
             if (avg->sources[0][0] <= 0.0) {
                 avg->sources[1][1] = pans[0] + 1.0;
                 if (avg->sources[1][1] < 0.0)
                     avg->sources[1][1] = 0.0;
                 avg->sources[0][0] = 1.0;
             }
             else {
                 avg->sources[0][0] = 1.0 - pans[0];
                 if (avg->sources[0][0] < 0.0)
                     avg->sources[0][0] = 0.0;
                 avg->sources[1][1] = 1.0;
             }
             if (ichan == 4) {
                 avg->sources[2][2] = avg->sources[0][0];
                 avg->sources[3][3] = avg->sources[1][1];
             }
         }
     }
     else if (avg->num_pans == 2) {
         if (ichan == 2 && ochan == 1) {
             avg->sources[0][0] = pans[0];
             avg->sources[0][1] = 0.0;
             avg->sources[1][0] = pans[1];
         }
         else if (ichan == 4 && ochan == 2) {
             avg->sources[0][0] = pans[0];
             avg->sources[0][1] = 0.0;
             avg->sources[1][1] = pans[0];
             avg->sources[2][0] = pans[1];
             avg->sources[3][1] = pans[1];
         }
         else if (ichan == 4 && ochan == 4) {
             /* pans[0] is front -> front, pans[1] is for back */
             avg->sources[0][0] = pans[0];
             avg->sources[0][1] = 0.0;
             avg->sources[1][1] = pans[0];
             avg->sources[2][2] = pans[1];
             avg->sources[3][3] = pans[1];
         }
     }
     else if (avg->num_pans == 4) {
         if (ichan == 2 && ochan == 2) {
             /* Shorthand for 2-channel case */
             avg->sources[0][0] = pans[0];
             avg->sources[0][1] = pans[1];
             avg->sources[0][2] = 0.0;
             avg->sources[0][3] = 0.0;
             avg->sources[1][0] = pans[2];
             avg->sources[1][1] = pans[3];
         }
         else if (ichan == 4 && ochan == 1) {
             avg->sources[0][0] = pans[0];
             avg->sources[0][1] = 0.0;
             avg->sources[0][2] = 0.0;
             avg->sources[0][3] = 0.0;
             avg->sources[1][0] = pans[1];
             avg->sources[2][0] = pans[2];
             avg->sources[3][0] = pans[3];
         }
     }
     return (ST_SUCCESS);
}

/*
 * Process either isamp or osamp samples, whichever is smaller.
 */

int st_avg_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                st_size_t *isamp, st_size_t *osamp)
{
    avg_t avg = (avg_t) effp->priv;
    int len, done;
    int ichan, ochan;
    int i, j;
    double samp;

    ichan = effp->ininfo.channels;
    ochan = effp->outinfo.channels;
    len = *isamp / ichan;
    if (len > *osamp / ochan)
        len = *osamp / ochan;
    for (done = 0; done < len; done++, ibuf += ichan, obuf += ochan) {
        for (j = 0; j < ochan; j++) {
            samp = 0.0;
            for (i = 0; i < ichan; i++)
                samp += ibuf[i] * avg->sources[i][j];
            if (samp < -CLIP_LEVEL)
                samp = -CLIP_LEVEL;
            else if (samp > CLIP_LEVEL)
                samp = CLIP_LEVEL;
            obuf[j] = samp;
        }
    }
    *isamp = len * ichan;
    *osamp = len * ochan;
    return (ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 *
 * Should have statistics on right, left, and output amplitudes.
 */
int st_avg_stop(eff_t effp)
{
    return (ST_SUCCESS); /* nothing to do */
}

