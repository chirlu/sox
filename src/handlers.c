/*
 * Originally created: July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "st_i.h"
#include "btrworth.h"

/*
 * Sound Tools file format and effect tables.
 */

st_format_t st_terminator_format =
{
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* File format handlers. */
st_format_t *st_formats[] = {
  &st_aiff_format,
  &st_al_format,
#if     defined(HAVE_ALSA)
  &st_alsa_format,
#endif
  &st_au_format,
  &st_auto_format,
  &st_avr_format,
  &st_cdr_format,
  &st_cvsd_format,
  &st_dat_format,
  &st_dvms_format,
#ifdef ENABLE_GSM
  &st_gsm_format,
#endif
  &st_hcom_format,
  &st_la_format,
  &st_lu_format,
  &st_maud_format,
#if defined(HAVE_LIBMAD) || defined(HAVE_LAME)
  &st_mp3_format,
#endif
  &st_nul_format,
#if     defined(HAVE_OSS)
  &st_ossdsp_format,
#endif
  &st_prc_format,
  &st_raw_format,
  &st_sb_format,
  &st_sf_format,
  &st_sl_format,
  &st_smp_format,
  &st_snd_format,
  &st_sphere_format,
#if     defined(HAVE_SUNAUDIO)
  &st_sun_format,
#endif
  &st_svx_format,
  &st_sw_format,
  &st_txw_format,
  &st_ub_format,
  &st_ul_format,
  &st_uw_format,
  &st_voc_format,
#ifdef HAVE_LIBVORBIS
  &st_vorbis_format,
#endif
  &st_vox_format,
  &st_wav_format,
  &st_wve_format,
  &st_terminator_format,
  NULL
};

/* Effects handlers. */

/*
 * ST_EFF_CHAN means that the number of channels can change.
 * ST_EFF_RATE means that the sample rate can change.
 * ST_EFF_MCHAN means that the effect is coded for multiple channels.
 *
 */

st_effect_t st_terminator_effect =
{
  0, 0, 0, 0, 0, 0, 0, 0
};

st_effect_t *st_effects[] = {
  &st_avg_effect,
  &st_band_effect,
  &st_bandpass_effect,
  &st_bandreject_effect,
  &st_chorus_effect,
  &st_compand_effect,
  &st_copy_effect,
  &st_dcshift_effect,
  &st_deemph_effect,
  &st_earwax_effect,
  &st_echo_effect,
  &st_echos_effect,
  &st_fade_effect,
  &st_filter_effect,
  &st_flanger_effect,
  &st_highp_effect,
  &st_highpass_effect,
  &st_lowp_effect,
  &st_lowpass_effect,
  &st_mask_effect,
  &st_mcompand_effect,
  &st_noiseprof_effect,
  &st_noisered_effect,
  &st_pan_effect,
  &st_phaser_effect,
  &st_pick_effect,
  &st_pitch_effect,
  &st_polyphase_effect,
  &st_rate_effect,
  &st_repeat_effect,
  &st_resample_effect,
  &st_reverb_effect,
  &st_reverse_effect,
  &st_silence_effect,
  &st_speed_effect,
  &st_stat_effect,
  &st_stretch_effect,
  &st_swap_effect,
  &st_synth_effect,
  &st_trim_effect,
  &st_vibro_effect,
  &st_vol_effect,
  &st_terminator_effect,
  NULL
};
