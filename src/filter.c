/*
 * Windowed sinc lowpass/bandpass/highpass filter.
 */

/*
 * November 18, 1999
 * Copyright 1994 Julius O. Smith
 * Copyright 1991 (?) Lance Norskog (?)
 * Copyright 1999 Stan Brooks <stabro@megsinet.net>
 *
 * -------------------------------------------------------------------
 * This source code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * -------------------------------------------------------------------
 *
 * REMARKS: (Stan Brooks speaking)
 * This code is heavily based on the resample.c code which was
 * apparently itself a rewrite (by Lance Norskog?) of code originally
 * by Julius O. Smith, and distributed under the GPL license...
 *
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "st_i.h"

#ifndef HAVE_MEMMOVE
#define memmove(dest,src,len) bcopy((src),(dest),(len))
#endif

/* this Float MUST match that in resample.h */
#define Float double/*float*/

#define ISCALE 0x10000
#define BUFFSIZE 8192

/* Private data for Lerp via LCM file */
typedef struct filterstuff {
	st_rate_t rate;
	st_sample_t freq0;/* low  corner freq */
	st_sample_t freq1;/* high corner freq */
	double beta;/* >2 is kaiser window beta, <=2 selects nuttall window */
	long Nwin;
	Float *Fp;/* [Xh+1] Filter coefficients */
	long Xh;/* number of past/future samples needed by filter  */
	long Xt;/* target to enter new data into X */
	Float *X, *Y;/* I/O buffers */
} *filter_t;

/* makeFilter() declared in resample.c */
extern int 
makeFilter(Float Fp[], long Nwing, double Froll, double Beta, long Num, int Normalize);

static void FiltWin(filter_t f, long Nx);

/*
 * Process options
 */
int st_filter_getopts(eff_t effp, int n, char **argv)
{
	filter_t f = (filter_t) effp->priv;

	f->beta = 16;  /* Kaiser window, beta 16 */
	f->Nwin = 128;
	
	f->freq0 = f->freq1 = 0;
	if (n >= 1) {
		char *p;
		p = argv[0];
		if (*p != '-') {
			f->freq1 = strtol(p, &p, 10);
		}
		if (*p == '-') {
			f->freq0 = f->freq1;
			f->freq1 = strtol(p+1, &p, 10);
		}
		if (*p) f->freq1 = f->freq0 = 0;
	}
	/* fprintf(stderr,"freq: %d-%d\n", f->freq0, f->freq1);fflush(stderr); */
	if (f->freq0 == 0 && f->freq1 == 0)
	{
		st_fail("Usage: filter low-high [ windowlength [ beta ] ]");
		return (ST_EOF);
	}

	if ((n >= 2) && !sscanf(argv[1], "%ld", &f->Nwin))
	{
		st_fail("Usage: filter low-high [ windowlength ]");
		return (ST_EOF);
	}
	else if (f->Nwin < 4) {
		st_fail("filter: window length (%ld) <4 is too short", f->Nwin);
		return (ST_EOF);
	}

	if ((n >= 3) && !sscanf(argv[2], "%lf", &f->beta))
	{
		st_fail("Usage: filter low-high [ windowlength [ beta ] ]");
		return (ST_EOF);
	}

	st_report("filter opts: %d-%d, window-len %d, beta %f\n", f->freq0, f->freq1, f->Nwin, f->beta);
	return (ST_SUCCESS);
}

/*
 * Prepare processing.
 */
int st_filter_start(eff_t effp)
{
	filter_t f = (filter_t) effp->priv;
	Float *Fp0, *Fp1;
	long Xh0, Xh1, Xh;
	int i;

	f->rate = effp->ininfo.rate;

	/* adjust upper frequency to Nyquist if necessary */
	if (f->freq1 > f->rate/2 || f->freq1 <= 0)
		f->freq1 = f->rate/2;

	if ((f->freq0 < 0) || (f->freq0 > f->freq1))
	{
		st_fail("filter: low(%d),high(%d) parameters must satisfy 0 <= low <= high <= %d",
					f->freq0, f->freq1, f->rate/2);
		return (ST_EOF);
	}
	
	Xh = f->Nwin/2;
	Fp0 = (Float *) malloc(sizeof(Float) * (Xh + 2)) + 1;
	if (f->freq0 > f->rate/200) {
		Xh0 = makeFilter(Fp0, Xh, 2.0*(double)f->freq0/f->rate, f->beta, 1, 0);
		if (Xh0 <= 1)
		{
			st_fail("filter: Unable to make low filter\n");
			return (ST_EOF);
		}
	} else {
		Xh0 = 0;
	}
	Fp1 = (Float *) malloc(sizeof(Float) * (Xh + 2)) + 1;
	/* need Fp[-1] and Fp[Xh] for makeFilter */
	if (f->freq1 < f->rate/2) {
		Xh1 = makeFilter(Fp1, Xh, 2.0*(double)f->freq1/f->rate, f->beta, 1, 0);
		if (Xh1 <= 1)
		{
			st_fail("filter: Unable to make high filter\n");
			return (ST_EOF);
		}
	} else {
		Fp1[0] = 1.0;
		Xh1 = 1;
	}
	/* now subtract Fp0[] from Fp1[] */
	Xh = (Xh0>Xh1)?  Xh0:Xh1; /* >=1, by above */
	for (i=0; i<Xh; i++) {
		Float c0,c1;
		c0 = (i<Xh0)? Fp0[i]:0;
		c1 = (i<Xh1)? Fp1[i]:0;
		Fp1[i] = c1-c0;
	}

	free(Fp0 - 1); /* all done with Fp0 */
	f->Fp = Fp1;

	Xh -= 1;       /* Xh = 0 can only happen if filter was identity 0-Nyquist */
	if (Xh<=0)
		st_warn("filter: adjusted freq %d-%d is identity", f->freq0, f->freq1);

	f->Nwin = 2*Xh + 1;  /* not really used afterwards */
	f->Xh = Xh;
	f->Xt = Xh;

	f->X = (Float *) malloc(sizeof(Float) * (2*BUFFSIZE + 2*Xh));
	f->Y = f->X + BUFFSIZE + 2*Xh;

	/* Need Xh zeros at beginning of X */
	for (i = 0; i < Xh; i++)
		f->X[i] = 0;
	return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
int st_filter_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                   st_size_t *isamp, st_size_t *osamp)
{
	filter_t f = (filter_t) effp->priv;
	long i, Nx, Nproc;

	/* constrain amount we actually process */
	/* fprintf(stderr,"Xh %d, Xt %d, isamp %d, ",f->Xh, f->Xt, *isamp);fflush(stderr); */
	Nx = BUFFSIZE + 2*f->Xh - f->Xt;
	if (Nx > *isamp) Nx = *isamp;
	if (Nx > *osamp) Nx = *osamp;
	*isamp = Nx;

	{
		Float *xp, *xtop;
		xp = f->X + f->Xt;
		xtop = xp + Nx;
		if (ibuf != NULL) {
			while (xp < xtop)
				*xp++ = (Float)(*ibuf++) / ISCALE;
		} else {
			while (xp < xtop)
				*xp++ = 0;
		}
	}

	Nproc = f->Xt + Nx - 2*f->Xh;

	if (Nproc <= 0) {
		f->Xt += Nx;
		*osamp = 0;
		return (ST_SUCCESS);
	}
	/* fprintf(stderr,"flow Nproc %d\n",Nproc); */
	FiltWin(f, Nproc);

	/* Copy back portion of input signal that must be re-used */
	Nx += f->Xt;
	if (f->Xh)
		memmove(f->X, f->X + Nx - 2*f->Xh, sizeof(Float)*2*f->Xh); 
	f->Xt = 2*f->Xh;

	for (i = 0; i < Nproc; i++)
		*obuf++ = f->Y[i] * ISCALE;

	*osamp = Nproc;
	return (ST_SUCCESS);
}

/*
 * Process tail of input samples.
 */
int st_filter_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
	filter_t f = (filter_t) effp->priv;
	long isamp_res, osamp_res;
	st_sample_t *Obuf;

	/* fprintf(stderr,"Xh %d, Xt %d  <--- DRAIN\n",f->Xh, f->Xt); */

	/* stuff end with Xh zeros */
	isamp_res = f->Xh;
	osamp_res = *osamp;
	Obuf = obuf;
	while (isamp_res>0 && osamp_res>0) {
		st_sample_t Isamp, Osamp;
		Isamp = isamp_res;
		Osamp = osamp_res;
		st_filter_flow(effp, NULL, Obuf, (st_size_t *)&Isamp, (st_size_t *)&Osamp);
	  /* fprintf(stderr,"DRAIN isamp,osamp  (%d,%d) -> (%d,%d)\n",
		 * isamp_res,osamp_res,Isamp,Osamp); */
		Obuf += Osamp;
		osamp_res -= Osamp;
		isamp_res -= Isamp;
	};
	*osamp -= osamp_res;
	/* fprintf(stderr,"DRAIN osamp %d\n", *osamp); */
	if (isamp_res)
		st_warn("drain overran obuf by %d\n", isamp_res); fflush(stderr);
	return (ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_filter_stop(eff_t effp)
{
	filter_t f = (filter_t) effp->priv;

	free(f->Fp - 1);
	free(f->X);
	return (ST_SUCCESS);
}

static double jprod(const Float *Fp, const Float *Xp, long ct)
{
	const Float *fp, *xp, *xq;
	double v = 0;
	
	fp = Fp + ct;	/* so sum starts with smaller coef's */
	xp = Xp - ct;
	xq = Xp + ct;
	while (fp > Fp) {   /* ct = 0 can happen */
		v += *fp * (*xp + *xq);
		xp++; xq--; fp--;
	}
	v += *fp * *xp;
	return v;
}

static void FiltWin(filter_t f, long Nx)
{
	Float *Y;
	Float *X, *Xend;

	Y = f->Y;
	X = f->X + f->Xh;			/* Ptr to current input sample */
	Xend = X + Nx;
	while (X < Xend) {
		*Y++ = jprod(f->Fp, X, f->Xh);
		X++;
	}
}
