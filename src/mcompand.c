/*
 * multiband compander effect for SoX
 * by Daniel Pouzzner <douzzer@mega.nu> 2002-Oct-8
 *
 * Compander code adapted from the SoX compand effect, by Nick Bailey
 *
 * Butterworth code adapted from the SoX Butterworth effect family, by
 * Jan Paul Schmidt
 *
 * SoX is Copyright 1999 Chris Bagwell And Nick Bailey
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
 * Usage:
 *   mcompand quoted_compand_args [crossover_frequency
 *      quoted_compand_args [...]]
 *
 *   quoted_compand_args are as for the compand effect:
 *
 *   attack1,decay1[,attack2,decay2...]
 *                  in-dB1,out-dB1[,in-dB2,out-dB2...]
 *                 [ gain [ initial-volume [ delay ] ] ] 
 *
 *   Beware a variety of headroom (clipping) bugaboos.
 *
 *   Here is an example application, an FM radio sound simulator (or
 *   broadcast signal conditioner, if the lowp at the end is skipped -
 *   note that the pipeline is set up with US-style 75us preemphasis).
 *
 *   sox -V -t raw -r 44100 -s -w -c 2 - -t raw -r 44100 -s -l -c 2 \
 *      - vol -3 db filter 8000- 32 100 mcompand ".005,.1 \
 *      -47,-40,-34,-34,-17,-33 0 0 0" 100 ".003,.05 \
 *      -47,-40,-34,-34,-17,-33 0 0 0" 400 ".000625,.0125 \
 *      -47,-40,-34,-34,-15,-33 0 0 0" 1600 ".0001,.025 \
 *      -47,-40,-34,-34,-31,-31,-0,-30 0 0 0" 6400 \
 *      "0,.025 -38,-31,-28,-28,-0,-25 0 0 0" vol 27 db vol -12 \
 *      db highpass 22 highpass 22 filter -17500 256 vol +12 db \
 *      vol -3 db lowp 1780
 *
 *   implementation details:
 *
 *   The input is divided into bands using aligned 2nd order
 *   Butterworth IIR's (code adapted from SoX's existing Butterworth
 *   effect).  This is akin to the crossover of a loudspeaker, and
 *   results in flat frequency response (within a db or so) absent
 *   compander action.
 *
 *   (Crossover design from "Electroacoustic System Design" by
 *   Tom Raymann, http://www.traymann.free-online.co.uk/soph.htm)
 *
 *   The imported Butterworth code is modified to handle
 *   interleaved-channel sample streams, to make the code clean and
 *   efficient.  The outputs of the array of companders is summed, and
 *   sample truncation is done on the final sum.
 *
 *   Modifications to the predictive compression code properly
 *   maintain alignment of the outputs of the array of companders when
 *   the companders have different prediction intervals (volume
 *   application delays).  Note that the predictive mode of the
 *   limiter needs some TLC - in fact, a rewrite - since what's really
 *   useful is to assure that a waveform won't be clipped, be slewing
 *   the volume in advance so that the peak is at limit (or below, if
 *   there's a higher subsequent peak visible in the lookahead window)
 *   once it's reached.  */

struct xy {
    double x [2];
    double y [2];
} ;

typedef struct butterworth_crossover {
  struct xy *xy_low, *xy_high;

  double a_low[3], a_high[3];
  double b_low[2], b_high[3];

  /*
   * Cut off frequencies for respective filters
   */
  double frequency_low, frequency_high;

  double bandwidth;
} *butterworth_crossover_t;

static int lowpass_setup (butterworth_crossover_t butterworth, double frequency, st_rate_t rate, int nchan) {
  double c;

  if (! (butterworth->xy_low = (struct xy *)malloc(nchan * sizeof(struct xy)))) {
    st_fail("Out of memory");
    return (ST_EOF);
  }
  memset(butterworth->xy_low,0,nchan * sizeof(struct xy));
  if (! (butterworth->xy_high = (struct xy *)malloc(nchan * sizeof(struct xy)))) {
    st_fail("Out of memory");
    return (ST_EOF);
  }
  memset(butterworth->xy_high,0,nchan * sizeof(struct xy));

  /* lowpass setup */
  butterworth->frequency_low = frequency/1.3;

  c = 1.0 / tan (M_PI * butterworth->frequency_low / rate);

  butterworth->a_low[0] = 1.0 / (1.0 + sqrt(2.0) * c + c * c);
  butterworth->a_low[1] = 2.0 * butterworth->a_low [0];
  butterworth->a_low[2] = butterworth->a_low [0];

  butterworth->b_low[0] = 2 * (1.0 - c * c) * butterworth->a_low[0];
  butterworth->b_low[1] = (1.0 - sqrt(2.0) * c + c * c) * butterworth->a_low[0];

  /* highpass setup */
  butterworth->frequency_high = frequency*1.3;

  c = tan (M_PI * butterworth->frequency_high / rate);

  butterworth->a_high[0] = 1.0 / (1.0 + sqrt (2.0) * c + c * c);
  butterworth->a_high[1] = -2.0 * butterworth->a_high[0];
  butterworth->a_high[2] = butterworth->a_high[0];

  butterworth->b_high[0] = 2 * (c * c - 1.0) * butterworth->a_high[0];
  butterworth->b_high[1] = (1.0 - sqrt(2.0) * c + c * c) * butterworth->a_high[0];

  return (ST_SUCCESS);
}

static int lowpass_flow(butterworth_crossover_t butterworth, int nChan, st_sample_t *ibuf, st_sample_t *lowbuf, st_sample_t *highbuf,
                         int len) {
  int chan;
  double in, out;

  int done;

  st_sample_t *ibufptr, *lowbufptr, *highbufptr;

  for (chan=0;chan<nChan;++chan) {
    ibufptr = ibuf+chan;
    lowbufptr = lowbuf+chan;
    highbufptr = highbuf+chan;

    for (done = chan; done < len; done += nChan) {
      in = *ibufptr;
      ibufptr += nChan;

      /*
       * Substituting butterworth->a [x] and butterworth->b [x] with
       * variables, which are set outside of the loop, did not increased
       * speed on my AMD Box. GCC seems to do a good job :o)
       */

      out =
        butterworth->a_low[0] * in +
        butterworth->a_low [1] * butterworth->xy_low[chan].x [0] +
        butterworth->a_low [2] * butterworth->xy_low[chan].x [1] -
        butterworth->b_low [0] * butterworth->xy_low[chan].y [0] -
        butterworth->b_low [1] * butterworth->xy_low[chan].y [1];

      butterworth->xy_low[chan].x [1] = butterworth->xy_low[chan].x [0];
      butterworth->xy_low[chan].x [0] = in;
      butterworth->xy_low[chan].y [1] = butterworth->xy_low[chan].y [0];
      butterworth->xy_low[chan].y [0] = out;

      if (out < ST_SAMPLE_MIN) {
        out = ST_SAMPLE_MIN;
      }
      else if (out > ST_SAMPLE_MAX) {
        out = ST_SAMPLE_MAX;
      }

      *lowbufptr = out;


      out =
        butterworth->a_high[0] * in +
        butterworth->a_high [1] * butterworth->xy_high[chan].x [0] +
        butterworth->a_high [2] * butterworth->xy_high[chan].x [1] -
        butterworth->b_high [0] * butterworth->xy_high[chan].y [0] -
        butterworth->b_high [1] * butterworth->xy_high[chan].y [1];

      butterworth->xy_high[chan].x [1] = butterworth->xy_high[chan].x [0];
      butterworth->xy_high[chan].x [0] = in;
      butterworth->xy_high[chan].y [1] = butterworth->xy_high[chan].y [0];
      butterworth->xy_high[chan].y [0] = out;

      if (out < ST_SAMPLE_MIN) {
        out = ST_SAMPLE_MIN;
      }
      else if (out > ST_SAMPLE_MAX) {
        out = ST_SAMPLE_MAX;
      }

      /* don't forget polarity reversal of high pass! */

      *highbufptr = -out;

      lowbufptr += nChan;
      highbufptr += nChan;
    }
  }

  return (ST_SUCCESS);
}

typedef struct comp_band {
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
  double topfreq;       /* upper bound crossover frequency */
  struct butterworth_crossover filter;
  st_sample_t *delay_buf;   /* Old samples, used for delay processing */
  st_ssize_t delay_size;    /* lookahead for this band (in samples) - function of delay, above */
  st_ssize_t delay_buf_ptr; /* Index into delay_buf */
  st_ssize_t delay_buf_cnt; /* No. of active entries in delay_buf */
} *comp_band_t;

typedef struct {
  int nBands;
  st_sample_t *band_buf1, *band_buf2, *band_buf3;
  int band_buf_len;
  st_ssize_t delay_buf_size;/* Size of delay_buf in samples */
  struct comp_band *bands;
} *compand_t;

/*
 * Process options
 *
 * Don't do initialization now.
 * The 'info' fields are not yet filled in.
 */
static int st_mcompand_getopts_1(comp_band_t l, int n, char **argv)
{
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
      if ((l->attackRate = (double *)malloc(sizeof(double) * rates)) == NULL ||
          (l->decayRate  = (double *)malloc(sizeof(double) * rates)) == NULL)
      {
        st_fail("Out of memory");
        return (ST_EOF);
      }

      if ((l->volume = (double *)malloc(sizeof(double) * rates)) == NULL)
      {
        st_fail("Out of memory");
        return (ST_EOF);
      }

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
        st_fail("compander: Odd number of transfer function parameters\n"
             "Each input value in dB must have a corresponding output value");
        return (ST_EOF);
      }

      tfers = 3 + commas/2; /* 0, 0 at start; 1, 1 at end */
      if ((l->transferIns  = (double *)malloc(sizeof(double) * tfers)) == NULL ||
          (l->transferOuts = (double *)malloc(sizeof(double) * tfers)) == NULL)
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

        /* If there is a delay, store it. */
        if (n >= 5) l->delay = atof(argv[4]);
        else l->delay = 0.0;
      }
    return (ST_SUCCESS);
}

static int parse_subarg(char *s, char **subargv, int *subargc) {
  char **ap;
  char *s_p;

  s_p = s;
  *subargc = 0;
  for (ap = subargv; (*ap = strtok(s_p, " \t")) != NULL;) {
    s_p = NULL;
    if (*subargc == 5) {
      ++*subargc;
      break;
    }
    if (**ap != '\0') {
      ++ap;
      ++*subargc;
    }
  }

  if (*subargc < 2 || *subargc > 5)
    {
      st_fail("Wrong number of arguments for the compander effect within mcompand\n"
           "Use: {<attack_time>,<decay_time>}+ {<dB_in>,<db_out>}+ "
           "[<dB_postamp> [<initial-volume> [<delay_time]]]\n"
           "where {}+ means `one or more in a comma-separated, "
           "white-space-free list'\n"
           "and [] indications possible omission.  dB values are floating\n"
           "point or `-inf'; times are in seconds.");
      return (ST_EOF);
    } else
      return ST_SUCCESS;
}

int st_mcompand_getopts(eff_t effp, int n, char **argv) 
{
  char *subargv[6], *cp;
  int subargc, i, len;

  compand_t c = (compand_t) effp->priv;

  c->band_buf1 = c->band_buf2 = c->band_buf3 = 0;
  c->band_buf_len = 0;

  /* how many bands? */
  if (! (n&1)) {
    st_fail("mcompand accepts only an odd number of arguments:\n"
            "  mcompand quoted_compand_args [xover_freq quoted_compand_args [...]");
    return ST_EOF;
  }
  c->nBands = (n+1)>>1;

  if (! (c->bands = (struct comp_band *)malloc(c->nBands * sizeof(struct comp_band)))) {
    st_fail("Out of memory");
    return ST_EOF;
  }
  memset(c->bands,0,c->nBands * sizeof(struct comp_band));

  for (i=0;i<c->nBands;++i) {
    len = strlen(argv[i<<1]);
    if (parse_subarg(argv[i<<1],subargv,&subargc) != ST_SUCCESS)
      return ST_EOF;
    if (st_mcompand_getopts_1(&c->bands[i], subargc, &subargv[0]) != ST_SUCCESS)
      return ST_EOF;
    if (i == (c->nBands-1))
      c->bands[i].topfreq = 0;
    else {
      c->bands[i].topfreq = strtod(argv[(i<<1)+1],&cp);
      if (*cp) {
        st_fail("bad frequency in args to mcompand");
        return ST_EOF;
      }
      if ((i>0) && (c->bands[i].topfreq < c->bands[i-1].topfreq)) {
        st_fail("mcompand crossover frequencies must be in ascending order.");
        return ST_EOF;
      }
    }
  }

  return ST_SUCCESS;
}

/*
 * Prepare processing.
 * Do all initializations.
 */
int st_mcompand_start(eff_t effp)
{
  compand_t c = (compand_t) effp->priv;
  comp_band_t l;
  int band, i;
  
  for (band=0;band<c->nBands;++band) {
    l = &c->bands[band];
    l->delay_size = c->bands[band].delay * effp->outinfo.rate * effp->outinfo.channels;
    if (l->delay_size > c->delay_buf_size)
      c->delay_buf_size = l->delay_size;
  }

  for (band=0;band<c->nBands;++band) {
    l = &c->bands[band];
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
    if (c->delay_buf_size > 0) {
      if ((l->delay_buf = (st_sample_t *)malloc(sizeof(long) * c->delay_buf_size)) == NULL) {
        st_fail("Out of memory");
        return (ST_EOF);
      }
      for (i = 0;  i < c->delay_buf_size;  i++)
        l->delay_buf[i] = 0;

    }
    l->delay_buf_ptr = 0;
    l->delay_buf_cnt = 0;

    if (l->topfreq != 0)
      lowpass_setup(&l->filter, l->topfreq, effp->outinfo.rate, effp->outinfo.channels);
  }
  return (ST_SUCCESS);
}

/*
 * Update a volume value using the given sample
 * value, the attack rate and decay rate
 */

static void doVolume(double *v, double samp, comp_band_t l, int chan)
{
  double s = samp/(~((st_sample_t)1<<31));
  double delta = s - *v;

  if (delta > 0.0) /* increase volume according to attack rate */
    *v += delta * l->attackRate[chan];
  else             /* reduce volume according to decay rate */
    *v += delta * l->decayRate[chan];
}

static int st_mcompand_flow_1(compand_t c, comp_band_t l, st_sample_t *ibuf, st_sample_t *obuf, int len, int filechans)
{
  int done, chan;

  for (done = 0; done < len; ibuf += filechans) {

    /* Maintain the volume fields by simulating a leaky pump circuit */

    if (l->expectedChannels == 1 && filechans > 1) {
      /* User is expecting same compander for all channels */
      double maxsamp = 0.0;
      for (chan = 0; chan < filechans; ++chan) {
        double rect = fabs(ibuf[chan]);
        if (rect > maxsamp)
          maxsamp = rect;
      }
      doVolume(&l->volume[0], maxsamp, l, 0);
    } else {
      for (chan = 0; chan < filechans; ++chan)
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

      if (c->delay_buf_size <= 0)
        obuf[done++] = ibuf[chan]*(outv/v)*l->outgain;
      else {


        /* note that this lookahead algorithm is really lame.  the response to a peak is released before the peak arrives.  fix! */

        /* because volume application delays differ band to band, but
           total delay doesn't, the volume is applied in an iteration
           preceding that in which the sample goes to obuf, except in
           the band(s) with the longest vol app delay.

           the offset between delay_buf_ptr and the sample to apply
           vol to, is a constant equal to the difference between this
           band's delay and the longest delay of all the bands. */

        if (l->delay_buf_cnt >= l->delay_size)
          l->delay_buf[(l->delay_buf_ptr + c->delay_buf_size - l->delay_size)%c->delay_buf_size] *= (outv/v)*l->outgain;
        if (l->delay_buf_cnt >= c->delay_buf_size)
          obuf[done++] = l->delay_buf[l->delay_buf_ptr];
        else
          l->delay_buf_cnt++;
        l->delay_buf[l->delay_buf_ptr++] = ibuf[chan];
        l->delay_buf_ptr %= c->delay_buf_size;
      }
    }
  }

  return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
int st_mcompand_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                     st_size_t *isamp, st_size_t *osamp) {
  compand_t c = (compand_t) effp->priv;
  comp_band_t l;
  int len = ((*isamp > *osamp) ? *osamp : *isamp);
  int band, i;
  st_sample_t *abuf, *bbuf, *cbuf, *oldabuf;

  if (c->band_buf_len < len) {
    if ((! (c->band_buf1 = (st_sample_t *)realloc(c->band_buf1,len*sizeof(st_sample_t)))) ||
        (! (c->band_buf2 = (st_sample_t *)realloc(c->band_buf2,len*sizeof(st_sample_t)))) ||
        (! (c->band_buf3 = (st_sample_t *)realloc(c->band_buf3,len*sizeof(st_sample_t))))) {
      st_fail("Out of memory");
      return (ST_EOF);
    }
    c->band_buf_len = len;
  }

  /* split ibuf into bands using butterworths, pipe each band through st_mcompand_flow_1, then add back together and write to obuf */

  memset(obuf,0,len * sizeof *obuf);
  for (band=0,abuf=ibuf,bbuf=c->band_buf2,cbuf=c->band_buf1;band<c->nBands;++band) {
    l = &c->bands[band];

    if (l->topfreq)
      lowpass_flow(&l->filter, effp->outinfo.channels, abuf, bbuf, cbuf, len);
    else {
      bbuf = abuf;
      abuf = cbuf;
    }
    if (abuf == ibuf)
      abuf = c->band_buf3;
    (void)st_mcompand_flow_1(c,l,bbuf,abuf,len,effp->outinfo.channels);
    for (i=0;i<len;++i)
      obuf[i] += abuf[i];
    oldabuf = abuf;
    abuf = cbuf;
    cbuf = oldabuf;
  }

  for (i=0;i<len;++i) {
    if (obuf[i] < ST_SAMPLE_MIN) {
      obuf[i] = ST_SAMPLE_MIN;
    }
    else if (obuf[i] > ST_SAMPLE_MAX) {
      obuf[i] = ST_SAMPLE_MAX;
    }
  }

  *isamp = *osamp = len;

  return ST_SUCCESS;
}

static int st_mcompand_drain_1(compand_t c, comp_band_t l, st_sample_t *obuf, int maxdrain, int band)
{
  int done;

  /*
   * Drain out delay samples.  Note that this loop does all channels.
   */
  for (done = 0;  done < maxdrain  &&  l->delay_buf_cnt > 0;  done++) {
    obuf[done] += l->delay_buf[l->delay_buf_ptr++];
    l->delay_buf_ptr %= c->delay_buf_size;
    l->delay_buf_cnt--;
  }

  /* tell caller number of samples played */
  return done;

}

/*
 * Drain out compander delay lines. 
 */
int st_mcompand_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
  int band, drained, mostdrained = 0;
  compand_t c = (compand_t)effp->priv;
  comp_band_t l;
  int i;

  memset(obuf,0,*osamp * sizeof *obuf);
  for (band=0;band<c->nBands;++band) {
    l = &c->bands[band];
    drained = st_mcompand_drain_1(c,l,obuf,*osamp,0);
    if (drained > mostdrained)
      mostdrained = drained;
  }

  for (i=0;i<mostdrained;++i) {
    if (obuf[i] < ST_SAMPLE_MIN) {
      obuf[i] = ST_SAMPLE_MIN;
    }
    else if (obuf[i] > ST_SAMPLE_MAX) {
      obuf[i] = ST_SAMPLE_MAX;
    }
  }

  *osamp = mostdrained;
  return (ST_SUCCESS);
}

/*
 * Clean up compander effect.
 */
int st_mcompand_stop(eff_t effp)
{
  compand_t c = (compand_t) effp->priv;
  comp_band_t l;
  int band;

  if (c->band_buf1) {
    free(c->band_buf1);
    c->band_buf1 = 0;
  }
  if (c->band_buf2) {
    free(c->band_buf2);
    c->band_buf2 = 0;
  }
  if (c->band_buf3) {
    free(c->band_buf3);
    c->band_buf3 = 0;
  }

  for (band=0;band<c->nBands;++band) {
    l = &c->bands[band];
    free((char *) l->transferOuts);
    free((char *) l->transferIns);
    free((char *) l->decayRate);
    free((char *) l->attackRate);
    if (l->delay_buf)
      free((char *) l->delay_buf);
    free((char *) l->volume);
    if (l->topfreq != 0) {
      free(l->filter.xy_low);
      free(l->filter.xy_high);
    }
  }
  free(c->bands);
  c->bands = 0;

  return (ST_SUCCESS);
}
