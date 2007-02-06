/*
 * Compander effect
 *
 * Written by Nick Bailey (nick@bailey-family.org.uk or
 *                         n.bailey@elec.gla.ac.uk)
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
#include "st_i.h"

/*
 * Compressor/expander effect for dsp.
 *
 * Flow diagram for one channel:
 *
 *               ------------      ---------------
 *              |            |    |               |     ---
 * ibuff ---+---| integrator |--->| transfer func |--->|   |
 *          |   |            |    |               |    |   |
 *          |    ------------      ---------------     |   |  * gain
 *          |                                          | * |----------->obuff
 *          |       -------                            |   |
 *          |      |       |                           |   |
 *          +----->| delay |-------------------------->|   |
 *                 |       |                            ---
 *                  -------
 *
 * Usage:
 *   compand attack1,decay1[,attack2,decay2...]
 *                  in-dB1,out-dB1[,in-dB2,out-dB2...]
 *                 [ gain [ initial-volume [ delay ] ] ] 
 *
 * Note: clipping can occur if the transfer function pushes things too
 * close to 0 dB.  In that case, use a negative gain, or reduce the
 * output level of the transfer function.
 */

static st_effect_t st_compand_effect;

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
  double outgain;       /* Post processor gain */
  double delay;         /* Delay to apply before companding */
  st_sample_t *delay_buf;   /* Old samples, used for delay processing */
  st_ssize_t delay_buf_size;/* Size of delay_buf in samples */
  st_ssize_t delay_buf_ptr; /* Index into delay_buf */
  st_ssize_t delay_buf_cnt; /* No. of active entries in delay_buf */
  short int delay_buf_full; /* Shows buffer situation (important for st_compand_drain) */
} *compand_t;

/*
 * Process options
 *
 * Don't do initialization now.
 * The 'info' fields are not yet filled in.
 */
static int st_compand_getopts(eff_t effp, int n, char **argv) 
{
    compand_t l = (compand_t) effp->priv;

    if (n < 2 || n > 5)
    {
      st_fail (st_compand_effect.usage);
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
      l->attackRate = (double *)xmalloc(sizeof(double) * rates);
      l->decayRate = (double *)xmalloc(sizeof(double) * rates);
      l->volume = (double *)xmalloc(sizeof(double) * rates);
      l->expectedChannels = rates;
      l->delay_buf = NULL;

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
        st_fail("compander: Odd number of transfer function parameters"
             "Each input value in dB must have a corresponding output value");
        return (ST_EOF);
      }

      tfers = 3 + commas/2; /* 0, 0 at start; 1, 1 at end */
      l->transferIns = (double *)xmalloc(sizeof(double) * tfers);
      l->transferOuts = (double *)xmalloc(sizeof(double) * tfers);
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

      /* If there is a delay, store it. */
      if (n >= 5) l->delay = atof(argv[4]);
      else l->delay = 0.0;
      }
    }
    return (ST_SUCCESS);
}

/*
 * Prepare processing.
 * Do all initializations.
 */
static int st_compand_start(eff_t effp)
{
  compand_t l = (compand_t) effp->priv;
  int i;

  st_debug("Starting compand effect");
  st_debug("Rate %ld, size %d, encoding %d, output gain %g.",
         effp->outinfo.rate, effp->outinfo.size, effp->outinfo.encoding,
         l->outgain);
  st_debug("%d input channel(s) expected: actually %d",
         l->expectedChannels, effp->outinfo.channels);
  for (i = 0; i < l->expectedChannels; ++i)
    st_debug("Channel %d: attack = %-12g decay = %-12g",
           i, l->attackRate[i], l->decayRate[i]);
  for (i = 0; i < l->transferPoints; ++i)
    st_debug("Transfer fn (linear): %12g -> %-12g",
           l->transferIns[i], l->transferOuts[i]);
  
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

  /* Allocate the delay buffer */
  l->delay_buf_size = l->delay * effp->outinfo.rate * effp->outinfo.channels;
  if (l->delay_buf_size > 0)
    l->delay_buf = (st_sample_t *)xmalloc(sizeof(long) * l->delay_buf_size);
  for (i = 0;  i < l->delay_buf_size;  i++)
    l->delay_buf[i] = 0;
  l->delay_buf_ptr = 0;
  l->delay_buf_cnt = 0;
  l->delay_buf_full= 0;

  return (ST_SUCCESS);
}

/*
 * Update a volume value using the given sample
 * value, the attack rate and decay rate
 */

static void doVolume(double *v, double samp, compand_t l, int chan)
{
  double s = -samp / ST_SAMPLE_MIN;
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
static int st_compand_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                    st_size_t *isamp, st_size_t *osamp)
{
  compand_t l = (compand_t) effp->priv;
  int len =  (*isamp > *osamp) ? *osamp : *isamp;
  int filechans = effp->outinfo.channels;
  int idone,odone;
  int64_t checkbuf; /* if st_sample_t of type int32_t */

  for (idone = 0,odone = 0; idone < len; ibuf += filechans) {
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
      double v = l->volume[l->expectedChannels > 1 ? chan : 0];
      double outv;

      if (v == 0)
        outv = 1;
      else {
        int piece;

        for (piece = 1; v > l->transferIns[piece]; ++piece);
        outv = (l->transferOuts[piece-1] +
          (l->transferOuts[piece] - l->transferOuts[piece-1]) *
          (v - l->transferIns[piece-1]) /
          (l->transferIns[piece] - l->transferIns[piece-1])) / v;
      }
      outv *= l->outgain;

      if (l->delay_buf_size <= 0)
      {
        checkbuf = ibuf[chan] * outv;
        ST_SAMPLE_CLIP_COUNT(checkbuf, effp->clips);
        obuf[odone] = checkbuf;

        idone++;
        odone++;
      }
      else
      {
        if (l->delay_buf_cnt >= l->delay_buf_size)
        {
            l->delay_buf_full=1; /* delay buffer is now definetly full */
            checkbuf = l->delay_buf[l->delay_buf_ptr] * outv;
            ST_SAMPLE_CLIP_COUNT(checkbuf, effp->clips);
            obuf[odone] = checkbuf;

            odone++;
            idone++;
        }
        else
        {
            l->delay_buf_cnt++;
            idone++; /* no "odone++" because we did not fill obuf[...] */
        }
        l->delay_buf[l->delay_buf_ptr++] = ibuf[chan];
        l->delay_buf_ptr %= l->delay_buf_size;
      }
    }
  }

  *isamp = idone; *osamp = odone;
  return (ST_SUCCESS);
}

/*
 * Drain out compander delay lines.
 */
static int st_compand_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
  compand_t l = (compand_t) effp->priv;
  st_size_t done;

  /*
   * Drain out delay samples.  Note that this loop does all channels.
   */
  if(l->delay_buf_full==0) l->delay_buf_ptr=0;
  for (done = 0;  done < *osamp  &&  l->delay_buf_cnt > 0;  done++) {
    obuf[done] = l->delay_buf[l->delay_buf_ptr++];
    l->delay_buf_ptr %= l->delay_buf_size;
    l->delay_buf_cnt--;
  }

  /* tell caller number of samples played */
  *osamp = done;

  if (l->delay_buf_cnt > 0)
      return ST_SUCCESS;
  else
      return ST_EOF;
}


/*
 * Clean up compander effect.
 */
static int st_compand_stop(eff_t effp)
{
  compand_t l = (compand_t) effp->priv;

  free((char *) l->delay_buf);

  l->delay_buf = NULL;

  return (ST_SUCCESS);
}

static int delete(eff_t effp)
{
  compand_t l = (compand_t) effp->priv;

  free((char *) l->transferOuts);
  free((char *) l->transferIns);
  free((char *) l->volume);
  free((char *) l->decayRate);
  free((char *) l->attackRate);

  l->transferOuts = NULL;
  l->transferIns = NULL;
  l->volume = NULL;
  l->decayRate = NULL;
  l->attackRate = NULL;

  return (ST_SUCCESS);
}

static st_effect_t st_compand_effect = {
   "compand",
   "Usage: {<attack_time>,<decay_time>}+ {<dB_in>,<db_out>}+ [<dB_postamp> [<initial-volume> [<delay_time]]]\n"
   "       where {}+ means e or more in a comma-separated, white-space-free list'\n"
   "       and [] indications possible omission.  dB values are floating\n"
   "       point or -inf'; times are in seconds.",
   ST_EFF_MCHAN,
   st_compand_getopts,
   st_compand_start,
   st_compand_flow,
   st_compand_drain,
   st_compand_stop,
  delete
};

const st_effect_t *st_compand_effect_fn(void)
{
    return &st_compand_effect;
}
