/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

/*
 * Sound Tools statistics "effect" file.
 *
 * Build various statistics on file and print them.
 *
 * Output is unmodified from input.
 *
 */

#include <math.h>
#include <string.h>
#include "st_i.h"

/* Private data for STAT effect */
typedef struct statstuff {
        double  min, max, mid;
        double  asum;
        double  sum1, sum2;     /* amplitudes */
        double  dmin, dmax;
        double  dsum1, dsum2;   /* deltas */
        double  scale;          /* scale-factor    */
        double  last;           /* previous sample */
        st_size_t read;         /* samples processed */
        int     volume;
        int     srms;
        int     fft;
        unsigned long   bin[4];
        double  *re;
        double  *im;
        unsigned long   fft_bits;
        unsigned long   fft_size;
        unsigned long   fft_offset;
} *stat_t;


/*
 * Process options
 */
int st_stat_getopts(eff_t effp, int n, char **argv)
{
        stat_t stat = (stat_t) effp->priv;

        stat->scale = ST_SAMPLE_MAX;
        stat->volume = 0;
        stat->srms = 0;
        stat->fft = 0;

        while (n>0)
        {
                if (!(strcmp(argv[0], "-v")))
                {
                        stat->volume = 1;
                }
                else if (!(strcmp(argv[0], "-s")))
                {
                        double scale;

                        if (n <= 1)
                        {
                          st_fail("-s option: invalid argument");
                          return (ST_EOF);
                        }
                        if (!sscanf(argv[1], "%lf", &scale))
                        {
                          st_fail("-s option: invalid argument");
                          return (ST_EOF);
                        }
                        stat->scale = scale;

                        /* Two option argument.  Account for this */
                        --n; ++argv;
                }
                else if (!(strcmp(argv[0], "-rms")))
                {
                        stat->srms = 1;
                }
                else if (!(strcmp(argv[0], "-freq")))
                {
                        stat->fft = 1;
                }
                else if (!(strcmp(argv[0], "-d"))) {
                        stat->volume = 2;
                }
                else
                {
                        st_fail("Summary effect: unknown option");
                        return(ST_EOF);
                }
                --n; ++argv;
        }
        return (ST_SUCCESS);
}

/*
 * Prepare processing.
 */
int st_stat_start(eff_t effp)
{
        stat_t stat = (stat_t) effp->priv;
        int i;
        unsigned long  bitmask;

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
        stat->re = 0;
        stat->im = 0;

        if (stat->fft)
        {
            bitmask = 0x80000000L;
            stat->fft_bits = 31;
            stat->fft_offset = 0;
            while (bitmask && !(stat->fft_size & bitmask))
            {
                bitmask = bitmask >> 1;
                stat->fft_bits--;
            }

            if (bitmask && (stat->fft_size & ~bitmask))
            {
                st_fail("FFT can only use sample buffers of 2^n. Buffer size used is %ld\n",stat->fft_size);
                return(ST_EOF);
            }

            stat->re = (double *)malloc(sizeof(double) * stat->fft_size);
            stat->im = (double *)malloc(sizeof(double) * stat->fft_size);

            if (!stat->re || !stat->im)
            {
                st_fail("Unable to allocate memory for FFT buffers.\n");
                return (ST_EOF);
            }
        }

        return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

static int FFT(short dir,long m,double *x,double *y);

int st_stat_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                 st_size_t *isamp, st_size_t *osamp)
{
        stat_t stat = (stat_t) effp->priv;
        int len, done, x;
        unsigned int x1;
        short count;
        float magnitude;
        float ffa;

        count = 0;
        len = ((*isamp > *osamp) ? *osamp : *isamp);
        if (len==0) return (ST_SUCCESS);

        if (stat->read == 0)    /* 1st sample */
                stat->min = stat->max = stat->mid = stat->last = (*ibuf)/stat->scale;

        if (stat->fft)
        {
            for (x = 0; x < len; x++)
            {
                stat->re[stat->fft_offset] = ibuf[x];
                stat->im[stat->fft_offset++] = 0;

                if (stat->fft_offset >= stat->fft_size)
                {
                    stat->fft_offset = 0;
                    FFT(1,stat->fft_bits,stat->re,stat->im);
                    ffa = (float)effp->ininfo.rate/stat->fft_size;
                    for (x1 = 0; x1 < stat->fft_size/2; x1++)
                    {
                        if (x1 == 0 || x1 == 1)
                        {
                            magnitude = 0.0; /* no DC */
                        }
                        else
                        {
                            magnitude = sqrt(stat->re[x1]*stat->re[x1] + stat->im[x1]*stat->im[x1]);
                            if (x1 == (stat->fft_size/2) - 1)
                                magnitude *= 2.0;
                        }
                        fprintf(stderr,"%f  %f\n",ffa*x1, magnitude);
                    }
                }

            }
        }


        for(done = 0; done < len; done++) {
                long lsamp;
                double samp, delta;
                /* work in scaled levels for both sample and delta */
                lsamp = *ibuf++;
                samp = (double)lsamp/stat->scale;
                stat->bin[(lsamp>>30)+2]++;
                *obuf++ = lsamp;


                if (stat->volume == 2)
                {
                    fprintf(stderr,"%08lx ",lsamp);
                    if (count++ == 5)
                    {
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
        return (ST_SUCCESS);
}

/*
 * Process tail of input samples.
 */
int st_stat_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
    stat_t stat = (stat_t) effp->priv;
    unsigned int x;
    float magnitude;
    float ffa;

    /* When we run out of samples, then we need to pad buffer with
     * zeros and then run FFT one last time.  Only perform this
     * operation if there are at least 1 sample in the buffer.
     */

    if (stat->fft && stat->fft_offset)
    {
        /* When we run out of samples, then we need to pad buffer with
         * zeros and then run FFT one last time.  Only perform this
         * operation if there are at least 1 sample in the buffer.
         */
        for (x = stat->fft_offset; x < stat->fft_size; x++)
        {
            stat->re[x] = 0;
            stat->im[x] = 0;
        }
        FFT(1,stat->fft_bits,stat->re,stat->im);
        ffa = (float)effp->ininfo.rate/stat->fft_size;
        for (x=0; x < stat->fft_size/2; x++)
        {
            if (x == 0 || x == 1)
            {
                magnitude = 0.0; /* no DC */
            }
            else
            {
                magnitude = sqrt(stat->re[x]*stat->re[x] + stat->im[x]*stat->im[x]);
                if (x != (stat->fft_size/2) - 1)
                    magnitude *= 2.0;
            }
            fprintf(stderr, "%f  %f\n",ffa*x, magnitude);
        }
    }

    *osamp = 0;
    return (ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.
 * Don't close input file!
 */
int st_stat_stop(eff_t effp)
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
                return (ST_SUCCESS);
        }
        if (stat->volume == 2) {
                fprintf(stderr, "\n\n");
        }
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

        if (amp>0) fprintf(stderr, "Volume adjustment: %12.3f\n", ST_SAMPLE_MAX/(amp*scale));

        if (stat->bin[2] == 0 && stat->bin[3] == 0)
                fprintf(stderr, "\nProbably text, not sound\n");
        else {

                x = (float)(stat->bin[0] + stat->bin[3]) / (float)(stat->bin[1] + stat->bin[2]);

                if (x >= 3.0)                  /* use opposite encoding */
                {
                        if (effp->ininfo.encoding == ST_ENCODING_UNSIGNED)
                        {
                                fprintf (stderr,"\nTry: -t raw -b -s \n");
                        }
                        else
                        {
                                fprintf (stderr,"\nTry: -t raw -b -u \n");
                        }

                }
                else if (x <= 1.0/3.0)
                {
                    ;;              /* correctly decoded */
                }
                else if (x >= 0.5 && x <= 2.0)       /* use ULAW */
                {
                        if (effp->ininfo.encoding == ST_ENCODING_ULAW)
                        {
                                fprintf (stderr,"\nTry: -t raw -b -u \n");
                        }
                        else
                        {
                                fprintf (stderr,"\nTry: -t raw -b -U \n");
                        }
                }
                else
                {
                        fprintf (stderr, "\nCan't guess the type\n");
                }
        }

        /* Release FFT memory if allocated */
        if (stat->re)
            free((void *)stat->re);
        if (stat->im)
            free((void *)stat->im);

        return (ST_SUCCESS);

}


/*
   This computes an in-place complex-to-complex FFT
   x and y are the real and imaginary arrays of 2^(m-1) points.
   dir =  1 gives forward transform
   dir = -1 gives reverse transform
*/
static int FFT(short dir,long m,double *re,double *im)
{
   long n,i,i1,j,k,i2,l,l1,l2;
   double c1,c2,tre,tim,t1,t2,u1,u2,z;

   /* Calculate the number of points */
   n = 1;
   for (i=0;i<m;i++)
      n *= 2;

   /* Do the bit reversal */
   i2 = n >> 1;

   j = 0;

   for (i=0;i<n-1;i++) {
      if (i < j) {
         tre = re[i];
         tim = im[i];
         re[i] = re[j];
         im[i] = im[j];
         re[j] = tre;
         im[j] = tim;
      }
      k = i2;
      while (k <= j) {
         j -= k;
         k >>= 1;
      }
      j += k;
   }

   /* Compute the FFT */
   c1 = -1.0;
   c2 = 0.0;
   l2 = 1;
   for (l=0;l<m;l++) {
      l1 = l2;
      l2 <<= 1;
      u1 = 1.0;
      u2 = 0.0;
      for (j=0;j<l1;j++) {
         for (i=j;i<n;i+=l2) {
            i1 = i + l1;
            t1 = u1 * re[i1] - u2 * im[i1];
            t2 = u1 * im[i1] + u2 * re[i1];
            re[i1] = re[i] - t1;
            im[i1] = im[i] - t2;
            re[i] += t1;
            im[i] += t2;
         }
         z =  u1 * c1 - u2 * c2;
         u2 = u1 * c2 + u2 * c1;
         u1 = z;
      }
      c2 = sqrt((1.0 - c1) / 2.0);
      if (dir == 1)
         c2 = -c2;
      c1 = sqrt((1.0 + c1) / 2.0);
   }

   /* Scaling for forward transform */
   if (dir == 1) {
      for (i=0;i<n;i++) {
         re[i] /= n;
         im[i] /= n;
      }
   }

   return ST_SUCCESS;
}
