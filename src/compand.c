/*
 * Compander effect
 *
 * Written by Nick Bailey (nick@polonius.demon.co.uk or
 * N.J.Bailey@leeds.ac.uk).  Hope page for this effect:
 * http://www.ee.keeds.ac.uk/homes/NJB/Softwere/Compand/compand.html
 *
 * Copyright 1999 Chris Bagwell And Nick Bailey
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Chris Bagwell And Nick Bailey are not responsible for 
 * the consequences of using this software.
 */

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "st.h"

/* Private data for SKEL file */
typedef struct {
  int expectedChannels; /* Also flags that channels aren't to be treated
			   individually when = 1 and input not mono */
  int transferPoints;   /* Number of points specified on the transfer
			   function */
  double *attackRate;   /* An array of attack rates */
  double *decayRate;    /*    ... and of decay rates */
  double *transferIns;  /*    ... and points on the transfer function */
  double *transferOuts;
  double *volume;       /* Current "volume" of each channel */
  LONG   *lastSamp;     /* Remeber the value of the previous sample */
  double outgain;       /* Post processor gain */
} *compand_t;

/*
 * Process options
 *
 * Don't do initialization now.
 * The 'info' fields are not yet filled in.
 */
int st_compand_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
    compand_t l = (compand_t) effp->priv;

    if (n < 2 || n > 4)
    {
      st_fail("Wrong number of arguments for the compander effect\n"
	   "Use: {<attack_time>,<decay_time>}+ {<dB_in>,<db_out>}+ "
	   "[<dB_postamp>]\n"
	   "where {}+ means `one or more in a comma-separated, "
	   "white-space-free list'\n"
	   "and [] indications possible omission.  dB values are floating\n"
	   "point or `-inf'; times are in seconds.");
      return (ST_EOF);
    }
    else { /* Right no. of args, but are they well formed? */
      char *s;
      int rates, tfers, i, commas;

      /* Start by checking the attack and decay rates */

      for (s = argv[0], commas = 0; *s; ++s)
	if (*s == ',') ++commas;

      if (commas % 2 == 0) /* There must be an even number of
			      attack/decay parameters */
      {
	st_fail("compander: Odd number of attack & decay rate parameters");
	return (ST_EOF);
      }

      rates = 1 + commas/2;
      if ((l->attackRate = malloc(sizeof(double) * rates)) == NULL ||
	  (l->decayRate  = malloc(sizeof(double) * rates)) == NULL ||
	  (l->volume     = malloc(sizeof(double) * rates)) == NULL ||
	  (l->lastSamp   = calloc(rates, sizeof(LONG)))    == NULL)
      {
	st_fail("Out of memory");
	return (ST_EOF);
      }
      l->expectedChannels = rates;

      /* Now tokenise the rates string and set up these arrays.  Keep
	 them in seconds at the moment: we don't know the sample rate yet. */

      s = strtok(argv[0], ","); i = 0;
      do {
	l->attackRate[i] = atof(s); s = strtok(NULL, ",");
	l->decayRate[i]  = atof(s); s = strtok(NULL, ",");
	++i;
      } while (s != NULL);

      /* Same business, but this time for the transfer function */

      for (s = argv[1], commas = 0; *s; ++s)
	if (*s == ',') ++commas;

      if (commas % 2 == 0) /* There must be an even number of
			      transfer parameters */
      {
	st_fail("compander: Odd number of transfer function parameters\n"
	     "Each input value in dB must have a corresponding output value");
	return (ST_EOF);
      }

      tfers = 3 + commas/2; /* 0, 0 at start; 1, 1 at end */
      if ((l->transferIns  = malloc(sizeof(double) * tfers)) == NULL ||
	  (l->transferOuts = malloc(sizeof(double) * tfers)) == NULL)
      {
	st_fail("Out of memory");
	return (ST_EOF);
      }
      l->transferPoints = tfers;
      l->transferIns[0] = 0.0; l->transferOuts[0] = 0.0;
      l->transferIns[tfers-1] = 1.0; l->transferOuts[tfers-1] = 1.0;
      s = strtok(argv[1], ","); i = 1;
      do {
	if (!strcmp(s, "-inf"))
	{
	  st_fail("Input signals of zero level must always generate zero output");
	  return (ST_EOF);
	}
	l->transferIns[i]  = pow(10.0, atof(s)/20.0);
	if (l->transferIns[i] > 1.0)
	{
	  st_fail("dB values are relative to maximum input, and, ipso facto, "
	       "cannot exceed 0");
	  return (ST_EOF);
	}
	if (l->transferIns[i] == 1.0) /* Final point was explicit */
	  --(l->transferPoints);
	if (i > 0 && l->transferIns[i] <= l->transferIns[i-1])
	{
	  st_fail("Transfer function points don't have strictly ascending "
	       "input amplitude");
	  return (ST_EOF);
	}
	s = strtok(NULL, ",");
	l->transferOuts[i] = strcmp(s, "-inf") ?
	                       pow(10.0, atof(s)/20.0) : 0;
	s = strtok(NULL, ",");
	++i;
      } while (s != NULL);
      
      /* If there is a postprocessor gain, store it */
      if (n >= 3) l->outgain = pow(10.0, atof(argv[2])/20.0);
      else l->outgain = 1.0;

      /* Set the initial "volume" to be attibuted to the input channels.
	 Unless specified, choose 1.0 (maximum) otherwise clipping will
	 result if the user has seleced a long attack time */
      for (i = 0; i < l->expectedChannels; ++i) {
	double v = n>=4 ? pow(10.0, atof(argv[3])/20) : 1.0;
	l->volume[i] = v;
      }
    }
    return (ST_SUCCESS);
}

/*
 * Prepare processing.
 * Do all initializations.
 */
int st_compand_start(effp)
eff_t effp;
{
  compand_t l = (compand_t) effp->priv;
  int i;

# ifdef DEBUG
  {
    printf("Starting compand effect\n");
    printf("\nRate %ld, size %d, encoding %d, output gain %g.\n",
	   effp->outinfo.rate, effp->outinfo.size, effp->outinfo.encoding,
	   l->outgain);
    printf("%d input channel(s) expected: actually %d\n",
	   l->expectedChannels, effp->outinfo.channels);
    printf("\nAttack and decay rates\n"
	     "======================\n");
    for (i = 0; i < l->expectedChannels; ++i)
      printf("Channel %d: attack = %-12g decay = %-12g\n",
	     i, l->attackRate[i], l->decayRate[i]);
    printf("\nTransfer function (linear values)\n"
	     "=================  =============\n");
    for (i = 0; i < l->transferPoints; ++i)
      printf("%12g -> %-12g\n",
	     l->transferIns[i], l->transferOuts[i]);
  }
# endif
  
  /* Convert attack and decay rates using number of samples */

  for (i = 0; i < l->expectedChannels; ++i) {
    if (l->attackRate[i] > 1.0/effp->outinfo.rate)
      l->attackRate[i] = 1.0 -
	exp(-1.0/(effp->outinfo.rate * l->attackRate[i]));
    else
      l->attackRate[i] = 1.0;
    if (l->decayRate[i] > 1.0/effp->outinfo.rate)
      l->decayRate[i] = 1.0 -
	exp(-1.0/(effp->outinfo.rate * l->decayRate[i]));
    else
      l->decayRate[i] = 1.0;
  }
  return (ST_SUCCESS);
}

/*
 * Update a volume value using the given sample
 * value, the attack rate and decay rate
 */

static void doVolume(double *v, double samp, compand_t l, int chan)
{
  double s = samp/(~((LONG)1<<31));
  double delta = s - *v;

  if (delta > 0.0) /* increase volume according to attack rate */
    *v += delta * l->attackRate[chan];
  else             /* reduce volume according to decay rate */
    *v += delta * l->decayRate[chan];
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

int st_compand_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
int *isamp, *osamp;
{
  compand_t l = (compand_t) effp->priv;
  int len =  (*isamp > *osamp) ? *osamp : *isamp;
  int filechans = effp->outinfo.channels;
  int done;

  for (done = 0; done < len;
       done += filechans, obuf += filechans, ibuf += filechans) {
    int chan;

    /* Maintain the volume fields by simulating a leaky pump circuit */

    for (chan = 0; chan < filechans; ++chan) {
      if (l->expectedChannels == 1 && filechans > 1) {
	/* User is expecting same compander for all channels */
	int i;
	double maxsamp = 0.0;
	for (i = 0; i < filechans; ++i) {
	  double rect = fabs(ibuf[i]);
	  if (rect > maxsamp) maxsamp = rect;
	}
	doVolume(&l->volume[0], maxsamp, l, 0);
	break;
      } else
	doVolume(&l->volume[chan], fabs(ibuf[chan]), l, chan);
    }

    /* Volume memory is updated: perform compand */

    for (chan = 0; chan < filechans; ++chan) {
      double v = l->expectedChannels > 1 ? 
	l->volume[chan] : l->volume[0];
      double outv;
      int piece;

      for (piece = 1 /* yes, 1 */;
	   piece < l->transferPoints;
	   ++piece)
	if (v >= l->transferIns[piece - 1] &&
	    v < l->transferIns[piece])
	  break;
      
      outv = l->transferOuts[piece-1] +
	(l->transferOuts[piece] - l->transferOuts[piece-1]) *
	(v - l->transferIns[piece-1]) /
	(l->transferIns[piece] - l->transferIns[piece-1]);

      obuf[chan] = ibuf[chan]*(outv/v)*l->outgain;
	
    }
  }

  *isamp = len; *osamp = len;
  return (ST_SUCCESS);
}
