
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
#include <math.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include "st.h"

/* resample includes */
#include "resdefs.h"
#include "resampl.h"

#define IBUFFSIZE 4096                         /* Input buffer size */
#define OBUFFSIZE (IBUFFSIZE*MAXFACTOR+2)      /* Calc'd out buffer size */

/* Private data for Lerp via LCM file */
typedef struct resamplestuff {
   double Factor;               /* Factor = Fout/Fin sample rates */
   double rolloff;              /* roll-off frequency */
   double beta;                 /* passband/stopband tuning magic */
   short InterpFilt;      	/* TRUE means interpolate filter coeffs */
   UHWORD Oskip;		/* number of bogus output samples at start */
   UHWORD LpScl, Nmult, Nwing;
   HWORD *Imp;         		/* impulse [MAXNWING] Filter coefficients */
   HWORD *ImpD;        		/* [MAXNWING] ImpD[n] = Imp[n+1]-Imp[n] */
   /* for resample main loop */
   UWORD Time;                  /* Current time/pos in input sample */
   UHWORD Xp, Xoff, Xread;
   HWORD *X, *Y; 		/* I/O buffers */
} *resample_t;

int makeFilter(P6(HWORD Imp[],
		  HWORD ImpD[],
		  UHWORD *LpScl,
		  UHWORD Nwing,
		  double Froll,
		  double Beta));
HWORD SrcUp(P10(HWORD X[],
		HWORD Y[],
		double Factor,
		UWORD *Time,
		UHWORD Nx,
		UHWORD Nwing,
		UHWORD LpScl,
		HWORD Imp[],
		HWORD ImpD[],
		BOOL Interp));
HWORD SrcUD(P10(HWORD X[],
		HWORD Y[],
		double Factor,
		UWORD *Time,
		UHWORD Nx,
		UHWORD Nwing,
		UHWORD LpScl,
		HWORD Imp[],
		HWORD ImpD[],
		BOOL Interp));
IWORD FilterUp(P7(HWORD Imp[],
		  HWORD ImpD[],
		  UHWORD Nwing,
		  BOOL Interp,
		  HWORD *Xp,
		  HWORD Ph,
		  HWORD Inc));
IWORD FilterUD(P8(HWORD Imp[],
		  HWORD ImpD[],
		  UHWORD Nwing,
		  BOOL Interp,
		  HWORD *Xp,
		  HWORD Ph,
		  HWORD Inc,
		  UHWORD dhb));

/*
 * Process options
 */
void resample_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
	resample_t resample = (resample_t) effp->priv;

	/* These defaults are conservative with respect to aliasing. */
	resample->rolloff = 0.8;
	resample->beta = 17.5;

	/* This used to fail, but with sox-12.15 it works. AW */
	if ((n >= 1) && !sscanf(argv[0], "%lf", &resample->rolloff))
		fail("Usage: resample [ rolloff [ beta ] ]");
	else if ((resample->rolloff < 0.01) || (resample->rolloff > 1.0))
	    fail("resample: rolloff factor (%f) no good, should be 0.01<x<1.0", 
			resample->rolloff);
	if ((n >= 2) && !sscanf(argv[1], "%lf", &resample->beta))
		fail("Usage: resample [ rolloff [ beta ] ]");
	else if (resample->beta < 1.0)
	        fail("resample: beta factor (%f) no good, should be >= 1.0", 
			resample->beta);
	report("resample opts: %f, %f\n", 
		resample->rolloff, resample->beta);
}

/*
 * Prepare processing.
 */
void resample_start(effp)
eff_t effp;
{
	resample_t resample = (resample_t) effp->priv;
	int i;
	
	resample->InterpFilt = 1;	/* interpolate filter: slower */
	resample->Factor = 
		(double)effp->outinfo.rate / (double)effp->ininfo.rate;
	
	/* Check for illegal constants */
	if (Np >= 16)
		fail("Error: Np>=16");
	if (Nb+Nhg+NLpScl >= 32)
		fail("Error: Nb+Nhg+NLpScl>=32");
	if (Nh+Nb > 32)
	      fail("Error: Nh+Nb>32");


	resample->Imp = (HWORD *) malloc(sizeof(HWORD) * MAXNWING);
	resample->ImpD = (HWORD *) malloc(sizeof(HWORD) * MAXNWING);
	resample->X = (HWORD *) malloc(sizeof(HWORD) * IBUFFSIZE);
	resample->Y = (HWORD *) malloc(sizeof(HWORD) * OBUFFSIZE);

	/* upsampling requires smaller Nmults */
	for(resample->Nmult = 37; resample->Nmult > 1; resample->Nmult -= 2) {
		/* # of filter coeffs in right wing */
		resample->Nwing = Npc*(resample->Nmult+1)/2;     
		/* This prevents just missing last coeff */
		/*   for integer conversion factors  */
		resample->Nwing += Npc/2 + 1;      

		/* returns error # or 0 for success */
		if (makeFilter(resample->Imp, resample->ImpD, 
				&resample->LpScl, resample->Nwing, 
				resample->rolloff, resample->beta))
				continue;
			else
				break;
			
	}

	if(resample->Nmult == 1)
		fail("resample: Unable to make filter\n");

	if (resample->Factor < 1)
		resample->LpScl = resample->LpScl*resample->Factor + 0.5;
	/* Calc reach of LP filter wing & give some creeping room */
	resample->Xoff = ((resample->Nmult+1)/2.0) * 
		MAX(1.0,1.0/resample->Factor) + 10;
	if (IBUFFSIZE < 2*resample->Xoff)      /* Check input buffer size */
		fail("IBUFFSIZE (or Factor) is too small");

	/* Current "now"-sample pointer for input */
	resample->Xp = resample->Xoff;             
	/* Position in input array to read into */
	resample->Xread = resample->Xoff;          
	/* Current-time pointer for converter */
	resample->Time = (resample->Xoff<<Np);     

	/* Set sample drop at beginning */
	resample->Oskip = resample->Xread * resample->Factor;

	/* Need Xoff zeros at begining of sample */
	for (i=0; i<resample->Xoff; i++)
		resample->X[i] = 0;
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
	resample_t resample = (resample_t) effp->priv;
	LONG i, last, creep, Nout, Nx;
	UHWORD Nproc;

	/* constrain amount we actually process */
	Nproc = IBUFFSIZE - resample->Xp;
	if (Nproc * resample->Factor >= OBUFFSIZE)
		Nproc = OBUFFSIZE / resample->Factor;
	if (Nproc * resample->Factor >= *osamp)
		Nproc = *osamp / resample->Factor;
	
	Nx = Nproc - resample->Xread;
	if (Nx <= 0)
		fail("Nx negative: %d", Nx);
	if (Nx > *isamp) {
		Nx = *isamp;
	}
	for(i = resample->Xread; i < Nx + resample->Xread  ; i++) 
		resample->X[i] = RIGHT(*ibuf++ + 0x8000, 16);
	last = i;
	Nproc = last - (resample->Xoff * 2);
	for(; i < last + resample->Xoff  ; i++) 
		resample->X[i] = 0;

	/* If we're draining out a buffer tail, 
	 * just do it next time or in drain.
	 */
	if ((Nx == *isamp) && (Nx <= resample->Xoff)) {
		/* fill in starting here next time */
		resample->Xread = last;
		/* leave *isamp alone, we consumed it */
		*osamp = 0;
		return;
	}


        /* SrcUp() is faster if we can use it */
	if (resample->Factor > 1)       /* Resample stuff in input buffer */
	    Nout = SrcUp(resample->X, resample->Y,
		resample->Factor, &resample->Time, Nproc,
		resample->Nwing, resample->LpScl,
		resample->Imp, resample->ImpD, 
		resample->InterpFilt);      
	else
            Nout = SrcUD(resample->X, resample->Y,
		resample->Factor, &resample->Time, Nproc,
		resample->Nwing, resample->LpScl,
		resample->Imp, resample->ImpD,
		resample->InterpFilt);

	/* Move converter Nproc samples back in time */
	resample->Time -= (Nproc<<Np); 
        /* Advance by number of samples processed */
	resample->Xp += Nproc;
	/* Calc time accumulation in Time */
	creep = (resample->Time>>Np) - resample->Xoff; 
	if (creep)
	{
		resample->Time -= (creep<<Np);   /* Remove time accumulation */
		resample->Xp += creep;     /* and add it to read pointer */
	}

	/* Copy back portion of input signal that must be re-used */
	for (i=0; i<last - resample->Xp + resample->Xoff; i++) 
	    resample->X[i] = resample->X[i + resample->Xp - resample->Xoff];

	/* Pos in input buff to read new data into */
	resample->Xread = i;                 
	resample->Xp = resample->Xoff;

	/* copy to output buffer, zero-filling beginning */
	/* zero-fill to preserve length and loop points */
	for(i = 0; i < resample->Oskip; i++) {
		*obuf++ = 0;
	}
	for(i = resample->Oskip; i < Nout + resample->Oskip; i++) {
		*obuf++ = LEFT(resample->Y[i], 16);
	}

	*isamp = Nx;
	*osamp = Nout;

	resample->Oskip = 0;
}

/*
 * Process tail of input samples.
 */
void resample_drain(effp, obuf, osamp)
eff_t effp;
ULONG *obuf;
ULONG *osamp;
{
	resample_t resample = (resample_t) effp->priv;
	LONG i, Nout;
	UHWORD Nx;
	
	Nx = resample->Xread - resample->Xoff;
	if (Nx <= resample->Xoff * 2) {
		/* zero-fill end */
		for(i = 0; i < resample->Xoff; i++)
			*obuf++ = 0;
		*osamp = resample->Xoff;
		return;
	}

	if (Nx * resample->Factor >= *osamp)
		fail("resample_drain: Overran output buffer!\n");

	/* fill out end with zeros */
	for(i = 0; i < resample->Xoff; i++)
		resample->X[i + resample->Xread] = 0;
        /* SrcUp() is faster if we can use it */
	if (resample->Factor >= 1)       /* Resample stuff in input buffer */
	    Nout = SrcUp(resample->X, resample->Y,
		resample->Factor, &resample->Time, Nx,
		resample->Nwing, resample->LpScl,
		resample->Imp, resample->ImpD, 
		resample->InterpFilt);      
	else
            Nout = SrcUD(resample->X, resample->Y,
		resample->Factor, &resample->Time, Nx,
		resample->Nwing, resample->LpScl,
		resample->Imp, resample->ImpD,
		resample->InterpFilt);
	
	for(i = resample->Oskip; i < Nout; i++) {
		*obuf++ = LEFT(resample->Y[i], 16);
	}
	*osamp = Nout - resample->Oskip;
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
void resample_stop(effp)
eff_t effp;
{
	resample_t resample = (resample_t) effp->priv;
	
	free(resample->Imp);
	free(resample->ImpD);
	free(resample->X);
	free(resample->Y);
}

/* From resample:filters.c */

/* Sampling rate up-conversion only subroutine;
 * Slightly faster than down-conversion;
 */
HWORD SrcUp(X, Y, Factor, Time, Nx, Nwing, LpScl, Imp, ImpD, Interp)
HWORD X[], Y[];
double Factor;
UWORD *Time;
UHWORD Nx, Nwing, LpScl;
HWORD Imp[], ImpD[];
BOOL Interp;
{
   HWORD *Xp, *Ystart;
   IWORD v;

   double dt;                  /* Step through input signal */ 
   UWORD dtb;                  /* Fixed-point version of Dt */
   UWORD endTime;              /* When Time reaches EndTime, return to user */

   dt = 1.0/Factor;            /* Output sampling period */
   dtb = dt*(1<<Np) + 0.5;     /* Fixed-point representation */

   Ystart = Y;
   endTime = *Time + (1<<Np)*(IWORD)Nx;
   while (*Time < endTime)
      {
      Xp = &X[*Time>>Np];      /* Ptr to current input sample */
      v = FilterUp(Imp, ImpD, Nwing, Interp, Xp, (HWORD)(*Time&Pmask),
         -1);                  /* Perform left-wing inner product */
      v += FilterUp(Imp, ImpD, Nwing, Interp, Xp+1, (HWORD)((-*Time)&Pmask),
         1);                   /* Perform right-wing inner product */
      v >>= Nhg;               /* Make guard bits */
      v *= LpScl;              /* Normalize for unity filter gain */
      *Y++ = v>>NLpScl;        /* Deposit output */
      *Time += dtb;            /* Move to next sample by time increment */
      }
   return (Y - Ystart);        /* Return the number of output samples */
}


/* Sampling rate conversion subroutine */

HWORD SrcUD(X, Y, Factor, Time, Nx, Nwing, LpScl, Imp, ImpD, Interp)
HWORD X[], Y[];
double Factor;
UWORD *Time;
UHWORD Nx, Nwing, LpScl;
HWORD Imp[], ImpD[];
BOOL Interp;
{
   HWORD *Xp, *Ystart;
   IWORD v;

   double dh;                  /* Step through filter impulse response */
   double dt;                  /* Step through input signal */
   UWORD endTime;              /* When Time reaches EndTime, return to user */
   UWORD dhb, dtb;             /* Fixed-point versions of Dh,Dt */

   dt = 1.0/Factor;            /* Output sampling period */
   dtb = dt*(1<<Np) + 0.5;     /* Fixed-point representation */

   dh = MIN(Npc, Factor*Npc);  /* Filter sampling period */
   dhb = dh*(1<<Na) + 0.5;     /* Fixed-point representation */

   Ystart = Y;
   endTime = *Time + (1<<Np)*(IWORD)Nx;
   while (*Time < endTime)
      {
      Xp = &X[*Time>>Np];      /* Ptr to current input sample */
      v = FilterUD(Imp, ImpD, Nwing, Interp, Xp, (HWORD)(*Time&Pmask),
          -1, dhb);            /* Perform left-wing inner product */
      v += FilterUD(Imp, ImpD, Nwing, Interp, Xp+1, (HWORD)((-*Time)&Pmask),
           1, dhb);            /* Perform right-wing inner product */
      v >>= Nhg;               /* Make guard bits */
      v *= LpScl;              /* Normalize for unity filter gain */
      *Y++ = v>>NLpScl;        /* Deposit output */
      *Time += dtb;            /* Move to next sample by time increment */
      }
   return (Y - Ystart);        /* Return the number of output samples */
}

void LpFilter();

int makeFilter(Imp, ImpD, LpScl, Nwing, Froll, Beta)
HWORD Imp[], ImpD[];
UHWORD *LpScl, Nwing;
double Froll, Beta;
{
   double DCgain, Scl, Maxh;
   double *ImpR;
   HWORD Dh;
   LONG i, temp;

   if (Nwing > MAXNWING)                      /* Check for valid parameters */
      return(1);
   if ((Froll<=0) || (Froll>1))
      return(2);
   if (Beta < 1)
      return(3);

   ImpR = (double *) malloc(sizeof(double) * MAXNWING);
   LpFilter(ImpR, (int)Nwing, Froll, Beta, Npc); /* Design a Kaiser-window */
                                                 /* Sinc low-pass filter */

   /* Compute the DC gain of the lowpass filter, and its maximum coefficient
    * magnitude. Scale the coefficients so that the maximum coeffiecient just
    * fits in Nh-bit fixed-point, and compute LpScl as the NLpScl-bit (signed)
    * scale factor which when multiplied by the output of the lowpass filter
    * gives unity gain. */
   DCgain = 0;
   Dh = Npc;                       /* Filter sampling period for factors>=1 */
   for (i=Dh; i<Nwing; i+=Dh)
      DCgain += ImpR[i];
   DCgain = 2*DCgain + ImpR[0];    /* DC gain of real coefficients */

   for (Maxh=i=0; i<Nwing; i++)
      Maxh = MAX(Maxh, fabs(ImpR[i]));

   Scl = ((1<<(Nh-1))-1)/Maxh;     /* Map largest coeff to 16-bit maximum */
   temp = fabs((1<<(NLpScl+Nh))/(DCgain*Scl));
   if (temp >= (1L<<16)) {
      free(ImpR);
      return(4);                   /* Filter scale factor overflows UHWORD */
    }
   *LpScl = temp;

   /* Scale filter coefficients for Nh bits and convert to integer */
   if (ImpR[0] < 0)                /* Need pos 1st value for LpScl storage */
      Scl = -Scl;
   for (i=0; i<Nwing; i++)         /* Scale them */
      ImpR[i] *= Scl;
   for (i=0; i<Nwing; i++)         /* Round them */
      Imp[i] = ImpR[i] + 0.5;

   /* ImpD makes linear interpolation of the filter coefficients faster */
   for (i=0; i<Nwing-1; i++)
      ImpD[i] = Imp[i+1] - Imp[i];
   ImpD[Nwing-1] = - Imp[Nwing-1];      /* Last coeff. not interpolated */

   free(ImpR);
   return(0);
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
int N, Num;
{
   double IBeta, temp;
   int i;

   /* Calculate filter coeffs: */
   c[0] = frq;
   for (i=1; i<N; i++)
      {
      temp = PI*(double)i/(double)Num;
      c[i] = sin(temp*frq)/temp;
      }

   /* Calculate and Apply Kaiser window to filter coeffs: */
   IBeta = 1.0/Izero(Beta);
   for (i=1; i<N; i++)
      {
      temp = (double)i / ((double)N * (double)1.0);
      c[i] *= Izero(Beta*sqrt(1.0-temp*temp)) * IBeta;
      }
}




IWORD FilterUp(Imp, ImpD, Nwing, Interp, Xp, Ph, Inc)
HWORD Imp[], ImpD[];
UHWORD Nwing;
BOOL Interp;
HWORD *Xp, Ph, Inc;
{
   HWORD a=0, *Hp, *Hdp=0, *End;
   IWORD v, t;

   v=0;
   Hp = &Imp[Ph>>Na];
   End = &Imp[Nwing];
   if (Interp)
      {
      Hdp = &ImpD[Ph>>Na];
      a = Ph & Amask;
      }
   /* Possible Bug: Hdp and a are not initialized if Interp == 0 */
   if (Inc == 1)                     /* If doing right wing...              */
      {                              /* ...drop extra coeff, so when Ph is  */
      End--;                         /*    0.5, we don't do too many mult's */
      if (Ph == 0)                   /* If the phase is zero...           */
         {                           /* ...then we've already skipped the */
         Hp += Npc;                  /*    first sample, so we must also  */
         Hdp += Npc;                 /*    skip ahead in Imp[] and ImpD[] */
         }
      }
   while (Hp < End)
      {
      t = *Hp;                       /* Get filter coeff */
      if (Interp)
         {
         t += (((IWORD)*Hdp)*a)>>Na;  /* t is now interp'd filter coeff */
         Hdp += Npc;                 /* Filter coeff differences step */
	 }
      t *= *Xp;      /* Mult coeff by input sample */
	  if (t & (1<<(Nhxn-1)))  /* Round, if needed */
		 t += (1<<(Nhxn-1));
      t >>= Nhxn;    /* Leave some guard bits, but come back some */
      v += t;        /* The filter output */
      Hp += Npc;     /* Filter coeff step */
      Xp += Inc;     /* Input signal step. NO CHECK ON ARRAY BOUNDS */
      }
   return(v);
}


IWORD FilterUD(Imp, ImpD, Nwing, Interp, Xp, Ph, Inc, dhb)
HWORD Imp[], ImpD[];
UHWORD Nwing;
BOOL Interp;
HWORD *Xp, Ph, Inc;
UHWORD dhb;
{
   HWORD a, *Hp, *Hdp, *End;
   IWORD v, t;
   UWORD Ho;

   v=0;
   Ho = (Ph*(UWORD)dhb)>>Np;
   End = &Imp[Nwing];
   if (Inc == 1)                     /* If doing right wing...              */
      {                              /* ...drop extra coeff, so when Ph is  */
      End--;                         /*    0.5, we don't do too many mult's */
      if (Ph == 0)                   /* If the phase is zero...           */
         Ho += dhb;                  /* ...then we've already skipped the */
      }                              /*    first sample, so we must also  */
                                     /*    skip ahead in Imp[] and ImpD[] */
   while ((Hp = &Imp[Ho>>Na]) < End)
      {
      t = *Hp;       /* Get IR sample */
      if (Interp)
         {
         Hdp = &ImpD[Ho>>Na]; /* get interp (lower Na) bits from diff table */
         a = Ho & Amask;                  /* a is logically between 0 and 1 */
         t += (((IWORD)*Hdp)*a)>>Na;      /* t is now interp'd filter coeff */
	 }
      t *= *Xp;      /* Mult coeff by input sample */
	  if (t & (1<<(Nhxn-1)))  /* Round, if needed */
		 t += (1<<(Nhxn-1));
      t >>= Nhxn;    /* Leave some guard bits, but come back some */
      v += t;        /* The filter output */
      Ho += dhb;     /* IR step */
      Xp += Inc;     /* Input signal step. NO CHECK ON ARRAY BOUNDS */
      }
   return(v);
}

