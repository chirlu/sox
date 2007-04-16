/*
 * Originally created: July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"

/*
 * libSoX file format and effect tables.
 */

/* File format handlers. */
sox_format_fn_t sox_format_fns[] = {
  sox_aiff_format_fn,
  sox_aifc_format_fn,
  sox_al_format_fn,
#ifdef HAVE_ALSA
  sox_alsa_format_fn,
#endif
  sox_amr_wb_format_fn,
#ifdef HAVE_LIBAO
  sox_ao_format_fn,
#endif
  sox_au_format_fn,
  sox_auto_format_fn,
  sox_avr_format_fn,
  sox_cdr_format_fn,
  sox_cvsd_format_fn,
  sox_dat_format_fn,
  sox_dvms_format_fn,
#ifdef HAVE_LIBAVFORMAT
  sox_ffmpeg_format_fn,
#endif
#ifdef HAVE_LIBFLAC
  sox_flac_format_fn,
#endif
  sox_gsm_format_fn,
  sox_hcom_format_fn,
  sox_ima_format_fn,
  sox_la_format_fn,
  sox_lpc10_format_fn,
  sox_lu_format_fn,
  sox_maud_format_fn,
#if defined(HAVE_LIBMAD) || defined(HAVE_LIBMP3LAME)
  sox_mp3_format_fn,
#endif
  sox_nul_format_fn,
#ifdef HAVE_OSS
  sox_ossdsp_format_fn,
#endif
  sox_prc_format_fn,
  sox_raw_format_fn,
  sox_s3_format_fn,
  sox_sb_format_fn,
  sox_sf_format_fn,
  sox_sl_format_fn,
  sox_smp_format_fn,
  sox_snd_format_fn,
  sox_sphere_format_fn,
#ifdef HAVE_SUN_AUDIO
  sox_sun_format_fn,
#endif
  sox_svx_format_fn,
  sox_sw_format_fn,
  sox_txw_format_fn,
  sox_u3_format_fn,
  sox_u4_format_fn,
  sox_ub_format_fn,
  sox_ul_format_fn,
  sox_uw_format_fn,
  sox_voc_format_fn,
#if defined HAVE_LIBVORBISENC && defined HAVE_LIBVORBISFILE
  sox_vorbis_format_fn,
#endif
  sox_vox_format_fn,
  sox_wav_format_fn,
  sox_wve_format_fn,
  sox_xa_format_fn,
  /* Prefer internal formats over libsndfile. Can be overridden
   * by using -t sndfile. */
#ifdef HAVE_SNDFILE_H
  sox_sndfile_format_fn,
#endif
  NULL
};

/* Effects handlers. */

/*
 * SOX_EFF_CHAN means that the number of channels can change.
 * SOX_EFF_RATE means that the sample rate can change.
 * SOX_EFF_MCHAN means that the effect is coded for multiple channels.
 *
 */

sox_effect_fn_t sox_effect_fns[] = {
  sox_allpass_effect_fn,
  sox_avg_effect_fn,
  sox_band_effect_fn,
  sox_bandpass_effect_fn,
  sox_bandreject_effect_fn,
  sox_bass_effect_fn,
  sox_chorus_effect_fn,
  sox_compand_effect_fn,
  sox_dcshift_effect_fn,
  sox_deemph_effect_fn,
  sox_dither_effect_fn,
  sox_earwax_effect_fn,
  sox_echo_effect_fn,
  sox_echos_effect_fn,
  sox_equalizer_effect_fn,
  sox_fade_effect_fn,
  sox_filter_effect_fn,
  sox_flanger_effect_fn,
  sox_highpass_effect_fn,
  sox_highp_effect_fn,
  sox_lowpass_effect_fn,
  sox_lowp_effect_fn,
  sox_mask_effect_fn,
  sox_mcompand_effect_fn,
  sox_mixer_effect_fn,
  sox_noiseprof_effect_fn,
  sox_noisered_effect_fn,
  sox_pad_effect_fn,
  sox_pan_effect_fn,
  sox_phaser_effect_fn,
  sox_pick_effect_fn,
  sox_pitch_effect_fn,
  sox_polyphase_effect_fn,
#ifdef HAVE_SAMPLERATE_H
  sox_rabbit_effect_fn,
#endif
  sox_rate_effect_fn,
  sox_repeat_effect_fn,
  sox_resample_effect_fn,
  sox_reverb_effect_fn,
  sox_reverse_effect_fn,
  sox_silence_effect_fn,
  sox_speed_effect_fn,
  sox_stat_effect_fn,
  sox_stretch_effect_fn,
  sox_swap_effect_fn,
  sox_synth_effect_fn,
  sox_treble_effect_fn,
  sox_tremolo_effect_fn,
  sox_trim_effect_fn,
  sox_vibro_effect_fn,
  sox_vol_effect_fn,
  NULL
};
