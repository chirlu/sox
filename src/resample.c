/*
 * Sound Tools rate change effect file.
 * Spiffy rate changer using Smith & Wesson Bandwidth-Limited Interpolation.
 * The algorithm is described in "Bandlimited Interpolation -
 * Introduction and Algorithm" by Julian O. Smith III.
 * Available on ccrma-ftp.stanford.edu as
 * pub/BandlimitedInterpolation.eps.Z or similar.
 *
 * The latest stand alone version of this algorithm can be found
 * at ftp://ccrma-ftp.stanford.edu/pub/NeXT/
 * under the name of resample-version.number.tar.Z
 *
 *
 * FILE: resample.h
 *   BY: Julius Smith (at CCRMA, Stanford U)
 * C BY: translated from SAIL to C by Christopher Lee Fraley
 *          (cf0v@andrew.cmu.edu)
 * DATE: 7-JUN-88
 * VERS: 2.0  (17-JUN-88, 3:00pm)
 *
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 *
 * This source code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Fixed bug: roll off frequency was wrong, too high by 2 when upsampling,
 * too low by 2 when downsampling.
 * Andreas Wilde, 12. Feb. 1999, andreas@eakaw2.et.tu-dresden.de
 *
 * October 29, 1999
 * Various changes, bugfixes(?), increased precision, by Stan Brooks.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "st_i.h"


/* Conversion constants */
#define Lc        7
#define Nc       (1<<Lc)
#define La        16
#define Na       (1<<La)
#define Lp       (Lc+La)
#define Np       (1<<Lp)
#define Amask    (Na-1)
#define Pmask    (Np-1)

#define MAXNWING  (80<<Lc)
/* Description of constants:
 *
 * Nc - is the number of look-up values available for the lowpass filter
 *    between the beginning of its impulse response and the "cutoff time"
 *    of the filter.  The cutoff time is defined as the reciprocal of the
 *    lowpass-filter cut off frequence in Hz.  For example, if the
 *    lowpass filter were a sinc function, Nc would be the index of the
 *    impulse-response lookup-table corresponding to the first zero-
 *    crossing of the sinc function.  (The inverse first zero-crossing
 *    time of a sinc function equals its nominal cutoff frequency in Hz.)
 *    Nc must be a power of 2 due to the details of the current
 *    implementation. The default value of 128 is sufficiently high that
 *    using linear interpolation to fill in between the table entries
 *    gives approximately 16-bit precision, and quadratic interpolation
 *    gives about 23-bit (float) precision in filter coefficients.
 *
 * Lc - is log base 2 of Nc.
 *
 * La - is the number of bits devoted to linear interpolation of the
 *    filter coefficients.
 *
 * Lp - is La + Lc, the number of bits to the right of the binary point
 *    in the integer "time" variable. To the left of the point, it indexes
 *    the input array (X), and to the right, it is interpreted as a number
 *    between 0 and 1 sample of the input X.  The default value of 23 is
 *    about right.  There is a constraint that the filter window must be
 *    "addressable" in a int32_t, more precisely, if Nmult is the number
 *    of sinc zero-crossings in the right wing of the filter window, then
 *    (Nwing<<Lp) must be expressible in 31 bits.
 *
 */

static st_effect_t st_resample_effect;

#define ISCALE 0x10000

/* largest factor for which exact-coefficients upsampling will be used */
#define NQMAX 511

#define BUFFSIZE 8192 /*16384*/  /* Total I/O buffer size */

/* Private data for Lerp via LCM file */
typedef struct resamplestuff {
   double Factor;     /* Factor = Fout/Fin sample rates */
   double rolloff;    /* roll-off frequency */
   double beta;       /* passband/stopband tuning magic */
   int quadr;         /* non-zero to use qprodUD quadratic interpolation */
   long Nmult;
   long Nwing;
   long Nq;
   double *Imp;        /* impulse [Nwing+1] Filter coefficients */

   double Time;       /* Current time/pos in input sample */
   long dhb;

   long a,b;          /* gcd-reduced input,output rates   */
   long t;            /* Current time/pos for exact-coeff's method */

   long Xh;           /* number of past/future samples needed by filter  */
   long Xoff;         /* Xh plus some room for creep  */
   long Xread;        /* X[Xread] is start-position to enter new samples */
   long Xp;           /* X[Xp] is position to start filter application   */
   unsigned long Xsize,Ysize; /* size (doubles) of X[],Y[]         */
   double *X, *Y;      /* I/O buffers */
} *resample_t;

static void LpFilter(double c[],
                     long N,
                     double frq,
                     double Beta,
                     long Num);

/* makeFilter is used by filter.c */
int makeFilter(double Imp[],
               long Nwing,
               double Froll,
               double Beta,
               long Num,
               int Normalize);

static long SrcUD(resample_t r, long Nx);
static long SrcEX(resample_t r, long Nx);


/*
 * Process options
 */
int st_resample_getopts(eff_t effp, int n, char **argv)
{
        resample_t r = (resample_t) effp->priv;

        /* These defaults are conservative with respect to aliasing. */
        r->rolloff = 0.80;
        r->beta = 16; /* anything <=2 means Nutall window */
        r->quadr = 0;
        r->Nmult = 45;

        if (n >= 1) {
                if (!strcmp(argv[0], "-qs")) {
                        r->quadr = 1;
                        n--; argv++;
                }
                else if (!strcmp(argv[0], "-q")) {
                        r->rolloff = 0.875;
                        r->quadr = 1;
                        r->Nmult = 75;
                        n--; argv++;
                }
                else if (!strcmp(argv[0], "-ql")) {
                        r->rolloff = 0.94;
                        r->quadr = 1;
                        r->Nmult = 149;
                        n--; argv++;
                }
        }

        if ((n >= 1) && (sscanf(argv[0], "%lf", &r->rolloff) != 1)) {
          st_fail(st_resample_effect.usage);
          return (ST_EOF);
        } else if ((r->rolloff <= 0.01) || (r->rolloff >= 1.0)) {
          st_fail("resample: rolloff factor (%f) no good, should be 0.01<x<1.0", r->rolloff);
          return(ST_EOF);
        }

        if ((n >= 2) && !sscanf(argv[1], "%lf", &r->beta)) {
        	st_fail(st_resample_effect.usage);
          	return (ST_EOF);
        } else if (r->beta <= 2.0) {
        	r->beta = 0;
                st_debug("resample opts: Nuttall window, cutoff %f", r->rolloff);
        } else
                st_debug("resample opts: Kaiser window, cutoff %f, beta %f", r->rolloff, r->beta);
        return (ST_SUCCESS);
}

/*
 * Prepare processing.
 */
int st_resample_start(eff_t effp)
{
  resample_t r = (resample_t) effp->priv;
  long Xoff, gcdrate;
  int i;

  /* The next line makes the "speed" effect accurate; it's needed because
   * ininfo.rate (st_rate_t) isn't floating point (but it's probably not worth
   * changing st_rate_t just because of this): */
  double in_rate = floor(effp->ininfo.rate / effp->globalinfo->speed + .5)
    * effp->globalinfo->speed;

  if (in_rate == effp->outinfo.rate)
    return ST_EFF_NULL;
          
  r->Factor = (double) effp->outinfo.rate / in_rate;

  gcdrate = st_gcd((long) effp->ininfo.rate, (long) effp->outinfo.rate);
  r->a = effp->ininfo.rate / gcdrate;
  r->b = effp->outinfo.rate / gcdrate;

  if (r->a <= r->b && r->b <= NQMAX) {
    r->quadr = -1;      /* exact coeffs */
    r->Nq = r->b;       /* max(r->a,r->b) */
  } else
    r->Nq = Nc; /* for now */

  /* Nwing: # of filter coeffs in right wing */
  r->Nwing = r->Nq * (r->Nmult / 2 + 1) + 1;

  r->Imp = (double *) xmalloc(sizeof(double) * (r->Nwing + 2)) + 1;
  /* need Imp[-1] and Imp[Nwing] for quadratic interpolation */
  /* returns error # <=0, or adjusted wing-len > 0 */
  i = makeFilter(r->Imp, r->Nwing, r->rolloff, r->beta, r->Nq, 1);
  if (i <= 0) {
    st_fail("resample: Unable to make filter");
    return (ST_EOF);
  }

  st_debug("Nmult: %ld, Nwing: %ld, Nq: %ld", r->Nmult, r->Nwing, r->Nq);

  if (r->quadr < 0) {     /* exact coeff's method */
    r->Xh = r->Nwing / r->b;
    st_debug("resample: rate ratio %ld:%ld, coeff interpolation not needed", r->a, r->b);
  } else {
    r->dhb = Np;        /* Fixed-point Filter sampling-time-increment */
    if (r->Factor < 1.0)
      r->dhb = r->Factor * Np + 0.5;
    r->Xh = (r->Nwing << La) / r->dhb;
    /* (Xh * dhb)>>La is max index into Imp[] */
  }

  /* reach of LP filter wings + some creeping room */
  Xoff = r->Xh + 10;
  r->Xoff = Xoff;

  /* Current "now"-sample pointer for input to filter */
  r->Xp = Xoff;
  /* Position in input array to read into */
  r->Xread = Xoff;
  /* Current-time pointer for converter */
  r->Time = Xoff;
  if (r->quadr < 0) {     /* exact coeff's method */
    r->t = Xoff * r->Nq;
  }
  i = BUFFSIZE - 2 * Xoff;
  if (i < r->Factor + 1.0 / r->Factor) {  /* Check input buffer size */
    st_fail("Factor is too small or large for BUFFSIZE");
    return (ST_EOF);
  }

  r->Xsize = 2 * Xoff + i / (1.0 + r->Factor);
  r->Ysize = BUFFSIZE - r->Xsize;
  st_debug("Xsize %d, Ysize %d, Xoff %d", r->Xsize, r->Ysize, r->Xoff);

  r->X = (double *) xmalloc(sizeof(double) * (BUFFSIZE));
  r->Y = r->X + r->Xsize;

  /* Need Xoff zeros at beginning of sample */
  for (i = 0; i < Xoff; i++)
    r->X[i] = 0;
  return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
int st_resample_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                     st_size_t *isamp, st_size_t *osamp)
{
        resample_t r = (resample_t) effp->priv;
        long i, last, Nout, Nx, Nproc;

        st_debug("Xp %d, Xread %d, isamp %d, ",r->Xp, r->Xread,*isamp);

        /* constrain amount we actually process */
        Nproc = r->Xsize - r->Xp;

        i = (r->Ysize < *osamp)? r->Ysize : *osamp;
        if (Nproc * r->Factor >= i)
          Nproc = i / r->Factor;

        Nx = Nproc - r->Xread; /* space for right-wing future-data */
        if (Nx <= 0)
        {
                st_fail("resample: Can not handle this sample rate change. Nx not positive: %d", Nx);
                return (ST_EOF);
        }
        if ((unsigned long)Nx > *isamp)
                Nx = *isamp;
        st_debug("Nx %d",Nx);

        if (ibuf == NULL) {
                for(i = r->Xread; i < Nx + r->Xread  ; i++) 
                        r->X[i] = 0;
        } else {
                for(i = r->Xread; i < Nx + r->Xread  ; i++) 
                        r->X[i] = (double)(*ibuf++)/ISCALE;
        }
        last = i;
        Nproc = last - r->Xoff - r->Xp;

        if (Nproc <= 0) {
                /* fill in starting here next time */
                r->Xread = last;
                /* leave *isamp alone, we consumed it */
                *osamp = 0;
                return (ST_SUCCESS);
        }
        if (r->quadr < 0) { /* exact coeff's method */
                long creep; 
                Nout = SrcEX(r, Nproc);
                st_debug("Nproc %d --> %d",Nproc,Nout);
                /* Move converter Nproc samples back in time */
                r->t -= Nproc * r->b;
                /* Advance by number of samples processed */
                r->Xp += Nproc;
                /* Calc time accumulation in Time */
                creep = r->t/r->b - r->Xoff; 
                if (creep)
                {
                  r->t -= creep * r->b;  /* Remove time accumulation   */
                  r->Xp += creep;        /* and add it to read pointer */
                  st_debug("Nproc %ld, creep %ld",Nproc,creep);
                }
        } else { /* approx coeff's method */
                long creep; 
                Nout = SrcUD(r, Nproc);
                st_debug("Nproc %d --> %d",Nproc,Nout);
                /* Move converter Nproc samples back in time */
                r->Time -= Nproc;
                /* Advance by number of samples processed */
                r->Xp += Nproc;
                /* Calc time accumulation in Time */
                creep = r->Time - r->Xoff; 
                if (creep)
                {
                  r->Time -= creep;   /* Remove time accumulation   */
                  r->Xp += creep;     /* and add it to read pointer */
                  st_debug("Nproc %ld, creep %ld",Nproc,creep);
                }
        }

        {
        long i,k;
        /* Copy back portion of input signal that must be re-used */
        k = r->Xp - r->Xoff;
        st_debug("k %d, last %d",k,last);
        for (i=0; i<last - k; i++) 
            r->X[i] = r->X[i+k];

        /* Pos in input buff to read new data into */
        r->Xread = i;                 
        r->Xp = r->Xoff;

        for(i=0; i < Nout; i++) { 
                double ftemp = r->Y[i] * ISCALE;

                ST_SAMPLE_CLIP_COUNT(ftemp, effp->clips);
                *obuf++ = ftemp;
        }

        *isamp = Nx;
        *osamp = Nout;

        }
        return (ST_SUCCESS);
}

/*
 * Process tail of input samples.
 */
int st_resample_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
        resample_t r = (resample_t) effp->priv;
        long isamp_res, osamp_res;
        st_sample_t *Obuf;
        int rc;

        st_debug("Xoff %d  <--- DRAIN",r->Xoff);

        /* stuff end with Xoff zeros */
        isamp_res = r->Xoff;
        osamp_res = *osamp;
        Obuf = obuf;
        while (isamp_res>0 && osamp_res>0) {
                st_sample_t Isamp, Osamp;
                Isamp = isamp_res;
                Osamp = osamp_res;
                rc = st_resample_flow(effp, NULL, Obuf, (st_size_t *)&Isamp, (st_size_t *)&Osamp);
                if (rc)
                    return rc;
                st_debug("DRAIN isamp,osamp  (%d,%d) -> (%d,%d)",
                         isamp_res,osamp_res,Isamp,Osamp);
                Obuf += Osamp;
                osamp_res -= Osamp;
                isamp_res -= Isamp;
        }
        *osamp -= osamp_res;
        st_debug("DRAIN osamp %d", *osamp);
        if (isamp_res)
                st_warn("drain overran obuf by %d", isamp_res);
        /* FIXME: This is very picky.  IF obuf is not big enough to
         * drain remaining samples, they will be lost.
         */
        return (ST_EOF);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_resample_stop(eff_t effp)
{
        resample_t r = (resample_t) effp->priv;
        
        free(r->Imp - 1);
        free(r->X);
        /* free(r->Y); Y is in same block starting at X */ 
        return (ST_SUCCESS);
}

/* over 90% of CPU time spent in this iprodUD() function */
/* quadratic interpolation */
static double qprodUD(const double Imp[], const double *Xp, long Inc, double T0, 
                      long dhb, long ct)
{
  const double f = 1.0/(1<<La);
  double v;
  long Ho;

  Ho = T0 * dhb;
  Ho += (ct-1)*dhb; /* so double sum starts with smallest coef's */
  Xp += (ct-1)*Inc;
  v = 0;
  do {
    double coef;
    long Hoh;
    Hoh = Ho>>La;
    coef = Imp[Hoh];
    {
      double dm,dp,t;
      dm = coef - Imp[Hoh-1];
      dp = Imp[Hoh+1] - coef;
      t =(Ho & Amask) * f;
      coef += ((dp-dm)*t + (dp+dm))*t*0.5;
    }
    /* filter coef, lower La bits by quadratic interpolation */
    v += coef * *Xp;   /* sum coeff * input sample */
    Xp -= Inc;     /* Input signal step. NO CHECK ON ARRAY BOUNDS */
    Ho -= dhb;     /* IR step */
  } while(--ct);
  return v;
}

/* linear interpolation */
static double iprodUD(const double Imp[], const double *Xp, long Inc, 
                      double T0, long dhb, long ct)
{
  const double f = 1.0/(1<<La);
  double v;
  long Ho;

  Ho = T0 * dhb;
  Ho += (ct-1)*dhb; /* so double sum starts with smallest coef's */
  Xp += (ct-1)*Inc;
  v = 0;
  do {
    double coef;
    long Hoh;
    Hoh = Ho>>La;
    /* if (Hoh >= End) break; */
    coef = Imp[Hoh] + (Imp[Hoh+1]-Imp[Hoh]) * (Ho & Amask) * f;
    /* filter coef, lower La bits by linear interpolation */
    v += coef * *Xp;   /* sum coeff * input sample */
    Xp -= Inc;     /* Input signal step. NO CHECK ON ARRAY BOUNDS */
    Ho -= dhb;     /* IR step */
  } while(--ct);
  return v;
}

/* From resample:filters.c */
/* Sampling rate conversion subroutine */

static long SrcUD(resample_t r, long Nx)
{
   double *Ystart, *Y;
   double Factor;
   double dt;                  /* Step through input signal */
   double time;
   double (*prodUD)(const double[], const double *, long, double, long, long);
   int n;

   prodUD = (r->quadr)? qprodUD:iprodUD; /* quadratic or linear interp */
   Factor = r->Factor;
   time = r->Time;
   dt = 1.0/Factor;        /* Output sampling period */
   st_debug("Factor %f, dt %f, ",Factor,dt);
   st_debug("Time %f, ",r->Time);
   /* (Xh * dhb)>>La is max index into Imp[] */
   st_debug("ct=%.2f %d",(double)r->Nwing*Na/r->dhb, r->Xh);
   st_debug("ct=%ld, T=%.6f, dhb=%6f, dt=%.6f",
                         r->Xh, time-floor(time),(double)r->dhb/Na,dt);
   Ystart = Y = r->Y;
   n = (int)ceil((double)Nx/dt);
   while(n--)
      {
      double *Xp;
      double v;
      double T;
      T = time-floor(time);        /* fractional part of Time */
      Xp = r->X + (long)time;      /* Ptr to current input sample */

      /* Past  inner product: */
      v = (*prodUD)(r->Imp, Xp, -1, T, r->dhb, r->Xh); /* needs Np*Nmult in 31 bits */
      /* Future inner product: */
      v += (*prodUD)(r->Imp, Xp+1, 1, (1.0-T), r->dhb, r->Xh); /* prefer even total */

      if (Factor < 1) v *= Factor;
      *Y++ = v;              /* Deposit output */
      time += dt;            /* Move to next sample by time increment */
      }
   r->Time = time;
   st_debug("Time %f",r->Time);
   return (Y - Ystart);        /* Return the number of output samples */
}

/* exact coeff's */
static double prodEX(const double Imp[], const double *Xp, 
                     long Inc, long T0, long dhb, long ct)
{
  double v;
  const double *Cp;

  Cp  = Imp + (ct-1)*dhb + T0; /* so double sum starts with smallest coef's */
  Xp += (ct-1)*Inc;
  v = 0;
  do {
    v += *Cp * *Xp;   /* sum coeff * input sample */
    Cp -= dhb;     /* IR step */
    Xp -= Inc;     /* Input signal step. */
  } while(--ct);
  return v;
}

static long SrcEX(resample_t r, long Nx)
{
   double *Ystart, *Y;
   double Factor;
   long a,b;
   long time;
   int n;

   Factor = r->Factor;
   time = r->t;
   a = r->a;
   b = r->b;
   Ystart = Y = r->Y;
   n = (Nx*b + (a-1))/a;
   while(n--)
      {
        double *Xp;
        double v;
        long T;
        T = time % b;              /* fractional part of Time */
        Xp = r->X + (time/b);      /* Ptr to current input sample */

        /* Past  inner product: */
        v = prodEX(r->Imp, Xp, -1, T, b, r->Xh);
        /* Future inner product: */
        v += prodEX(r->Imp, Xp+1, 1, b-T, b, r->Xh);

        if (Factor < 1) v *= Factor;
        *Y++ = v;             /* Deposit output */
        time += a;            /* Move to next sample by time increment */
      }
   r->t = time;
   return (Y - Ystart);        /* Return the number of output samples */
}

int makeFilter(double Imp[], long Nwing, double Froll, double Beta, 
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

   ImpR = (double *) xmalloc(sizeof(double) * Mwing);

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
      st_debug("DCgain err=%.12f",DCgain-1.0);
  
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

/* LpFilter()
 *
 * reference: "Digital Filters, 2nd edition"
 *            R.W. Hamming, pp. 178-179
 *
 * Izero() computes the 0th order modified bessel function of the first kind.
 *    (Needed to compute Kaiser window).
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


#define IzeroEPSILON 1E-21               /* Max error acceptable in Izero */

static double Izero(double x)
{
   double sum, u, halfx, temp;
   long n;

   sum = u = n = 1;
   halfx = x/2.0;
   do {
      temp = halfx/(double)n;
      n += 1;
      temp *= temp;
      u *= temp;
      sum += u;
   } while (u >= IzeroEPSILON*sum);
   return(sum);
}

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
      double IBeta = 1.0/Izero(Beta);
      for (i=1; i<N; i++) {
         double x = (double)i / (double)(N);
         c[i] *= Izero(Beta*sqrt(1.0-x*x)) * IBeta;
      }
   } else { /* Apply Nuttall window: */
      for(i = 0; i < N; i++) {
         double x = M_PI*i / N;
         c[i] *= 0.36335819 + 0.4891775*cos(x) + 0.1365995*cos(2*x) + 0.0106411*cos(3*x);
      }
   }
}

static st_effect_t st_resample_effect = {
   "resample",
   "Usage: resample [ -qs | -q | -ql ] [ rolloff [ beta ] ]",
   ST_EFF_RATE,
   st_resample_getopts,
   st_resample_start,
   st_resample_flow,
   st_resample_drain,
   st_resample_stop,
  st_effect_nothing
};

const st_effect_t *st_resample_effect_fn(void)
{
    return &st_resample_effect;
}
