/* General purpose, i.e. non SoX specific, utility functions and macros.
 *
 * (c) 2006-8 Chris Bagwell and SoX contributors
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

#include "xmalloc.h"

/*---------------------------- Portability stuff -----------------------------*/

#ifdef __GNUC__
#define NORET __attribute__((noreturn))
#define PRINTF __attribute__ ((format (printf, 1, 2)))
#define UNUSED __attribute__ ((unused))
#else
#define NORET
#define PRINTF
#define UNUSED
#endif

#ifdef _MSC_VER
#define __STDC__ 1
#define O_BINARY _O_BINARY
#define O_CREAT _O_CREAT
#define O_RDWR _O_RDWR
#define O_TRUNC _O_TRUNC
#define S_IFMT _S_IFMT
#define S_IFREG _S_IFREG
#define S_IREAD _S_IREAD
#define S_IWRITE _S_IWRITE
#define fdopen _fdopen
#define fstat _fstat
#define ftime _ftime
#define inline __inline
#define isatty _isatty
#define mktemp _mktemp
#define off_t _off_t
#define open _open
#define popen _popen
#define stat _stat
#define strdup _strdup
#define timeb _timeb
#define unlink _unlink
#endif

#if defined(DOS) || defined(WIN32) || defined(__NT__) || defined(__DJGPP__) || defined(__OS2__)
  #define LAST_SLASH(path) max(strrchr(path, '/'), strrchr(path, '\\'))
  #define IS_ABSOLUTE(path) ((path)[0] == '/' || (path)[0] == '\\' || (path)[1] == ':')
  #define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
  #define LAST_SLASH(path) strrchr(path, '/')
  #define IS_ABSOLUTE(path) ((path)[0] == '/')
  #define SET_BINARY_MODE(file)
#endif

#ifdef WORDS_BIGENDIAN
  #define MACHINE_IS_BIGENDIAN 1
  #define MACHINE_IS_LITTLEENDIAN 0
#else
  #define MACHINE_IS_BIGENDIAN 0
  #define MACHINE_IS_LITTLEENDIAN 1
#endif

/*--------------------------- Language extensions ----------------------------*/

/* Compile-time ("static") assertion */
/*   e.g. assert_static(sizeof(int) >= 4, int_type_too_small)    */
#define assert_static(e,f) enum {assert_static__##f = 1/(e)}
#define array_length(a) (sizeof(a)/sizeof(a[0]))
#define field_offset(type, field) ((size_t)&(((type *)0)->field))
#define unless(x) if (!(x))

/*------------------------------- Maths stuff --------------------------------*/

#include <math.h>

#ifdef min
#undef min
#endif
#define min(a, b) ((a) <= (b) ? (a) : (b))

#ifdef max
#undef max
#endif
#define max(a, b) ((a) >= (b) ? (a) : (b))

#define range_limit(x, lower, upper) (min(max(x, lower), upper))

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2  1.57079632679489661923  /* pi/2 */
#endif
#ifndef M_LN10
#define M_LN10  2.30258509299404568402  /* natural log of 10 */
#endif
#ifndef M_SQRT2
#define M_SQRT2  sqrt(2.)
#endif

#define sqr(a) ((a) * (a))
#define sign(x) ((x) < 0? -1 : 1)

/* Numerical Recipes in C, p. 284 */
#define ranqd1(x) ((x) = 1664525L * (x) + 1013904223L) /* int32_t x */
#define dranqd1(x) (ranqd1(x) * (1. / (65536. * 32768.))) /* [-1,1) */

#define dB_to_linear(x) exp((x) * M_LN10 * 0.05)
#define linear_to_dB(x) (log10(x) * 20)
