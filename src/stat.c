/*
 * Sound Tools statistics "effect" file.
 *
 * Compute various statistics on file and print them.
 *
 * Output is unmodified from input.
 *
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include <math.h>
#include <string.h>
#include "st_i.h"
#include "FFT.h"

/* Private data for stat effect */
typedef struct statstuff {
  double min, max, mid;
  double asum;
  double sum1, sum2;            /* amplitudes */
  double dmin, dmax;
  double dsum1, dsum2;          /* deltas */
  double scale;                 /* scale-factor */
  double last;                  /* previous sample */
  st_size_t read;               /* samples processed */
  int volume;
  int srms;
  int fft;
  unsigned long bin[4];
  float *re_in;
  float *re_out;
  unsigned long fft_size;
  unsigned long fft_offset;
} *stat_t;


/*
 * Process options
 */
static int st_stat_getopts(eff_t effp, int n, char **argv)
{
  stat_t stat = (stat_t) effp->priv;

  stat->scale = ST_SAMPLE_MAX;
  stat->volume = 0;
  stat->srms = 0;
  stat->fft = 0;

  for (; n > 0; n--, argv++) {
    if (!(strcmp(*argv, "-v")))
      stat->volume = 1;
    else if (!(strcmp(*argv, "-s"))) {
      if (n <= 1) {
        st_fail("-s option: invalid argument");
        return ST_EOF;
      }
      n--, argv++;              /* Move to next argument. */
      if (!sscanf(*argv, "%lf", &stat->scale)) {
        st_fail("-s option: invalid argument");
        return ST_EOF;
      }
    } else if (!(strcmp(*argv, "-rms")))
      stat->srms = 1;
    else if (!(strcmp(*argv, "-freq")))
      stat->fft = 1;
    else if (!(strcmp(*argv, "-d")))
      stat->volume = 2;
    else {
      st_fail("Summary effect: unknown option");
      return ST_EOF;
    }
  }

  return ST_SUCCESS;
}

/*
 * Prepare processing.
 */
static int st_stat_start(eff_t effp)
{
  stat_t stat = (stat_t) effp->priv;
  int i;

  stat->min = stat->max = stat->mid = 0;
  stat->asum = 0;
  stat->sum1 = stat->sum2 = 0;

  stat->dmin = stat->dmax = 0;
  stat->dsum1 = stat->dsum2 = 0;

  stat->last = 0;
  stat->read = 0;

  for (i = 0; i < 4; i++)
    stat->bin[i] = 0;

  stat->fft_size = 4096;
  stat->re_in = stat->re_out = NULL;

  if (stat->fft) {
    stat->fft_offset = 0;
    stat->re_in = (float *)xmalloc(sizeof(float) * stat->fft_size);
    stat->re_out = (float *)xmalloc(sizeof(float) * (stat->fft_size / 2));
  }

  return ST_SUCCESS;
}

/*
 * Print power spectrum to given stream
 */
static void print_power_spectrum(unsigned samples, float rate, float *re_in, float *re_out)
{
  float ffa = rate / samples;
  unsigned i;
  
  PowerSpectrum(samples, re_in, re_out);
  for (i = 0; i < samples / 2; i++)
    fprintf(stderr, "%f  %f\n", ffa * i, re_out[i]);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
static int st_stat_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf,
                        st_size_t *isamp, st_size_t *osamp)
{
  stat_t stat = (stat_t) effp->priv;
  int done, x, len = min(*isamp, *osamp);
  short count = 0;

  if (len == 0)
    return ST_SUCCESS;

  if (stat->read == 0)          /* 1st sample */
    stat->min = stat->max = stat->mid = stat->last = (*ibuf)/stat->scale;

  if (stat->fft) {
    for (x = 0; x < len; x++) {
      stat->re_in[stat->fft_offset++] = ST_SAMPLE_TO_FLOAT_DWORD(ibuf[x], effp->clips);

      if (stat->fft_offset >= stat->fft_size) {
        stat->fft_offset = 0;
        print_power_spectrum(stat->fft_size, effp->ininfo.rate, stat->re_in, stat->re_out);
      }

    }
  }

  for (done = 0; done < len; done++) {
    long lsamp = *ibuf++;
    double delta, samp = (double)lsamp / stat->scale;
    /* work in scaled levels for both sample and delta */
    stat->bin[(lsamp >> 30) + 2]++;
    *obuf++ = lsamp;

    if (stat->volume == 2) {
        fprintf(stderr,"%08lx ",lsamp);
        if (count++ == 5) {
            fprintf(stderr,"\n");
            count = 0;
        }
    }

    /* update min/max */
    if (stat->min > samp)
      stat->min = samp;
    else if (stat->max < samp)
      stat->max = samp;
    stat->mid = stat->min / 2 + stat->max / 2;

    stat->sum1 += samp;
    stat->sum2 += samp*samp;
    stat->asum += fabs(samp);

    delta = fabs(samp - stat->last);
    if (delta < stat->dmin)
      stat->dmin = delta;
    else if (delta > stat->dmax)
      stat->dmax = delta;

    stat->dsum1 += delta;
    stat->dsum2 += delta*delta;

    stat->last = samp;
  }
  stat->read += len;
  *isamp = *osamp = len;
  /* Process all samples */

  return ST_SUCCESS;
}

/*
 * Process tail of input samples.
 */
static int st_stat_drain(eff_t effp, st_sample_t *obuf UNUSED, st_size_t *osamp)
{
  stat_t stat = (stat_t) effp->priv;

  /* When we run out of samples, then we need to pad buffer with
   * zeros and then run FFT one last time to process any unprocessed
   * samples.
   */
  if (stat->fft && stat->fft_offset) {
    unsigned int x;

    for (x = stat->fft_offset; x < stat->fft_size; x++)
      stat->re_in[x] = 0;
      
    print_power_spectrum(stat->fft_size, effp->ininfo.rate, stat->re_in, stat->re_out);
  }

  *osamp = 0;
  return ST_EOF;
}

/*
 * Do anything required when you stop reading samples.
 * Don't close input file!
 */
static int st_stat_stop(eff_t effp)
{
  stat_t stat = (stat_t) effp->priv;
  double amp, scale, rms = 0, freq;
  double x, ct;

  ct = stat->read;

  if (stat->srms) {  /* adjust results to units of rms */
    double f;
    rms = sqrt(stat->sum2/ct);
    f = 1.0/rms;
    stat->max *= f;
    stat->min *= f;
    stat->mid *= f;
    stat->asum *= f;
    stat->sum1 *= f;
    stat->sum2 *= f*f;
    stat->dmax *= f;
    stat->dmin *= f;
    stat->dsum1 *= f;
    stat->dsum2 *= f*f;
    stat->scale *= rms;
  }

  scale = stat->scale;

  amp = -stat->min;
  if (amp < stat->max)
    amp = stat->max;

  /* Just print the volume adjustment */
  if (stat->volume == 1 && amp > 0) {
    fprintf(stderr, "%.3f\n", ST_SAMPLE_MAX/(amp*scale));
    return ST_SUCCESS;
  }
  if (stat->volume == 2)
    fprintf(stderr, "\n\n");
  /* print out the info */
  fprintf(stderr, "Samples read:      %12u\n", stat->read);
  fprintf(stderr, "Length (seconds):  %12.6f\n", (double)stat->read/effp->ininfo.rate/effp->ininfo.channels);
  if (stat->srms)
    fprintf(stderr, "Scaled by rms:     %12.6f\n", rms);
  else
    fprintf(stderr, "Scaled by:         %12.1f\n", scale);
  fprintf(stderr, "Maximum amplitude: %12.6f\n", stat->max);
  fprintf(stderr, "Minimum amplitude: %12.6f\n", stat->min);
  fprintf(stderr, "Midline amplitude: %12.6f\n", stat->mid);
  fprintf(stderr, "Mean    norm:      %12.6f\n", stat->asum/ct);
  fprintf(stderr, "Mean    amplitude: %12.6f\n", stat->sum1/ct);
  fprintf(stderr, "RMS     amplitude: %12.6f\n", sqrt(stat->sum2/ct));

  fprintf(stderr, "Maximum delta:     %12.6f\n", stat->dmax);
  fprintf(stderr, "Minimum delta:     %12.6f\n", stat->dmin);
  fprintf(stderr, "Mean    delta:     %12.6f\n", stat->dsum1/(ct-1));
  fprintf(stderr, "RMS     delta:     %12.6f\n", sqrt(stat->dsum2/(ct-1)));
  freq = sqrt(stat->dsum2/stat->sum2)*effp->ininfo.rate/(M_PI*2);
  fprintf(stderr, "Rough   frequency: %12d\n", (int)freq);

  if (amp>0)
    fprintf(stderr, "Volume adjustment: %12.3f\n", ST_SAMPLE_MAX/(amp*scale));

  if (stat->bin[2] == 0 && stat->bin[3] == 0)
    fprintf(stderr, "\nProbably text, not sound\n");
  else {

    x = (float)(stat->bin[0] + stat->bin[3]) / (float)(stat->bin[1] + stat->bin[2]);

    if (x >= 3.0) {             /* use opposite encoding */
      if (effp->ininfo.encoding == ST_ENCODING_UNSIGNED)
        fprintf(stderr,"\nTry: -t raw -b -s \n");
      else
        fprintf(stderr,"\nTry: -t raw -b -u \n");
    } else if (x <= 1.0 / 3.0)
      ;                         /* correctly decoded */
    else if (x >= 0.5 && x <= 2.0) { /* use ULAW */
      if (effp->ininfo.encoding == ST_ENCODING_ULAW)
        fprintf(stderr,"\nTry: -t raw -b -u \n");
      else
        fprintf(stderr,"\nTry: -t raw -b -U \n");
    } else
      fprintf(stderr, "\nCan't guess the type\n");
  }

  /* Release FFT memory */
  free(stat->re_in);
  free(stat->re_out);

  return ST_SUCCESS;

}

static st_effect_t st_stat_effect = {
  "stat",
  "Usage: [ -s N ] [ -rms ] [-freq] [ -v ] [ -d ]",
  ST_EFF_MCHAN | ST_EFF_REPORT,
  st_stat_getopts,
  st_stat_start,
  st_stat_flow,
  st_stat_drain,
  st_stat_stop,
  st_effect_nothing
};

const st_effect_t *st_stat_effect_fn(void)
{
  return &st_stat_effect;
}
