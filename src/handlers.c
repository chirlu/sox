/*
 * Originally created: July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

#include "st.h"
#include "btrworth.h"

/*
 * Sound Tools file format and effect tables.
 */

/* File format handlers. */

static char *aiffnames[] = {
	"aiff",
	"aif",
	(char *) 0
};
extern int  st_aiffstartread();
extern LONG st_aiffread();
extern int  st_aiffstopread();
extern int  st_aiffstartwrite();
extern LONG st_aiffwrite();
extern int  st_aiffstopwrite();
extern int  st_aiffseek();

static char *alnames[] = {
	"al",
	(char *) 0
};
extern int st_alstartread();
extern int st_alstartwrite();

#if	defined(ALSA_PLAYER)
static char *alsanames[] = {
	"alsa",
	(char *) 0
};
extern int st_alsastartread();
extern int st_alsastartwrite();
#endif

static char *aunames[] = {
	"au",
#ifndef	DOS
	"snd",
#endif
	(char *) 0
};

extern int  st_austartread();
extern LONG st_auread();
extern int  st_austartwrite();
extern LONG st_auwrite();
extern int  st_austopwrite();
extern int  st_auseek();

static char *autonames[] = {
	"auto",
	(char *) 0
};

extern int st_autostartread();
extern int st_autostartwrite();

static char *avrnames[] = {
	"avr",
	(char *) 0
};

extern int  st_avrstartread();
extern int  st_avrstartwrite();
extern LONG st_avrwrite();
extern int  st_avrstopwrite();

static char *cdrnames[] = {
	"cdr",
	(char *) 0
};

extern int  st_cdrstartread();
extern LONG st_cdrread();
extern int  st_cdrstopread();
extern int  st_cdrstartwrite();
extern LONG st_cdrwrite();
extern int  st_cdrstopwrite();

static char *cvsdnames[] = {
        "cvs",
	"cvsd",
	(char *)0
};

extern int  st_cvsdstartread();
extern LONG st_cvsdread();
extern int  st_cvsdstopread();
extern int  st_cvsdstartwrite();
extern LONG st_cvsdwrite();
extern int  st_cvsdstopwrite();

static char *datnames[] = {
	"dat",
	(char *) 0
};

extern int  st_datstartread();
extern LONG st_datread();
extern int  st_datstartwrite();
extern LONG st_datwrite();

static char *dvmsnames[] = {
        "vms",
	"dvms",
	(char *)0
};

extern int st_dvmsstartread();
extern int st_dvmsstartwrite();
extern int st_dvmsstopwrite();

#ifdef HAVE_LIBGSM
static char *gsmnames[] = {
        "gsm",
	(char *) 0
};

extern int  st_gsmstartread();
extern LONG st_gsmread();
extern int  st_gsmstopread();
extern int  st_gsmstartwrite();
extern LONG st_gsmwrite();
extern int  st_gsmstopwrite();
#endif

static char *hcomnames[] = {
	"hcom",
	(char *) 0
};

extern int  st_hcomstartread();
extern LONG st_hcomread();
extern int  st_hcomstopread();
extern int  st_hcomstartwrite();
extern LONG st_hcomwrite();
extern int  st_hcomstopwrite();

static char *maudnames[] = {
        "maud",
        (char *) 0,
};

extern int  st_maudstartread();
extern LONG st_maudread();
extern int  st_maudstopread();
extern LONG st_maudwrite();
extern int  st_maudstartwrite();
extern int  st_maudstopwrite();

static char *nulnames[] = {
        "nul",
        (char *) 0,
};

extern int  st_nulstartread();
extern LONG st_nulread();
extern int  st_nulstopread();
extern LONG st_nulwrite();
extern int  st_nulstartwrite();
extern int  st_nulstopwrite();

#if	defined(OSS_PLAYER)
static char *ossdspnames[] = {
	"ossdsp",
	(char *) 0
};

extern int st_ossdspstartread();
extern int st_ossdspstartwrite();
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

extern int st_sbstartread();
extern int st_sbstartwrite();

static char *sfnames[] = {
	"sf",
	(char *) 0
};

extern int st_sfstartread();
extern int st_sfstartwrite();
extern int st_sfseek();

static char *slnames[] = {
	"sl",
	(char *) 0,
};

extern int st_slstartread();
extern int st_slstartwrite();

static char *smpnames[] = {
	"smp",
	(char *) 0,
};

extern int  st_smpstartread();
extern LONG st_smpread();
extern LONG st_smpwrite();
extern int  st_smpstartwrite();
extern int  st_smpstopwrite();
extern int  st_smpseek();

static char *sndtnames[] = {
	"sndt",
#ifdef	DOS
	"snd",
#endif
	(char *) 0
}; 

extern int  st_sndtstartread();
extern int  st_sndtstartwrite();
extern LONG st_sndtwrite();
extern int  st_sndtstopwrite();
extern int  st_sndseek();

static char *spherenames[] = {
	"sph",
	(char *) 0
};

extern int  st_spherestartread();
extern LONG st_sphereread();
extern int  st_spherestartwrite();
extern LONG st_spherewrite();
extern int  st_spherestopwrite();

#if	defined(SUNAUDIO_PLAYER)
static char *sunnames[] = {
	"sunau",
	(char *) 0
};

extern int st_sunstartread();
extern int st_sunstartwrite();
#endif

static char *svxnames[] = {
	"8svx",
	(char *) 0
};

extern int  st_svxstartread();
extern LONG st_svxread();
extern int  st_svxstopread();
extern int  st_svxstartwrite();
extern LONG st_svxwrite();
extern int  st_svxstopwrite();

static char *swnames[] = {
	"sw",
	(char *) 0
};

extern int st_swstartread();
extern int st_swstartwrite();

static char *txwnames[] = {
    "txw",
    (char *)0
};

extern int  st_txwstartread();
extern LONG st_txwread();
extern int  st_txwstopread();
extern int  st_txwstartwrite();
extern LONG st_txwwrite();
extern int  st_txwstopwrite();

static char *ubnames[] = {
	"ub",
	"sou",
	"fssd",
#ifdef	MAC
	"snd",
#endif
	(char *) 0
};

extern int st_ubstartread();
extern int st_ubstartwrite();

static char *ulnames[] = {
	"ul",
	(char *) 0
};

extern int st_ulstartread();
extern int st_ulstartwrite();

static char *uwnames[] = {
	"uw",
	(char *) 0
};

extern int st_uwstartread();
extern int st_uwstartwrite();

static char *vocnames[] = {
	"voc",
	(char *) 0
};

extern int  st_vocstartread();
extern LONG st_vocread();
extern int  st_vocstopread();
extern int  st_vocstartwrite();
extern LONG st_vocwrite();
extern int  st_vocstopwrite();

#ifdef HAVE_LIBVORBIS
static char *vorbisnames[] = {
	"vorbis",
	"ogg",
	(char *) 0
};

extern int  st_vorbisstartread();
extern LONG st_vorbisread();
extern int  st_vorbisstopread();
extern int  st_vorbisstartwrite();
extern LONG st_vorbiswrite();
extern int  st_vorbisstopwrite();
#endif

static char *wavnames[] = {
	"wav",
	(char *) 0
};

extern int  st_wavstartread();
extern LONG st_wavread();
extern int  st_wavstartwrite();
extern LONG st_wavwrite();
extern int  st_wavstopwrite();
extern int  st_wavseek();

static char *wvenames[] = {
      "wve",
      (char *) 0
};

extern int  st_wvestartread();
extern LONG st_wveread();
extern int  st_wvestartwrite();
extern LONG st_wvewrite();
extern int  st_wvestopwrite();
extern int  st_wveseek();

extern int  st_nothing();
extern LONG st_nothing_success();

st_format_t st_formats[] = {
	{aiffnames, 					/* SGI/Apple AIFF */

	    ST_FILE_STEREO | ST_FILE_LOOPS | ST_FILE_SEEK,
	        st_aiffstartread, st_aiffread, st_aiffstopread,
		st_aiffstartwrite, st_aiffwrite, st_aiffstopwrite, st_aiffseek},
	{alnames, ST_FILE_STEREO,		/* a-law byte raw */
		st_alstartread, st_rawread, st_rawstopread,
		st_alstartwrite, st_rawwrite, st_rawstopwrite, st_nothing},	
#if	defined(ALSA_PLAYER)
	{alsanames, ST_FILE_STEREO,		/* /dev/snd/pcmXX */
		st_alsastartread, st_rawread, st_rawstopread,
		st_alsastartwrite, st_rawwrite, st_rawstopwrite, st_nothing},
#endif
	{aunames, ST_FILE_STEREO | ST_FILE_SEEK,		/* SPARC .au w/header */
		st_austartread, st_auread, st_rawstopread,
		st_austartwrite, st_auwrite, st_austopwrite, st_auseek},	
	{autonames, ST_FILE_STEREO,		/* Guess from header */
		st_autostartread, st_nothing_success, st_nothing,
		st_autostartwrite, st_nothing_success, st_nothing, st_nothing},
	{avrnames, ST_FILE_STEREO,		/* AVR format */
		st_avrstartread, st_rawread, st_nothing,	
		st_avrstartwrite, st_avrwrite, st_avrstopwrite, st_nothing},
	{cdrnames, ST_FILE_STEREO | ST_FILE_SEEK,		/* CD-R format */
		st_cdrstartread, st_cdrread, st_cdrstopread,
		st_cdrstartwrite, st_cdrwrite, st_cdrstopwrite, st_rawseek},
	{cvsdnames, 0,			/* Cont. Variable Slope Delta */
	        st_cvsdstartread, st_cvsdread, st_cvsdstopread,
	        st_cvsdstartwrite, st_cvsdwrite, st_cvsdstopwrite, st_nothing},
	{datnames, 0,				/* Text data samples */
		st_datstartread, st_datread, st_nothing,
		st_datstartwrite, st_datwrite, st_nothing, st_nothing},
	{dvmsnames, 0,			/* Cont. Variable Solot Delta */
	        st_dvmsstartread, st_cvsdread, st_cvsdstopread,
	        st_dvmsstartwrite, st_cvsdwrite, st_dvmsstopwrite, st_nothing},
#ifdef HAVE_LIBGSM
	{gsmnames, 0,				/* GSM 06.10 */
	        st_gsmstartread, st_gsmread, st_gsmstopread,
	        st_gsmstartwrite, st_gsmwrite, st_gsmstopwrite, st_nothing},
#endif
	{hcomnames, 0,				/* Mac FSSD/HCOM */
		st_hcomstartread, st_hcomread, st_hcomstopread, 
		st_hcomstartwrite, st_hcomwrite, st_hcomstopwrite, st_nothing},
        {maudnames, ST_FILE_STEREO,    		/* Amiga MAUD */
		st_maudstartread, st_maudread, st_maudstopread,
		st_maudstartwrite, st_maudwrite, st_maudstopwrite, st_nothing},
        {nulnames, ST_FILE_STEREO,    		/* NUL */
 		st_nulstartread, st_nulread, st_nulstopread,
 		st_nulstartwrite, st_nulwrite, st_nulstopwrite},
#if	defined(OSS_PLAYER)
	{ossdspnames, ST_FILE_STEREO,		/* OSS /dev/dsp player */
		st_ossdspstartread, st_rawread, st_rawstopread,
		st_ossdspstartwrite, st_rawwrite, st_rawstopwrite, st_nothing},
#endif
	{rawnames, ST_FILE_STEREO | ST_FILE_SEEK,		/* Raw format */
		st_rawstartread, st_rawread, st_rawstopread,
		st_rawstartwrite, st_rawwrite, st_rawstopwrite, st_rawseek},
	{sbnames, ST_FILE_STEREO,		/* signed byte raw */
		st_sbstartread, st_rawread, st_rawstopread,
		st_sbstartwrite, st_rawwrite, st_rawstopwrite, st_nothing},	
	{sfnames, ST_FILE_STEREO | ST_FILE_SEEK,		/* IRCAM Sound File */
		st_sfstartread, st_rawread, st_rawstopread,
		st_sfstartwrite, st_rawwrite, st_rawstopwrite, st_sfseek},
	{ slnames, ST_FILE_STEREO,		/* signed long raw */
	    	st_slstartread, st_rawread, st_rawstopread,
		st_slstartwrite, st_rawwrite, st_rawstopwrite, st_nothing },
	{smpnames, ST_FILE_STEREO | ST_FILE_LOOPS | ST_FILE_SEEK,/* SampleVision sound */
		st_smpstartread, st_smpread, st_nothing,
		st_smpstartwrite, st_smpwrite, st_smpstopwrite, st_smpseek},
	{sndtnames, ST_FILE_STEREO | ST_FILE_SEEK,		/* Sndtool Sound File */
		st_sndtstartread, st_rawread, st_rawstopread, 
		st_sndtstartwrite, st_sndtwrite, st_sndtstopwrite, st_sndseek},
	{spherenames, ST_FILE_STEREO,		/* NIST Sphere File */
	        st_spherestartread, st_sphereread, st_rawstopread,
		st_spherestartwrite, st_spherewrite, st_spherestopwrite, st_nothing},
#if	defined(SUNAUDIO_PLAYER)
	{sunnames, ST_FILE_STEREO,		/* Sun /dev/audio player */
		st_sunstartread, st_rawread, st_rawstopread,
		st_sunstartwrite, st_rawwrite, st_rawstopwrite, st_nothing},
#endif
	{svxnames, ST_FILE_STEREO,		/* Amiga 8SVX */
		st_svxstartread, st_svxread, st_svxstopread,
		st_svxstartwrite, st_svxwrite, st_svxstopwrite, st_nothing},
	{swnames, ST_FILE_STEREO,		/* signed word raw */
		st_swstartread, st_rawread, st_rawstopread,
		st_swstartwrite, st_rawwrite, st_rawstopwrite, st_nothing},
	{txwnames, 0,			/* Yamaha TX16W and SY99 waves */
	        st_txwstartread, st_txwread, st_txwstopread, 
	        st_txwstartwrite, st_txwwrite, st_txwstopwrite, st_nothing},
	{ubnames, ST_FILE_STEREO,		/* unsigned byte raw */
		st_ubstartread, st_rawread, st_rawstopread,
		st_ubstartwrite, st_rawwrite, st_rawstopwrite, st_nothing},
	{ulnames, ST_FILE_STEREO,		/* u-law byte raw */
		st_ulstartread, st_rawread, st_rawstopread,
		st_ulstartwrite, st_rawwrite, st_rawstopwrite, st_nothing},	
	{uwnames, ST_FILE_STEREO,		/* unsigned word raw */
		st_uwstartread, st_rawread, st_rawstopread,
		st_uwstartwrite, st_rawwrite, st_rawstopwrite, st_nothing},	
	{vocnames, ST_FILE_STEREO,		/* Sound Blaster .VOC */
		st_vocstartread, st_vocread, st_vocstopread,
		st_vocstartwrite, st_vocwrite, st_vocstopwrite, st_nothing},
#ifdef HAVE_LIBVORBIS
	{vorbisnames, ST_FILE_STEREO,		/* Ogg Vorbis */
		st_vorbisstartread, st_vorbisread, st_vorbisstopread,
		st_vorbisstartwrite, st_vorbiswrite, st_vorbisstopwrite},
#endif
	{wavnames, ST_FILE_STEREO | ST_FILE_SEEK,		/* Microsoftt RIFF */
		st_wavstartread, st_wavread, st_nothing,
		st_wavstartwrite, st_wavwrite, st_wavstopwrite, st_wavseek},	
	{wvenames, ST_FILE_SEEK,				/* Psion .wve */
		st_wvestartread, st_wveread, st_rawstopread,
		st_wvestartwrite, st_wvewrite, st_wvestopwrite, st_wveseek},
	{0, 0,
	 0, 0, 0, 0, 0, 0}
};

/* Effects handlers. */

extern int st_null_drain();		/* dummy drain routine */

extern int st_avg_getopts();
extern int st_avg_start();
extern int st_avg_flow();
extern int st_avg_stop();

extern int st_band_getopts();
extern int st_band_start();
extern int st_band_flow();
extern int st_band_stop(); 
extern int st_bandpass_getopts();
extern int st_bandpass_start();

extern int st_bandreject_getopts();
extern int st_bandreject_start();

extern int st_chorus_getopts();
extern int st_chorus_start();
extern int st_chorus_flow();
extern int st_chorus_drain();
extern int st_chorus_stop();

extern int st_compand_getopts();
extern int st_compand_start();
extern int st_compand_flow();
extern int st_compand_drain();
extern int st_compand_stop();

extern int st_copy_getopts(); 
extern int st_copy_start();
extern int st_copy_flow();
extern int st_copy_stop();

extern int st_dcshift_getopts();
extern int st_dcshift_start();
extern int st_dcshift_flow();
extern int st_dcshift_stop();

extern int st_deemph_getopts();
extern int st_deemph_start();
extern int st_deemph_flow();
extern int st_deemph_stop();

extern int st_echo_getopts();
extern int st_echo_start();
extern int st_echo_flow();
extern int st_echo_drain();
extern int st_echo_stop();

extern int st_echos_getopts();
extern int st_echos_start();
extern int st_echos_flow();
extern int st_echos_drain();
extern int st_echos_stop();

extern int st_earwax_getopts();
extern int st_earwax_start();
extern int st_earwax_flow();
extern int st_earwax_drain();
extern int st_earwax_stop();

extern int st_fade_getopts();
extern int st_fade_start();
extern int st_fade_flow();
extern int st_fade_drain();
extern int st_fade_stop();

extern int st_filter_getopts();
extern int st_filter_start();
extern int st_filter_flow();
extern int st_filter_drain();
extern int st_filter_stop();

extern int st_flanger_getopts();
extern int st_flanger_start();
extern int st_flanger_flow();
extern int st_flanger_drain();
extern int st_flanger_stop();

extern int st_highp_getopts();
extern int st_highp_start();
extern int st_highp_flow();
extern int st_highp_stop();

extern int st_highpass_getopts();
extern int st_highpass_start();

extern int st_lowp_getopts();
extern int st_lowp_start();
extern int st_lowp_flow();
extern int st_lowp_stop();

extern int st_lowpass_getopts();
extern int st_lowpass_start();

extern int st_map_getopts();
extern int st_map_start();
extern int st_map_flow();

extern int st_mask_getopts();
extern int st_mask_flow();

extern int st_pan_getopts();
extern int st_pan_start();
extern int st_pan_flow();
extern int st_pan_stop();

extern int st_phaser_getopts();
extern int st_phaser_start();
extern int st_phaser_flow();
extern int st_phaser_drain();
extern int st_phaser_stop();

extern int st_pick_getopts();
extern int st_pick_start();
extern int st_pick_flow();
extern int st_pick_stop();

extern int st_pitch_getopts();
extern int st_pitch_start();
extern int st_pitch_flow();
extern int st_pitch_drain();
extern int st_pitch_stop();

extern int st_poly_getopts();
extern int st_poly_start();
extern int st_poly_flow();
extern int st_poly_drain();
extern int st_poly_stop();

extern int st_rate_getopts();
extern int st_rate_start();
extern int st_rate_flow();
extern int st_rate_stop();

extern int st_resample_getopts();
extern int st_resample_start();
extern int st_resample_flow();
extern int st_resample_drain();
extern int st_resample_stop();

extern int st_reverb_getopts();
extern int st_reverb_start();
extern int st_reverb_flow();
extern int st_reverb_drain();
extern int st_reverb_stop();

extern int st_reverse_getopts();
extern int st_reverse_start();
extern int st_reverse_flow();
extern int st_reverse_drain();
extern int st_reverse_stop();

extern int st_silence_getopts();
extern int st_silence_start();
extern int st_silence_flow();
extern int st_silence_drain();
extern int st_silence_stop();

extern int st_speed_getopts();
extern int st_speed_start();
extern int st_speed_flow();
extern int st_speed_drain();
extern int st_speed_stop();

extern int st_split_getopts();
extern int st_split_start();
extern int st_split_flow();
extern int st_split_stop();

extern int st_stat_getopts();
extern int st_stat_start();
extern int st_stat_flow();
extern int st_stat_drain();
extern int st_stat_stop();

extern int st_stretch_getopts();
extern int st_stretch_start();
extern int st_stretch_flow();
extern int st_stretch_drain();
extern int st_stretch_stop();

extern int st_swap_getopts();
extern int st_swap_start();
extern int st_swap_flow();
extern int st_swap_drain();
extern int st_swap_stop();

extern int st_synth_getopts(); 
extern int st_synth_start();
extern int st_synth_flow();
extern int st_synth_drain();
extern int st_synth_stop();

extern int st_vibro_getopts();
extern int st_vibro_start();
extern int st_vibro_flow();
extern int st_vibro_stop();

extern int st_vol_getopts();
extern int st_vol_start();
extern int st_vol_flow();
extern int st_vol_stop();

extern int st_trim_getopts(); 
extern int st_trim_start();
extern int st_trim_flow();
extern int st_trim_stop();

/*
 * ST_EFF_CHAN means that the number of channels can change.
 * ST_EFF_RATE means that the sample rate can change.
 * ST_EFF_MCHAN means that the effect is coded for multiple channels.
 *
 */

st_effect_t st_effects[] = {
	{"null", 0, 			/* stand-in, never gets called */
		st_nothing, st_nothing, st_nothing, 
		st_null_drain, st_nothing},
	{"avg", ST_EFF_CHAN, 
		st_avg_getopts, st_avg_start, st_avg_flow, 
		st_null_drain, st_avg_stop},
	{"band", 0, 
		st_band_getopts, st_band_start, st_band_flow, 
		st_null_drain, st_band_stop},
	{"bandpass", 0, 
		st_bandpass_getopts, st_bandpass_start, st_butterworth_flow, 
		st_null_drain, st_nothing},
	{"bandreject", 0, 
		st_bandreject_getopts, st_bandreject_start, st_butterworth_flow, 
		st_null_drain, st_nothing},
	{"chorus", 0,
	        st_chorus_getopts, st_chorus_start, st_chorus_flow,
	 	st_chorus_drain, st_chorus_stop},
	{"compand", ST_EFF_MCHAN,
	        st_compand_getopts, st_compand_start, st_compand_flow,
		st_compand_drain, st_compand_stop},
	{"copy", ST_EFF_MCHAN, 
		st_copy_getopts, st_copy_start, st_copy_flow, 
		st_null_drain, st_nothing},
	{"dcshift", ST_EFF_MCHAN, 
		st_dcshift_getopts, st_dcshift_start, st_dcshift_flow, 
		st_null_drain, st_dcshift_stop},
	{"deemph", ST_EFF_MCHAN,
	        st_deemph_getopts, st_deemph_start, st_deemph_flow,
	        st_null_drain, st_deemph_stop},
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
		st_null_drain, st_highp_stop},
	{"highpass", 0, 
		st_highpass_getopts, st_highpass_start, st_butterworth_flow, 
		st_null_drain, st_nothing},
	{"lowp", 0, 
		st_lowp_getopts, st_lowp_start, st_lowp_flow, 
		st_null_drain, st_lowp_stop},
	{"lowpass", 0, 
		st_lowpass_getopts, st_lowpass_start, st_butterworth_flow, 
		st_null_drain, st_nothing},
	{"map", ST_EFF_REPORT, 
		st_map_getopts, st_map_start, st_map_flow, 
		st_null_drain, st_nothing},
	{"mask", ST_EFF_MCHAN, 
		st_mask_getopts, st_nothing, st_mask_flow, 
		st_null_drain, st_nothing},
	{"pan", ST_EFF_MCHAN | ST_EFF_CHAN, 
		st_pan_getopts, st_pan_start, st_pan_flow, 
		st_null_drain, st_pan_stop},
	{"phaser", 0,
	        st_phaser_getopts, st_phaser_start, st_phaser_flow,
	        st_phaser_drain, st_phaser_stop},
	{"pick", ST_EFF_MCHAN | ST_EFF_CHAN, 
		st_pick_getopts, st_pick_start, st_pick_flow, 
		st_null_drain, st_pick_stop},
	{"pitch", 0,
	        st_pitch_getopts, st_pitch_start, st_pitch_flow,
	        st_pitch_drain, st_pitch_stop},
	{"polyphase", ST_EFF_RATE,
	        st_poly_getopts, st_poly_start, st_poly_flow,
	        st_poly_drain, st_poly_stop},
	{"rate", ST_EFF_RATE, 
		st_rate_getopts, st_rate_start, st_rate_flow, 
		st_null_drain, st_nothing},
	{"resample", ST_EFF_RATE, 
		st_resample_getopts, st_resample_start, st_resample_flow, 
		st_resample_drain, st_resample_stop},
	{"reverb", 0,
	        st_reverb_getopts, st_reverb_start, st_reverb_flow,
	        st_reverb_drain, st_reverb_stop},
	{"reverse", 0, 
		st_reverse_getopts, st_reverse_start, 
		st_reverse_flow, st_reverse_drain, st_reverse_stop},
	{"silence", 0, 
		st_silence_getopts, st_silence_start, 
		st_silence_flow, st_silence_drain, st_silence_stop},
	{"speed", 0, 
		st_speed_getopts, st_speed_start, 
		st_speed_flow, st_speed_drain, st_speed_stop},
	{"split", ST_EFF_MCHAN | ST_EFF_CHAN, 
		st_split_getopts, st_split_start, st_split_flow, 
		st_null_drain, st_split_stop},
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
	{"vibro", 0, 
		st_vibro_getopts, st_vibro_start, st_vibro_flow, 
		st_null_drain, st_nothing},
	{"vol", ST_EFF_MCHAN, 
		st_vol_getopts, st_vol_start, st_vol_flow, 
		st_null_drain, st_vol_stop},
        {"trim", ST_EFF_MCHAN, 
                st_trim_getopts, st_trim_start, st_trim_flow, 
                st_null_drain, st_nothing},

	{0, 0, 0, 0, 0, 0, 0}
};


