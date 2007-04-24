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
#else
#define NORET
#endif

/* C language enhancements: */

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

/* Array-length operator */
#define array_length(a) (sizeof(a)/sizeof(a[0]))

/* declared in misc.c */
typedef struct {char const *text; int value;} enum_item;
#define ENUM_ITEM(prefix, item) {#item, prefix##item},
enum_item const * find_enum_text(
    char const * text, enum_item const * enum_items);
typedef enum {SOX_SHORT, SOX_INT, SOX_FLOAT, SOX_DOUBLE} sox_data_t;
typedef enum {SOX_WAVE_SINE, SOX_WAVE_TRIANGLE} sox_wave_t;
extern enum_item const sox_wave_enum[];
sox_bool is_uri(char const * text);
FILE * xfopen(char const * identifier, char const * mode);

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

sox_ssample_t sox_gcd(sox_ssample_t a, sox_ssample_t b);
sox_ssample_t sox_lcm(sox_ssample_t a, sox_ssample_t b);

#ifndef HAVE_STRCASECMP
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(char const * s1, char const * s2, size_t n);
#endif

#ifndef HAVE_STRDUP
char *strdup(const char *s);
#endif

/* Read and write basic data types from "ft" stream.  Uses ft->swap for
 * possible byte swapping.
 */
/* declared in misc.c */
size_t sox_readbuf(ft_t ft, void *buf, sox_size_t len);
int sox_skipbytes(ft_t ft, sox_size_t n);
int sox_padbytes(ft_t ft, sox_size_t n);
size_t sox_writebuf(ft_t ft, void const *buf, sox_size_t len);
int sox_reads(ft_t ft, char *c, sox_size_t len);
int sox_writes(ft_t ft, char const * c);

sox_size_t sox_read_b_buf(ft_t ft, uint8_t *buf, sox_size_t len);
sox_size_t sox_read_w_buf(ft_t ft, uint16_t *buf, sox_size_t len);
sox_size_t sox_read_3_buf(ft_t ft, uint24_t *buf, sox_size_t len);
sox_size_t sox_read_dw_buf(ft_t ft, uint32_t *buf, sox_size_t len);
sox_size_t sox_read_f_buf(ft_t ft, float *buf, sox_size_t len);
sox_size_t sox_read_df_buf(ft_t ft, double *buf, sox_size_t len);

sox_size_t sox_write_b_buf(ft_t ft, uint8_t *buf, sox_size_t len);
sox_size_t sox_write_w_buf(ft_t ft, uint16_t *buf, sox_size_t len);
sox_size_t sox_write_3_buf(ft_t ft, uint24_t *buf, sox_size_t len);
sox_size_t sox_write_dw_buf(ft_t ft, uint32_t *buf, sox_size_t len);
sox_size_t sox_write_f_buf(ft_t ft, float *buf, sox_size_t len);
sox_size_t sox_write_df_buf(ft_t ft, double *buf, sox_size_t len);

#define sox_readb(ft, ub) (sox_read_b_buf(ft, ub, 1) == 1 ? SOX_SUCCESS : SOX_EOF)
int sox_writeb(ft_t ft, uint8_t ub);
#define sox_readw(ft, uw) (sox_read_w_buf(ft, uw, 1) == 1 ? SOX_SUCCESS : SOX_EOF)
int sox_writew(ft_t ft, uint16_t uw);
#define sox_read3(ft, u3) (sox_read_3_buf(ft, u3, 1) == 1 ? SOX_SUCCESS : SOX_EOF)
int sox_write3(ft_t ft, uint24_t u3);
#define sox_readdw(ft, udw) (sox_read_dw_buf(ft, udw, 1) == 1 ? SOX_SUCCESS : SOX_EOF)
int sox_writedw(ft_t ft, uint32_t udw);
#define sox_readf(ft, f) (sox_read_f_buf(ft, f, 1) == 1 ? SOX_SUCCESS : SOX_EOF)
int sox_writef(ft_t ft, float f);
#define sox_readdf(ft, d) (sox_read_df_buf(ft, d, 1) == 1 ? SOX_SUCCESS : SOX_EOF)
int sox_writedf(ft_t ft, double d);
int sox_seeki(ft_t ft, sox_ssize_t offset, int whence);
sox_size_t sox_filelength(ft_t ft);
int sox_flush(ft_t ft);
sox_ssize_t sox_tell(ft_t ft);
int sox_eof(ft_t ft);
int sox_error(ft_t ft);
void sox_rewind(ft_t ft);
void sox_clearerr(ft_t ft);

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
float sox_swapf(float f);
uint32_t sox_swap3(uint32_t udw);
double sox_swapdf(double d);

/* util.c */
typedef void (*sox_output_message_handler_t)(unsigned level, const char *filename, const char *fmt, va_list ap);
extern sox_output_message_handler_t sox_output_message_handler;
extern unsigned sox_output_verbosity_level;
void sox_output_message(FILE *file, const char *filename, const char *fmt, va_list ap);

void sox_fail(const char *, ...);
void sox_warn(const char *, ...);
void sox_report(const char *, ...);
void sox_debug(const char *, ...);
void sox_debug_more(char const * fmt, ...);
void sox_debug_most(char const * fmt, ...);

#define sox_fail       sox_message_filename=__FILE__,sox_fail
#define sox_warn       sox_message_filename=__FILE__,sox_warn
#define sox_report     sox_message_filename=__FILE__,sox_report
#define sox_debug      sox_message_filename=__FILE__,sox_debug
#define sox_debug_more sox_message_filename=__FILE__,sox_debug_more
#define sox_debug_most sox_message_filename=__FILE__,sox_debug_most

void sox_fail_errno(ft_t, int, const char *, ...);

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

/* The following is used at times in libsox when alloc()ing buffers
 * to perform file I/O.  It can be useful to pass in similar sized
 * data to get max performance.
 */
extern sox_size_t sox_bufsiz;

extern const char sox_readerr[];
extern const char sox_writerr[];
extern uint8_t const cswap[256];

/*=============================================================================
 * File Handlers
 *=============================================================================
 */

/* Psion record header check, defined in misc.c and used in prc.c and auto.c */
const char prc_header[41];
int prc_checkheader(ft_t ft, char *head);

typedef const sox_format_t *(*sox_format_fn_t)(void);

typedef struct {
  char *name;
  sox_format_fn_t fn;
} sox_format_tab_t;

extern sox_format_tab_t sox_format_fns[];

const sox_format_t *sox_auto_format_fn(void);
const sox_format_t *sox_raw_format_fn(void);
const sox_format_t *sox_al_format_fn(void);
const sox_format_t *sox_la_format_fn(void);
const sox_format_t *sox_lu_format_fn(void);
const sox_format_t *sox_s3_format_fn(void);
const sox_format_t *sox_sb_format_fn(void);
const sox_format_t *sox_sl_format_fn(void);
const sox_format_t *sox_sw_format_fn(void);
const sox_format_t *sox_u3_format_fn(void);
const sox_format_t *sox_ub_format_fn(void);
const sox_format_t *sox_u4_format_fn(void);
const sox_format_t *sox_ul_format_fn(void);
const sox_format_t *sox_uw_format_fn(void);

/* Raw I/O */
int sox_rawstartread(ft_t ft);
sox_size_t sox_rawread(ft_t ft, sox_ssample_t *buf, sox_size_t nsamp);
int sox_rawstopread(ft_t ft);
int sox_rawstartwrite(ft_t ft);
sox_size_t sox_rawwrite(ft_t ft, const sox_ssample_t *buf, sox_size_t nsamp);
int sox_rawseek(ft_t ft, sox_size_t offset);

/* The following functions can be used to simply return success if
 * a file handler or effect doesn't need to do anything special
 */
int sox_format_nothing(ft_t ft);
sox_size_t sox_format_nothing_read(ft_t ft, sox_ssample_t *buf, sox_size_t len);
sox_size_t sox_format_nothing_write(ft_t ft, const sox_ssample_t *buf, sox_size_t len);
int sox_format_nothing_seek(ft_t ft, sox_size_t offset);
int sox_effect_nothing(eff_t effp);
int sox_effect_nothing_flow(eff_t effp, const sox_ssample_t *ibuf, sox_ssample_t *obuf, sox_size_t *isamp, sox_size_t *osamp);
int sox_effect_nothing_drain(eff_t effp, sox_ssample_t *obuf, sox_size_t *osamp);
int sox_effect_nothing_getopts(eff_t effp, int n, char **argv UNUSED);

int sox_rawstart(ft_t ft, sox_bool default_rate, sox_bool default_channels, sox_encoding_t encoding, int size);
#define sox_rawstartread(ft) sox_rawstart(ft, sox_false, sox_false, SOX_ENCODING_UNKNOWN, -1)
#define sox_rawstartwrite sox_rawstartread
#define sox_rawstopread sox_format_nothing
#define sox_rawstopwrite sox_format_nothing

/*=============================================================================
 * Effects
 *=============================================================================
 */

typedef const sox_effect_t *(*sox_effect_fn_t)(void);

extern sox_effect_fn_t sox_effect_fns[];

extern const sox_effect_t *sox_allpass_effect_fn(void);
extern const sox_effect_t *sox_avg_effect_fn(void);
extern const sox_effect_t *sox_band_effect_fn(void);
extern const sox_effect_t *sox_bandpass_effect_fn(void);
extern const sox_effect_t *sox_bandreject_effect_fn(void);
extern const sox_effect_t *sox_bass_effect_fn(void);
extern const sox_effect_t *sox_chorus_effect_fn(void);
extern const sox_effect_t *sox_compand_effect_fn(void);
extern const sox_effect_t *sox_dcshift_effect_fn(void);
extern const sox_effect_t *sox_deemph_effect_fn(void);
extern const sox_effect_t *sox_dither_effect_fn(void);
extern const sox_effect_t *sox_earwax_effect_fn(void);
extern const sox_effect_t *sox_echo_effect_fn(void);
extern const sox_effect_t *sox_echos_effect_fn(void);
extern const sox_effect_t *sox_equalizer_effect_fn(void);
extern const sox_effect_t *sox_fade_effect_fn(void);
extern const sox_effect_t *sox_filter_effect_fn(void);
extern const sox_effect_t *sox_flanger_effect_fn(void);
extern const sox_effect_t *sox_highpass_effect_fn(void);
extern const sox_effect_t *sox_highp_effect_fn(void);
extern const sox_effect_t *sox_lowpass_effect_fn(void);
extern const sox_effect_t *sox_lowp_effect_fn(void);
extern const sox_effect_t *sox_mask_effect_fn(void);
extern const sox_effect_t *sox_mcompand_effect_fn(void);
extern const sox_effect_t *sox_mixer_effect_fn(void);
extern const sox_effect_t *sox_noiseprof_effect_fn(void);
extern const sox_effect_t *sox_noisered_effect_fn(void);
extern const sox_effect_t *sox_pad_effect_fn(void);
extern const sox_effect_t *sox_pan_effect_fn(void);
extern const sox_effect_t *sox_phaser_effect_fn(void);
extern const sox_effect_t *sox_pick_effect_fn(void);
extern const sox_effect_t *sox_pitch_effect_fn(void);
extern const sox_effect_t *sox_polyphase_effect_fn(void);
#ifdef HAVE_SAMPLERATE_H
extern const sox_effect_t *sox_rabbit_effect_fn(void);
#endif
extern const sox_effect_t *sox_rate_effect_fn(void);
extern const sox_effect_t *sox_repeat_effect_fn(void);
extern const sox_effect_t *sox_resample_effect_fn(void);
extern const sox_effect_t *sox_reverb_effect_fn(void);
extern const sox_effect_t *sox_reverse_effect_fn(void);
extern const sox_effect_t *sox_silence_effect_fn(void);
extern const sox_effect_t *sox_speed_effect_fn(void);
extern const sox_effect_t *sox_stat_effect_fn(void);
extern const sox_effect_t *sox_stretch_effect_fn(void);
extern const sox_effect_t *sox_swap_effect_fn(void);
extern const sox_effect_t *sox_synth_effect_fn(void);
extern const sox_effect_t *sox_treble_effect_fn(void);
extern const sox_effect_t *sox_tremolo_effect_fn(void);
extern const sox_effect_t *sox_trim_effect_fn(void);
extern const sox_effect_t *sox_vibro_effect_fn(void);
extern const sox_effect_t *sox_vol_effect_fn(void);

/* Needed in rate.c */
int sox_resample_start(eff_t effp);
int sox_resample_getopts(eff_t effp, int n, char **argv);
int sox_resample_flow(eff_t effp, const sox_ssample_t *ibuf, sox_ssample_t *obuf, sox_size_t *isamp, sox_size_t *osamp);
int sox_resample_drain(eff_t effp, sox_ssample_t *obuf, sox_size_t *osamp);
int sox_resample_stop(eff_t effp);

#endif
