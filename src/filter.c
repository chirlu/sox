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

#include "sox_i.h"

#define ISCALE 0x10000
#define BUFFSIZE 8192

/* Private data for Lerp via LCM file */
typedef struct filterstuff {
        sox_rate_t rate;
        sox_ssample_t freq0;/* low  corner freq */
        sox_ssample_t freq1;/* high corner freq */
        double beta;/* >2 is kaiser window beta, <=2 selects nuttall window */
        long Nwin;
        double *Fp;/* [Xh+1] Filter coefficients */
        long Xh;/* number of past/future samples needed by filter  */
        long Xt;/* target to enter new data into X */
        double *X, *Y;/* I/O buffers */
} *filter_t;

/* makeFilter() declared in resample.c */
extern int 
makeFilter(double Fp[], long Nwing, double Froll, double Beta, long Num, int Normalize);

static void FiltWin(filter_t f, long Nx);

/*
 * Process options
 */
static int sox_filter_getopts(sox_effect_t * effp, int n, char **argv)
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
        sox_debug("freq: %d-%d", f->freq0, f->freq1);
        if (f->freq0 == 0 && f->freq1 == 0)
          return sox_usage(effp);

        if ((n >= 2) && !sscanf(argv[1], "%ld", &f->Nwin))
          return sox_usage(effp);
        else if (f->Nwin < 4) {
                sox_fail("filter: window length (%ld) <4 is too short", f->Nwin);
                return (SOX_EOF);
        }

        if ((n >= 3) && !sscanf(argv[2], "%lf", &f->beta))
          return sox_usage(effp);

        sox_debug("filter opts: %d-%d, window-len %d, beta %f", f->freq0, f->freq1, f->Nwin, f->beta);
        return (SOX_SUCCESS);
}

/*
 * Prepare processing.
 */
static int sox_filter_start(sox_effect_t * effp)
{
        filter_t f = (filter_t) effp->priv;
        double *Fp0, *Fp1;
        long Xh0, Xh1, Xh;
        int i;

        f->rate = effp->ininfo.rate;

        /* adjust upper frequency to Nyquist if necessary */
        if (f->freq1 > (sox_ssample_t)f->rate/2 || f->freq1 <= 0)
                f->freq1 = f->rate/2;

        if ((f->freq0 < 0) || (f->freq0 > f->freq1))
        {
                sox_fail("filter: low(%d),high(%d) parameters must satisfy 0 <= low <= high <= %g",
                                        f->freq0, f->freq1, f->rate/2);
                return (SOX_EOF);
        }
        
        Xh = f->Nwin/2;
        Fp0 = (double *) xmalloc(sizeof(double) * (Xh + 2)) + 1;
        if (f->freq0 > (sox_ssample_t)f->rate/200) {
                Xh0 = makeFilter(Fp0, Xh, 2.0*(double)f->freq0/f->rate, f->beta, 1, 0);
                if (Xh0 <= 1)
                {
                        sox_fail("filter: Unable to make low filter");
                        return (SOX_EOF);
                }
        } else {
                Xh0 = 0;
        }
        Fp1 = (double *) xmalloc(sizeof(double) * (Xh + 2)) + 1;
        /* need Fp[-1] and Fp[Xh] for makeFilter */
        if (f->freq1 < (sox_ssample_t)f->rate/2) {
                Xh1 = makeFilter(Fp1, Xh, 2.0*(double)f->freq1/f->rate, f->beta, 1, 0);
                if (Xh1 <= 1)
                {
                        sox_fail("filter: Unable to make high filter");
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
                sox_warn("filter: adjusted freq %d-%d is identity", f->freq0, f->freq1);

        f->Nwin = 2*Xh + 1;  /* not really used afterwards */
        f->Xh = Xh;
        f->Xt = Xh;

        f->X = (double *) xmalloc(sizeof(double) * (2*BUFFSIZE + 2*Xh));
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
static int sox_filter_flow(sox_effect_t * effp, const sox_ssample_t *ibuf, sox_ssample_t *obuf, 
                   sox_size_t *isamp, sox_size_t *osamp)
{
        filter_t f = (filter_t) effp->priv;
        sox_size_t Nx;
        long i, Nproc;

        /* constrain amount we actually process */
        /* sox_debug("Xh %d, Xt %d, isamp %d, ",f->Xh, f->Xt, *isamp); */
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
        sox_debug("flow Nproc %d",Nproc);
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
static int sox_filter_drain(sox_effect_t * effp, sox_ssample_t *obuf, sox_size_t *osamp)
{
        filter_t f = (filter_t) effp->priv;
        long isamp_res, osamp_res;
        sox_ssample_t *Obuf;

        sox_debug("Xh %d, Xt %d  <--- DRAIN",f->Xh, f->Xt);

        /* stuff end with Xh zeros */
        isamp_res = f->Xh;
        osamp_res = *osamp;
        Obuf = obuf;
        while (isamp_res>0 && osamp_res>0) {
                sox_ssample_t Isamp, Osamp;
                Isamp = isamp_res;
                Osamp = osamp_res;
                sox_filter_flow(effp, NULL, Obuf, (sox_size_t *)&Isamp, (sox_size_t *)&Osamp);
          /* sox_debug("DRAIN isamp,osamp  (%d,%d) -> (%d,%d)",
                 * isamp_res,osamp_res,Isamp,Osamp); */
                Obuf += Osamp;
                osamp_res -= Osamp;
                isamp_res -= Isamp;
        };
        *osamp -= osamp_res;
        /* sox_debug("DRAIN osamp %d", *osamp); */
        if (isamp_res)
                sox_warn("drain overran obuf by %d", isamp_res);
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
        filter_t f = (filter_t) effp->priv;

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

static void FiltWin(filter_t f, long Nx)
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
  NULL
};

const sox_effect_handler_t *sox_filter_effect_fn(void)
{
    return &sox_filter_effect;
}
