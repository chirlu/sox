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

/* File format handlers. */

/* SGI/Apple AIFF */
static char *aiffnames[] = {
	"aiff",
	"aif",
	(char *) 0
};

/* a-law byte raw */
static char *alnames[] = {
	"al",
	(char *) 0
};

#if	defined(ALSA_PLAYER)
/* /dev/snd/pcmXX */
static char *alsanames[] = {
	"alsa",
	(char *) 0
};
#endif

/* SPARC .au w/header */
static char *aunames[] = {
	"au",
	"snd",
	(char *) 0
};

static char *autonames[] = {
	"auto",
	(char *) 0
};

static char *avrnames[] = {
	"avr",
	(char *) 0
};

static char *cdrnames[] = {
	"cdr",
	(char *) 0
};

/* Cont. Variable Slope Delta */
static char *cvsdnames[] = {
        "cvs",
	"cvsd",
	(char *)0
};

/* Text data samples */
static char *datnames[] = {
	"dat",
	(char *) 0
};

/* Cont. Variable Solot Delta */
static char *dvmsnames[] = {
        "vms",
	"dvms",
	(char *)0
};

#ifdef ENABLE_GSM
/* GSM 06.10 */
static char *gsmnames[] = {
        "gsm",
	(char *) 0
};
#endif

/* Mac FSSD/HCOM */
static char *hcomnames[] = {
	"hcom",
	(char *) 0
};

/* inverse a-law byte raw */
static char *lanames[] = {
	"la",
	(char *) 0
};

/* inverse u-law byte raw */
static char *lunames[] = {
	"lu",
	(char *) 0
};

/* Amiga MAUD */
static char *maudnames[] = {
        "maud",
        (char *) 0,
};

static char *nulnames[] = {
        "nul",
        (char *) 0,
};

#if	defined(OSS_PLAYER)
/* OSS /dev/dsp player */
static char *ossdspnames[] = {
	"ossdsp",
	(char *) 0
};
#endif

static char *rawnames[] = {
	"raw",
	(char *) 0
};

/* raw prototypes are defined in st.h since they are used globally. */

static char *sbnames[] = {
	"sb",
	(char *) 0
};

/* IRCAM Sound File */
static char *sfnames[] = {
	"sf",
	(char *) 0
};

static char *slnames[] = {
	"sl",
	(char *) 0,
};

/* SampleVision sound */
static char *smpnames[] = {
	"smp",
	(char *) 0,
};

/* Sndtool Sound File */
static char *sndtnames[] = {
	"sndt",
	(char *) 0
}; 

/* NIST Sphere File */
static char *spherenames[] = {
	"sph",
	(char *) 0
};

#if	defined(SUNAUDIO_PLAYER)
/* Sun /dev/audio player */
static char *sunnames[] = {
	"sunau",
	(char *) 0
};
#endif

/* Amiga 8SVX */
static char *svxnames[] = {
	"8svx",
	(char *) 0
};

static char *swnames[] = {
	"sw",
	(char *) 0
};

/* Yamaha TX16W and SY99 waves */
static char *txwnames[] = {
    "txw",
    (char *)0
};

static char *ubnames[] = {
	"ub",
	"sou",
	"fssd",
	(char *) 0
};

static char *ulnames[] = {
	"ul",
	(char *) 0
};

static char *uwnames[] = {
	"uw",
	(char *) 0
};

/* Sound Blaster .VOC */
static char *vocnames[] = {
	"voc",
	(char *) 0
};

#ifdef HAVE_LIBVORBIS
/* Ogg Vorbis */
static char *vorbisnames[] = {
	"vorbis",
	"ogg",
	(char *) 0
};
#endif

/* Microsoftt RIFF */
static char *wavnames[] = {
	"wav",
	(char *) 0
};

/* Psion .wve */
static char *wvenames[] = {
      "wve",
      (char *) 0
};


st_format_t st_formats[] = {
    {aiffnames,
	ST_FILE_STEREO | ST_FILE_LOOPS | ST_FILE_SEEK,
	st_aiffstartread, st_aiffread, st_aiffstopread,
	st_aiffstartwrite, st_aiffwrite, st_aiffstopwrite, st_aiffseek},
    {alnames, ST_FILE_STEREO,
	st_alstartread, st_rawread, st_rawstopread,
	st_alstartwrite, st_rawwrite, st_rawstopwrite, st_format_nothing_seek},
#ifdef ALSA_PLAYER
    {alsanames, ST_FILE_STEREO,
	st_alsastartread, st_rawread, st_rawstopread,
	st_alsastartwrite, st_rawwrite, st_rawstopwrite, 
	st_format_nothing_seek},
#endif
    {aunames, ST_FILE_STEREO | ST_FILE_SEEK,
	st_austartread, st_auread, st_rawstopread,
	st_austartwrite, st_auwrite, st_austopwrite, 
	st_auseek},
    {autonames, ST_FILE_STEREO,
	st_autostartread, st_format_nothing_io, st_format_nothing,
	st_autostartwrite, st_format_nothing_io, st_format_nothing, 
	st_format_nothing_seek},
    {avrnames, ST_FILE_STEREO,
	st_avrstartread, st_rawread, st_format_nothing,
	st_avrstartwrite, st_avrwrite, st_avrstopwrite, 
	st_format_nothing_seek},
    {cdrnames, ST_FILE_STEREO | ST_FILE_SEEK,
	st_cdrstartread, st_cdrread, st_cdrstopread,
	st_cdrstartwrite, st_cdrwrite, st_cdrstopwrite, 
	st_rawseek},
    {cvsdnames, 0,
	st_cvsdstartread, st_cvsdread, st_cvsdstopread,
	st_cvsdstartwrite, st_cvsdwrite, st_cvsdstopwrite, 
	st_format_nothing_seek},
    {datnames, 0,
	st_datstartread, st_datread, st_format_nothing,
	st_datstartwrite, st_datwrite, st_format_nothing, 
	st_format_nothing_seek},
    {dvmsnames, 0,
	st_dvmsstartread, st_cvsdread, st_cvsdstopread,
	st_dvmsstartwrite, st_cvsdwrite, st_dvmsstopwrite, st_format_nothing_seek},
#ifdef ENABLE_GSM
    {gsmnames, 0,
	st_gsmstartread, st_gsmread, st_gsmstopread,
	st_gsmstartwrite, st_gsmwrite, st_gsmstopwrite, st_format_nothing_seek},
#endif
    {hcomnames, 0,
	st_hcomstartread, st_hcomread, st_hcomstopread, 
	st_hcomstartwrite, st_hcomwrite, st_hcomstopwrite, st_format_nothing_seek},
    {lanames, ST_FILE_STEREO,
	st_lastartread, st_rawread, st_rawstopread,
	st_lastartwrite, st_rawwrite, st_rawstopwrite, st_format_nothing_seek},
    {lunames, ST_FILE_STEREO,
	st_lustartread, st_rawread, st_rawstopread,
	st_lustartwrite, st_rawwrite, st_rawstopwrite, st_format_nothing_seek},
    {maudnames, ST_FILE_STEREO,
	st_maudstartread, st_maudread, st_maudstopread,
	st_maudstartwrite, st_maudwrite, st_maudstopwrite, st_format_nothing_seek},
    {nulnames, ST_FILE_STEREO,
	st_nulstartread, st_nulread, st_nulstopread,
	st_nulstartwrite, st_nulwrite, st_nulstopwrite, st_format_nothing_seek},
#ifdef OSS_PLAYER
    {ossdspnames, ST_FILE_STEREO,
	st_ossdspstartread, st_rawread, st_rawstopread,
	st_ossdspstartwrite, st_rawwrite, st_rawstopwrite, st_format_nothing_seek},
#endif
    {rawnames, ST_FILE_STEREO | ST_FILE_SEEK,
	st_rawstartread, st_rawread, st_rawstopread,
	st_rawstartwrite, st_rawwrite, st_rawstopwrite, st_rawseek},
    {sbnames, ST_FILE_STEREO,
	st_sbstartread, st_rawread, st_rawstopread,
	st_sbstartwrite, st_rawwrite, st_rawstopwrite, st_format_nothing_seek},
    {sfnames, ST_FILE_STEREO | ST_FILE_SEEK,
	st_sfstartread, st_rawread, st_rawstopread,
	st_sfstartwrite, st_rawwrite, st_rawstopwrite, st_sfseek},
    { slnames, ST_FILE_STEREO,
	st_slstartread, st_rawread, st_rawstopread,
	st_slstartwrite, st_rawwrite, st_rawstopwrite, st_format_nothing_seek},
    {smpnames, ST_FILE_STEREO | ST_FILE_LOOPS | ST_FILE_SEEK,
	st_smpstartread, st_smpread, st_format_nothing,
	st_smpstartwrite, st_smpwrite, st_smpstopwrite, st_smpseek},
    {sndtnames, ST_FILE_STEREO | ST_FILE_SEEK,
	st_sndtstartread, st_rawread, st_rawstopread, 
	st_sndtstartwrite, st_sndtwrite, st_sndtstopwrite, st_sndseek},
    {spherenames, ST_FILE_STEREO,
	st_spherestartread, st_sphereread, st_rawstopread,
	st_spherestartwrite, st_spherewrite, st_spherestopwrite, 
	st_format_nothing_seek},
#ifdef SUNAUDIO_PLAYER
    {sunnames, ST_FILE_STEREO,
	st_sunstartread, st_rawread, st_rawstopread,
	st_sunstartwrite, st_rawwrite, st_rawstopwrite, st_format_nothing_seek},
#endif
    {svxnames, ST_FILE_STEREO,
	st_svxstartread, st_svxread, st_svxstopread,
	st_svxstartwrite, st_svxwrite, st_svxstopwrite, st_format_nothing_seek},
    {swnames, ST_FILE_STEREO,
	st_swstartread, st_rawread, st_rawstopread,
	st_swstartwrite, st_rawwrite, st_rawstopwrite, st_format_nothing_seek},
    {txwnames, 0,
	st_txwstartread, st_txwread, st_txwstopread, 
	st_txwstartwrite, st_txwwrite, st_txwstopwrite, st_format_nothing_seek},
    {ubnames, ST_FILE_STEREO,
	st_ubstartread, st_rawread, st_rawstopread,
	st_ubstartwrite, st_rawwrite, st_rawstopwrite, st_format_nothing_seek},
    {ulnames, ST_FILE_STEREO,
	st_ulstartread, st_rawread, st_rawstopread,
	st_ulstartwrite, st_rawwrite, st_rawstopwrite, st_format_nothing_seek},	
    {uwnames, ST_FILE_STEREO,
	st_uwstartread, st_rawread, st_rawstopread,
	st_uwstartwrite, st_rawwrite, st_rawstopwrite, st_format_nothing_seek},
    {vocnames, ST_FILE_STEREO,
	st_vocstartread, st_vocread, st_vocstopread,
	st_vocstartwrite, st_vocwrite, st_vocstopwrite, st_format_nothing_seek},
#ifdef HAVE_LIBVORBIS
    {vorbisnames, ST_FILE_STEREO,
	st_vorbisstartread, st_vorbisread, st_vorbisstopread,
	st_vorbisstartwrite, st_vorbiswrite, st_vorbisstopwrite,
        st_format_nothing_seek},
#endif
    {wavnames, ST_FILE_STEREO | ST_FILE_SEEK,
	st_wavstartread, st_wavread, st_format_nothing,
	st_wavstartwrite, st_wavwrite, st_wavstopwrite, st_wavseek},	
    {wvenames, ST_FILE_SEEK,
	st_wvestartread, st_wveread, st_rawstopread,
	st_wvestartwrite, st_wvewrite, st_wvestopwrite, st_wveseek},
    {0, 0,
	0, 0, 0, 0, 0, 0}
};

/* Effects handlers. */

/*
 * ST_EFF_CHAN means that the number of channels can change.
 * ST_EFF_RATE means that the sample rate can change.
 * ST_EFF_MCHAN means that the effect is coded for multiple channels.
 *
 */

st_effect_t st_effects[] = {
	{"avg", ST_EFF_CHAN, 
		st_avg_getopts, st_avg_start, st_avg_flow, 
		st_effect_nothing_drain, st_avg_stop},
	{"band", 0, 
		st_band_getopts, st_band_start, st_band_flow, 
		st_effect_nothing_drain, st_band_stop},
	{"bandpass", 0, 
		st_bandpass_getopts, st_bandpass_start, st_butterworth_flow, 
		st_effect_nothing_drain, st_effect_nothing},
	{"bandreject", 0, 
		st_bandreject_getopts, st_bandreject_start, st_butterworth_flow, 
		st_effect_nothing_drain, st_effect_nothing},
	{"chorus", 0,
	        st_chorus_getopts, st_chorus_start, st_chorus_flow,
	 	st_chorus_drain, st_chorus_stop},
	{"compand", ST_EFF_MCHAN,
	        st_compand_getopts, st_compand_start, st_compand_flow,
		st_compand_drain, st_compand_stop},
	{"copy", ST_EFF_MCHAN, 
		st_copy_getopts, st_copy_start, st_copy_flow, 
		st_effect_nothing_drain, st_effect_nothing},
	{"dcshift", ST_EFF_MCHAN, 
		st_dcshift_getopts, st_dcshift_start, st_dcshift_flow, 
		st_effect_nothing_drain, st_dcshift_stop},
	{"deemph", ST_EFF_MCHAN,
	        st_deemph_getopts, st_deemph_start, st_deemph_flow,
	        st_effect_nothing_drain, st_deemph_stop},
	{"earwax", ST_EFF_MCHAN, 
		st_earwax_getopts, st_earwax_start, st_earwax_flow, 
		st_earwax_drain, st_earwax_stop},
	{"echo", 0, 
		st_echo_getopts, st_echo_start, st_echo_flow, 
		st_echo_drain, st_echo_stop},
	{"echos", 0, 
		st_echos_getopts, st_echos_start, st_echos_flow,
	        st_echos_drain, st_echos_stop},
	{"fade", ST_EFF_MCHAN, 
		st_fade_getopts, st_fade_start, st_fade_flow,
	        st_fade_drain, st_fade_stop},
	{ "filter", 0,
	    	st_filter_getopts, st_filter_start, st_filter_flow,
		st_filter_drain, st_filter_stop},
	{"flanger", 0,
	        st_flanger_getopts, st_flanger_start, st_flanger_flow,
	        st_flanger_drain, st_flanger_stop},
	{"highp", 0, 
		st_highp_getopts, st_highp_start, st_highp_flow, 
		st_effect_nothing_drain, st_highp_stop},
	{"highpass", 0, 
		st_highpass_getopts, st_highpass_start, st_butterworth_flow, 
		st_effect_nothing_drain, st_effect_nothing},
	{"lowp", 0, 
		st_lowp_getopts, st_lowp_start, st_lowp_flow, 
		st_effect_nothing_drain, st_lowp_stop},
	{"lowpass", 0, 
		st_lowpass_getopts, st_lowpass_start, st_butterworth_flow, 
		st_effect_nothing_drain, st_effect_nothing},
	{"map", ST_EFF_REPORT, 
		st_map_getopts, st_map_start, st_map_flow, 
		st_effect_nothing_drain, st_effect_nothing},
	{"mask", ST_EFF_MCHAN, 
		st_mask_getopts, st_effect_nothing, st_mask_flow, 
		st_effect_nothing_drain, st_effect_nothing},
	{"pan", ST_EFF_MCHAN | ST_EFF_CHAN, 
		st_pan_getopts, st_pan_start, st_pan_flow, 
		st_effect_nothing_drain, st_pan_stop},
	{"phaser", 0,
	        st_phaser_getopts, st_phaser_start, st_phaser_flow,
	        st_phaser_drain, st_phaser_stop},
	{"pitch", 0,
	        st_pitch_getopts, st_pitch_start, st_pitch_flow,
	        st_pitch_drain, st_pitch_stop},
	{"polyphase", ST_EFF_RATE,
	        st_poly_getopts, st_poly_start, st_poly_flow,
	        st_poly_drain, st_poly_stop},
	{"rate", ST_EFF_RATE, 
		st_rate_getopts, st_rate_start, st_rate_flow, 
		st_effect_nothing_drain, st_effect_nothing},
	{"resample", ST_EFF_RATE, 
		st_resample_getopts, st_resample_start, st_resample_flow, 
		st_resample_drain, st_resample_stop},
	{"reverb", 0,
	        st_reverb_getopts, st_reverb_start, st_reverb_flow,
	        st_reverb_drain, st_reverb_stop},
	{"reverse", 0, 
		st_reverse_getopts, st_reverse_start, 
		st_reverse_flow, st_reverse_drain, st_reverse_stop},
	{"silence", ST_EFF_MCHAN, 
		st_silence_getopts, st_silence_start, 
		st_silence_flow, st_silence_drain, st_silence_stop},
	{"speed", 0, 
		st_speed_getopts, st_speed_start, 
		st_speed_flow, st_speed_drain, st_speed_stop},
	{"stat", ST_EFF_MCHAN | ST_EFF_REPORT,
		st_stat_getopts, st_stat_start, st_stat_flow, 
		st_stat_drain, st_stat_stop},
	{"stretch", 0,
	        st_stretch_getopts, st_stretch_start, st_stretch_flow,
	        st_stretch_drain, st_stretch_stop},
	{"swap", ST_EFF_MCHAN,
		st_swap_getopts, st_swap_start, st_swap_flow, 
		st_swap_drain, st_swap_stop},
        {"synth", ST_EFF_MCHAN, 
                st_synth_getopts, st_synth_start, st_synth_flow, 
                st_synth_drain, st_synth_stop},
        {"trim", ST_EFF_MCHAN, 
                st_trim_getopts, st_trim_start, st_trim_flow, 
                st_effect_nothing_drain, st_effect_nothing},
	{"vibro", 0, 
		st_vibro_getopts, st_vibro_start, st_vibro_flow, 
		st_effect_nothing_drain, st_effect_nothing},
	{"vol", ST_EFF_MCHAN, 
		st_vol_getopts, st_vol_start, st_vol_flow, 
		st_effect_nothing_drain, st_vol_stop},
	{0, 0, 0, 0, 0, 0, 0}
};


