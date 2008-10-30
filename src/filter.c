/* libSoX Windowed sinc lowpass/bandpass/highpass filter     November 18, 1999
 * Copyright 1999 Stan Brooks <stabro@megsinet.net>
 * Copyright 1994 Julius O. Smith
 * Copyright 1991 (?) Lance Norskog (?)
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
 *
 * REMARKS: (Stan Brooks speaking)
 * This code is a rewrite of filterkit.c from the `resample' package
 * by Julius O. Smith, now distributed under the LGPL.
 */

#include "sox_i.h"

#include <string.h>
#include <stdlib.h>

#define ISCALE 0x10000
#define BUFFSIZE 8192
#define MAXNWING  (80<<7)

/* Private data for Lerp via LCM file */
typedef struct {
        sox_rate_t rate;
        double freq0;/* low  corner freq */
        double freq1;/* high corner freq */
        double beta;/* >2 is kaiser window beta, <=2 selects nuttall window */
        long Nwin;
        double *Fp;/* [Xh+1] Filter coefficients */
        long Xh;/* number of past/future samples needed by filter  */
        long Xt;/* target to enter new data into X */
        double *X, *Y;/* I/O buffers */
} priv_t;

/* lsx_makeFilter is used by resample.c */
int lsx_makeFilter(double Fp[], long Nwing, double Froll, double Beta, long Num, int Normalize);

/* LpFilter()
 *
 * reference: "Digital Filters, 2nd edition"
 *            R.W. Hamming, pp. 178-179
 *
 * LpFilter() computes the coeffs of a Kaiser-windowed low pass filter with
 *    the following characteristics:
 *
 *       c[]  = array in which to store computed coeffs
 *       frq  = roll-off frequency of filter
 *       N    = Half the window length in number of coeffs
 *       Beta = parameter of Kaiser window
 *       Num  = number of coeffs before 1/frq
 *
 * Beta trades the rejection of the lowpass filter against the transition
 *    width from passband to stopband.  Larger Beta means a slower
 *    transition and greater stopband rejection.  See Rabiner and Gold
 *    (Theory and Application of DSP) under Kaiser windows for more about
 *    Beta.  The following table from Rabiner and Gold gives some feel
 *    for the effect of Beta:
 *
 * All ripples in dB, width of transition band = D*N where N = window length
 *
 *               BETA    D       PB RIP   SB RIP
 *               2.120   1.50  +-0.27      -30
 *               3.384   2.23    0.0864    -40
 *               4.538   2.93    0.0274    -50
 *               5.658   3.62    0.00868   -60
 *               6.764   4.32    0.00275   -70
 *               7.865   5.0     0.000868  -80
 *               8.960   5.7     0.000275  -90
 *               10.056  6.4     0.000087  -100
 */


static void LpFilter(double *c, long N, double frq, double Beta, long Num)
{
   long i;

   /* Calculate filter coeffs: */
   c[0] = frq;
   for (i=1; i<N; i++) {
      double x = M_PI*(double)i/(double)(Num);
      c[i] = sin(x*frq)/x;
   }

   if (Beta>2) { /* Apply Kaiser window to filter coeffs: */
      double IBeta = 1.0/lsx_bessel_I_0(Beta);
      for (i=1; i<N; i++) {
         double x = (double)i / (double)(N);
         c[i] *= lsx_bessel_I_0(Beta*sqrt(1.0-x*x)) * IBeta;
      }
   } else { /* Apply Nuttall window: */
      for(i = 0; i < N; i++) {
         double x = M_PI*i / N;
         c[i] *= 0.3635819 + 0.4891775*cos(x) + 0.1365995*cos(2*x) + 0.0106411*cos(3*x);
      }
   }
}

int lsx_makeFilter(double Imp[], long Nwing, double Froll, double Beta,
               long Num, int Normalize)
{
   double *ImpR;
   long Mwing, i;

   if (Nwing > MAXNWING)                      /* Check for valid parameters */
      return(-1);
   if ((Froll<=0) || (Froll>1))
      return(-2);

   /* it does help accuracy a bit to have the window stop at
    * a zero-crossing of the sinc function */
   Mwing = floor((double)Nwing/(Num/Froll))*(Num/Froll) +0.5;
   if (Mwing==0)
      return(-4);

   ImpR = lsx_malloc(sizeof(double) * Mwing);

   /* Design a Nuttall or Kaiser windowed Sinc low-pass filter */
   LpFilter(ImpR, Mwing, Froll, Beta, Num);

   if (Normalize) { /* 'correct' the DC gain of the lowpass filter */
      long Dh;
      double DCgain;
      DCgain = 0;
      Dh = Num;                  /* Filter sampling period for factors>=1 */
      for (i=Dh; i<Mwing; i+=Dh)
         DCgain += ImpR[i];
      DCgain = 2*DCgain + ImpR[0];    /* DC gain of real coefficients */
      lsx_debug("DCgain err=%.12f",DCgain-1.0);

      DCgain = 1.0/DCgain;
      for (i=0; i<Mwing; i++)
         Imp[i] = ImpR[i]*DCgain;

   } else {
      for (i=0; i<Mwing; i++)
         Imp[i] = ImpR[i];
   }
   free(ImpR);
   for (i=Mwing; i<=Nwing; i++) Imp[i] = 0;
   /* Imp[Mwing] and Imp[-1] needed for quadratic interpolation */
   Imp[-1] = Imp[1];

   return(Mwing);
}

static void FiltWin(priv_t * f, long Nx);

/*
 * Process options
 */
static int sox_filter_getopts(sox_effect_t * effp, int n, char **argv)
{
        priv_t * f = (priv_t *) effp->priv;

        f->beta = 16;  /* Kaiser window, beta 16 */
        f->Nwin = 128;

        f->freq0 = f->freq1 = 0;
        if (n >= 1) {
                char *p;
                p = argv[0];
                if (*p != '-') {
                        f->freq1 = lsx_parse_frequency(p, &p);
                }
                if (*p == '-') {
                        f->freq0 = f->freq1;
                        f->freq1 = lsx_parse_frequency(p+1, &p);
                }
                if (*p) f->freq1 = f->freq0 = 0;
        }
        lsx_debug("freq: %g-%g", f->freq0, f->freq1);
        if (f->freq0 == 0 && f->freq1 == 0)
          return lsx_usage(effp);

        if ((n >= 2) && !sscanf(argv[1], "%ld", &f->Nwin))
          return lsx_usage(effp);
        else if (f->Nwin < 4) {
                lsx_fail("filter: window length (%ld) <4 is too short", f->Nwin);
                return (SOX_EOF);
        }

        if ((n >= 3) && !sscanf(argv[2], "%lf", &f->beta))
          return lsx_usage(effp);

        lsx_debug("filter opts: %g-%g, window-len %ld, beta %f", f->freq0, f->freq1, f->Nwin, f->beta);
        return (SOX_SUCCESS);
}

static int p2(long n)
{
  int N;
  for (N = 1; n; n >>= 1, N <<= 1);
  return max(N, 1024);
}

/*
 * Prepare processing.
 */
static int sox_filter_start(sox_effect_t * effp)
{
        priv_t * f = (priv_t *) effp->priv;
        double *Fp0, *Fp1;
        long Xh0, Xh1, Xh;
        int i;

        f->rate = effp->in_signal.rate;

        /* adjust upper frequency to Nyquist if necessary */
        if (f->freq1 > (sox_sample_t)f->rate/2 || f->freq1 <= 0)
                f->freq1 = f->rate/2;

        if ((f->freq0 < 0) || (f->freq0 > f->freq1))
        {
                lsx_fail("filter: low(%g),high(%g) parameters must satisfy 0 <= low <= high <= %g",
                                        f->freq0, f->freq1, f->rate/2);
                return (SOX_EOF);
        }

        Xh = f->Nwin/2;
        Fp0 = lsx_malloc(sizeof(double) * (Xh + 2));
        ++Fp0;
        if (f->freq0 > (sox_sample_t)f->rate/200) {
                Xh0 = lsx_makeFilter(Fp0, Xh, 2.0*(double)f->freq0/f->rate, f->beta, (size_t) 1, 0);
                if (Xh0 <= 1)
                {
                        lsx_fail("filter: Unable to make low filter");
                        return (SOX_EOF);
                }
        } else {
                Xh0 = 0;
        }
        Fp1 = lsx_malloc(sizeof(double) * (Xh + 2));
        ++Fp1;
        /* need Fp[-1] and Fp[Xh] for lsx_makeFilter */
        if (f->freq1 < (sox_sample_t)f->rate/2) {
                Xh1 = lsx_makeFilter(Fp1, Xh, 2.0*(double)f->freq1/f->rate, f->beta, (size_t) 1, 0);
                if (Xh1 <= 1)
                {
                        lsx_fail("filter: Unable to make high filter");
                        return (SOX_EOF);
                }
        } else {
                Fp1[0] = 1.0;
                Xh1 = 1;
        }
        /* now subtract Fp0[] from Fp1[] */
        Xh = (Xh0>Xh1)?  Xh0:Xh1; /* >=1, by above */
        for (i=0; i<Xh; i++) {
                double c0,c1;
                c0 = (i<Xh0)? Fp0[i]:0;
                c1 = (i<Xh1)? Fp1[i]:0;
                Fp1[i] = c1-c0;
        }

        free(Fp0 - 1); /* all done with Fp0 */
        f->Fp = Fp1;

        Xh -= 1;       /* Xh = 0 can only happen if filter was identity 0-Nyquist */
        if (Xh<=0)
                lsx_warn("filter: adjusted freq %g-%g is identity", f->freq0, f->freq1);

        f->Nwin = 2*Xh + 1;  /* not really used afterwards */
        f->Xh = Xh;
        f->Xt = Xh;

   if (effp->global_info->plot == sox_plot_gnuplot) {
     int N = p2(2 * Xh + 1);
     double * h = lsx_calloc(N, sizeof(*h));
     double * H = lsx_malloc((N / 2 + 1) * sizeof(*H));
     for (i = 1; i < Xh + 1; ++i)
       h[i - 1] = f->Fp[Xh + 1 - i];
     for (i = 0; i < Xh + 1; ++i)
       h[Xh + i] = f->Fp[i];
     lsx_power_spectrum(N, h, H);
     free(h);
     printf(
       "# gnuplot file\n"
       "set title 'SoX effect: filter frequency=%g-%g'\n"
       "set xlabel 'Frequency (Hz)'\n"
       "set ylabel 'Amplitude Response (dB)'\n"
       "set grid xtics ytics\n"
       "set key off\n"
       "plot '-' with lines\n"
       , f->freq0, f->freq1);
     for (i = 0; i <= N/2; ++i)
       printf("%g %g\n", i * effp->in_signal.rate / N, 10 * log10(H[i]));
     printf(
       "e\n"
       "pause -1 'Hit return to continue'\n");
     free(H);
     return SOX_EOF;
   }

   if (effp->global_info->plot == sox_plot_octave) {
     printf("%% GNU Octave file (may also work with MATLAB(R) )\nb=[");
     for (i = 1; i < Xh + 1; ++i)
       printf("%24.16e\n", f->Fp[Xh + 1 - i]);
     for (i = 0; i < Xh + 1; ++i)
       printf("%24.16e\n", f->Fp[i]);
     printf("];\n"
       "[h,w]=freqz(b);\n"
       "plot(%g*w/pi,20*log10(h))\n"
       "title('SoX effect: filter frequency=%g-%g')\n"
       "xlabel('Frequency (Hz)')\n"
       "ylabel('Amplitude Response (dB)')\n"
       "grid on\n"
       "disp('Hit return to continue')\n"
       "pause\n"
       , effp->in_signal.rate / 2, f->freq0, f->freq1);
     return SOX_EOF;
   }

        f->X = lsx_malloc(sizeof(double) * (2*BUFFSIZE + 2*Xh));
        f->Y = f->X + BUFFSIZE + 2*Xh;

        /* Need Xh zeros at beginning of X */
        for (i = 0; i < Xh; i++)
                f->X[i] = 0;
        return (SOX_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
static int sox_filter_flow(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf,
                   size_t *isamp, size_t *osamp)
{
        priv_t * f = (priv_t *) effp->priv;
        size_t Nx;
        long i, Nproc;

        /* constrain amount we actually process */
        /* lsx_debug("Xh %d, Xt %d, isamp %d, ",f->Xh, f->Xt, *isamp); */
        Nx = BUFFSIZE + 2*f->Xh - f->Xt;
        if (Nx > *isamp) Nx = *isamp;
        if (Nx > *osamp) Nx = *osamp;
        *isamp = Nx;

        {
                double *xp, *xtop;
                xp = f->X + f->Xt;
                xtop = xp + Nx;
                if (ibuf != NULL) {
                        while (xp < xtop)
                                *xp++ = (double)(*ibuf++) / ISCALE;
                } else {
                        while (xp < xtop)
                                *xp++ = 0;
                }
        }

        Nproc = f->Xt + Nx - 2*f->Xh;

        if (Nproc <= 0) {
                f->Xt += Nx;
                *osamp = 0;
                return (SOX_SUCCESS);
        }
        lsx_debug("flow Nproc %ld",Nproc);
        FiltWin(f, Nproc);

        /* Copy back portion of input signal that must be re-used */
        Nx += f->Xt;
        if (f->Xh)
                memmove(f->X, f->X + Nx - 2*f->Xh, sizeof(double)*2*f->Xh);
        f->Xt = 2*f->Xh;

        for (i = 0; i < Nproc; i++)
                *obuf++ = f->Y[i] * ISCALE;

        *osamp = Nproc;
        return (SOX_SUCCESS);
}

/*
 * Process tail of input samples.
 */
static int sox_filter_drain(sox_effect_t * effp, sox_sample_t *obuf, size_t *osamp)
{
        priv_t * f = (priv_t *) effp->priv;
        long isamp_res, osamp_res;
        sox_sample_t *Obuf;

        lsx_debug("Xh %ld, Xt %ld  <--- DRAIN",f->Xh, f->Xt);

        /* stuff end with Xh zeros */
        isamp_res = f->Xh;
        osamp_res = *osamp;
        Obuf = obuf;
        while (isamp_res>0 && osamp_res>0) {
                size_t Isamp, Osamp;
                Isamp = isamp_res;
                Osamp = osamp_res;
                sox_filter_flow(effp, NULL, Obuf, &Isamp, &Osamp);
          /* lsx_debug("DRAIN isamp,osamp  (%d,%d) -> (%d,%d)",
                 * isamp_res,osamp_res,Isamp,Osamp); */
                Obuf += Osamp;
                osamp_res -= Osamp;
                isamp_res -= Isamp;
        };
        *osamp -= osamp_res;
        /* lsx_debug("DRAIN osamp %d", *osamp); */
        if (isamp_res)
                lsx_warn("drain overran obuf by %ld", isamp_res);
        /* FIXME: This is very picky. osamp better be big enough to grab
         * all remaining samples or they will be discarded.
         */
        return (SOX_EOF);
}

/*
 * Do anything required when you stop reading samples.
 * Don't close input file!
 */
static int sox_filter_stop(sox_effect_t * effp)
{
        priv_t * f = (priv_t *) effp->priv;

        free(f->Fp - 1);
        free(f->X);
        return (SOX_SUCCESS);
}

static double jprod(const double *Fp, const double *Xp, long ct)
{
        const double *fp, *xp, *xq;
        double v = 0;

        fp = Fp + ct;   /* so sum starts with smaller coef's */
        xp = Xp - ct;
        xq = Xp + ct;
        while (fp > Fp) {   /* ct = 0 can happen */
                v += *fp * (*xp + *xq);
                xp++; xq--; fp--;
        }
        v += *fp * *xp;
        return v;
}

static void FiltWin(priv_t * f, long Nx)
{
        double *Y;
        double *X, *Xend;

        Y = f->Y;
        X = f->X + f->Xh;                       /* Ptr to current input sample */
        Xend = X + Nx;
        while (X < Xend) {
                *Y++ = jprod(f->Fp, X, f->Xh);
                X++;
        }
}

static sox_effect_handler_t sox_filter_effect = {
  "filter",
  "low-high [ windowlength [ beta ] ]",
  0,
  sox_filter_getopts,
  sox_filter_start,
  sox_filter_flow,
  sox_filter_drain,
  sox_filter_stop,
  NULL, sizeof(priv_t)
};

const sox_effect_handler_t *sox_filter_effect_fn(void)
{
    return &sox_filter_effect;
}
