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
#include "compandt.h"

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
 *   useful is to assure that a waveform won't be clipped, by slewing
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

static int lowpass_setup (butterworth_crossover_t butterworth, double frequency, sox_rate_t rate, sox_size_t nchan) {
  double c;

  butterworth->xy_low = (struct xy *)xcalloc(nchan, sizeof(struct xy));
  butterworth->xy_high = (struct xy *)xcalloc(nchan, sizeof(struct xy));

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

  return (SOX_SUCCESS);
}

static int lowpass_flow(sox_effect_t effp, butterworth_crossover_t butterworth, sox_size_t nChan, sox_ssample_t *ibuf, sox_ssample_t *lowbuf, sox_ssample_t *highbuf,
                         sox_size_t len) {
  sox_size_t chan;
  double in, out;

  sox_size_t done;

  sox_ssample_t *ibufptr, *lowbufptr, *highbufptr;

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

      SOX_SAMPLE_CLIP_COUNT(out, effp->clips);

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

      SOX_SAMPLE_CLIP_COUNT(out, effp->clips);

      /* don't forget polarity reversal of high pass! */

      *highbufptr = -out;

      lowbufptr += nChan;
      highbufptr += nChan;
    }
  }

  return (SOX_SUCCESS);
}

typedef struct comp_band {
  sox_compandt_t transfer_fn;

  sox_size_t expectedChannels; /* Also flags that channels aren't to be treated
                           individually when = 1 and input not mono */
  double *attackRate;   /* An array of attack rates */
  double *decayRate;    /*    ... and of decay rates */
  double *volume;       /* Current "volume" of each channel */
  double delay;         /* Delay to apply before companding */
  double topfreq;       /* upper bound crossover frequency */
  struct butterworth_crossover filter;
  sox_ssample_t *delay_buf;   /* Old samples, used for delay processing */
  sox_size_t delay_size;    /* lookahead for this band (in samples) - function of delay, above */
  sox_ssize_t delay_buf_ptr; /* Index into delay_buf */
  sox_size_t delay_buf_cnt; /* No. of active entries in delay_buf */
} *comp_band_t;

typedef struct {
  sox_size_t nBands;
  sox_ssample_t *band_buf1, *band_buf2, *band_buf3;
  sox_size_t band_buf_len;
  sox_size_t delay_buf_size;/* Size of delay_buf in samples */
  struct comp_band *bands;
} *compand_t;

/*
 * Process options
 *
 * Don't do initialization now.
 * The 'info' fields are not yet filled in.
 */
static int sox_mcompand_getopts_1(comp_band_t l, sox_size_t n, char **argv)
{
      char *s;
      sox_size_t rates, i, commas;

      /* Start by checking the attack and decay rates */

      for (s = argv[0], commas = 0; *s; ++s)
        if (*s == ',') ++commas;

      if (commas % 2 == 0) /* There must be an even number of
                              attack/decay parameters */
      {
        sox_fail("compander: Odd number of attack & decay rate parameters");
        return (SOX_EOF);
      }

      rates = 1 + commas/2;
      l->attackRate = (double *)xmalloc(sizeof(double) * rates);
      l->decayRate  = (double *)xmalloc(sizeof(double) * rates);
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

      if (!sox_compandt_parse(&l->transfer_fn, argv[1], n>2 ? argv[2] : 0))
        return SOX_EOF;

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
    return (SOX_SUCCESS);
}

static int parse_subarg(char *s, char **subargv, sox_size_t *subargc) {
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
      sox_fail("Wrong number of parameters for the compander effect within mcompand; usage:\n"
  "\tattack1,decay1{,attack2,decay2} [soft-knee-dB:]in-dB1[,out-dB1]{,in-dB2,out-dB2} [gain [initial-volume-dB [delay]]]\n"
  "\twhere {} means optional and repeatable and [] means optional.\n"
  "\tdB values are floating point or -inf'; times are in seconds.");
      return (SOX_EOF);
    } else
      return SOX_SUCCESS;
}

static int sox_mcompand_getopts(sox_effect_t effp, int n, char **argv) 
{
  char *subargv[6], *cp;
  sox_size_t subargc, i, len;

  compand_t c = (compand_t) effp->priv;

  c->band_buf1 = c->band_buf2 = c->band_buf3 = 0;
  c->band_buf_len = 0;

  /* how many bands? */
  if (! (n&1)) {
    sox_fail("mcompand accepts only an odd number of arguments:\n"
            "  mcompand quoted_compand_args [xover_freq quoted_compand_args [...]");
    return SOX_EOF;
  }
  c->nBands = (n+1)>>1;

  c->bands = (struct comp_band *)xcalloc(c->nBands, sizeof(struct comp_band));

  for (i=0;i<c->nBands;++i) {
    len = strlen(argv[i<<1]);
    if (parse_subarg(argv[i<<1],subargv,&subargc) != SOX_SUCCESS)
      return SOX_EOF;
    if (sox_mcompand_getopts_1(&c->bands[i], subargc, &subargv[0]) != SOX_SUCCESS)
      return SOX_EOF;
    if (i == (c->nBands-1))
      c->bands[i].topfreq = 0;
    else {
      c->bands[i].topfreq = strtod(argv[(i<<1)+1],&cp);
      if (*cp) {
        sox_fail("bad frequency in args to mcompand");
        return SOX_EOF;
      }
      if ((i>0) && (c->bands[i].topfreq < c->bands[i-1].topfreq)) {
        sox_fail("mcompand crossover frequencies must be in ascending order.");
        return SOX_EOF;
      }
    }
  }

  return SOX_SUCCESS;
}

/*
 * Prepare processing.
 * Do all initializations.
 */
static int sox_mcompand_start(sox_effect_t effp)
{
  compand_t c = (compand_t) effp->priv;
  comp_band_t l;
  sox_size_t i;
  sox_size_t band;
  
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
    if (c->delay_buf_size > 0)
      l->delay_buf = (sox_ssample_t *)xcalloc(sizeof(long), c->delay_buf_size);
    l->delay_buf_ptr = 0;
    l->delay_buf_cnt = 0;

    if (l->topfreq != 0)
      lowpass_setup(&l->filter, l->topfreq, effp->outinfo.rate, effp->outinfo.channels);
  }
  return (SOX_SUCCESS);
}

/*
 * Update a volume value using the given sample
 * value, the attack rate and decay rate
 */

static void doVolume(double *v, double samp, comp_band_t l, sox_size_t chan)
{
  double s = samp/(~((sox_ssample_t)1<<31));
  double delta = s - *v;

  if (delta > 0.0) /* increase volume according to attack rate */
    *v += delta * l->attackRate[chan];
  else             /* reduce volume according to decay rate */
    *v += delta * l->decayRate[chan];
}

static int sox_mcompand_flow_1(sox_effect_t effp, compand_t c, comp_band_t l, const sox_ssample_t *ibuf, sox_ssample_t *obuf, sox_size_t len, sox_size_t filechans)
{
  sox_size_t done, chan;

  for (done = 0; done < len; ibuf += filechans) {

    /* Maintain the volume fields by simulating a leaky pump circuit */

    if (l->expectedChannels == 1 && filechans > 1) {
      /* User is expecting same compander for all channels */
      double maxsamp = 0.0;
      for (chan = 0; chan < filechans; ++chan) {
        double rect = fabs((double)ibuf[chan]);
        if (rect > maxsamp)
          maxsamp = rect;
      }
      doVolume(&l->volume[0], maxsamp, l, 0);
    } else {
      for (chan = 0; chan < filechans; ++chan)
        doVolume(&l->volume[chan], fabs((double)ibuf[chan]), l, chan);
    }

    /* Volume memory is updated: perform compand */
    for (chan = 0; chan < filechans; ++chan) {
      int ch = l->expectedChannels > 1 ? chan : 0;
      double level_in_lin = l->volume[ch];
      double level_out_lin = sox_compandt(&l->transfer_fn, level_in_lin);
      double checkbuf;

      if (c->delay_buf_size <= 0) {
        checkbuf = ibuf[chan] * level_out_lin;
        SOX_SAMPLE_CLIP_COUNT(checkbuf, effp->clips);
        obuf[done++] = checkbuf;

      } else {
        /* FIXME: note that this lookahead algorithm is really lame:
           the response to a peak is released before the peak
           arrives. */

        /* because volume application delays differ band to band, but
           total delay doesn't, the volume is applied in an iteration
           preceding that in which the sample goes to obuf, except in
           the band(s) with the longest vol app delay.

           the offset between delay_buf_ptr and the sample to apply
           vol to, is a constant equal to the difference between this
           band's delay and the longest delay of all the bands. */

        if (l->delay_buf_cnt >= l->delay_size) {
          checkbuf = l->delay_buf[(l->delay_buf_ptr + c->delay_buf_size - l->delay_size)%c->delay_buf_size] * level_out_lin;
          SOX_SAMPLE_CLIP_COUNT(checkbuf, effp->clips);
          l->delay_buf[(l->delay_buf_ptr + c->delay_buf_size - l->delay_size)%c->delay_buf_size] = checkbuf;
        }
        if (l->delay_buf_cnt >= c->delay_buf_size)
          obuf[done++] = l->delay_buf[l->delay_buf_ptr];
        else
          l->delay_buf_cnt++;
        l->delay_buf[l->delay_buf_ptr++] = ibuf[chan];
        l->delay_buf_ptr %= c->delay_buf_size;
      }
    }
  }

  return (SOX_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
static int sox_mcompand_flow(sox_effect_t effp, const sox_ssample_t *ibuf, sox_ssample_t *obuf, 
                     sox_size_t *isamp, sox_size_t *osamp) {
  compand_t c = (compand_t) effp->priv;
  comp_band_t l;
  sox_size_t len = min(*isamp, *osamp);
  sox_size_t band, i;
  sox_ssample_t *abuf, *bbuf, *cbuf, *oldabuf, *ibuf_copy;
  double out;

  if (c->band_buf_len < len) {
    c->band_buf1 = (sox_ssample_t *)xrealloc(c->band_buf1,len*sizeof(sox_ssample_t));
    c->band_buf2 = (sox_ssample_t *)xrealloc(c->band_buf2,len*sizeof(sox_ssample_t));
    c->band_buf3 = (sox_ssample_t *)xrealloc(c->band_buf3,len*sizeof(sox_ssample_t));
    c->band_buf_len = len;
  }

  ibuf_copy = (sox_ssample_t *)xmalloc(*isamp * sizeof(sox_ssample_t));
  memcpy(ibuf_copy, ibuf, *isamp * sizeof(sox_ssample_t));

  /* split ibuf into bands using butterworths, pipe each band through sox_mcompand_flow_1, then add back together and write to obuf */

  memset(obuf,0,len * sizeof *obuf);
  for (band=0,abuf=ibuf_copy,bbuf=c->band_buf2,cbuf=c->band_buf1;band<c->nBands;++band) {
    l = &c->bands[band];

    if (l->topfreq)
      lowpass_flow(effp, &l->filter, effp->outinfo.channels, abuf, bbuf, cbuf, len);
    else {
      bbuf = abuf;
      abuf = cbuf;
    }
    if (abuf == ibuf_copy)
      abuf = c->band_buf3;
    (void)sox_mcompand_flow_1(effp, c,l,bbuf,abuf,len,effp->outinfo.channels);
    for (i=0;i<len;++i)
    {
      out = obuf[i] + abuf[i];
      SOX_SAMPLE_CLIP_COUNT(out, effp->clips);
      obuf[i] = out;
    }
    oldabuf = abuf;
    abuf = cbuf;
    cbuf = oldabuf;
  }

  *isamp = *osamp = len;

  free(ibuf_copy);

  return SOX_SUCCESS;
}

static int sox_mcompand_drain_1(sox_effect_t effp, compand_t c, comp_band_t l, sox_ssample_t *obuf, sox_size_t maxdrain)
{
  sox_size_t done;
  double out;

  /*
   * Drain out delay samples.  Note that this loop does all channels.
   */
  for (done = 0;  done < maxdrain  &&  l->delay_buf_cnt > 0;  done++) {
    out = obuf[done] + l->delay_buf[l->delay_buf_ptr++];
    SOX_SAMPLE_CLIP_COUNT(out, effp->clips);
    obuf[done] = out;
    l->delay_buf_ptr %= c->delay_buf_size;
    l->delay_buf_cnt--;
  }

  /* tell caller number of samples played */
  return done;

}

/*
 * Drain out compander delay lines. 
 */
static int sox_mcompand_drain(sox_effect_t effp, sox_ssample_t *obuf, sox_size_t *osamp)
{
  sox_size_t band, drained, mostdrained = 0;
  compand_t c = (compand_t)effp->priv;
  comp_band_t l;

  memset(obuf,0,*osamp * sizeof *obuf);
  for (band=0;band<c->nBands;++band) {
    l = &c->bands[band];
    drained = sox_mcompand_drain_1(effp, c,l,obuf,*osamp);
    if (drained > mostdrained)
      mostdrained = drained;
  }

  *osamp = mostdrained;

  if (mostdrained)
      return SOX_SUCCESS;
  else
      return SOX_EOF;
}

/*
 * Clean up compander effect.
 */
static int sox_mcompand_stop(sox_effect_t effp)
{
  compand_t c = (compand_t) effp->priv;
  comp_band_t l;
  sox_size_t band;

  free(c->band_buf1);
  c->band_buf1 = NULL;
  free(c->band_buf2);
  c->band_buf2 = NULL;
  free(c->band_buf3);
  c->band_buf3 = NULL;

  for (band = 0; band < c->nBands; band++) {
    l = &c->bands[band];
    free(l->delay_buf);
    if (l->topfreq != 0) {
      free(l->filter.xy_low);
      free(l->filter.xy_high);
    }
  }

  return SOX_SUCCESS;
}

static int sox_mcompand_kill(sox_effect_t effp)
{
  compand_t c = (compand_t) effp->priv;
  comp_band_t l;
  sox_size_t band;

  for (band = 0; band < c->nBands; band++) {
    l = &c->bands[band];
    sox_compandt_kill(&l->transfer_fn);
    free(l->decayRate);
    free(l->attackRate);
    free(l->volume);
  }
  free(c->bands);
  c->bands = NULL;

  return SOX_SUCCESS;
}

static sox_effect_handler_t sox_mcompand_effect = {
  "mcompand",
  "Usage: mcompand quoted_compand_args [crossover_frequency quoted_compand_args [...]]\n"
  "\n"
  "quoted_compand_args are as for the compand effect:\n"
  "\n"
  "  attack1,decay1[,attack2,decay2...]\n"
  "                 in-dB1,out-dB1[,in-dB2,out-dB2...]\n"
  "                [ gain [ initial-volume [ delay ] ] ]\n",
  SOX_EFF_MCHAN,
  sox_mcompand_getopts,
  sox_mcompand_start,
  sox_mcompand_flow,
  sox_mcompand_drain,
  sox_mcompand_stop,
  sox_mcompand_kill
};

const sox_effect_handler_t *sox_mcompand_effect_fn(void)
{
    return &sox_mcompand_effect;
}
