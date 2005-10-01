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
void st_fail(const char *, ...) NORET;
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

/* The following functions can be used to simply return success if
 * a file handler or effect doesn't need to do anything special
 */
int st_format_nothing(ft_t ft);
st_ssize_t st_format_nothing_io(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_format_nothing_seek(ft_t ft, st_size_t offset);
int st_effect_nothing(eff_t effp);
int st_effect_nothing_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);

int st_aiffstartread(ft_t ft);
st_ssize_t st_aiffread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_aiffstopread(ft_t ft);
int st_aiffstartwrite(ft_t ft);
st_ssize_t st_aiffwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_aiffstopwrite(ft_t ft);
int st_aiffseek(ft_t ft, st_size_t offset);

int st_alstartread(ft_t ft);
int st_alstartwrite(ft_t ft);

#ifdef HAVE_ALSA
int st_alsastartread(ft_t ft);
st_ssize_t st_alsaread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_alsastopread(ft_t ft);
int st_alsastartwrite(ft_t ft);
st_ssize_t st_alsawrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_alsastopwrite(ft_t ft);
#endif

int st_austartread(ft_t ft);
st_ssize_t st_auread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_austartwrite(ft_t ft);
st_ssize_t st_auwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_austopwrite(ft_t ft);
int st_auseek(ft_t ft, st_size_t offset);

int st_autostartread(ft_t ft);
int st_autostartwrite(ft_t ft);

int st_avrstartread(ft_t ft);
int st_avrstartwrite(ft_t ft);
st_ssize_t st_avrwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_avrstopwrite(ft_t ft);

int st_cdrstartread(ft_t ft);
st_ssize_t st_cdrread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_cdrstopread(ft_t ft);
int st_cdrstartwrite(ft_t ft);
st_ssize_t st_cdrwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_cdrstopwrite(ft_t ft);

int st_cvsdstartread(ft_t ft);
st_ssize_t st_cvsdread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_cvsdstopread(ft_t ft);
int st_cvsdstartwrite(ft_t ft);
st_ssize_t st_cvsdwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_cvsdstopwrite(ft_t ft);

int st_datstartread(ft_t ft);
st_ssize_t st_datread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_datstartwrite(ft_t ft);
st_ssize_t st_datwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);

int st_dvmsstartread(ft_t ft);
int st_dvmsstartwrite(ft_t ft);
int st_dvmsstopwrite(ft_t ft);

#ifdef ENABLE_GSM
int st_gsmstartread(ft_t ft);
st_ssize_t st_gsmread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_gsmstopread(ft_t ft);
int st_gsmstartwrite(ft_t ft);
st_ssize_t st_gsmwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_gsmstopwrite(ft_t ft);
#endif

int st_hcomstartread(ft_t ft);
st_ssize_t st_hcomread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_hcomstopread(ft_t ft);
int st_hcomstartwrite(ft_t ft);
st_ssize_t st_hcomwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_hcomstopwrite(ft_t ft);

int st_lastartread(ft_t ft);
int st_lastartwrite(ft_t ft);

int st_lustartread(ft_t ft);
int st_lustartwrite(ft_t ft);

int st_maudstartread(ft_t ft);
st_ssize_t st_maudread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_maudstopread(ft_t ft);
st_ssize_t st_maudwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_maudstartwrite(ft_t ft);
int st_maudstopwrite(ft_t ft);

int st_mp3startread(ft_t ft);
st_ssize_t st_mp3read(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_mp3stopread(ft_t ft);
st_ssize_t st_mp3write(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_mp3startwrite(ft_t ft);
int st_mp3stopwrite(ft_t ft);

int st_nulstartread(ft_t ft);
st_ssize_t st_nulread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_nulstopread(ft_t ft);
int st_nulstartwrite(ft_t ft);
st_ssize_t st_nulwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_nulstopwrite(ft_t ft);

#ifdef HAVE_OSS
int st_ossdspstartread(ft_t ft);
int st_ossdspstartwrite(ft_t ft);
#endif

int st_prcstartread(ft_t ft);
st_ssize_t st_prcread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_prcstartwrite(ft_t ft);
st_ssize_t st_prcwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_prcstopwrite(ft_t ft);
int st_prcseek(ft_t ft, st_size_t offset);

int st_rawstartread(ft_t ft);
st_ssize_t st_rawread(ft_t ft, st_sample_t *buf, st_ssize_t nsamp);
int st_rawstopread(ft_t ft);
int st_rawstartwrite(ft_t ft);
st_ssize_t st_rawwrite(ft_t ft, st_sample_t *buf, st_ssize_t nsamp);
int st_rawstopwrite(ft_t ft);
int st_rawseek(ft_t ft, st_size_t offset);

int st_sbstartread(ft_t ft);
int st_sbstartwrite(ft_t ft);

int st_sfstartread(ft_t ft);
int st_sfstartwrite(ft_t ft);
int st_sfseek(ft_t ft, st_size_t offset);

int st_slstartread(ft_t ft);
int st_slstartwrite(ft_t ft);

int st_smpstartread(ft_t ft);
st_ssize_t st_smpread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_smpstopread(ft_t ft);
int st_smpstartwrite(ft_t ft);
st_ssize_t st_smpwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_smpstopwrite(ft_t ft);
int st_smpseek(ft_t ft, st_size_t offset);

int st_sndtstartread(ft_t ft);
int st_sndtstartwrite(ft_t ft);
st_ssize_t st_sndtwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_sndtstopwrite(ft_t ft);
int st_sndseek(ft_t ft, st_size_t offset);

int st_spherestartread(ft_t ft);
st_ssize_t st_sphereread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_spherestartwrite(ft_t ft);
st_ssize_t st_spherewrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_spherestopwrite(ft_t ft);

#ifdef HAVE_SUNAUDIO
int st_sunstartread(ft_t ft);
int st_sunstartwrite(ft_t ft);
#endif

int st_svxstartread(ft_t ft);
st_ssize_t st_svxread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_svxstopread(ft_t ft);
int st_svxstartwrite(ft_t ft);
st_ssize_t st_svxwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_svxstopwrite(ft_t ft);

int st_swstartread(ft_t ft);
int st_swstartwrite(ft_t ft);

int st_txwstartread(ft_t ft);
st_ssize_t st_txwread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_txwstopread(ft_t ft);
int st_txwstartwrite(ft_t ft);
st_ssize_t st_txwwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_txwstopwrite(ft_t ft);

int st_ubstartread(ft_t ft);
int st_ubstartwrite(ft_t ft);

int st_ulstartread(ft_t ft);
int st_ulstartwrite(ft_t ft);

int st_uwstartread(ft_t ft);
int st_uwstartwrite(ft_t ft);

int st_vocstartread(ft_t ft);
st_ssize_t st_vocread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_vocstopread(ft_t ft);
int st_vocstartwrite(ft_t ft);
st_ssize_t st_vocwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_vocstopwrite(ft_t ft);

#ifdef HAVE_LIBVORBIS
int st_vorbisstartread(ft_t ft);
st_ssize_t st_vorbisread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_vorbisstopread(ft_t ft);
int st_vorbisstartwrite(ft_t ft);
st_ssize_t st_vorbiswrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_vorbisstopwrite(ft_t ft);
#endif

int st_voxstartread(ft_t ft);
st_ssize_t st_voxread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_voxstopread(ft_t ft);
int st_voxstartwrite(ft_t ft);
st_ssize_t st_voxwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_voxstopwrite(ft_t ft);

int st_wavstartread(ft_t ft);
st_ssize_t st_wavread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_wavstopread(ft_t ft);
int st_wavstartwrite(ft_t ft);
st_ssize_t st_wavwrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_wavstopwrite(ft_t ft);
int st_wavseek(ft_t ft, st_size_t offset);

int st_wvestartread(ft_t ft);
st_ssize_t st_wveread(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_wvestartwrite(ft_t ft);
st_ssize_t st_wvewrite(ft_t ft, st_sample_t *buf, st_ssize_t len);
int st_wvestopwrite(ft_t ft);
int st_wveseek(ft_t ft, st_size_t offset);

/*=============================================================================
 * Effects
 *=============================================================================
 */
int st_avg_getopts(eff_t effp, int argc, char **argv);
int st_avg_start(eff_t effp);
int st_avg_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                st_size_t *isamp, st_size_t *osamp);
int st_avg_stop(eff_t effp);

int st_band_getopts(eff_t effp, int argc, char **argv);
int st_band_start(eff_t effp);
int st_band_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                 st_size_t *isamp, st_size_t *osamp);
int st_band_stop(eff_t effp);
int st_bandpass_getopts(eff_t effp, int argc, char **argv);
int st_bandpass_start(eff_t effp);

int st_bandreject_getopts(eff_t effp, int argc, char **argv);
int st_bandreject_start(eff_t effp);

int st_chorus_getopts(eff_t effp, int argc, char **argv);
int st_chorus_start(eff_t effp);
int st_chorus_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                   st_size_t *isamp, st_size_t *osamp);
int st_chorus_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_chorus_stop(eff_t effp);

int st_compand_getopts(eff_t effp, int argc, char **argv);
int st_compand_start(eff_t effp);
int st_compand_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                    st_size_t *isamp, st_size_t *osamp);
int st_compand_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_compand_stop(eff_t effp);

int st_copy_getopts(eff_t effp, int argc, char **argv);
int st_copy_start(eff_t effp);
int st_copy_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                 st_size_t *isamp, st_size_t *osamp);
int st_copy_stop(eff_t effp);

int st_dcshift_getopts(eff_t effp, int argc, char **argv);
int st_dcshift_start(eff_t effp);
int st_dcshift_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                    st_size_t *isamp, st_size_t *osamp);
int st_dcshift_stop(eff_t effp);

int st_deemph_getopts(eff_t effp, int argc, char **argv);
int st_deemph_start(eff_t effp);
int st_deemph_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                   st_size_t *isamp, st_size_t *osamp);
int st_deemph_stop(eff_t effp);

int st_earwax_getopts(eff_t effp, int argc, char **argv);
int st_earwax_start(eff_t effp);
int st_earwax_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                   st_size_t *isamp, st_size_t *osamp);
int st_earwax_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_earwax_stop(eff_t effp);

int st_echo_getopts(eff_t effp, int argc, char **argv);
int st_echo_start(eff_t effp);
int st_echo_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                 st_size_t *isamp, st_size_t *osamp);
int st_echo_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_echo_stop(eff_t effp);

int st_echos_getopts(eff_t effp, int argc, char **argv);
int st_echos_start(eff_t effp);
int st_echos_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                  st_size_t *isamp, st_size_t *osamp);
int st_echos_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_echos_stop(eff_t effp);

int st_fade_getopts(eff_t effp, int argc, char **argv);
int st_fade_start(eff_t effp);
int st_fade_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                 st_size_t *isamp, st_size_t *osamp);
int st_fade_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_fade_stop(eff_t effp);

int st_filter_getopts(eff_t effp, int argc, char **argv);
int st_filter_start(eff_t effp);
int st_filter_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                   st_size_t *isamp, st_size_t *osamp);
int st_filter_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_filter_stop(eff_t effp);

int st_flanger_getopts(eff_t effp, int argc, char **argv);
int st_flanger_start(eff_t effp);
int st_flanger_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                    st_size_t *isamp, st_size_t *osamp);
int st_flanger_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_flanger_stop(eff_t effp);

int st_highp_getopts(eff_t effp, int argc, char **argv);
int st_highp_start(eff_t effp);
int st_highp_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                  st_size_t *isamp, st_size_t *osamp);
int st_highp_stop(eff_t effp);

int st_highpass_getopts(eff_t effp, int argc, char **argv);
int st_highpass_start(eff_t effp);

int st_lowp_getopts(eff_t effp, int argc, char **argv);
int st_lowp_start(eff_t effp);
int st_lowp_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                 st_size_t *isamp, st_size_t *osamp);
int st_lowp_stop(eff_t effp);

int st_lowpass_getopts(eff_t effp, int argc, char **argv);
int st_lowpass_start(eff_t effp);

int st_mask_getopts(eff_t effp, int argc, char **argv);
int st_mask_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                 st_size_t *isamp, st_size_t *osamp);

int st_mcompand_getopts(eff_t effp, int argc, char **argv);
int st_mcompand_start(eff_t effp);
int st_mcompand_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                     st_size_t *isamp, st_size_t *osamp);
int st_mcompand_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_mcompand_stop(eff_t effp);

int st_noiseprof_getopts(eff_t effp, int argc, char **argv);
int st_noiseprof_start(eff_t effp);
int st_noiseprof_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                     st_size_t *isamp, st_size_t *osamp);
int st_noiseprof_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_noiseprof_stop(eff_t effp);

int st_noisered_getopts(eff_t effp, int argc, char **argv);
int st_noisered_start(eff_t effp);
int st_noisered_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                     st_size_t *isamp, st_size_t *osamp);
int st_noisered_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_noisered_stop(eff_t effp);

int st_pan_getopts(eff_t effp, int argc, char **argv);
int st_pan_start(eff_t effp);
int st_pan_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                st_size_t *isamp, st_size_t *osamp);
int st_pan_stop(eff_t effp);

int st_phaser_getopts(eff_t effp, int argc, char **argv);
int st_phaser_start(eff_t effp);
int st_phaser_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                   st_size_t *isamp, st_size_t *osamp);
int st_phaser_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_phaser_stop(eff_t effp);

int st_pitch_getopts(eff_t effp, int argc, char **argv);
int st_pitch_start(eff_t effp);
int st_pitch_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                  st_size_t *isamp, st_size_t *osamp);
int st_pitch_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_pitch_stop(eff_t effp);

int st_poly_getopts(eff_t effp, int argc, char **argv);
int st_poly_start(eff_t effp);
int st_poly_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                 st_size_t *isamp, st_size_t *osamp);
int st_poly_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_poly_stop(eff_t effp);

int st_rate_getopts(eff_t effp, int argc, char **argv);
int st_rate_start(eff_t effp);
int st_rate_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                 st_size_t *isamp, st_size_t *osamp);
int st_rate_stop(eff_t effp);

int st_repeat_getopts(eff_t effp, int argc, char **argv);
int st_repeat_start(eff_t effp);
int st_repeat_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                     st_size_t *isamp, st_size_t *osamp);
int st_repeat_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_repeat_stop(eff_t effp);

int st_resample_getopts(eff_t effp, int argc, char **argv);
int st_resample_start(eff_t effp);
int st_resample_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                     st_size_t *isamp, st_size_t *osamp);
int st_resample_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_resample_stop(eff_t effp);

int st_reverb_getopts(eff_t effp, int argc, char **argv);
int st_reverb_start(eff_t effp);
int st_reverb_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                   st_size_t *isamp, st_size_t *osamp);
int st_reverb_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_reverb_stop(eff_t effp);

int st_reverse_getopts(eff_t effp, int argc, char **argv);
int st_reverse_start(eff_t effp);
int st_reverse_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                    st_size_t *isamp, st_size_t *osamp);
int st_reverse_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_reverse_stop(eff_t effp);

int st_silence_getopts(eff_t effp, int argc, char **argv);
int st_silence_start(eff_t effp);
int st_silence_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                    st_size_t *isamp, st_size_t *osamp);
int st_silence_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_silence_stop(eff_t effp);

int st_speed_getopts(eff_t effp, int argc, char **argv);
int st_speed_start(eff_t effp);
int st_speed_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                  st_size_t *isamp, st_size_t *osamp);
int st_speed_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_speed_stop(eff_t effp);

int st_stat_getopts(eff_t effp, int argc, char **argv);
int st_stat_start(eff_t effp);
int st_stat_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                 st_size_t *isamp, st_size_t *osamp);
int st_stat_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_stat_stop(eff_t effp);

int st_stretch_getopts(eff_t effp, int argc, char **argv);
int st_stretch_start(eff_t effp);
int st_stretch_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                    st_size_t *isamp, st_size_t *osamp);
int st_stretch_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_stretch_stop(eff_t effp);

int st_swap_getopts(eff_t effp, int argc, char **argv);
int st_swap_start(eff_t effp);
int st_swap_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                 st_size_t *isamp, st_size_t *osamp);
int st_swap_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_swap_stop(eff_t effp);

int st_synth_getopts(eff_t effp, int argc, char **argv);
int st_synth_start(eff_t effp);
int st_synth_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                  st_size_t *isamp, st_size_t *osamp);
int st_synth_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
int st_synth_stop(eff_t effp);

int st_trim_getopts(eff_t effp, int argc, char **argv);
int st_trim_start(eff_t effp);
int st_trim_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                 st_size_t *isamp, st_size_t *osamp);
int st_trim_stop(eff_t effp);
st_size_t st_trim_get_start(eff_t effp);
void st_trim_clear_start(eff_t effp);

int st_vibro_getopts(eff_t effp, int argc, char **argv);
int st_vibro_start(eff_t effp);
int st_vibro_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                  st_size_t *isamp, st_size_t *osamp);
int st_vibro_stop(eff_t effp);

int st_vol_getopts(eff_t effp, int argc, char **argv);
int st_vol_start(eff_t effp);
int st_vol_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                st_size_t *isamp, st_size_t *osamp);
int st_vol_stop(eff_t effp);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
