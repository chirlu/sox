/* aliases */
#define gFFTBitTable lsx_gFFTBitTable
#define MaxFastBits lsx_MaxFastBits
#define FFT lsx_FFT
#define RealFFT lsx_RealFFT
#define PowerSpectrum lsx_PowerSpectrum
#define WindowFunc lsx_WindowFunc

/*
 * FFT.h
 *
 * Based on FFT.h from Audacity, with the following permission from
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
 */

/*
 * This is the function you will use the most often.
 * Given an array of floats, this will compute the power
 * spectrum by doing a Real FFT and then computing the
 * sum of the squares of the real and imaginary parts.
 * Note that the output array is half the length of the
 * input array, and that NumSamples must be a power of two.
 */

void PowerSpectrum(unsigned NumSamples, const float *In, float *Out);

/*
 * Computes an FFT when the input data is real but you still
 * want complex data as output.  The output arrays are half
 * the length of the input, and NumSamples must be a power of
 * two.
 */

void RealFFT(unsigned NumSamples,
             const float *RealIn, float *RealOut, float *ImagOut);

/*
 * Computes a FFT of complex input and returns complex output.
 * Currently this is the only function here that supports the
 * inverse transform as well.
 */

void FFT(unsigned NumSamples,
         int InverseTransform,
         const float *RealIn, float *ImagIn, float *RealOut, float *ImagOut);

/*
 * Applies a windowing function to the data in place
 */
typedef enum {RECTANGULAR, /* no window */
              BARTLETT,    /* triangular */
              HAMMING,
              HANNING} windowfunc_t;

void WindowFunc(windowfunc_t whichFunction, int NumSamples, float *data);
