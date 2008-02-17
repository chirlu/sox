/*
 * libSoX Internal header
 *
 *   This file is meant for libsox internal use only
 *
 * Copyright 2001-2007 Chris Bagwell and SoX Contributors
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
#include "xmalloc.h"

#include <stdarg.h>
#include <errno.h>

#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif

/* various gcc optimizations and portablity defines */
#ifdef __GNUC__
#define NORET __attribute__((noreturn))
#define PRINTF __attribute__ ((format (printf, 1, 2)))
#else
#define NORET
#define PRINTF
#endif

#ifdef _MSC_VER
#define __STDC__ 1
#define S_IFMT   _S_IFMT
#define S_IFREG  _S_IFREG
#define O_BINARY _O_BINARY
#define fstat _fstat
#define ftime _ftime
#define inline __inline
#define isatty _isatty
#define popen _popen
#define stat _stat
#define strdup _strdup
#define timeb _timeb
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

/* Compile-time ("static") assertion */
/*   e.g. assert_static(sizeof(int) >= 4, int_type_too_small)    */
#define assert_static(e,f) enum {assert_static__##f = 1/(e)}

#ifdef min
#undef min
#endif
#define min(a, b) ((a) <= (b) ? (a) : (b))

#ifdef max
#undef max
#endif
#define max(a, b) ((a) >= (b) ? (a) : (b))

#define range_limit(x, lower, upper) (min(max(x, lower), upper))

#define sqr(a) ((a) * (a))

/* Array-length operator */
#define array_length(a) (sizeof(a)/sizeof(a[0]))

/* declared in misc.c */
char const * find_file_extension(char const * pathname);
typedef struct {char const *text; unsigned value;} enum_item;
#define ENUM_ITEM(prefix, item) {#item, prefix##item},
enum_item const * find_enum_text(
    char const * text, enum_item const * enum_items);
enum_item const * find_enum_value(unsigned value, enum_item const * enum_items);
typedef enum {SOX_SHORT, SOX_INT, SOX_FLOAT, SOX_DOUBLE} sox_data_t;
typedef enum {SOX_WAVE_SINE, SOX_WAVE_TRIANGLE} sox_wave_t;
extern enum_item const sox_wave_enum[];
sox_bool is_uri(char const * text);
FILE * xfopen(char const * identifier, char const * mode);

/* Function we supply if it's missing from system libc */
#ifndef HAVE_STRRSTR
char *strrstr(const char *s, const char *t);
#endif

/* Define fseeko and ftello for platforms lacking them */
#ifndef HAVE_FSEEKO
#define fseeko fseek
#define ftello ftell
#define off_t long
#endif

/* Digitise one cycle of a wave and store it as
 * a table of samples of a specified data-type.
 */
void sox_generate_wave_table(
    sox_wave_t wave_type,
    sox_data_t data_type,
    void * table,       /* Really of type indicated by data_type. */
    uint32_t table_size,/* Number of points on the x-axis. */
    double min,         /* Minimum value on the y-axis. (e.g. -1) */
    double max,         /* Maximum value on the y-axis. (e.g. +1) */
    double phase);      /* Phase at 1st point; 0..2pi. (e.g. pi/2 for cosine) */

sox_sample_t sox_gcd(sox_sample_t a, sox_sample_t b);
sox_sample_t sox_lcm(sox_sample_t a, sox_sample_t b);

#ifndef HAVE_STRCASECMP
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(char const * s1, char const * s2, size_t n);
#endif

sox_bool strcaseends(char const * str, char const * end);
sox_bool strends(char const * str, char const * end);

#ifndef HAVE_STRDUP
char *strdup(const char *s);
#endif



/*---------------------------- Declared in misc.c ----------------------------*/

/* Read and write basic data types from "ft" stream.  Uses ft->swap for
 * possible byte swapping.
 */
size_t sox_readbuf(sox_format_t * ft, void *buf, sox_size_t len);
int sox_skipbytes(sox_format_t * ft, sox_size_t n);
int sox_padbytes(sox_format_t * ft, sox_size_t n);
size_t sox_writebuf(sox_format_t * ft, void const *buf, sox_size_t len);
int sox_reads(sox_format_t * ft, char *c, sox_size_t len);
int sox_writes(sox_format_t * ft, char const * c);
void set_signal_defaults(sox_signalinfo_t * signal);
void set_endianness_if_not_already_set(sox_format_t * ft);

sox_size_t sox_read_b_buf(sox_format_t * ft, uint8_t *buf, sox_size_t len);
sox_size_t sox_read_w_buf(sox_format_t * ft, uint16_t *buf, sox_size_t len);
sox_size_t sox_read_3_buf(sox_format_t * ft, uint24_t *buf, sox_size_t len);
sox_size_t sox_read_dw_buf(sox_format_t * ft, uint32_t *buf, sox_size_t len);
sox_size_t sox_read_f_buf(sox_format_t * ft, float *buf, sox_size_t len);
sox_size_t sox_read_df_buf(sox_format_t * ft, double *buf, sox_size_t len);

sox_size_t sox_write_b_buf(sox_format_t * ft, uint8_t *buf, sox_size_t len);
sox_size_t sox_write_w_buf(sox_format_t * ft, uint16_t *buf, sox_size_t len);
sox_size_t sox_write_3_buf(sox_format_t * ft, uint24_t *buf, sox_size_t len);
sox_size_t sox_write_dw_buf(sox_format_t * ft, uint32_t *buf, sox_size_t len);
sox_size_t sox_write_f_buf(sox_format_t * ft, float *buf, sox_size_t len);
sox_size_t sox_write_df_buf(sox_format_t * ft, double *buf, sox_size_t len);

#define sox_readb(ft, ub) (sox_read_b_buf(ft, ub, 1) == 1 ? SOX_SUCCESS : SOX_EOF)
int sox_writeb(sox_format_t * ft, unsigned ub);
#define sox_readw(ft, uw) (sox_read_w_buf(ft, uw, 1) == 1 ? SOX_SUCCESS : SOX_EOF)
int sox_writew(sox_format_t * ft, unsigned uw);
int sox_writesb(sox_format_t * ft, signed);
int sox_writesw(sox_format_t * ft, signed);
#define sox_read3(ft, u3) (sox_read_3_buf(ft, u3, 1) == 1 ? SOX_SUCCESS : SOX_EOF)
int sox_write3(sox_format_t * ft, unsigned u3);
#define sox_readdw(ft, udw) (sox_read_dw_buf(ft, udw, 1) == 1 ? SOX_SUCCESS : SOX_EOF)
int sox_writedw(sox_format_t * ft, unsigned udw);
#define sox_readf(ft, f) (sox_read_f_buf(ft, f, 1) == 1 ? SOX_SUCCESS : SOX_EOF)
int sox_writef(sox_format_t * ft, float f);
#define sox_readdf(ft, d) (sox_read_df_buf(ft, d, 1) == 1 ? SOX_SUCCESS : SOX_EOF)
int sox_writedf(sox_format_t * ft, double d);
int sox_seeki(sox_format_t * ft, sox_ssize_t offset, int whence);
sox_size_t sox_filelength(sox_format_t * ft);
int sox_flush(sox_format_t * ft);
sox_ssize_t sox_tell(sox_format_t * ft);
int sox_eof(sox_format_t * ft);
int sox_error(sox_format_t * ft);
void sox_rewind(sox_format_t * ft);
void sox_clearerr(sox_format_t * ft);

/* Utilities to read/write values endianness-independently */
uint32_t get32_le(unsigned char **p);
uint16_t get16_le(unsigned char **p);
void put32_le(unsigned char **p, uint32_t val);
void put16_le(unsigned char **p, unsigned val);
void put32_be(unsigned char **p, int32_t val);
void put16_be(unsigned char **p, int val);

/* Utilities to byte-swap values, use libc optimized macros if possible  */
#ifdef HAVE_BYTESWAP_H
#define sox_swapw(x) bswap_16(x)
#define sox_swapdw(x) bswap_32(x)
#else
#define sox_swapw(uw) (((uw >> 8) | (uw << 8)) & 0xffff)
#define sox_swapdw(udw) ((udw >> 24) | ((udw >> 8) & 0xff00) | ((udw << 8) & 0xff0000) | (udw << 24))
#endif
void sox_swapf(float * f);
uint32_t sox_swap3(uint32_t udw);
double sox_swapdf(double d);



/*---------------------------- Declared in util.c ----------------------------*/

typedef void (*sox_output_message_handler_t)(unsigned level, const char *filename, const char *fmt, va_list ap);
void sox_output_message(FILE *file, const char *filename, const char *fmt, va_list ap);

void sox_fail(const char *, ...) PRINTF;
void sox_warn(const char *, ...) PRINTF;
void sox_report(const char *, ...) PRINTF;
void sox_debug(const char *, ...) PRINTF;
void sox_debug_more(char const * fmt, ...) PRINTF;
void sox_debug_most(char const * fmt, ...) PRINTF;

#define sox_fail       sox_globals.subsystem=__FILE__,sox_fail
#define sox_warn       sox_globals.subsystem=__FILE__,sox_warn
#define sox_report     sox_globals.subsystem=__FILE__,sox_report
#define sox_debug      sox_globals.subsystem=__FILE__,sox_debug
#define sox_debug_more sox_globals.subsystem=__FILE__,sox_debug_more
#define sox_debug_most sox_globals.subsystem=__FILE__,sox_debug_most

void sox_fail_errno(sox_format_t *, int, const char *, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 3, 4)));
#else
;
#endif

size_t num_comments(comments_t comments);
void append_comment(comments_t * comments, char const * comment);
void append_comments(comments_t * comments, char const * comment);
comments_t copy_comments(comments_t comments);
void delete_comments(comments_t * comments);
char * cat_comments(comments_t comments);
char const * find_comment(comments_t comments, char const * id);



#ifdef WORDS_BIGENDIAN
#define SOX_IS_BIGENDIAN 1
#define SOX_IS_LITTLEENDIAN 0
#else
#define SOX_IS_BIGENDIAN 0
#define SOX_IS_LITTLEENDIAN 1
#endif

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2  1.57079632679489661923  /* pi/2 */
#endif
#ifndef M_LN10
#define M_LN10  2.30258509299404568402  /* natural log of 10 */
#endif
#define dB_to_linear(x) exp((x) * M_LN10 * 0.05)
#define linear_to_dB(x) (log10(x) * 20)

typedef struct sox_globals /* Global parameters (for effects & formats) */
{
  unsigned     verbosity;
/* The following is used at times in libsox when alloc()ing buffers
 * to perform file I/O.  It can be useful to pass in similar sized
 * data to get max performance.
 */
  sox_size_t   bufsiz;
  char const * stdin_in_use_by;
  char const * stdout_in_use_by;
  sox_output_message_handler_t output_message_handler;
  char const * subsystem;

} sox_globals_t;

struct sox_effects_globals /* Global parameters (for effects) */
{
  sox_plot_t plot;         /* To help the user choose effect & options */
  double speed;            /* Gather up all speed changes here, then resample */
  sox_globals_t * global_info;
};

typedef struct sox_formats_globals /* Global parameters (for formats) */
{
  sox_globals_t * global_info;
} sox_formats_globals;


extern sox_globals_t sox_globals;
extern sox_effects_globals_t sox_effects_globals;

extern const char sox_readerr[];
extern const char sox_writerr[];
extern uint8_t const cswap[256];



/*------------------------------ File Handlers -------------------------------*/

/* Psion record header check, defined in misc.c and used in prc.c and auto.c */
extern const char prc_header[41];
int prc_checkheader(sox_format_t * ft, char *head);

typedef const sox_format_handler_t *(*sox_format_fn_t)(void);

typedef struct {
  char *name;
  sox_format_fn_t fn;
} sox_format_tab_t;

extern unsigned sox_formats;
extern sox_format_tab_t sox_format_fns[];

/* Raw I/O */
int sox_rawstartread(sox_format_t * ft);
sox_size_t sox_rawread(sox_format_t * ft, sox_sample_t *buf, sox_size_t nsamp);
int sox_rawstopread(sox_format_t * ft);
int sox_rawstartwrite(sox_format_t * ft);
sox_size_t sox_rawwrite(sox_format_t * ft, const sox_sample_t *buf, sox_size_t nsamp);
int sox_rawseek(sox_format_t * ft, sox_size_t offset);
int sox_rawstart(sox_format_t * ft, sox_bool default_rate, sox_bool default_channels, sox_encoding_t encoding, unsigned size);
#define sox_rawstartread(ft) sox_rawstart(ft, sox_false, sox_false, SOX_ENCODING_UNKNOWN, 0)
#define sox_rawstartwrite sox_rawstartread
#define sox_rawstopread NULL
#define sox_rawstopwrite NULL



/*--------------------------------- Effects ----------------------------------*/

int sox_usage(sox_effect_t * effp);
typedef const sox_effect_handler_t *(*sox_effect_fn_t)(void);
extern sox_effect_fn_t sox_effect_fns[];
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
      sox_fail("parameter `%s' must be between %g and %g", #name, (double)min, (double)max); \
      return sox_usage(effp); \
    } \
    p->name = d; \
    --argc, ++argv; \
  } \
}

#define TEXTUAL_PARAMETER(name, enum_table) { \
  enum_item const * e; \
  if (argc == 0) break; \
  e = find_enum_text(*argv, enum_table); \
  if (e != NULL) { \
    p->name = e->value; \
    --argc, ++argv; \
  } \
}

struct sox_effects_chain {
#define SOX_MAX_EFFECTS 20
  sox_effect_t * effects[SOX_MAX_EFFECTS];
  unsigned length;
  sox_sample_t **ibufc, **obufc; /* Channel interleave buffers */
  sox_effects_globals_t global_info;
};

#endif
