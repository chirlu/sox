/*
 *
 * FFT.h
 *
 * Based on FFT.h from Audacity, which contained the following text:
 *
 *     This file contains a few FFT routines, including a real-FFT
 *     routine that is almost twice as fast as a normal complex FFT,
 *     and a power spectrum routine when you know you don't care
 *     about phase information.
 *
 *     Some of this code was based on a free implementation of an FFT
 *     by Don Cross, available on the web at:
 *
 *        http://www.intersrv.com/~dcross/fft.html
 *
 *     The basic algorithm for his code was based on Numerican Recipes
 *     in Fortran.  I optimized his code further by reducing array
 *     accesses, caching the bit reversal table, and eliminating
 *     float-to-double conversions, and I added the routines to
 *     calculate a real FFT and a real power spectrum.
 *
 *     Note: all of these routines use single-precision floats.
 *     I have found that in practice, floats work well until you
 *     get above 8192 samples.  If you need to do a larger FFT,
 *     you need to use doubles.
 *
 * This file is now part of SoX, and is copyright Ian Turner and others.
 * 
 * SoX is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Foobar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SoX; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef M_PI
#define	M_PI		3.14159265358979323846  /* pi */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This is the function you will use the most often.
 * Given an array of floats, this will compute the power
 * spectrum by doing a Real FFT and then computing the
 * sum of the squares of the real and imaginary parts.
 * Note that the output array is half the length of the
 * input array, and that NumSamples must be a power of two.
 */

void PowerSpectrum(int NumSamples, float *In, float *Out);

/*
 * Computes an FFT when the input data is real but you still
 * want complex data as output.  The output arrays are half
 * the length of the input, and NumSamples must be a power of
 * two.
 */

void RealFFT(int NumSamples,
             float *RealIn, float *RealOut, float *ImagOut);

/*
 * Computes a FFT of complex input and returns complex output.
 * Currently this is the only function here that supports the
 * inverse transform as well.
 */

void FFT(int NumSamples,
         int InverseTransform,
         float *RealIn, float *ImagIn, float *RealOut, float *ImagOut);

/*
 * Applies a windowing function to the data in place
 */
typedef enum {RECTANGULAR = 0, /* no window */
              BARTLETT = 1,    /* triangular */
              HAMMING = 2,
              HANNING = 3} windowfunc_t;

void WindowFunc(windowfunc_t whichFunction, int NumSamples, float *data);

#ifdef __cplusplus
}
#endif
