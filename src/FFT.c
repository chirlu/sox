/*
 *
 * FFT.c
 *
 * Based on FFT.cpp from Audacity, with the following permission from
 * its author, Dominic Mazzoni (in particular, relicensing the code
 * for use in SoX):
 *
 *  I hereby license you under the LGPL all of the code in FFT.cpp
 *  from any version of Audacity, with the exception of the windowing
 *  function code, as I wrote the rest of the line [sic] that appears
 *  in any version of Audacity (or they are derived from Don Cross or
 *  NR, which is okay).
 *
 *  -- Dominic Mazzoni <dominic@audacityteam.org>, 18th November 2006
 *
 * As regards the windowing function, WindowFunc, Dominic granted a
 * license to it too, writing on the same day:
 *
 *  OK, we're good. That's the original version that I wrote, before
 *  others contributed.
 *
 * Some of this code was based on a free implementation of an FFT
 * by Don Cross, available on the web at:
 *
 * http://www.intersrv.com/~dcross/fft.html [no longer, it seems]
 *
 * The basic algorithm for his code was based on Numerical Recipes
 * in Fortran.
 *
 * This file is now part of SoX, and is copyright Ian Turner and others.
 * 
 * SoX is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Foobar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, write to the Free Software
 * Foundation, Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301,
 * USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "st_i.h"

#include "FFT.h"

int **gFFTBitTable = NULL;
const int MaxFastBits = 16;

static int IsPowerOfTwo(int x)
{
   if (x < 2)
      return 0;

   return !(x & (x-1));         /* Thanks to 'byang' for this cute trick! */
}

static int NumberOfBitsNeeded(int PowerOfTwo)
{
   int i;

   if (PowerOfTwo < 2) {
      st_debug("Error: FFT called with size %d", PowerOfTwo);
      exit(2);
   }

   for (i = 0;; i++)
      if (PowerOfTwo & (1 << i))
         return i;
}

static int ReverseBits(int index, int NumBits)
{
   int i, rev;

   for (i = rev = 0; i < NumBits; i++) {
      rev = (rev << 1) | (index & 1);
      index >>= 1;
   }

   return rev;
}


/* This function permanently allocates about 250Kb (actually (2**16)-2 ints). */
static void InitFFT(void)
{
   int len, b;
   
   gFFTBitTable = (int**)xcalloc(MaxFastBits, sizeof(*gFFTBitTable));
   
   for (b = 1, len = 2; b <= MaxFastBits; b++) {
      int i;

      gFFTBitTable[b - 1] = (int*)xcalloc(len, sizeof(**gFFTBitTable));
      for (i = 0; i < len; i++)
        gFFTBitTable[b - 1][i] = ReverseBits(i, b);

      len <<= 1;
   }
}

#define FastReverseBits(i, NumBits) \
   (NumBits <= MaxFastBits) ? gFFTBitTable[NumBits - 1][i] : ReverseBits(i, NumBits)

/*
 * Complex Fast Fourier Transform
 */
void FFT(int NumSamples,
         int InverseTransform,
         const float *RealIn, float *ImagIn, float *RealOut, float *ImagOut)
{
   int NumBits;                 /* Number of bits needed to store indices */
   int i, j, k, n;
   int BlockSize, BlockEnd;

   double angle_numerator = 2.0 * M_PI;
   float tr, ti;                /* temp real, temp imaginary */

   if (!IsPowerOfTwo(NumSamples)) {
      st_debug("%d is not a power of two", NumSamples);
      exit(2);
   }

   if (!gFFTBitTable)
      InitFFT();

   if (InverseTransform)
      angle_numerator = -angle_numerator;

   NumBits = NumberOfBitsNeeded(NumSamples);

   /*
    **   Do simultaneous data copy and bit-reversal ordering into outputs...
    */

   for (i = 0; i < NumSamples; i++) {
      j = FastReverseBits(i, NumBits);
      RealOut[j] = RealIn[i];
      ImagOut[j] = (ImagIn == NULL) ? 0.0 : ImagIn[i];
   }

   /*
    **   Do the FFT itself...
    */

   BlockEnd = 1;
   for (BlockSize = 2; BlockSize <= NumSamples; BlockSize <<= 1) {

      double delta_angle = angle_numerator / (double) BlockSize;

      float sm2 = sin(-2 * delta_angle);
      float sm1 = sin(-delta_angle);
      float cm2 = cos(-2 * delta_angle);
      float cm1 = cos(-delta_angle);
      float w = 2 * cm1;
      float ar0, ar1, ar2, ai0, ai1, ai2;

      for (i = 0; i < NumSamples; i += BlockSize) {
         ar2 = cm2;
         ar1 = cm1;

         ai2 = sm2;
         ai1 = sm1;

         for (j = i, n = 0; n < BlockEnd; j++, n++) {
            ar0 = w * ar1 - ar2;
            ar2 = ar1;
            ar1 = ar0;

            ai0 = w * ai1 - ai2;
            ai2 = ai1;
            ai1 = ai0;

            k = j + BlockEnd;
            tr = ar0 * RealOut[k] - ai0 * ImagOut[k];
            ti = ar0 * ImagOut[k] + ai0 * RealOut[k];

            RealOut[k] = RealOut[j] - tr;
            ImagOut[k] = ImagOut[j] - ti;

            RealOut[j] += tr;
            ImagOut[j] += ti;
         }
      }

      BlockEnd = BlockSize;
   }

   /*
      **   Need to normalize if inverse transform...
    */

   if (InverseTransform) {
      float denom = (float) NumSamples;

      for (i = 0; i < NumSamples; i++) {
         RealOut[i] /= denom;
         ImagOut[i] /= denom;
      }
   }
}

/*
 * Real Fast Fourier Transform
 *
 * This function was based on the code in Numerical Recipes for C.
 * In Num. Rec., the inner loop is based on a single 1-based array
 * of interleaved real and imaginary numbers.  Because we have two
 * separate zero-based arrays, our indices are quite different.
 * Here is the correspondence between Num. Rec. indices and our indices:
 *
 * i1  <->  real[i]
 * i2  <->  imag[i]
 * i3  <->  real[n/2-i]
 * i4  <->  imag[n/2-i]
 */

void RealFFT(int NumSamples, const float *RealIn, float *RealOut, float *ImagOut)
{
   int Half = NumSamples / 2;
   int i, i3;
   float theta = M_PI / Half;
   float wtemp = (float) sin(0.5 * theta);
   float wpr = -2.0 * wtemp * wtemp;
   float wpi = (float) sin(theta);
   float wr = 1.0 + wpr;
   float wi = wpi;
   float h1r, h1i, h2r, h2i;
   float *tmpReal, *tmpImag;

   tmpReal = (float*)xcalloc(NumSamples, sizeof(float));
   tmpImag = tmpReal + Half;

   for (i = 0; i < Half; i++) {
      tmpReal[i] = RealIn[2 * i];
      tmpImag[i] = RealIn[2 * i + 1];
   }

   FFT(Half, 0, tmpReal, tmpImag, RealOut, ImagOut);

   for (i = 1; i < Half / 2; i++) {
     i3 = Half - i;
       
     h1r = 0.5 * (RealOut[i] + RealOut[i3]);
     h1i = 0.5 * (ImagOut[i] - ImagOut[i3]);
     h2r = 0.5 * (ImagOut[i] + ImagOut[i3]);
     h2i = -0.5 * (RealOut[i] - RealOut[i3]);
       
     RealOut[i] = h1r + wr * h2r - wi * h2i;
     ImagOut[i] = h1i + wr * h2i + wi * h2r;
     RealOut[i3] = h1r - wr * h2r + wi * h2i;
     ImagOut[i3] = -h1i + wr * h2i + wi * h2r;
       
     wtemp = wr;
     wr = wr * wpr - wi * wpi + wr;
     wi = wi * wpr + wtemp * wpi + wi;
   }
     
   h1r = RealOut[0];
   RealOut[0] += ImagOut[0];
   ImagOut[0] = h1r - ImagOut[0];

   free(tmpReal);
}

/*
 * PowerSpectrum
 *
 * This function computes the same as RealFFT, above, but
 * adds the squares of the real and imaginary part of each
 * coefficient, extracting the power and throwing away the
 * phase.
 *
 * For speed, it does not call RealFFT, but duplicates some
 * of its code.
 */

void PowerSpectrum(int NumSamples, const float *In, float *Out)
{
  int Half, i, i3;
  float theta, wtemp, wpr, wpi, wr, wi;
  float h1r, h1i, h2r, h2i, rt, it;
  float *tmpReal;
  float *tmpImag;
  float *RealOut;
  float *ImagOut;

  Half = NumSamples / 2;

  theta = M_PI / Half;

  tmpReal = (float*)xcalloc(Half * 4, sizeof(float));
  tmpImag = tmpReal + Half;
  RealOut = tmpImag + Half;
  ImagOut = RealOut + Half;

  for (i = 0; i < Half; i++) {
    tmpReal[i] = In[2 * i];
    tmpImag[i] = In[2 * i + 1];
  }

  FFT(Half, 0, tmpReal, tmpImag, RealOut, ImagOut);
  
  wtemp = (float) sin(0.5 * theta);

  wpr = -2.0 * wtemp * wtemp;
  wpi = (float) sin(theta);
  wr = 1.0 + wpr;
  wi = wpi;
    
  for (i = 1; i < Half / 2; i++) {
      
    i3 = Half - i;
    
    h1r = 0.5 * (RealOut[i] + RealOut[i3]);
    h1i = 0.5 * (ImagOut[i] - ImagOut[i3]);
    h2r = 0.5 * (ImagOut[i] + ImagOut[i3]);
    h2i = -0.5 * (RealOut[i] - RealOut[i3]);
    
    rt = h1r + wr * h2r - wi * h2i;
    it = h1i + wr * h2i + wi * h2r;

    Out[i] = rt * rt + it * it;

    rt = h1r - wr * h2r + wi * h2i;
    it = -h1i + wr * h2i + wi * h2r;
    
    Out[i3] = rt * rt + it * it;
    
    wr = (wtemp = wr) * wpr - wi * wpi + wr;
    wi = wi * wpr + wtemp * wpi + wi;
  }

  rt = (h1r = RealOut[0]) + ImagOut[0];
  it = h1r - ImagOut[0];
  Out[0] = rt * rt + it * it;
  
  rt = RealOut[Half / 2];
  it = ImagOut[Half / 2];
  Out[Half / 2] = rt * rt + it * it;
  
  free(tmpReal);
}

/*
 * Windowing Functions
 */

void WindowFunc(windowfunc_t whichFunction, int NumSamples, float *in)
{
    int i;
    
    switch (whichFunction) {
    case BARTLETT:
        for (i = 0; i < NumSamples / 2; i++) {
            in[i] *= (i / (float) (NumSamples / 2));
            in[i + (NumSamples / 2)] *=
                (1.0 - (i / (float) (NumSamples / 2)));
        }
        break;
        
    case HAMMING:
        for (i = 0; i < NumSamples; i++)
            in[i] *= 0.54 - 0.46 * cos(2 * M_PI * i / (NumSamples - 1));
        break;
        
    case HANNING:
        for (i = 0; i < NumSamples; i++)
            in[i] *= 0.50 - 0.50 * cos(2 * M_PI * i / (NumSamples - 1));
        break;
    case RECTANGULAR:
        break;
    }
}
