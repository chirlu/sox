/* libSoX rate change effect file.
 *
 * July 14, 1998
 * Copyright 1998  K. Bradley, Carnegie Mellon University
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
 * October 29, 1999
 * Various changes, bugfixes, speedups, by Stan Brooks.
 *
 */

#include "sox_i.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define Float float/*double*/
#define ISCALE 0x10000
#define MF 30

typedef struct {
  int up, down;   /* up/down conversion factors for this stage      */
  int filt_len;   /* # coefficients in filter_array                 */
  Float *filt_array; /* filter coefficients                         */
  int held;       /* # samples held in input but not yet processed  */
  int hsize;      /* # samples of past-history kept in lower window */
  int size;       /* # samples current data which window can accept */
  Float *window;  /* this is past_hist[hsize], then input[size]     */
} polystage;

typedef struct {
  unsigned lcmrate;             /* least common multiple of rates */
  unsigned inskip, outskip;     /* LCM increments for I & O rates */
  double Factor;                 /* out_rate/in_rate               */
  unsigned long total;           /* number of filter stages        */
  size_t oskip;               /* output samples to skip at start*/
  double inpipe;                 /* output samples 'in the pipe'   */
  polystage *stage[MF];          /* array of pointers to polystage structs */
  int win_type;
  int win_width;
  Float cutoff;
  int m1[MF], m2[MF], b1[MF], b2[MF]; /* arrays used in optimize_factors */
} priv_t;

/*
 * Process options
 */
static int sox_poly_getopts(sox_effect_t * effp, int n, char **argv)
{
  priv_t * rate = (priv_t *) effp->priv;

  rate->win_type = 0;           /* 0: nuttall, 1: hamming */
  rate->win_width = 1024;
  rate->cutoff = 0.95;

  while (n >= 2) {
    /* Window type check */
    if(!strcmp(argv[0], "-w")) {
      if(!strcmp(argv[1], "ham"))
        rate->win_type = 1;
      if(!strcmp(argv[1], "nut"))
        rate->win_type = 0;
      argv += 2;
      n -= 2;
      continue;
    }

    /* Window width check */
    if(!strcmp(argv[0], "-width")) {
        rate->win_width = atoi(argv[1]);
      argv += 2;
      n -= 2;
      continue;
    }

    /* Cutoff frequency check */
    if(!strcmp(argv[0], "-cutoff")) {
      rate->cutoff = atof(argv[1]);
      argv += 2;
      n -= 2;
      continue;
    }

    lsx_fail("Polyphase: unknown argument (%s %s)!", argv[0], argv[1]);
    return SOX_EOF;
  }

  return SOX_SUCCESS;
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

static int prime(unsigned n, int *q0)
{
  const unsigned short *p;
  int pr, *q;

  p = primes;
  q = q0;
  lsx_debug("factors(%d) =",n);
  while (n > 1) {
    while ((pr = *p) && (n % pr)) p++;
    if (!pr) {
      lsx_fail("Number %d too large of a prime.",n);
      pr = n;
    }
    *q++ = pr;
    n /= pr;
  }
  *q = 0;
  for (pr=0; pr<q-q0; pr++) lsx_debug(" %d",q0[pr]);
  lsx_debug(" ");
  return (q-q0);
}

static int permute(int *m, int *l, int ct, int ct1, size_t amalg)
{
  size_t k, n;
  int *p;
  int *q;

  p=l; q=m;
  while (ct1>ct) { *q++=1; ct++;}
  while ((*q++=*p++)) ;
  if (ct<=1) return ct;

  for (k=ct; k>1; ) {
    int tmp;
    unsigned long j;
    j = (rand()%32768) + ((rand()%32768)<<13); /* reasonably big */
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
  /*for (k=0; k<p-m; k++) lsx_debug(" %d",m[k]);*/
  /*lsx_debug("");*/
  return (p-m);
}

static int optimize_factors(priv_t * rate, unsigned numer, unsigned denom, int *l1, int *l2)
{
  unsigned f_min;
  int c_min,u_min,ct1,ct2;
  size_t amalg;
  int k;

  memset(rate->m1,0,sizeof(int)*MF);
  memset(rate->m2,0,sizeof(int)*MF);
  memset(rate->b1,0,sizeof(int)*MF);
  memset(rate->b2,0,sizeof(int)*MF);

  f_min = numer; if (f_min>denom) f_min = denom;
  c_min = 1<<30;
  u_min = 0;

  /* Find the prime factors of numer and denom */
  ct1 = prime(numer,l1);
  ct2 = prime(denom,l2);

  for (amalg = max(9,l2[0]); amalg <= (size_t)(9+l2[ct2-1]); amalg++) {
    for (k = 0; k<100000; k++) {
      unsigned f;
      int u,u1,u2,j,cost;
      cost = 0;
      f = denom;
      u = min(ct1,ct2) + 1;
      /*lsx_debug("pfacts(%d): ", numer);*/
      u1 = permute(rate->m1,l1,ct1,u,amalg);
      /*lsx_debug("pfacts(%d): ", denom);*/
      u2 = permute(rate->m2,l2,ct2,u,amalg);
      u = max(u1,u2);
      for (j=0; j<u; j++) {
        if (j>=u1) rate->m1[j]=1;
        if (j>=u2) rate->m2[j]=1;
        f = (f * rate->m1[j])/rate->m2[j];
        if (f < f_min) goto fail;
        cost += f + rate->m1[j]*rate->m2[j];
      }
      if (c_min>cost) {
        c_min = cost;
        u_min = u;
        if (sox_globals.verbosity >= 4) {
          lsx_debug("c_min %d, [%d-%d]:",c_min,numer,denom);
          for (j=0; j<u; j++)
            lsx_debug(" (%d,%d)",rate->m1[j],rate->m2[j]);
          lsx_debug(" ");
        }
        memcpy(rate->b1,rate->m1,u*sizeof(int));
        memcpy(rate->b2,rate->m2,u*sizeof(int));
      }
     fail:
        ;;
    }
    if (u_min) break;
  }
  if (u_min) {
    memcpy(l1,rate->b1,u_min*sizeof(int));
    memcpy(l2,rate->b2,u_min*sizeof(int));
  }
  l1[u_min] = 0;
  l2[u_min] = 0;
  return u_min;
}

/* Calculate a Nuttall window of a given length.
   Buffer must already be allocated to appropriate size.
   */

static void nuttall(Float *buffer, int length)
{
  int j;
  double N;
  int N1;

  if(buffer == NULL || length <= 0)
    lsx_fail("Illegal buffer %p or length %d to nuttall.", (void *)buffer, length);

  /* Initial variable setups. */
  N = length;
  N1 = length/2;

  for(j = 0; j < length; j++) {
    buffer[j] = 0.3635819 +
      0.4891775 * cos(2*M_PI*1*(j - N1) / N) +
      0.1365995 * cos(2*M_PI*2*(j - N1) / N) +
      0.0106411 * cos(2*M_PI*3*(j - N1) / N);
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
      lsx_fail("Illegal buffer %p or length %d to hamming.",(void *)buffer,length);

    N1 = length/2;
    for(j=0;j<length;j++)
      buffer[j] = 0.5 - 0.46 * cos(M_PI*j/N1);
}

/* Calculate the sinc function properly */

static Float sinc(double value)
{
    return(fabs(value) < 1E-50 ? 1.0 : sin(value) / value);
}

/* Design a low-pass FIR filter using window technique.
   Length of filter is in length, cutoff frequency in cutoff.
   0 < cutoff <= 1.0 (normalized frequency)

   buffer must already be allocated.
*/
static void fir_design(priv_t * rate, Float *buffer, int length, double cutoff)
{
    int j;
    double sum;

    if(buffer == NULL || length < 0 || cutoff < 0 || cutoff > M_PI)
      lsx_fail("Illegal buffer %p, length %d, or cutoff %f.",(void *)buffer,length,cutoff);

    /* Use the user-option of window type */
    if (rate->win_type == 0)
      nuttall(buffer, length); /* Design Nuttall window:  ** dB cutoff */
    else
      hamming(buffer,length);  /* Design Hamming window:  43 dB cutoff */

    /* lsx_debug("# fir_design length=%d, cutoff=%8.4f",length,cutoff); */
    /* Design filter:  windowed sinc function */
    sum = 0.0;
    for(j=0;j<length;j++) {
      buffer[j] *= sinc(M_PI*cutoff*(j-length/2)); /* center at length/2 */
      /* lsx_debug("%.1f %.6f",(float)j,buffer[j]); */
      sum += buffer[j];
    }
    sum = (double)1.0/sum;
    /* Normalize buffer to have gain of 1.0: prevent roundoff error */
    for(j=0;j<length;j++) {
      buffer[j] *= sum;
    }
    /* lsx_debug("# end"); */
}

#define RIBLEN 2048

static int sox_poly_start(sox_effect_t * effp)
{
    priv_t * rate = (priv_t *) effp->priv;
    static int l1[MF], l2[MF];
    double skip = 0;
    int total, size, uprate;
    int k;

    if (effp->in_signal.rate == effp->out_signal.rate)
      return SOX_EFF_NULL;

    effp->out_signal.channels = effp->in_signal.channels;

    rate->lcmrate = lsx_lcm((unsigned)effp->in_signal.rate,
                           (unsigned)effp->out_signal.rate);

    /* Cursory check for LCM overflow.
     * If both rates are below 65k, there should be no problem.
     * 16 bits x 16 bits = 32 bits, which we can handle.
     */

    rate->inskip = rate->lcmrate / (sox_sample_t)effp->in_signal.rate;
    rate->outskip = rate->lcmrate / (sox_sample_t)effp->out_signal.rate;
    rate->Factor = (double)rate->inskip / (double)rate->outskip;
    rate->inpipe = 0;
    {
      int f = RIBLEN/max(rate->inskip,rate->outskip);
      if (f == 0) f = 1;
      size = f * rate->outskip; /* reasonable input block size */
    }

    /* Find the prime factors of inskip and outskip */
    total = optimize_factors(rate, rate->inskip, rate->outskip, l1, l2);
    rate->total = total;
    /* l1 and l2 are now lists of the up/down factors for conversion */

    lsx_debug("Poly:  input rate %g, output rate %g.  %d stages.",
            effp->in_signal.rate, effp->out_signal.rate,total);
    lsx_debug("Poly:  window: %s  size: %d  cutoff: %f.",
            (rate->win_type == 0) ? ("nut") : ("ham"), rate->win_width, rate->cutoff);

    /* Create an array of filters and past history */
    uprate = effp->in_signal.rate;
    for (k = 0; k < total; k++) {
      int j, prod, f_cutoff, f_len;
      polystage *s;

      rate->stage[k] = s = lsx_malloc(sizeof(polystage));
      s->up = l1[k];
      s->down = l2[k];
      f_cutoff = max(s->up, s->down);
      f_len = max(20 * f_cutoff, rate->win_width);
      prod = s->up * s->down;
      if (prod > 2*f_len) prod = s->up;
      f_len = ((f_len+prod-1)/prod) * prod; /* reduces rounding-errors in polyphase() */
      s->size = size;
      s->hsize = f_len/s->up; /* this much of window is past-history */
      s->held = 0;
      lsx_debug("Poly:  stage %d:  Up by %d, down by %d,  i_samps %d, hsize %d",
              k+1,s->up,s->down,size, s->hsize);
      s->filt_len = f_len;
      s->filt_array = lsx_malloc(sizeof(Float) * f_len);
      s->window = lsx_malloc(sizeof(Float) * (s->hsize+size));
      /* zero past_history section of window */
      for(j = 0; j < s->hsize; j++)
        s->window[j] = 0.0;

      uprate *= s->up;
      lsx_debug("Poly:         :  filt_len %d, cutoff freq %.1f",
              f_len, uprate * rate->cutoff / f_cutoff);
      uprate /= s->down;
      fir_design(rate, s->filt_array, f_len, rate->cutoff / f_cutoff);

      skip *= s->up;
      skip += f_len;
      skip /= s->down;

      size = (size * s->up) / s->down;  /* this is integer */
    }
    rate->oskip = skip/2;
    { /* bogus last stage is for output buffering */
      polystage *s;
      rate->stage[k] = s = lsx_malloc(sizeof(polystage));
      s->up = s->down = 0;
      s->size = size;
      s->hsize = 0;
      s->held = 0;
      s->filt_len = 0;
      s->filt_array = NULL;
      s->window = lsx_malloc(sizeof(Float) * size);
    }
    lsx_debug("Poly:  output samples %d, oskip %lu",size, (unsigned long)rate->oskip);
    return (SOX_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

/* REMARK: putting this in a separate subroutine improves gcc's optimization */
static double sox_prod(const Float *q, int qstep, const Float *p, int n)
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
  /*for (mm=0; mm<s->filt_len; mm++) lsx_debug("cf_%d %f",mm,s->filt_array[mm]);*/
  /* assumes s->size divisible by down (now true) */
  o_top = output + (s->size * up) / down;
  /*lsx_debug(" isize %d, osize %d, up %d, down %d, N %d", s->size, o_top-output, up, down, f_len);*/
  for (mm=0, o=output; o < o_top; mm+=down, o++) {
    double sum;
    const Float *p, *q;
    q = s->filt_array + (mm%up);   /* decimated coef pointer */
    p  = in + (mm/up);
    sum = sox_prod(q, up, p, f_len/up);
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

static int sox_poly_flow(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf,
                 size_t *isamp, size_t *osamp)
{
  priv_t * rate = (priv_t *) effp->priv;
  polystage *s0,*s1;

  /* Sanity check:  how much can we tolerate? */
  /* lsx_debug("*isamp=%d *osamp=%d",*isamp,*osamp); */
  s0 = rate->stage[0];            /* the first stage */
  s1 = rate->stage[rate->total];  /* the 'last' stage is output buffer */
  {
    size_t in_size, gap, k;

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
    size_t k;
    /* input buffer full, output buffer empty, so do process */

    for(k=0; k<rate->total; k++) {
      polystage *s;
      Float *out;

      s = rate->stage[k];

      out = rate->stage[k+1]->window + rate->stage[k+1]->hsize;

      /* lsx_debug("k=%d  insize=%d",k,in_size); */
      polyphase(out, s);

      /* copy input history into lower portion of rate->window[k] */
      update_hist(s->window, s->hsize, s->size);
      s->held = 0;
    }

    s1->held = s1->size;
    s1->hsize = 0;

  }

  {
    sox_sample_t *q;
    size_t out_size;
    size_t oskip;
    Float *out_buf;
    size_t k;

    oskip = rate->oskip;
                out_size = s1->held;
                out_buf = s1->window + s1->hsize;

    if(ibuf == NULL && out_size > ceil(rate->inpipe)) {
      out_size = ceil(rate->inpipe);
    }

                if (out_size > oskip + *osamp) out_size = oskip + *osamp;

    for(q=obuf, k=oskip; k < out_size; k++)
    {
        float f;
        f = out_buf[k] * ISCALE; /* should clip-limit */
        SOX_SAMPLE_CLIP_COUNT(f, effp->clips);
        *q++ = f;
    }

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
  return (SOX_SUCCESS);

}

/*
 * Process tail of input samples.
 */
static int sox_poly_drain(sox_effect_t * effp, sox_sample_t *obuf, size_t *osamp)
{
  size_t in_size;
  /* Call "flow" with NULL input. */
  sox_poly_flow(effp, NULL, obuf, &in_size, osamp);
  return (SOX_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.
 * Don't close input file!
 */
static int sox_poly_stop(sox_effect_t * effp)
{
    priv_t * rate = (priv_t *)effp->priv;
    size_t k;

    for (k = 0; k <= rate->total; k++) {
      free(rate->stage[k]->window);
      free(rate->stage[k]->filt_array);
      free(rate->stage[k]);
    }

    return SOX_SUCCESS;
}

static sox_effect_handler_t sox_polyphase_effect = {
  "polyphase",
  "-w {nut|ham}   window type\n"
  "       -width n       window width in samples [default 1024]\n"
  "\n"
  "       -cutoff float  frequency cutoff for base bandwidth [default 0.95]",
  SOX_EFF_RATE | SOX_EFF_DEPRECATED,
  sox_poly_getopts,
  sox_poly_start,
  sox_poly_flow,
  sox_poly_drain,
  sox_poly_stop,
  NULL, sizeof(priv_t)
};

const sox_effect_handler_t *sox_polyphase_effect_fn(void)
{
    return &sox_polyphase_effect;
}
