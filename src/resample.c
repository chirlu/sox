
/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

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
 * NOTE: This source badly needs to be updated to reflect the latest
 * version of the above software!  Someone please perform this and
 * send patches to cbagwell@sprynet.com.
 */
/* Fixed bug: roll off frequency was wrong, too high by 2 when upsampling,
 * too low by 2 when downsampling.
 * Andreas Wilde, 12. Feb. 1999, andreas@eakaw2.et.tu-dresden.de
*/
/*
 * October 29, 1999
 * Various changes, bugfixes(?), increased precision, by Stan Brooks.
 *
 * This source code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include <math.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include "st.h"

/* resample includes */
#include "resdefs.h"
#include "resampl.h"

#define Float double/*float*/
#define ISCALE 0x10000

#define BUFFSIZE 8192 /*16384*/               /* Total buffer size */
#define L64 long long

/* Private data for Lerp via LCM file */
typedef struct resamplestuff {
   double Factor;     /* Factor = Fout/Fin sample rates */
   double rolloff;    /* roll-off frequency */
   double beta;       /* passband/stopband tuning magic */
   int quadr;         /* non-zero to use qprodUD quadratic interpolation */
   LONG Nmult;
   LONG Nwing;
   LONG Nq;
   Float *Imp;        /* impulse [Nwing+1] Filter coefficients */
   double Time;       /* Current time/pos in input sample */
   LONG dhb;
   LONG Xh;           /* number of past/future samples needed by filter  */
	 LONG Xoff;         /* Xh plus some room for creep  */
   LONG Xread;        /* X[Xread] is start-position to enter new samples */
   LONG Xp;           /* X[Xp] is position to start filter application   */
	 LONG Xsize,Ysize;  /* size (Floats) of X[],Y[]         */
   Float *X, *Y;      /* I/O buffers */
} *resample_t;

void LpFilter(P5(double c[],
		LONG N,
		double frq,
		double Beta,
		LONG Num));

int makeFilter(P5(Float Imp[],
		  LONG Nwing,
		  double Froll,
		  double Beta,
		  LONG Num));

static LONG SrcUD(P2(resample_t r, LONG Nx));


#if 0
static u_int32_t iprodC;
static u_int32_t iprodM;
#endif

/*
 * Process options
 */
void resample_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
	resample_t r = (resample_t) effp->priv;

	/* These defaults are conservative with respect to aliasing. */
	r->rolloff = 0.8;
	r->beta = 16; /* anything <=2 means Nutall window */
	r->quadr = 0;
	r->Nmult = 39;

	/* This used to fail, but with sox-12.15 it works. AW */
	if ((n >= 1)) {
		if (!strcmp(argv[0], "-qs")) {
			r->quadr = 1;
			n--; argv++;
		}
		else if (!strcmp(argv[0], "-q")) {
			r->rolloff = 0.9;
			r->quadr = 1;
			r->Nmult = 75;
			n--; argv++;
		}
		else if (!strcmp(argv[0], "-ql")) {
			r->rolloff = 0.9;
			r->quadr = 1;
			r->Nmult = 149;
			n--; argv++;
		}
	}

	if ((n >= 1) && !sscanf(argv[0], "%lf", &r->rolloff))
	  fail("Usage: resample [ rolloff [ beta ] ]");
	else if ((r->rolloff <= 0.01) || (r->rolloff >= 1.0))
	  fail("resample: rolloff factor (%f) no good, should be 0.01<x<1.0", r->rolloff);

	if ((n >= 2) && !sscanf(argv[1], "%lf", &r->beta))
	  fail("Usage: resample [ rolloff [ beta ] ]");
	else if (r->beta <= 2.0) {
	  r->beta = 0;
		report("resample opts: Nuttall window, cutoff %f\n", r->rolloff);
	} else {
		report("resample opts: Kaiser window, cutoff %f, beta %f\n", r->rolloff, r->beta);
	}

}

/*
 * Prepare processing.
 */
void resample_start(effp)
eff_t effp;
{
	resample_t r = (resample_t) effp->priv;
	LONG Xoff;
	int i;

	r->Factor = (double)effp->outinfo.rate / (double)effp->ininfo.rate;

	r->Nq = Nc; /* for now */

	/* Check for illegal constants */
# if 0
	if (Lp >= 16) fail("Error: Lp>=16");
	if (Nb+Nhg+NLpScl >= 32) fail("Error: Nb+Nhg+NLpScl>=32");
	if (Nh+Nb > 32) fail("Error: Nh+Nb>32");
# endif

	/* Nwing: # of filter coeffs in right wing */
	r->Nwing = r->Nq * (r->Nmult/2+1) + 1;

	r->Imp = (Float *)malloc(sizeof(Float) * (r->Nwing+2)) + 1;
	/* need Imp[-1] and Imp[Nwing] for quadratic interpolation */
	/* returns error # <=0, or adjusted wing-len > 0 */
	i = makeFilter(r->Imp, r->Nwing, r->rolloff, r->beta, r->Nq);
	if (i <= 0)
		fail("resample: Unable to make filter\n");

	report("Nmult: %ld, Nwing: %ld, Nq: %ld\n",r->Nmult,r->Nwing,r->Nq);

	r->dhb = Np;  /* Fixed-point Filter sampling-time-increment */
  if (r->Factor<1.0) r->dhb = r->Factor*Np + 0.5;
  r->Xh = (r->Nwing<<La)/r->dhb;
	/* (Xh * dhb)>>La is max index into Imp[] */

	/* reach of LP filter wings + some creeping room */
	Xoff = r->Xh + 10;
	r->Xoff = Xoff;

	/* Current "now"-sample pointer for input to filter */
	r->Xp = Xoff;
	/* Position in input array to read into */
	r->Xread = Xoff;
	/* Current-time pointer for converter */
	r->Time = Xoff;

	i = BUFFSIZE - 2*Xoff;
	if (i < r->Factor + 1.0/r->Factor)      /* Check input buffer size */
		fail("Factor is too small or large for BUFFSIZE");
	
	r->Xsize = 2*Xoff + i/(1.0+r->Factor);
	r->Ysize = BUFFSIZE - r->Xsize;
	report("Xsize %d, Ysize %d, Xoff %d",r->Xsize,r->Ysize,r->Xoff);

	r->X = (Float *) malloc(sizeof(Float) * (BUFFSIZE));
	r->Y = r->X + r->Xsize;

	/* Need Xoff zeros at beginning of sample */
	for (i=0; i<Xoff; i++)
		r->X[i] = 0;
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

void resample_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
LONG *isamp, *osamp;
{
	resample_t r = (resample_t) effp->priv;
	LONG i, last, Nout, Nx, Nproc;

	/* constrain amount we actually process */
	//fprintf(stderr,"Xp %d, Xread %d, isamp %d, ",r->Xp, r->Xread,*isamp);

	Nproc = r->Xsize - r->Xp;

	i = MIN(r->Ysize, *osamp);
	if (Nproc * r->Factor >= i)
	  Nproc = i / r->Factor;

	Nx = Nproc - r->Xread; /* space for right-wing future-data */
	if (Nx <= 0)
		fail("Nx not positive: %d", Nx);
	if (Nx > *isamp)
		Nx = *isamp;
	//fprintf(stderr,"Nx %d\n",Nx);

	if (ibuf == NULL) {
		for(i = r->Xread; i < Nx + r->Xread  ; i++) 
			r->X[i] = 0;
	} else {
		for(i = r->Xread; i < Nx + r->Xread  ; i++) 
			r->X[i] = (Float)(*ibuf++)/ISCALE;
	}
	last = i;
	Nproc = last - r->Xoff - r->Xp;

	if (Nproc <= 0) {
		/* fill in starting here next time */
		r->Xread = last;
		/* leave *isamp alone, we consumed it */
		*osamp = 0;
		return;
	}
	Nout = SrcUD(r, Nproc);
	//fprintf(stderr,"Nproc %d --> %d\n",Nproc,Nout);
	/* Move converter Nproc samples back in time */
	r->Time -= Nproc;
	/* Advance by number of samples processed */
	r->Xp += Nproc;
	/* Calc time accumulation in Time */
	{
	  LONG creep = r->Time - r->Xoff; 
	  if (creep)
	  {
	  	  r->Time -= creep;   /* Remove time accumulation   */
	  	  r->Xp += creep;     /* and add it to read pointer */
	  	  /* fprintf(stderr,"Nproc %ld, creep %ld\n",Nproc,creep); */
	  }
	}

	{
	LONG i,k;
	/* Copy back portion of input signal that must be re-used */
	k = r->Xp - r->Xoff;
	//fprintf(stderr,"k %d, last %d\n",k,last);
	for (i=0; i<last - k; i++) 
	    r->X[i] = r->X[i+k];

	/* Pos in input buff to read new data into */
	r->Xread = i;                 
	r->Xp = r->Xoff;

	for(i=0; i < Nout; i++)
		*obuf++ = r->Y[i] * ISCALE;

	*isamp = Nx;
	*osamp = Nout;

	}
}

/*
 * Process tail of input samples.
 */
void resample_drain(effp, obuf, osamp)
eff_t effp;
LONG *obuf;
LONG *osamp;
{
	resample_t r = (resample_t) effp->priv;
	LONG i, Nout, Nx;
	
	//fprintf(stderr,"Xp %d, Xread %d  <--- DRAIN\n",r->Xp, r->Xread);
	if (r->Xsize - r->Xread < r->Xoff)
		fail("resample_drain: Problem!\n");

	/* fill out end with Xoff zeros */
	for(i = 0; i < r->Xoff; i++)
		r->X[i + r->Xread] = 0;

	Nx = r->Xread - r->Xp;

	if (Nx * r->Factor >= *osamp)
		fail("resample_drain: Overran output buffer!\n");

	/* Resample stuff in input buffer */
	Nout = SrcUD(r, Nx);
	//fprintf(stderr,"Nproc %d --> %d\n",Nx,Nout);

	for(i = 0; i < Nout; i++)
		*obuf++ = r->Y[i] * ISCALE;

	*osamp = Nout;
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
void resample_stop(effp)
eff_t effp;
{
	resample_t r = (resample_t) effp->priv;
	
	free(r->Imp - 1);
	free(r->X);
	/* free(r->Y); Y is in same block starting at X */ 
	/*report("iC %d, iM %d, ratio %d", iprodC, iprodM, iprodM/iprodC);*/
}

/* over 90% of CPU time spent in this iprodUD() function */
/* quadratic interpolation */
static double qprodUD(Imp, Xp, Inc, T0, dhb, ct)
const Float Imp[], *Xp;
LONG Inc, dhb, ct;
double T0;
{
  const double f = 1.0/(1<<La);
  double v;
  LONG Ho;

	Ho = T0 * dhb;
	Ho += (ct-1)*dhb; /* so Float sum starts with smallest coef's */
	Xp += (ct-1)*Inc;
	v = 0;
  do {
    Float coef;
    LONG Hoh;
    Hoh = Ho>>La;
		coef = Imp[Hoh];
    {
      Float dm,dp,t;
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
static double iprodUD(Imp, Xp, Inc, T0, dhb, ct)
const Float Imp[], *Xp;
LONG Inc, dhb, ct;
double T0;
{
  const double f = 1.0/(1<<La);
  double v;
  LONG Ho;

  Ho = T0 * dhb;
	Ho += (ct-1)*dhb; /* so Float sum starts with smallest coef's */
  Xp += (ct-1)*Inc;
	v = 0;
  do {
    Float coef;
    LONG Hoh;
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

static LONG SrcUD(r, Nx)
resample_t r;
LONG Nx;
{
   Float *Ystart, *Y;
   double Factor;
   double dt;                  /* Step through input signal */
   double time;
   double (*prodUD)();
   int n;

   prodUD = (r->quadr)? qprodUD:iprodUD; /* quadratic or linear interp */
   Factor = r->Factor;
   time = r->Time;
   dt = 1.0/Factor;        /* Output sampling period */
   /*fprintf(stderr,"Factor %f, dt %f, ",Factor,dt); */
   /*fprintf(stderr,"Time %f, ",r->Time);*/
	 /* (Xh * dhb)>>La is max index into Imp[] */
	 /*fprintf(stderr,"ct=%d\n",ct);*/
   /*fprintf(stderr,"ct=%.2f %d\n",(double)r->Nwing*Na/r->dhb, r->Xh);*/
   Ystart = Y = r->Y;
   n = (int)ceil((double)Nx/dt);
   while(n--)
      {
      Float *Xp;
      double v;
      double T;
      T = time-floor(time);        /* fractional part of Time */
			Xp = r->X + (LONG)time;      /* Ptr to current input sample */

      /* Past  inner product: */
      v = (*prodUD)(r->Imp, Xp, -1, T, r->dhb, r->Xh); /* needs Np*Nmult in 31 bits */
      /* Future inner product: */
      v += (*prodUD)(r->Imp, Xp+1, 1, (1.0-T), r->dhb, r->Xh); /* prefer even total */

      if (Factor < 1) v *= Factor;
      *Y++ = v;              /* Deposit output */
      time += dt;            /* Move to next sample by time increment */
      }
   r->Time = time;
   /*fprintf(stderr,"Time %f\n",r->Time);*/
   return (Y - Ystart);        /* Return the number of output samples */
}

int makeFilter(Imp, Nwing, Froll, Beta, Num)
Float Imp[];
LONG Nwing, Num;
double Froll, Beta;
{
   double DCgain;
   double *ImpR;
   LONG Mwing, Dh, i;

   if (Nwing > MAXNWING)                      /* Check for valid parameters */
      return(-1);
   if ((Froll<=0) || (Froll>1))
      return(-2);

   /* it does help accuracy a bit to have the window stop at
    * a zero-crossing of the sinc function */
   Mwing = floor((double)Nwing/(Num/Froll))*(Num/Froll) +0.5;
   if (Mwing==0)
      return(-4);

   ImpR = (double *) malloc(sizeof(double) * Mwing);

   /* Design a Nuttall or Kaiser windowed Sinc low-pass filter */
   LpFilter(ImpR, Mwing, Froll, Beta, Num);

   /* 'correct' the DC gain of the lowpass filter */
   DCgain = 0;
   Dh = Num;                  /* Filter sampling period for factors>=1 */
   for (i=Dh; i<Mwing; i+=Dh)
      DCgain += ImpR[i];
   DCgain = 2*DCgain + ImpR[0];    /* DC gain of real coefficients */
   report("DCgain err=%.12f",DCgain-1.0);

   for (i=0; i<Mwing; i++) {
      Imp[i] = ImpR[i]/DCgain;
   }
   free(ImpR);
   for (i=Mwing; i<=Nwing; i++) Imp[i] = 0;
	 /* Imp[Nwing] and Imp[-1] needed for quadratic interpolation */
   Imp[-1] = Imp[1];

#  if 0
   {
      double v = 0;
      int s=-1;
      for (i=Num; i<Mwing; i+=Num, s=-s)
      v += s*Imp[i];
      report("NYQgain %.12f vs %.12f",Imp[0], -2*v);
      v = -2*v - Imp[0];
      v *= (double)Nc/(double)(2*Mwing+1);
      for (i=0; i<Mwing; i++)
          Imp[i] += v*cos(M_PI*i/Nc);

   }
#  endif
   
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

double Izero(x)
double x;
{
   double sum, u, halfx, temp;
   LONG n;

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

void LpFilter(c,N,frq,Beta,Num)
double c[], frq, Beta;
LONG N, Num;
{
   LONG i;

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
