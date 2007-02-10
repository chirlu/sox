/*
 * Originally created: July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "st_i.h"

/*
 * Sound Tools file format and effect tables.
 */

/* File format handlers. */
st_format_fn_t st_format_fns[] = {
  st_aiff_format_fn,
  st_aifc_format_fn,
  st_al_format_fn,
#ifdef HAVE_ALSA
  st_alsa_format_fn,
#endif
  st_au_format_fn,
  st_auto_format_fn,
  st_avr_format_fn,
  st_cdr_format_fn,
  st_cvsd_format_fn,
  st_dat_format_fn,
  st_dvms_format_fn,
#ifdef HAVE_LIBFLAC
  st_flac_format_fn,
#endif
  st_gsm_format_fn,
  st_hcom_format_fn,
  st_ima_format_fn,
  st_la_format_fn,
  st_lu_format_fn,
  st_maud_format_fn,
#if defined(HAVE_LIBMAD) || defined(HAVE_LIBMP3LAME)
  st_mp3_format_fn,
#endif
  st_nul_format_fn,
#ifdef HAVE_OSS
  st_ossdsp_format_fn,
#endif
  st_prc_format_fn,
  st_raw_format_fn,
  st_s3_format_fn,
  st_sb_format_fn,
  st_sf_format_fn,
  st_sl_format_fn,
  st_smp_format_fn,
  st_snd_format_fn,
  st_sphere_format_fn,
#ifdef HAVE_SUN_AUDIO
  st_sun_format_fn,
#endif
  st_svx_format_fn,
  st_sw_format_fn,
  st_txw_format_fn,
  st_u3_format_fn,
  st_u4_format_fn,
  st_ub_format_fn,
  st_ul_format_fn,
  st_uw_format_fn,
  st_voc_format_fn,
#if defined HAVE_LIBVORBISENC && defined HAVE_LIBVORBISFILE
  st_vorbis_format_fn,
#endif
  st_vox_format_fn,
  st_wav_format_fn,
  st_wve_format_fn,
  st_xa_format_fn,
  /* Prefer internal formats over libsndfile. Can be overridden
   * by using -t sndfile. */
#ifdef HAVE_SNDFILE_H
  st_sndfile_format_fn,
#endif
  NULL
};

/* Effects handlers. */

/*
 * ST_EFF_CHAN means that the number of channels can change.
 * ST_EFF_RATE means that the sample rate can change.
 * ST_EFF_MCHAN means that the effect is coded for multiple channels.
 *
 */

st_effect_fn_t st_effect_fns[] = {
  st_allpass_effect_fn,
  st_avg_effect_fn,
  st_band_effect_fn,
  st_bandpass_effect_fn,
  st_bandreject_effect_fn,
  st_bass_effect_fn,
  st_chorus_effect_fn,
  st_compand_effect_fn,
  st_dcshift_effect_fn,
  st_deemph_effect_fn,
  st_dither_effect_fn,
  st_earwax_effect_fn,
  st_echo_effect_fn,
  st_echos_effect_fn,
  st_equalizer_effect_fn,
  st_fade_effect_fn,
  st_filter_effect_fn,
  st_flanger_effect_fn,
  st_highpass_effect_fn,
  st_highp_effect_fn,
  st_lowpass_effect_fn,
  st_lowp_effect_fn,
  st_mask_effect_fn,
  st_mcompand_effect_fn,
  st_mixer_effect_fn,
  st_noiseprof_effect_fn,
  st_noisered_effect_fn,
  st_pad_effect_fn,
  st_pan_effect_fn,
  st_phaser_effect_fn,
  st_pick_effect_fn,
  st_pitch_effect_fn,
  st_polyphase_effect_fn,
#ifdef HAVE_SAMPLERATE_H
  st_rabbit_effect_fn,
#endif
  st_rate_effect_fn,
  st_repeat_effect_fn,
  st_resample_effect_fn,
  st_reverb_effect_fn,
  st_reverse_effect_fn,
  st_silence_effect_fn,
  st_speed_effect_fn,
  st_stat_effect_fn,
  st_stretch_effect_fn,
  st_swap_effect_fn,
  st_synth_effect_fn,
  st_treble_effect_fn,
  st_tremolo_effect_fn,
  st_trim_effect_fn,
  st_vibro_effect_fn,
  st_vol_effect_fn,
  NULL
};
