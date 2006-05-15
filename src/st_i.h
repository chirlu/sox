#ifndef ST_I_H
#define ST_I_H
/*
 * Sound Tools Internal - October 11, 2001
 *
 *   This file is meant for libst internal use only
 *
 * Copyright 2001 Chris Bagwell
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Chris Bagwell And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "stconfig.h"
#include "st.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif

/* various gcc optimizations and portablity defines */
#ifdef __GNUC__
#define NORET __attribute__((noreturn))
#else
#define NORET
#endif

#ifdef USE_REGPARM
#define REGPARM(n) __attribute__((regparm(n)))
#else
#define REGPARM(n)
#endif

/* declared in misc.c */
st_sample_t st_clip24(st_sample_t) REGPARM(1);
void st_sine(int *buf, st_ssize_t len, int max, int depth);
void st_triangle(int *buf, st_ssize_t len, int max, int depth);

st_sample_t st_gcd(st_sample_t a, st_sample_t b) REGPARM(2);
st_sample_t st_lcm(st_sample_t a, st_sample_t b) REGPARM(2);

#ifndef HAVE_STRCASECMP
int strcasecmp(const char *s1, const char *s2);
#endif

#ifndef HAVE_STRDUP
char *strdup(const char *s);
#endif

#ifndef HAVE_RAND
int rand(void);
void srand(unsigned int seed);
#endif
void st_initrand(void);

#ifndef HAVE_STRERROR
char *strerror(int errorcode);
#endif

/* Read and write basic data types from "ft" stream.  Uses ft->swap for
 * possible byte swapping.
 */
/* declared in misc.c */
st_ssize_t st_readbuf(ft_t ft, void *buf, size_t size, st_ssize_t len);
st_ssize_t st_writebuf(ft_t ft, void *buf, size_t size, st_ssize_t len);
int st_reads(ft_t ft, char *c, st_ssize_t len);
int st_writes(ft_t ft, char *c);
int st_readb(ft_t ft, uint8_t *ub);
int st_writeb(ft_t ft, uint8_t ub);
int st_readw(ft_t ft, uint16_t *uw);
int st_writew(ft_t ft, uint16_t uw);
int st_readdw(ft_t ft, uint32_t *udw);
int st_writedw(ft_t ft, uint32_t udw);
int st_readf(ft_t ft, float *f);
int st_writef(ft_t ft, float f);
int st_readdf(ft_t ft, double *d);
int st_writedf(ft_t ft, double d);
int st_seeki(ft_t ft, st_size_t offset, int whence);
st_size_t st_filelength(ft_t ft);
int st_flush(ft_t ft);
st_size_t st_tell(ft_t ft);
int st_eof(ft_t ft);
int st_error(ft_t ft);
void st_rewind(ft_t ft);
void st_clearerr(ft_t ft);

/* Utilities to byte-swap values, use libc optimized macro's if possible  */
#ifdef HAVE_BYTESWAP_H
#define st_swapw(x) bswap_16(x)
#define st_swapdw(x) bswap_32(x)
#else
uint16_t st_swapw(uint16_t uw);
uint32_t st_swapdw(uint32_t udw);
#endif
float st_swapf(float f);
uint32_t st_swap24(uint32_t udw);
double st_swapd(double d);

/* util.c */
void st_report(const char *, ...);
void st_warn(const char *, ...);
void st_fail(const char *, ...);
void st_fail_errno(ft_t, int, const char *, ...);

int st_is_bigendian(void);
int st_is_littleendian(void);

#ifdef WORDS_BIGENDIAN
#define ST_IS_BIGENDIAN 1
#define ST_IS_LITTLEENDIAN 0
#else
#define ST_IS_BIGENDIAN st_is_bigendian()
#define ST_IS_LITTLEENDIAN st_is_littleendian()
#endif

/* Warning, this is a MAX value used in the library.  Each format and
 * effect may have its own limitations of rate.
 */
#define ST_MAXRATE      50L * 1024 /* maximum sample rate in library */

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2  1.57079632679489661923  /* pi/2 */
#endif

#define ST_INT8_MAX (127)
#define ST_INT16_MAX (32676)
#define ST_INT32_MAX (2147483647)
#define ST_INT64_MAX (9223372036854775807)

/* The following is used at times in libst when alloc()ing buffers
 * to perform file I/O.  It can be useful to pass in similar sized
 * data to get max performance.
 */
#define ST_BUFSIZ 8192

/*=============================================================================
 * File Handlers
 *=============================================================================
 */

st_format_t st_svx_format;
st_format_t st_aiff_format;
#ifdef HAVE_ALSA
st_format_t st_alsa_format;
#endif
st_format_t st_au_format;
st_format_t st_auto_format;
st_format_t st_avr_format;
st_format_t st_cdr_format;
st_format_t st_cvsd_format;
st_format_t st_dvms_format;
st_format_t st_dat_format;
#ifdef ENABLE_GSM
st_format_t st_gsm_format;
#endif
st_format_t st_hcom_format;
st_format_t st_maud_format;
st_format_t st_mp3_format;
st_format_t st_nul_format;
#ifdef HAVE_OSS
st_format_t st_ossdsp_format;
#endif
st_format_t st_prc_format;
st_format_t st_raw_format;
st_format_t st_al_format;
st_format_t st_la_format;
st_format_t st_lu_format;
st_format_t st_sb_format;
st_format_t st_sl_format;
st_format_t st_sw_format;
st_format_t st_ub_format;
st_format_t st_ul_format;
st_format_t st_uw_format;
st_format_t st_sf_format;
st_format_t st_smp_format;
st_format_t st_snd_format;
st_format_t st_sphere_format;
#ifdef HAVE_SUNAUDIO
st_format_t st_sun_format;
#endif
st_format_t st_txw_format;
st_format_t st_voc_format;
#ifdef HAVE_LIBVORBIS
st_format_t st_vorbis_format;
#endif
st_format_t st_vox_format;
st_format_t st_wav_format;
st_format_t st_wve_format;

/* Raw I/O
 */
int st_rawstartread(ft_t ft);
st_ssize_t st_rawread(ft_t ft, st_sample_t *buf, st_ssize_t nsamp);
int st_rawstopread(ft_t ft);
int st_rawstartwrite(ft_t ft);
st_ssize_t st_rawwrite(ft_t ft, st_sample_t *buf, st_ssize_t nsamp);
int st_rawstopwrite(ft_t ft);
int st_rawseek(ft_t ft, st_size_t offset);

/* The following functions can be used to simply return success if
 * a file handler or effect doesn't need to do anything special
 */
int st_format_nothing(ft_t ft);
st_ssize_t st_format_nothing_io(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_format_nothing_seek(ft_t ft, st_size_t offset);
int st_effect_nothing(eff_t effp);
int st_effect_nothing_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);

/*=============================================================================
 * Effects
 *=============================================================================
 */

st_effect_t st_avg_effect;
st_effect_t st_pick_effect;
st_effect_t st_band_effect;
st_effect_t st_bandpass_effect;
st_effect_t st_bandreject_effect;
st_effect_t st_chorus_effect;
st_effect_t st_compand_effect;
st_effect_t st_copy_effect;
st_effect_t st_dcshift_effect;
st_effect_t st_deemph_effect;
st_effect_t st_earwax_effect;
st_effect_t st_echo_effect;
st_effect_t st_echos_effect;
st_effect_t st_fade_effect;
st_effect_t st_filter_effect;
st_effect_t st_flanger_effect;
st_effect_t st_highp_effect;
st_effect_t st_highpass_effect;
st_effect_t st_lowp_effect;
st_effect_t st_lowpass_effect;
st_effect_t st_mask_effect;
st_effect_t st_mcompand_effect;
st_effect_t st_noiseprof_effect;
st_effect_t st_noisered_effect;
st_effect_t st_pan_effect;
st_effect_t st_phaser_effect;
st_effect_t st_pitch_effect;
st_effect_t st_polyphase_effect;
st_effect_t st_rate_effect;
st_effect_t st_repeat_effect;
st_effect_t st_resample_effect;
st_effect_t st_reverb_effect;
st_effect_t st_reverse_effect;
st_effect_t st_silence_effect;
st_effect_t st_speed_effect;
st_effect_t st_stat_effect;
st_effect_t st_stretch_effect;
st_effect_t st_swap_effect;
st_effect_t st_synth_effect;
st_effect_t st_trim_effect;
st_effect_t st_vibro_effect;
st_effect_t st_vol_effect;

/* Needed in sox.c
 */
st_size_t st_trim_get_start(eff_t effp);
void st_trim_clear_start(eff_t effp);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
