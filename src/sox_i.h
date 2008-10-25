/* libSoX Internal header
 *
 *   This file is meant for libSoX internal use only
 *
 * Copyright 2001-2008 Chris Bagwell and SoX Contributors
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Chris Bagwell And SoX Contributors are not responsible for
 * the consequences of using this software.
 */

#ifndef SOX_I_H
#define SOX_I_H

#include "soxconfig.h"
#include "sox.h"
#include "util.h"

#include <errno.h>
typedef enum {SOX_SHORT, SOX_INT, SOX_FLOAT, SOX_DOUBLE} sox_data_t;
typedef enum {SOX_WAVE_SINE, SOX_WAVE_TRIANGLE} lsx_wave_t;
extern lsx_enum_item const lsx_wave_enum[];

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h> /* For off_t not found in stdio.h */
#endif

/* Define fseeko and ftello for platforms lacking them */
#ifndef HAVE_FSEEKO
#define fseeko fseek
#define ftello ftell
#ifndef _MSC_VER
#define off_t long
#endif
#endif

#ifdef _FILE_OFFSET_BITS
assert_static(sizeof(off_t) == _FILE_OFFSET_BITS >> 3, OFF_T_BUILD_PROBLEM);
#endif

#if defined __GNUC__
#define FMT_size_t "zu"
#elif defined _MSC_VER
#define FMT_size_t "Iu"
#else
#define FMT_size_t "lu"
#endif

void lsx_debug_more(char const * fmt, ...) PRINTF;
void lsx_debug_most(char const * fmt, ...) PRINTF;

#define lsx_debug_more sox_globals.subsystem=__FILE__,lsx_debug_more
#define lsx_debug_most sox_globals.subsystem=__FILE__,lsx_debug_most

/* Digitise one cycle of a wave and store it as
 * a table of samples of a specified data-type.
 */
void lsx_generate_wave_table(
    lsx_wave_t wave_type,
    sox_data_t data_type,
    void * table,       /* Really of type indicated by data_type. */
    size_t table_size,  /* Number of points on the x-axis. */
    double min,         /* Minimum value on the y-axis. (e.g. -1) */
    double max,         /* Maximum value on the y-axis. (e.g. +1) */
    double phase);      /* Phase at 1st point; 0..2pi. (e.g. pi/2 for cosine) */
char const * lsx_parsesamples(sox_rate_t rate, const char *str, size_t *samples, int def);
double lsx_parse_frequency(char const * text, char * * end_ptr);

unsigned lsx_gcd(unsigned a, unsigned b);
unsigned lsx_lcm(unsigned a, unsigned b);

void lsx_prepare_spline3(double const * x, double const * y, int n,
    double start_1d, double end_1d, double * y_2d);
double lsx_spline3(double const * x, double const * y, double const * y_2d,
    int n, double x1);

double lsx_bessel_I_0(double x);
int lsx_set_dft_length(int num_taps);
extern int * lsx_fft_br;
extern double * lsx_fft_sc;
void lsx_safe_rdft(int len, int type, double * d);
void lsx_safe_cdft(int len, int type, double * d);
void lsx_power_spectrum(int n, double const * in, double * out);
void lsx_power_spectrum_f(int n, float const * in, float * out);
void lsx_apply_hann_f(float h[], const int num_points);
void lsx_apply_hann(double h[], const int num_points);
void lsx_apply_hamming(double h[], const int num_points);
void lsx_apply_bartlett(double h[], const int num_points);
void lsx_apply_blackman(double h[], const int num_points, double alpha);
void lsx_apply_blackman_nutall(double h[], const int num_points);
double lsx_kaiser_beta(double att);
void lsx_apply_kaiser(double h[], const int num_points, double beta);

#ifndef HAVE_STRCASECMP
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(char const * s1, char const * s2, size_t n);
#endif

#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#define lsx_swapw(x) bswap_16(x)
#define lsx_swapdw(x) bswap_32(x)
#else
#define lsx_swapw(uw) (((uw >> 8) | (uw << 8)) & 0xffff)
#define lsx_swapdw(udw) ((udw >> 24) | ((udw >> 8) & 0xff00) | ((udw << 8) & 0xff0000) | (udw << 24))
#endif



/*------------------------ Implemented in libsoxio.c -------------------------*/

/* Read and write basic data types from "ft" stream. */
size_t lsx_readbuf(sox_format_t * ft, void *buf, size_t len);
int lsx_skipbytes(sox_format_t * ft, size_t n);
int lsx_padbytes(sox_format_t * ft, size_t n);
size_t lsx_writebuf(sox_format_t * ft, void const *buf, size_t len);
int lsx_reads(sox_format_t * ft, char *c, size_t len);
int lsx_writes(sox_format_t * ft, char const * c);
void lsx_set_signal_defaults(sox_signalinfo_t * signal);
#define lsx_writechars(ft, chars, len) (lsx_writebuf(ft, chars, len) == len? SOX_SUCCESS : SOX_EOF)

size_t lsx_read_3_buf(sox_format_t * ft, uint24_t *buf, size_t len);
size_t lsx_read_b_buf(sox_format_t * ft, uint8_t *buf, size_t len);
size_t lsx_read_df_buf(sox_format_t * ft, double *buf, size_t len);
size_t lsx_read_dw_buf(sox_format_t * ft, uint32_t *buf, size_t len);
size_t lsx_read_qw_buf(sox_format_t * ft, uint64_t *buf, size_t len);
size_t lsx_read_f_buf(sox_format_t * ft, float *buf, size_t len);
size_t lsx_read_w_buf(sox_format_t * ft, uint16_t *buf, size_t len);

size_t lsx_write_3_buf(sox_format_t * ft, uint24_t *buf, size_t len);
size_t lsx_write_b_buf(sox_format_t * ft, uint8_t *buf, size_t len);
size_t lsx_write_df_buf(sox_format_t * ft, double *buf, size_t len);
size_t lsx_write_dw_buf(sox_format_t * ft, uint32_t *buf, size_t len);
size_t lsx_write_qw_buf(sox_format_t * ft, uint64_t *buf, size_t len);
size_t lsx_write_f_buf(sox_format_t * ft, float *buf, size_t len);
size_t lsx_write_w_buf(sox_format_t * ft, uint16_t *buf, size_t len);

int lsx_read3(sox_format_t * ft, uint24_t * u3);
int lsx_readb(sox_format_t * ft, uint8_t * ub);
int lsx_readchars(sox_format_t * ft, char * chars, size_t len);
int lsx_readdf(sox_format_t * ft, double * d);
int lsx_readdw(sox_format_t * ft, uint32_t * udw);
int lsx_readqw(sox_format_t * ft, uint64_t * udw);
int lsx_readf(sox_format_t * ft, float * f);
int lsx_readw(sox_format_t * ft, uint16_t * uw);

UNUSED static int lsx_readsb(sox_format_t * ft, int8_t * sb)
  {return lsx_readb(ft, (uint8_t *)sb);}
UNUSED static int lsx_readsw(sox_format_t * ft, int16_t * sw)
  {return lsx_readw(ft, (uint16_t *)sw);}

int lsx_write3(sox_format_t * ft, unsigned u3);
int lsx_writeb(sox_format_t * ft, unsigned ub);
int lsx_writedf(sox_format_t * ft, double d);
int lsx_writedw(sox_format_t * ft, unsigned udw);
int lsx_writeqw(sox_format_t * ft, uint64_t uqw);
int lsx_writef(sox_format_t * ft, double f);
int lsx_writew(sox_format_t * ft, unsigned uw);

int lsx_writesb(sox_format_t * ft, signed);
int lsx_writesw(sox_format_t * ft, signed);

int lsx_eof(sox_format_t * ft);
int lsx_error(sox_format_t * ft);
int lsx_flush(sox_format_t * ft);
int lsx_seeki(sox_format_t * ft, off_t offset, int whence);
int lsx_unreadb(sox_format_t * ft, unsigned ub);
size_t lsx_filelength(sox_format_t * ft);
off_t lsx_tell(sox_format_t * ft);
void lsx_clearerr(sox_format_t * ft);
void lsx_rewind(sox_format_t * ft);

int lsx_offset_seek(sox_format_t * ft, off_t byte_offset, off_t to_sample);

void lsx_fail_errno(sox_format_t *, int, const char *, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 3, 4)));
#else
;
#endif

typedef struct sox_formats_globals { /* Global parameters (for formats) */
  sox_globals_t * global_info;
} sox_formats_globals;



/*------------------------------ File Handlers -------------------------------*/

int lsx_check_read_params(sox_format_t * ft, unsigned channels,
    sox_rate_t rate, sox_encoding_t encoding, unsigned bits_per_sample,
    off_t num_samples);
sox_sample_t lsx_sample_max(sox_encodinginfo_t const * encoding);
#define SOX_FORMAT_HANDLER(name) \
sox_format_handler_t const * sox_##name##_format_fn(void); \
sox_format_handler_t const * sox_##name##_format_fn(void)
#define div_bits(size, bits) (off_t)((double)(size) * 8 / bits)

/* Raw I/O */
int lsx_rawstartread(sox_format_t * ft);
size_t lsx_rawread(sox_format_t * ft, sox_sample_t *buf, size_t nsamp);
int lsx_rawstopread(sox_format_t * ft);
int lsx_rawstartwrite(sox_format_t * ft);
size_t lsx_rawwrite(sox_format_t * ft, const sox_sample_t *buf, size_t nsamp);
int lsx_rawseek(sox_format_t * ft, uint64_t offset);
int lsx_rawstart(sox_format_t * ft, sox_bool default_rate, sox_bool default_channels, sox_bool default_length, sox_encoding_t encoding, unsigned size);
#define lsx_rawstartread(ft) lsx_rawstart(ft, sox_false, sox_false, sox_false, SOX_ENCODING_UNKNOWN, 0)
#define lsx_rawstartwrite lsx_rawstartread
#define lsx_rawstopread NULL
#define lsx_rawstopwrite NULL

extern sox_format_handler_t const * sox_sndfile_format_fn(void);

char * lsx_cat_comments(sox_comments_t comments);

/*--------------------------------- Effects ----------------------------------*/

int lsx_usage(sox_effect_t * effp);
char * lsx_usage_lines(char * * usage, char const * const * lines, size_t n);
#define EFFECT(f) extern sox_effect_handler_t const * sox_##f##_effect_fn(void);
#include "effects.h"
#undef EFFECT

#define NUMERIC_PARAMETER(name, min, max) { \
  char * end_ptr; \
  double d; \
  if (argc == 0) break; \
  d = strtod(*argv, &end_ptr); \
  if (end_ptr != *argv) { \
    if (d < min || d > max || *end_ptr != '\0') {\
      lsx_fail("parameter `%s' must be between %g and %g", #name, (double)min, (double)max); \
      return lsx_usage(effp); \
    } \
    p->name = d; \
    --argc, ++argv; \
  } \
}

#define TEXTUAL_PARAMETER(name, enum_table) { \
  lsx_enum_item const * e; \
  if (argc == 0) break; \
  e = lsx_find_enum_text(*argv, enum_table); \
  if (e != NULL) { \
    p->name = e->value; \
    --argc, ++argv; \
  } \
}

#define GETOPT_NUMERIC(ch, name, min, max) case ch:{ \
  char * end_ptr; \
  double d = strtod(optarg, &end_ptr); \
  if (end_ptr == optarg || d < min || d > max || *end_ptr != '\0') {\
    lsx_fail("parameter `%s' must be between %g and %g", #name, (double)min, (double)max); \
    return lsx_usage(effp); \
  } \
  p->name = d; \
  break; \
}

int lsx_effect_set_imin(sox_effect_t * effp, size_t imin);

#endif
