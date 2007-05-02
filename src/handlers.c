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

#define PLUGIN(module) {#module, NULL}

/* File format handlers. */
sox_format_tab_t sox_format_fns[] = {
  /* Built-in formats */
  PLUGIN(auto),

  /* Raw file formats */
  PLUGIN(raw),
  PLUGIN(s1),
  PLUGIN(s2),
  PLUGIN(s3),
  PLUGIN(s4),
  PLUGIN(u1),
  PLUGIN(u2),
  PLUGIN(u3),
  PLUGIN(u4),
  PLUGIN(al),
  PLUGIN(la),
  PLUGIN(ul),
  PLUGIN(lu),

  /* Plugin file formats */
  PLUGIN(aiff),
  PLUGIN(aifc),
#ifdef HAVE_LIBAMRWB
  PLUGIN(amr_wb),
#endif
  PLUGIN(au),
  PLUGIN(avr),
  PLUGIN(cdr),
  PLUGIN(cvsd),
  PLUGIN(dvms),
  PLUGIN(dat),
#ifdef HAVE_LIBAVPLUGIN
  PLUGIN(ffmpeg),
#endif
#ifdef HAVE_LIBFLAC
  PLUGIN(flac),
#endif
  PLUGIN(gsm),
  PLUGIN(hcom),
  PLUGIN(lpc10),
  PLUGIN(maud),
#if defined(HAVE_LIBMAD) || defined(HAVE_LIBMP3LAME)
  PLUGIN(mp3),
#endif
  PLUGIN(nul),
  PLUGIN(prc),
  PLUGIN(sf),
  PLUGIN(smp),
  PLUGIN(sndrtool),
  PLUGIN(sphere),
  PLUGIN(svx),
  PLUGIN(txw),
  PLUGIN(voc),
#if defined HAVE_LIBVORBISENC && defined HAVE_LIBVORBISFILE
  PLUGIN(vorbis),
#endif
  PLUGIN(vox),
  PLUGIN(ima),
  PLUGIN(wav),
  PLUGIN(wve),
  PLUGIN(xa),
  /* Prefer internal formats over libsndfile by placing sndfile last
     Can be overridden by using -t sndfile. */
#ifdef HAVE_SNDFILE_H
  PLUGIN(sndfile),
#endif

  /* I/O formats */
#ifdef HAVE_ALSA
  PLUGIN(alsa),
#endif
#ifdef HAVE_LIBAO
  PLUGIN(ao),
#endif
#ifdef HAVE_OSS
  PLUGIN(oss),
#endif
#ifdef HAVE_SUN_AUDIO
  PLUGIN(sun),
#endif

  /* End marker */
  {NULL, NULL}
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
