
/*
 * July 14, 1998
 * Copyright 1998  K. Bradley, Carnegie Mellon University
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools rate change effect file.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "st.h"

typedef struct _list {
    int number;
    float *data_buffer;
    struct _list *next;
} List;

typedef struct polyphase {

  unsigned long	lcmrate;	   /* least common multiple of rates */
  unsigned long inskip, outskip;   /* LCM increments for I & O rates */
  unsigned long total;
  unsigned long intot, outtot;	   /* total samples in terms of LCM rate */
  long	lastsamp;

  float **filt_array;
  float **past_hist;
  float *input_buffer;
  int *filt_len;

  List *l1, *l2;
  
} *poly_t;

/*
 * Process options
 */

/* Options:  

   -w <nut / ham>        :  window type
   -width <short / long> :  window width
                            short = 128 samples
			    long  = 1024 samples
	  <num>	            num:  explicit number
 
   -cutoff <float>       :  frequency cutoff for base bandwidth.
                            Default = 0.95 = 95%
*/

static int win_type  = 0;
static int win_width = 1024;
static float cutoff = 0.95;
   
void poly_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
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

    fail("Polyphase: unknown argument (%s %s)!", argv[0], argv[1]);
  }
}

/*
 * Prepare processing.
 */

static int primes[] = {
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
  919, 929, 937, 941, 947, 953, 967, 971, 977, 983, 991, 997
};

#ifndef max
#define max(x,y) ((x > y) ? x : y)
#endif

List *prime(number)
int number;
{
    int j;
    List *element = NULL;

    if(number == 1)
      return NULL;

    for(j=167;j>= 0;j--) {
	if(number % primes[j] == 0) {
	    element = (List *) malloc(sizeof(List));
	    element->number = primes[j];
	    element->data_buffer = NULL;
	    element->next = prime(number / primes[j]);
	    break;
	}
    }

    if(element == NULL) {
	fail("Number %d too large of a prime.\n",number);
    }

    return element;
}

List *prime_inv(number)
int number;
{
    int j;
    List *element = NULL;

    if(number == 1)
      return NULL;

    for(j=0;j<168;j++) {
	if(number % primes[j] == 0) {
	    element = (List *) malloc(sizeof(List));
	    element->number = primes[j];
	    element->data_buffer = NULL;
	    element->next = prime_inv(number / primes[j]);
	    break;
	}
    }

    if(element == NULL) {
	fail("Number %d too large of a prime.\n",number);
    }

    return element;
}

#ifndef PI
#define PI 3.14159265358979
#endif

/* Calculate a Nuttall window of a given length.
   Buffer must already be allocated to appropriate size.
   */

void nuttall(buffer, length)
float *buffer;
int length;
{
  int j;
  double N;
  double N1;

  if(buffer == NULL || length < 0)
    fail("Illegal buffer %p or length %d to nuttall.\n", buffer, length);

  /* Initial variable setups. */
  N = (double) length - 1.0;
  N1 = N / 2.0;

  for(j = 0; j < length; j++) {
    buffer[j] = 0.36335819 + 
      0.4891775 * cos(2*PI*1*(j - N1) / N) +
      0.1365995 * cos(2*PI*2*(j - N1) / N) + 
      0.0106411 * cos(2*PI*3*(j - N1) / N);
  }
}
/* Calculate a Hamming window of given length.
   Buffer must already be allocated to appropriate size.
*/

void hamming(buffer, length)
float *buffer;
int length;
{
    int j;

    if(buffer == NULL || length < 0)
      fail("Illegal buffer %p or length %d to hamming.\n",buffer,length);

    for(j=0;j<length;j++) 
      buffer[j] = 0.5 - 0.46 * cos(2*PI*j/(length-1));
}

/* Calculate the sinc function properly */

float sinc(value)
float value;
{   
    return(fabs(value) < 1E-50 ? 1.0 : sin(value) / value);
}

/* Design a low-pass FIR filter using window technique.
   Length of filter is in length, cutoff frequency in cutoff.
   0 < cutoff <= 1.0 (normalized frequency)

   buffer must already be allocated.
*/
void fir_design(buffer, length, cutoff)
float *buffer;
int length;
float cutoff;
{
    int j;
    float sum;
    float *ham_win;

    if(buffer == NULL || length < 0 || cutoff < 0 || cutoff > PI)
      fail("Illegal buffer %p, length %d, or cutoff %f.\n",buffer,length,cutoff);

    /* Design Hamming window:  43 dB cutoff */
    ham_win = (float *)malloc(sizeof(float) * length);

    /* Use the user-option of window type */
    if(win_type == 0) 
      nuttall(ham_win, length);
    else
      hamming(ham_win,length);

    /* Design filter:  windowed sinc function */
    sum = 0.0;
    for(j=0;j<length;j++) {
	buffer[j] = sinc(PI*cutoff*(j-length/2)) * ham_win[j] / (2*cutoff);
	sum += buffer[j];
    }

    /* Normalize buffer to have gain of 1.0: prevent roundoff error */
    for(j=0;j<length;j++)
      buffer[j] /= sum;

    free((void *) ham_win);
}
    

void poly_start(effp)
eff_t effp;
{
    poly_t rate = (poly_t) effp->priv;
    List *t, *t2;
    int num_l1, num_l2;
    int j,k;
    float f_cutoff;

    extern long lcm();
	
    rate->lcmrate = lcm((long)effp->ininfo.rate, (long)effp->outinfo.rate);

    /* Cursory check for LCM overflow.  
     * If both rate are below 65k, there should be no problem.
     * 16 bits x 16 bits = 32 bits, which we can handle.
     */

    rate->inskip = rate->lcmrate / effp->ininfo.rate;
    rate->outskip = rate->lcmrate / effp->outinfo.rate; 

    /* Find the prime factors of inskip and outskip */
    rate->l1 = prime(rate->inskip);

    /* If we're going up, order things backwards. */
    if(effp->ininfo.rate < effp->outinfo.rate)
      rate->l2 = prime_inv(rate->outskip);
    else
      rate->l2 = prime(rate->outskip);
    
    /* Find how many factors there were */
    if(rate->l1 == NULL)
      num_l1 = 0;
    else
      for(num_l1=0, t = rate->l1; t != NULL; num_l1++, t=t->next);

    if(rate->l2 == NULL) 
      num_l2 = 0;
    else
      for(num_l2=0, t = rate->l2; t != NULL; num_l2++, t=t->next);

    k = 0;
    t = rate->l1;

    /* Compact the lists to be less than 10 */
    while(k < num_l1 - 1) {
	if(t->number * t->next->number < 10) {
	    t->number = t->number * t->next->number;
	    t2 = t->next;
	    t->next = t->next->next;
	    t2->next = NULL;
	    free((void *) t2);
	    num_l1--;
	} else {
	    k++;
	    t = t->next;
	}
    }

    k = 0;
    t = rate->l2;

    while(k < num_l2 - 1) {
	if(t->number * t->next->number < 10) {
	    t->number = t->number * t->next->number;
	    t2 = t->next;
	    t->next = t->next->next;
	    t2->next = NULL;
	    free((void *) t2);
	    num_l2--;
	} else {
	    k++;
	    t = t->next;
	}
    }

    /* l1 and l2 are now lists of the prime factors compacted,
       meaning that they're the lists of up/down sampling we need
       */

    /* Stretch them to be the same length by padding with 1 (no-op) */
    if(num_l1 < num_l2) {
	t = rate->l1;

	if(t == NULL) {
	    rate->l1 = (List *)malloc(sizeof(List));
	    rate->l1->next = NULL;
	    rate->l1->number = 1;
	    rate->l1->data_buffer = NULL;
	    t = rate->l1;
	    num_l1++;
	}

	while(t->next != NULL)
	  t = t->next;

	for(k=0;k<num_l2-num_l1;k++) {
	    t->next = (List *) malloc(sizeof(List));
	    t->next->number = 1;
	    t->next->data_buffer = NULL;
	    t = t->next;
	}

	t->next = NULL;
	num_l1 = num_l2;
    } else {
	t = rate->l2;

	if(t == NULL) {
	    rate->l2 = (List *)malloc(sizeof(List));
	    rate->l2->next = NULL;
	    rate->l2->number = 1;
	    rate->l2->data_buffer = NULL;
	    t = rate->l2;
	    num_l2++;
	}
	  
	/*
	  while(t->next != NULL)
	  t = t->next;
	  */

	for(k=0;k<num_l1-num_l2;k++) {
	  t = rate->l2;
	  rate->l2 = (List *) malloc(sizeof(List));
	  rate->l2->number = 1;
	  rate->l2->data_buffer = NULL;
	  rate->l2->next = t;
	}

	/* t->next = NULL; */
	num_l2 = num_l1;
    }

    /* l1 and l2 are now the same size. */
    rate->total = num_l1;

    report("Poly:  input rate %d, output rate %d.  %d stages.",effp->ininfo.rate, effp->outinfo.rate,num_l1);
    report("Poly:  window: %s  size: %d  cutoff: %f.", (win_type == 0) ? ("nut") : ("ham"), win_width, cutoff);

    for(k=0, t=rate->l1, t2=rate->l2;k<num_l1;k++,t=t->next,t2=t2->next)
      report("Poly:  stage %d:  Up by %d, down by %d.",k+1,t->number,t2->number);

    /* We'll have an array of filters and past history */
    rate->filt_array = (float **) malloc(sizeof(float *) * num_l1);
    rate->past_hist = (float **) malloc(sizeof(float *) * num_l1);
    rate->filt_len = (int *) malloc(sizeof(int) * num_l1);

    for(k = 0, t = rate->l1, t2 = rate->l2; k < num_l1; k++) {

      rate->filt_len[k] = max(2 * 10 * max(t->number,t2->number), win_width);
      rate->filt_array[k] = (float *) malloc(sizeof(float) * rate->filt_len[k]);
      rate->past_hist[k] = (float *) malloc(sizeof(float) * rate->filt_len[k]);
      
      t->data_buffer = (float *) malloc(sizeof(float) * 1024 * rate->inskip);
	
      for(j = 0; j < rate->filt_len[k]; j++)
	rate->past_hist[k][j] = 0.0;

      f_cutoff = (t->number > t2->number) ? 
	(float) t->number : (float) t2->number;

      fir_design(rate->filt_array[k], rate->filt_len[k]-1, cutoff / f_cutoff);

      t = t->next;
      t2 = t2->next;
    }

    rate->input_buffer = (float *) malloc(sizeof(float) * 2048);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

static float *h;
static int M, L, N;

void polyphase_init(coef, num_coef, up_rate, down_rate)
float *coef;
int num_coef;
int up_rate;
int down_rate;
{
    h = coef;
    M = down_rate;
    L = up_rate;
    N = num_coef;
}
    
void polyphase(input, output, past, num_samples_input)
float *input;
float *output;
float *past;
int num_samples_input;
{
    int num_output;
    int m,n;
    float sum;
    float inp;
    int base;
    int h_base;

    num_output = num_samples_input * L / M;

    for(m=0;m<num_output;m++) {
	sum = 0.0;
	base = (int) (m*M/L);
	h_base = (m*M) % L;

	for(n=0;n<N / L;n++) {
	    if(base - n < 0)
	      inp = past[base - n + N];
	    else
	      inp = input[base - n];

	    sum += h[n*L + h_base] * inp;
	}

	output[m] = sum * L * 0.95;
    }
}

void poly_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
long *ibuf, *obuf;
int *isamp, *osamp;
{
  poly_t rate = (poly_t) effp->priv;
  float *temp_buf, *temp_buf2;
  int j,k;
  List *t1, *t2;
  int in_size, out_size;

  /* Sanity check:  how much can we tolerate? */
  in_size = *isamp;
  out_size = in_size * rate->inskip / rate->outskip;
  if(out_size > *osamp) {
    in_size = *osamp * rate->outskip / rate->inskip;
    *isamp = in_size;
  }
  
  /* Check to see if we're really draining */
  if(ibuf != NULL) {
    for(k=0;k<*isamp;k++)
      rate->input_buffer[k] = (float) (ibuf[k] >> 16);
  } else {
    for(k=0;k<*isamp;k++)
      rate->input_buffer[k] = 0.0;
  }      
  
  temp_buf = rate->input_buffer;
  
  t1 = rate->l1;
  t2 = rate->l2;
  
  for(k=0;k<rate->total;k++,t1=t1->next,t2=t2->next) {
    
    polyphase_init(rate->filt_array[k], rate->filt_len[k], 
		   t1->number,t2->number);
    
    out_size = (in_size) * t1->number / t2->number;
    
    temp_buf2 = t1->data_buffer;
    
    polyphase(temp_buf, temp_buf2, rate->past_hist[k], in_size);
    
    for(j = 0; j < rate->filt_len[k]; j++) 
      rate->past_hist[k][j] = temp_buf[j+in_size - rate->filt_len[k]];
    
    in_size = out_size;
    
    temp_buf = temp_buf2;
  }
  
  if(out_size > *osamp)
    out_size = *osamp;
  
  *osamp = out_size;

  if(ibuf != NULL) {
    for(k=0;k < out_size;k++)
      obuf[k] = ((int) temp_buf[k]) << 16;
  } else {

    /* Wait for all-zero samples to come through.
       Should happen eventually with all-zero
       input */
    int found = 0;

    for(k=0; k < out_size; k++) {
      obuf[k] = ((int) temp_buf[k] << 16);
      if(obuf[k] != 0)
	found = 1;
    }
    if(!found)
      *osamp = 0;
  }
}

/*
 * Process tail of input samples.
 */
void poly_drain(effp, obuf, osamp)
eff_t effp;
long *obuf;
long *osamp;
{
  long in_size = 1024;

  /* Call "flow" with NULL input. */
  poly_flow(effp, NULL, obuf, &in_size, osamp);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
void poly_stop(effp)
eff_t effp;
{
    List *t, *t2;
    poly_t rate = (poly_t) effp->priv;
    int k;

    /* Free lists */
    for(t = rate->l1; t != NULL; ) {
	t2 = t->next;
	t->next = NULL;
	if(t->data_buffer != NULL)
	  free((void *) t->data_buffer);
	free((void *) t);
	t = t2;
    }

    for(t = rate->l2; t != NULL; ) {
	t2 = t->next;
	t->next = NULL;
	if(t->data_buffer != NULL)
	  free((void *) t->data_buffer);
	free((void *) t);
	t = t2;
    }

    for(k = 0; k < rate->total;k++) {
	free((void *) rate->past_hist[k]);
	free((void *) rate->filt_array[k]);
    }

    free((void *) rate->past_hist);
    free((void *) rate->filt_array);
    free((void *) rate->filt_len);
}

