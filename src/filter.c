/*
 * Windowed sinc lowpass/bandpass/highpass filter.
 */

/*
 * November 7, 1999
 *
 * This source code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "st.h"

#ifndef HAVE_MEMMOVE
#define memmove(dest,src,len) bcopy((src),(dest),(len))
#endif

#define Float double/*float*/

#define ISCALE 0x10000
#define BUFFSIZE 8192

/* Private data for Lerp via LCM file */
typedef struct filterstuff {
	LONG rate;
	LONG freq0;										/* low  corner freq */
	LONG freq1;										/* high corner freq */
	double rolloff;								/* roll-off frequency */
	double beta;   								/* >2 is kaiser window beta, <=2 selects nuttall window */
	LONG Nwin;
	Float *Fp;										/* [Xh+1] Filter coefficients */
	LONG Xh;											/* number of past/future samples needed by filter  */
	LONG Xt;											/* target to enter new data into X */
	Float *X, *Y;									/* I/O buffers */
} *filter_t;

extern int 
makeFilter(P5(Float Fp[], LONG Nwing, double Froll, double Beta, LONG Num));

static void FiltWin(P2(filter_t f, LONG Nx));

/*
 * Process options
 */
void filter_getopts(effp, n, argv)
eff_t effp;
int n;
char **argv;
{
	filter_t f = (filter_t) effp->priv;

	f->beta = 16;  /* Kaiser window, beta 16 */
	f->Nwin = 128;
	
	f->freq0 = f->freq1 = 0;
	if (n >= 1) {
		char *p;
		p = argv[0];
		if (*p != '-')
			f->freq1 = strtol(p, &p, 10);
		if (*p == '-') {
			f->freq0 = f->freq1;
			f->freq1 = strtol(p+1, &p, 10);
		}
		if (*p) f->freq1 = f->freq0 = 0;
	}
	/* fprintf(stderr,"freq: %d-%d\n", f->freq0, f->freq1);fflush(stderr); */
	if (f->freq0 == 0 && f->freq1 == 0)
		fail("Usage: filter low-high [ windowlength [ beta ] ]");

	if ((n >= 2) && !sscanf(argv[1], "%ld", &f->Nwin))
		fail("Usage: filter low-high [ windowlength ]");
	else if (f->Nwin < 4) {
		fail("filter: window length (%ld) <4 is too short", f->Nwin);
	}

	if ((n >= 3) && !sscanf(argv[2], "%lf", &f->beta))
		fail("Usage: filter low-high [ windowlength [ beta ] ]");

	report("filter opts: cutoff %f, window-len %d, beta %f\n", f->rolloff, f->Nwin, f->beta);
}

/*
 * Prepare processing.
 */
void filter_start(effp)
eff_t effp;
{
	filter_t f = (filter_t) effp->priv;
	Float *Fp0, *Fp1;
	LONG Xh0, Xh1, Xh;
	int i;

	f->rate = effp->ininfo.rate;

	if (f->freq1 > f->rate/2 || f->freq1 <= 0)
		f->freq1 = f->rate/2;
	if ((f->freq0 < 0) || (f->freq0 > f->freq1))
		fail("filter: low(%d),high(%d) parameters must satisfy 0 <= low <= high <= %d",
					f->freq0, f->freq1, f->rate/2);
	
	Xh = f->Nwin/2;
	Fp0 = (Float *) malloc(sizeof(Float) * (Xh + 2)) + 1;
	if (f->freq0 > f->rate/200) {
		Xh0 = makeFilter(Fp0, Xh, 2.0*(double)f->freq0/f->rate, f->beta, 1);
		if (Xh0 <= 1)
			fail("filter: Unable to make low filter\n");
	} else {
		Xh0 = 0;
	}
	Fp1 = (Float *) malloc(sizeof(Float) * (Xh + 2)) + 1;
	/* need Fp[-1] and Fp[Xh] for makeFilter */
	if (f->freq1 < f->rate/2) {
		Xh1 = makeFilter(Fp1, Xh, 2.0*(double)f->freq1/f->rate, f->beta, 1);
		if (Xh1 <= 1)
			fail("filter: Unable to make high filter\n");
	} else {
		Fp1[0] = 1.0;
		Xh1 = 1;
	}
	/* now subtract Fp0[] from Fp1[] */
	Xh = (Xh0>Xh1)?  Xh0:Xh1;
	Xh -= 1;
	for (i=0; i<Xh; i++) {
		Float c0,c1;
		c0 = (i<Xh0)? Fp0[i]:0;
		c1 = (i<Xh1)? Fp1[i]:0;
		Fp1[i] = c1-c0;
	}

	free(Fp0 - 1); /* all done with Fp0 */
	f->Fp = Fp1;

	Xh -= 1;
	f->Nwin = 2*Xh + 1;
	f->Xh = Xh;
	f->Xt = Xh;

	f->X = (Float *) malloc(sizeof(Float) * (2*BUFFSIZE + 2*Xh));
	f->Y = f->X + BUFFSIZE + 2*Xh;

	/* Need Xh zeros at beginning of X */
	for (i = 0; i < Xh; i++)
		f->X[i] = 0;
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

void filter_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
LONG *isamp, *osamp;
{
	filter_t f = (filter_t) effp->priv;
	LONG i, Nx, Nproc;

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
		return;
	}
	/* fprintf(stderr,"flow Nproc %d\n",Nproc); */
	FiltWin(f, Nproc);

	/* Copy back portion of input signal that must be re-used */
	Nx += f->Xt;
	memmove(f->X, f->X + Nx - 2*f->Xh, sizeof(Float)*2*f->Xh); 
	f->Xt = 2*f->Xh;

	for (i = 0; i < Nproc; i++)
		*obuf++ = f->Y[i] * ISCALE;

	*osamp = Nproc;
}

/*
 * Process tail of input samples.
 */
void filter_drain(effp, obuf, osamp)
eff_t effp;
LONG *obuf;
LONG *osamp;
{
	filter_t f = (filter_t) effp->priv;
	LONG isamp_res, *Obuf, osamp_res;

	/* fprintf(stderr,"Xh %d, Xt %d  <--- DRAIN\n",f->Xh, f->Xt); */

	/* stuff end with Xh zeros */
	isamp_res = f->Xh;
	osamp_res = *osamp;
	Obuf = obuf;
	while (isamp_res>0 && osamp_res>0) {
		LONG Isamp, Osamp;
		Isamp = isamp_res;
		Osamp = osamp_res;
		filter_flow(effp, NULL, Obuf, &Isamp, &Osamp);
	  /* fprintf(stderr,"DRAIN isamp,osamp  (%d,%d) -> (%d,%d)\n",
		 * isamp_res,osamp_res,Isamp,Osamp); */
		Obuf += Osamp;
		osamp_res -= Osamp;
		isamp_res -= Isamp;
	};
	*osamp -= osamp_res;
	/* fprintf(stderr,"DRAIN osamp %d\n", *osamp); */
	if (isamp_res)
		warn("drain overran obuf by %d\n", isamp_res); fflush(stderr);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
void filter_stop(effp)
eff_t effp;
{
	filter_t f = (filter_t) effp->priv;

	free(f->Fp - 1);
	free(f->X);
}

static double jprod(Fp, Xp, ct)
const Float Fp[], *Xp;
LONG ct;
{
	const Float *fp, *xp, *xq;
	double v = 0;
	
	fp = Fp + ct;					/* so sum starts with smaller coef's */
	xp = Xp - ct;
	xq = Xp + ct;
	do {
		v += *fp * (*xp + *xq);
		xp++; xq--;
	} while (--fp > Fp);
	v += *fp * *xp;
	return v;
}

static void FiltWin(f, Nx)
filter_t f;
LONG Nx;
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
