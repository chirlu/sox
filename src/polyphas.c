/*
 * July 14, 1998
 * Copyright 1998  K. Bradley, Carnegie Mellon University
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * October 29, 1999
 * Various changes, bugfixes, speedups, by Stan Brooks.
 *
 */

/*
 * Sound Tools rate change effect file.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "st_i.h"

#define Float float/*double*/
#define ISCALE 0x10000
#define MF 30

typedef struct {
  int up,down;    /* up/down conversion factors for this stage      */
  int filt_len;   /* # coefficients in filter_array                 */
  Float *filt_array; /* filter coefficients                         */
  int held;       /* # samples held in input but not yet processed  */
  int hsize;      /* # samples of past-history kept in lower window */
  int size;       /* # samples current data which window can accept */
  Float *window;  /* this is past_hist[hsize], then input[size]     */
} polystage;

typedef struct polyphase {

  st_rate_t lcmrate;             /* least common multiple of rates */
  st_rate_t inskip, outskip;     /* LCM increments for I & O rates */
  double Factor;                 /* out_rate/in_rate               */
  unsigned long total;           /* number of filter stages        */
  st_size_t oskip;               /* output samples to skip at start*/
  double inpipe;                 /* output samples 'in the pipe'   */
  polystage *stage[MF];          /* array of pointers to polystage structs */

} *poly_t;

/*
 * Process options
 */

/* Options:

   -w <nut / ham>        :  window type
   -width <short / long> :  window width
                            short = 128 samples
                            long  = 1024 samples
   <num>                    num:  explicit number

   -cutoff <float>       :  frequency cutoff for base bandwidth.
                            Default = 0.95 = 95%
*/

static int win_type  = 0;
static int win_width = 1024;
static Float cutoff = 0.95;

int st_poly_getopts(eff_t effp, int n, char **argv)
{
  /* 0: nuttall
     1: hamming */
  win_type = 0;

  /* width:  short = 128
             long = 1024 (default) */
  win_width = 1024;

  /* cutoff:  frequency cutoff of base bandwidth in percentage. */
  cutoff = 0.95;

  while(n >= 2) {

    /* Window type check */
    if(!strcmp(argv[0], "-w")) {
      if(!strcmp(argv[1], "ham"))
        win_type = 1;
      if(!strcmp(argv[1], "nut"))
        win_type = 0;

      argv += 2;
      n -= 2;
      continue;
    }

    /* Window width check */
    if(!strcmp(argv[0], "-width")) {
      if(!strcmp(argv[1], "short"))
        win_width = 128;
      else if(!strcmp(argv[1], "long"))
        win_width = 1024;
      else
        win_width = atoi(argv[1]);

      argv += 2;
      n -= 2;
      continue;
    }

    /* Cutoff frequency check */
    if(!strcmp(argv[0], "-cutoff")) {
      cutoff = atof(argv[1]);
      argv += 2;
      n -= 2;
      continue;
    }

    st_fail("Polyphase: unknown argument (%s %s)!", argv[0], argv[1]);
    return (ST_EOF);
  }
  return (ST_SUCCESS);
}

/*
 * Prepare processing.
 */
static const unsigned short primes[] = {
  2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37,
  41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89,
  97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151,
  157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223,
  227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281,
  283, 293, 307, 311, 313, 317, 331, 337, 347, 349, 353, 359,
  367, 373, 379, 383, 389, 397, 401, 409, 419, 421, 431, 433,
  439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503,
  509, 521, 523, 541, 547, 557, 563, 569, 571, 577, 587, 593,
  599, 601, 607, 613, 617, 619, 631, 641, 643, 647, 653, 659,
  661, 673, 677, 683, 691, 701, 709, 719, 727, 733, 739, 743,
  751, 757, 761, 769, 773, 787, 797, 809, 811, 821, 823, 827,
  829, 839, 853, 857, 859, 863, 877, 881, 883, 887, 907, 911,
  919, 929, 937, 941, 947, 953, 967, 971, 977, 983, 991, 997,
  0
};

#ifndef max
#define max(x,y) ((x > y) ? x : y)
#endif

#ifndef min
#define min(x,y) ((x < y) ? x : y)
#endif

static int prime(int n, int *q0)
{
  const unsigned short *p;
  int pr, *q;

  p = primes;
  q = q0;
  st_report("factors(%d) =",n);
  while (n > 1) {
    while ((pr = *p) && (n % pr)) p++;
    if (!pr) {
      st_fail("Number %d too large of a prime.\n",n);
      pr = n;
    }
    *q++ = pr;
    n /= pr;
  }
  *q = 0;
  for (pr=0; pr<q-q0; pr++) st_report(" %d",q0[pr]);
  st_report("\n");
  return (q-q0);
}

static int permute(int *m, int *l, int ct, int ct1, int amalg)
{
  int k, n;
  int *p;
  int *q;

  p=l; q=m;
  while (ct1>ct) { *q++=1; ct++;}
  while ((*q++=*p++)) ;
  if (ct<=1) return ct;

  for (k=ct; k>1; ) {
    int tmp;
    unsigned long j;
    j = (rand()%32768L) + ((rand()%32768L)<<13); /* reasonably big */
    j = j % k; /* non-negative! */
    k--;
    if (j != k) {
      tmp = m[k]; m[k]=m[j]; m[j]=tmp;
    }
  }
  /* now m is a 'random' permutation of l */
  p = q = m;
  n = *q++;
  while ((k=*q++)) {
    if ((n * k <= amalg) && (rand() & 1)) {
      n *= k;
    } else {
      *p++ = n;
      n = k;
    }
  }
  if (n) *p++=n;
  *p = 0;
  /*for (k=0; k<p-m; k++) st_report(" %d",m[k]);*/
  /*st_report("\n");*/
  return (p-m);
}

static int optimize_factors(int numer, int denom, int *l1, int *l2)
{
  int f_min,c_min,u_min,ct1,ct2;
  int amalg;
  int k;
  static int m1[MF],m2[MF];
  static int b1[MF],b2[MF];

  memset(m1,0,sizeof(int)*MF);
  memset(m2,0,sizeof(int)*MF);
  memset(b1,0,sizeof(int)*MF);
  memset(b2,0,sizeof(int)*MF);

  f_min = numer; if (f_min>denom) f_min = denom;
  c_min = 1<<30;
  u_min = 0;

  /* Find the prime factors of numer and denom */
  ct1 = prime(numer,l1);
  ct2 = prime(denom,l2);

  for (amalg = max(9,l2[0]); amalg<= 9+l2[ct2-1]; amalg++) {
    for (k = 0; k<100000; k++) {
      int u,u1,u2,j,f,cost;
      cost = 0;
      f = denom;
      u = min(ct1,ct2) + 1;
      /*st_report("pfacts(%d): ", numer);*/
      u1 = permute(m1,l1,ct1,u,amalg);
      /*st_report("pfacts(%d): ", denom);*/
      u2 = permute(m2,l2,ct2,u,amalg);
      u = max(u1,u2);
      for (j=0; j<u; j++) {
        if (j>=u1) m1[j]=1;
        if (j>=u2) m2[j]=1;
        f = (f * m1[j])/m2[j];
        if (f < f_min) goto fail;
        cost += f + m1[j]*m2[j];
      }
      if (c_min>cost) {
        c_min = cost;
        u_min = u;
#       if 0
        st_report("c_min %d, [%d-%d]:",c_min,numer,denom);
        for (j=0; j<u; j++)
          st_report(" (%d,%d)",m1[j],m2[j]);
        st_report("\n");
#       endif
       memcpy(b1,m1,u*sizeof(int));
       memcpy(b2,m2,u*sizeof(int));
      }
     fail:
        ;;
    }
    if (u_min) break;
  }
  if (u_min) {
    memcpy(l1,b1,u_min*sizeof(int));
    memcpy(l2,b2,u_min*sizeof(int));
  }
  l1[u_min] = 0;
  l2[u_min] = 0;
  return u_min;
}

#ifndef PI
#define PI 3.14159265358979
#endif

/* Calculate a Nuttall window of a given length.
   Buffer must already be allocated to appropriate size.
   */

static void nuttall(Float *buffer, int length)
{
  int j;
  double N;
  int N1;

  if(buffer == NULL || length <= 0)
    st_fail("Illegal buffer %p or length %d to nuttall.\n", buffer, length);

  /* Initial variable setups. */
  N = length;
  N1 = length/2;

  for(j = 0; j < length; j++) {
    buffer[j] = 0.3635819 +
      0.4891775 * cos(2*PI*1*(j - N1) / N) +
      0.1365995 * cos(2*PI*2*(j - N1) / N) +
      0.0106411 * cos(2*PI*3*(j - N1) / N);
  }
}
/* Calculate a Hamming window of given length.
   Buffer must already be allocated to appropriate size.
*/

static void hamming(Float *buffer, int length)
{
    int j;
    int N1;

    if(buffer == NULL || length <= 0)
      st_fail("Illegal buffer %p or length %d to hamming.\n",buffer,length);

    N1 = length/2;
    for(j=0;j<length;j++)
      buffer[j] = 0.5 - 0.46 * cos(PI*j/N1);
}

/* Calculate the sinc function properly */

static Float sinc(Float value)
{
    return(fabs(value) < 1E-50 ? 1.0 : sin(value) / value);
}

/* Design a low-pass FIR filter using window technique.
   Length of filter is in length, cutoff frequency in cutoff.
   0 < cutoff <= 1.0 (normalized frequency)

   buffer must already be allocated.
*/
static void fir_design(Float *buffer, int length, Float cutoff)
{
    int j;
    double sum;

    if(buffer == NULL || length < 0 || cutoff < 0 || cutoff > PI)
      st_fail("Illegal buffer %p, length %d, or cutoff %f.\n",buffer,length,cutoff);

    /* Use the user-option of window type */
    if(win_type == 0)
      nuttall(buffer, length); /* Design Nuttall window:  ** dB cutoff */
    else
      hamming(buffer,length);  /* Design Hamming window:  43 dB cutoff */

    /* st_report("# fir_design length=%d, cutoff=%8.4f\n",length,cutoff); */
    /* Design filter:  windowed sinc function */
    sum = 0.0;
    for(j=0;j<length;j++) {
      buffer[j] *= sinc(PI*cutoff*(j-length/2)); /* center at length/2 */
      /* st_report("%.1f %.6f\n",(float)j,buffer[j]); */
      sum += buffer[j];
    }
    sum = (double)1.0/sum;
    /* Normalize buffer to have gain of 1.0: prevent roundoff error */
    for(j=0;j<length;j++) {
      buffer[j] *= sum;
    }
    /* st_report("# end\n\n"); */
}

#define RIBLEN 2048

int st_poly_start(eff_t effp)
{
    poly_t rate = (poly_t) effp->priv;
    static int l1[MF], l2[MF];
    double skip = 0;
    int total, size, uprate;
    int k;

    if (effp->ininfo.rate == effp->outinfo.rate)
    {
        st_fail("Input and Output rate must not be the same to use polyphase effect");
        return(ST_EOF);
    }

    st_initrand();

    rate->lcmrate = st_lcm((st_sample_t)effp->ininfo.rate,
                           (st_sample_t)effp->outinfo.rate);

    /* Cursory check for LCM overflow.
     * If both rate are below 65k, there should be no problem.
     * 16 bits x 16 bits = 32 bits, which we can handle.
     */

    rate->inskip = rate->lcmrate / effp->ininfo.rate;
    rate->outskip = rate->lcmrate / effp->outinfo.rate;
    rate->Factor = (double)rate->inskip / (double)rate->outskip;
    rate->inpipe = 0;
    {
      int f = RIBLEN/max(rate->inskip,rate->outskip);
      if (f == 0) f = 1;
      size = f * rate->outskip; /* reasonable input block size */
    }

    /* Find the prime factors of inskip and outskip */
    total = optimize_factors(rate->inskip, rate->outskip, l1, l2);
    rate->total = total;
    /* l1 and l2 are now lists of the up/down factors for conversion */

    st_report("Poly:  input rate %d, output rate %d.  %d stages.",
            effp->ininfo.rate, effp->outinfo.rate,total);
    st_report("Poly:  window: %s  size: %d  cutoff: %f.",
            (win_type == 0) ? ("nut") : ("ham"), win_width, cutoff);

    /* Create an array of filters and past history */
    uprate = effp->ininfo.rate;
    for (k = 0; k < total; k++) {
      int j, prod, f_cutoff, f_len;
      polystage *s;

      rate->stage[k] = s = (polystage*) malloc(sizeof(polystage));
      s->up = l1[k];
      s->down = l2[k];
      f_cutoff = max(s->up, s->down);
      f_len = max(20 * f_cutoff, win_width);
      prod = s->up * s->down;
      if (prod > 2*f_len) prod = s->up;
      f_len = ((f_len+prod-1)/prod) * prod; /* reduces rounding-errors in polyphase() */
      s->size = size;
      s->hsize = f_len/s->up; /* this much of window is past-history */
      s->held = 0;
      st_report("Poly:  stage %d:  Up by %d, down by %d,  i_samps %d, hsize %d",
              k+1,s->up,s->down,size, s->hsize);
      s->filt_len = f_len;
      s->filt_array = (Float *) malloc(sizeof(Float) * f_len);
      s->window = (Float *) malloc(sizeof(Float) * (s->hsize+size));
      /* zero past_history section of window */
      for(j = 0; j < s->hsize; j++)
        s->window[j] = 0.0;

      uprate *= s->up;
      st_report("Poly:         :  filt_len %d, cutoff freq %.1f",
              f_len, uprate*cutoff/f_cutoff);
      uprate /= s->down;
      fir_design(s->filt_array, f_len, cutoff/f_cutoff);
      /* s->filt_array[f_len-1]=0; */

                        skip *= s->up;
                        skip += f_len;
                        skip /= s->down;

                        size = (size * s->up) / s->down;  /* this is integer */
    }
    rate->oskip = skip/2;
                { /* bogus last stage is for output buffering */
      polystage *s;
      rate->stage[k] = s = (polystage*) malloc(sizeof(polystage));
      s->up = s->down = 0;
      s->size = size;
      s->hsize = 0;
      s->held = 0;
      s->filt_len = 0;
      s->filt_array = NULL;
      s->window = (Float *) malloc(sizeof(Float) * size);
    }
    st_report("Poly:  output samples %d, oskip %d",size, rate->oskip);
    return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

/* REMARK: putting this in a separate subroutine improves gcc's optimization */
static double st_prod(const Float *q, int qstep, const Float *p, int n)
{
  double sum = 0;
  const Float *p0;
  p0 = p-n;
  while (p>p0) {
    sum += *p * *q;
    q += qstep;
    p -= 1;
  }
  return sum;
}

static void polyphase(Float *output, polystage *s)
{
  int mm;
  int up = s->up;
  int down = s->down;
  int f_len = s->filt_len;
  const Float *in;
  Float *o; /* output pointer */
  Float *o_top;

  in = s->window + s->hsize;
  /*for (mm=0; mm<s->filt_len; mm++) st_report("cf_%d %f\n",mm,s->filt_array[mm]);*/
  /* assumes s->size divisible by down (now true) */
  o_top = output + (s->size * up) / down;
  /*st_report(" isize %d, osize %d, up %d, down %d, N %d", s->size, o_top-output, up, down, f_len);*/
  for (mm=0, o=output; o < o_top; mm+=down, o++) {
    double sum;
    const Float *p, *q;
    q = s->filt_array + (mm%up);   /* decimated coef pointer */
    p  = in + (mm/up);
    sum = st_prod(q, up, p, f_len/up);
    *o = sum * up;
  }
}

static void update_hist(Float *hist, int hist_size, int in_size)
{
  Float *p, *p1, *q;
  p = hist;
  p1 = hist+hist_size;
  q = hist+in_size;
  while (p<p1)
    *p++ = *q++;

}

static st_sample_t clipfloat(Float sample)
{
        if (sample > ST_SAMPLE_MAX)
        return ST_SAMPLE_MAX;
        if (sample < ST_SAMPLE_MIN)
        return ST_SAMPLE_MIN;
        return sample;
}

int st_poly_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                 st_size_t *isamp, st_size_t *osamp)
{
  poly_t rate = (poly_t) effp->priv;
  polystage *s0,*s1;

  /* Sanity check:  how much can we tolerate? */
  /* st_report("*isamp=%d *osamp=%d\n",*isamp,*osamp); fflush(stderr); */
  s0 = rate->stage[0];            /* the first stage */
  s1 = rate->stage[rate->total];  /* the 'last' stage is output buffer */
  {
    int in_size, gap, k;

    in_size = *isamp;
    gap = s0->size - s0->held; /* space available in this 'input' buffer */
    if ((in_size > gap) || (ibuf==NULL)) {
      *isamp = in_size = gap;
    }
    if (in_size > 0) {
      Float *q;
                        q = s0->window + s0->hsize;
      if (s0!=s1) q += s0->held;  /* the last (output) buffer doesn't shift history */
      if (ibuf != NULL) {
                                rate->inpipe += rate->Factor * in_size;
        for (k=0; k<in_size; k++)
          *q++ = (Float)ibuf[k] / ISCALE;
      } else { /* ibuf==NULL is draining */
        for(k=0;k<in_size;k++)
          *q++ = 0.0;
      }
      s0->held += in_size;
    }
  }

  if (s0->held == s0->size && s1->held == 0) {
    int k;
    /* input buffer full, output buffer empty, so do process */

    for(k=0; k<rate->total; k++) {
      polystage *s;
      Float *out;

      s = rate->stage[k];

      out = rate->stage[k+1]->window + rate->stage[k+1]->hsize;

      /* st_report("k=%d  insize=%d\n",k,in_size); fflush(stderr); */
      polyphase(out, s);

      /* copy input history into lower portion of rate->window[k] */
      update_hist(s->window, s->hsize, s->size);
      s->held = 0;
    }

    s1->held = s1->size;
    s1->hsize = 0;

  }

  {
    st_sample_t *q;
    st_size_t out_size;
    st_size_t oskip;
    Float *out_buf;
    int k;

    oskip = rate->oskip;
                out_size = s1->held;
                out_buf = s1->window + s1->hsize;

    if(ibuf == NULL && out_size > ceil(rate->inpipe)) {
      out_size = ceil(rate->inpipe);
    }

                if (out_size > oskip + *osamp) out_size = oskip + *osamp;

    for(q=obuf, k=oskip; k < out_size; k++)
      *q++ = clipfloat(out_buf[k] * ISCALE); /* should clip-limit */

                *osamp = q-obuf;
                rate->inpipe -= *osamp;
                oskip -= out_size - *osamp;
                rate->oskip = oskip;

                s1->hsize += out_size;
    s1->held -= out_size;
    if (s1->held == 0) {
      s1->hsize = 0;
    }

  }
  return (ST_SUCCESS);

}

/*
 * Process tail of input samples.
 */
int st_poly_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
  st_size_t in_size;
  /* Call "flow" with NULL input. */
  st_poly_flow(effp, NULL, obuf, &in_size, osamp);
  return (ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.
 * Don't close input file!
 */
int st_poly_stop(eff_t effp)
{
    poly_t rate = (poly_t) effp->priv;
    polystage *s;
    int k;

    for(k = 0; k <= rate->total; k++) {
      s = rate->stage[k];
      free((void *) s->window);
      if (s->filt_array) free((void *) s->filt_array);
      free((void *) s);
    }
    return (ST_SUCCESS);
}
