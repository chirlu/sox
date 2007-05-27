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

#ifdef HAVE_LTDL_H
/* FIXME: Use a vector, not a fixed-size array */
  #define MAX_FORMATS 256
  unsigned sox_formats = 0;
  sox_format_tab_t sox_format_fns[MAX_FORMATS];
#else
  #define FORMAT(f) extern sox_format_t const * sox_##f##_format_fn(void);
  #include "formats.h"
  #undef FORMAT
  sox_format_tab_t sox_format_fns[] = {
  #define FORMAT(f) {0, sox_##f##_format_fn},
  #include "formats.h"
  #undef FORMAT
  };
  unsigned sox_formats = array_length(sox_format_fns);
#endif 

/* Effects handlers. */

/* FIXME: Generate this list automatically */
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
#ifdef HAVE_LADSPA_H
  sox_ladspa_effect_fn,
#endif
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
