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
#ifdef	NeXT
	"snd",
#endif
	(char *) 0
};

extern int  st_austartread();
extern LONG st_auread();
extern int  st_austartwrite();
extern LONG st_auwrite();
extern int  st_austopwrite();

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

static char *sndrnames[] = {
	"sndr",
	(char *) 0
};

extern int st_sndrstartwrite();

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

static char *wavnames[] = {
	"wav",
	(char *) 0
};

extern int  st_wavstartread();
extern LONG st_wavread();
extern int  st_wavstartwrite();
extern LONG st_wavwrite();
extern int  st_wavstopwrite();

static char *wvenames[] = {
      "wve",
      (char *) 0
};

extern int  st_wvestartread();
extern LONG st_wveread();
extern int  st_wvestartwrite();
extern LONG st_wvewrite();
extern int  st_wvestopwrite();

extern int  st_nothing();
extern LONG st_nothing_success();

st_format_t st_formats[] = {
	{aiffnames, ST_FILE_STEREO,		/* SGI/Apple AIFF */
		st_aiffstartread, st_aiffread, st_aiffstopread,
		st_aiffstartwrite, st_aiffwrite, st_aiffstopwrite},
	{alnames, ST_FILE_STEREO,		/* a-law byte raw */
		st_alstartread, st_rawread, st_rawstopread,
		st_alstartwrite, st_rawwrite, st_rawstopwrite},	
#if	defined(ALSA_PLAYER)
	{alsanames, ST_FILE_STEREO,		/* /dev/snd/pcmXX */
		st_alsastartread, st_rawread, st_rawstopread,
		st_alsastartwrite, st_rawwrite, st_rawstopwrite},
#endif
	{aunames, ST_FILE_STEREO,		/* SPARC .au w/header */
		st_austartread, st_auread, st_rawstopread,
		st_austartwrite, st_auwrite, st_austopwrite},	
	{autonames, ST_FILE_STEREO,		/* Guess from header */
		st_autostartread, st_nothing_success, st_nothing,
		st_autostartwrite, st_nothing_success, st_nothing},
	{avrnames, ST_FILE_STEREO,		/* AVR format */
		st_avrstartread, st_rawread, st_nothing,	
		st_avrstartwrite, st_avrwrite, st_avrstopwrite},
	{cdrnames, ST_FILE_STEREO,		/* CD-R format */
		st_cdrstartread, st_cdrread, st_cdrstopread,
		st_cdrstartwrite, st_cdrwrite, st_cdrstopwrite},
	{cvsdnames, 0,			/* Cont. Variable Slope Delta */
	        st_cvsdstartread, st_cvsdread, st_cvsdstopread,
	        st_cvsdstartwrite, st_cvsdwrite, st_cvsdstopwrite},
	{datnames, 0,				/* Text data samples */
		st_datstartread, st_datread, st_nothing,
		st_datstartwrite, st_datwrite, st_nothing},
	{dvmsnames, 0,			/* Cont. Variable Solot Delta */
	        st_dvmsstartread, st_cvsdread, st_cvsdstopread,
	        st_dvmsstartwrite, st_cvsdwrite, st_dvmsstopwrite},
#ifdef HAVE_LIBGSM
	{gsmnames, 0,				/* GSM 06.10 */
	        st_gsmstartread, st_gsmread, st_gsmstopread,
	        st_gsmstartwrite, st_gsmwrite, st_gsmstopwrite},
#endif
	{hcomnames, 0,				/* Mac FSSD/HCOM */
		st_hcomstartread, st_hcomread, st_hcomstopread, 
		st_hcomstartwrite, st_hcomwrite, st_hcomstopwrite},
        {maudnames, ST_FILE_STEREO,    		/* Amiga MAUD */
		st_maudstartread, st_maudread, st_maudstopread,
		st_maudstartwrite, st_maudwrite, st_maudstopwrite},
#if	defined(OSS_PLAYER)
	{ossdspnames, ST_FILE_STEREO,		/* OSS /dev/dsp player */
		st_ossdspstartread, st_rawread, st_rawstopread,
		st_ossdspstartwrite, st_rawwrite, st_rawstopwrite},
#endif
	{rawnames, ST_FILE_STEREO,		/* Raw format */
		st_rawstartread, st_rawread, st_rawstopread,
		st_rawstartwrite, st_rawwrite, st_rawstopwrite},
	{sbnames, ST_FILE_STEREO,		/* signed byte raw */
		st_sbstartread, st_rawread, st_rawstopread,
		st_sbstartwrite, st_rawwrite, st_rawstopwrite},	
	{sfnames, ST_FILE_STEREO,		/* IRCAM Sound File */
		st_sfstartread, st_rawread, st_rawstopread,
		st_sfstartwrite, st_rawwrite, st_rawstopwrite},
	{ slnames, ST_FILE_STEREO,		/* signed long raw */
	    	st_slstartread, st_rawread, st_rawstopread,
		st_slstartwrite, st_rawwrite, st_rawstopwrite },
	{smpnames, ST_FILE_STEREO | ST_FILE_LOOPS,/* SampleVision sound */
		st_smpstartread, st_smpread, st_nothing,
		st_smpstartwrite, st_smpwrite, st_smpstopwrite},
	{sndrnames, ST_FILE_STEREO,		/* Sounder Sound File */
		st_sndtstartread, st_rawread, st_rawstopread,
		st_sndrstartwrite, st_rawwrite, st_rawstopwrite},
	{sndtnames, ST_FILE_STEREO,		/* Sndtool Sound File */
		st_sndtstartread, st_rawread, st_rawstopread, 
		st_sndtstartwrite, st_sndtwrite, st_sndtstopwrite},
#if	defined(SUNAUDIO_PLAYER)
	{sunnames, ST_FILE_STEREO,		/* Sun /dev/audio player */
		st_sunstartread, st_rawread, st_rawstopread,
		st_sunstartwrite, st_rawwrite, st_rawstopwrite},
#endif
	{svxnames, ST_FILE_STEREO,		/* Amiga 8SVX */
		st_svxstartread, st_svxread, st_svxstopread,
		st_svxstartwrite, st_svxwrite, st_svxstopwrite},
	{swnames, ST_FILE_STEREO,		/* signed word raw */
		st_swstartread, st_rawread, st_rawstopread,
		st_swstartwrite, st_rawwrite, st_rawstopwrite},
	{txwnames, 0,			/* Yamaha TX16W and SY99 waves */
	        st_txwstartread, st_txwread, st_txwstopread, 
	        st_txwstartwrite, st_txwwrite, st_txwstopwrite},
	{ubnames, ST_FILE_STEREO,		/* unsigned byte raw */
		st_ubstartread, st_rawread, st_rawstopread,
		st_ubstartwrite, st_rawwrite, st_rawstopwrite},
	{ulnames, ST_FILE_STEREO,		/* u-law byte raw */
		st_ulstartread, st_rawread, st_rawstopread,
		st_ulstartwrite, st_rawwrite, st_rawstopwrite},	
	{uwnames, ST_FILE_STEREO,		/* unsigned word raw */
		st_uwstartread, st_rawread, st_rawstopread,
		st_uwstartwrite, st_rawwrite, st_rawstopwrite},	
	{vocnames, ST_FILE_STEREO,		/* Sound Blaster .VOC */
		st_vocstartread, st_vocread, st_vocstopread,
		st_vocstartwrite, st_vocwrite, st_vocstopwrite},
	{wavnames, ST_FILE_STEREO,		/* Microsoftt RIFF */
		st_wavstartread, st_wavread, st_nothing,
		st_wavstartwrite, st_wavwrite, st_wavstopwrite},	
	{wvenames, 0,				/* Psion .wve */
		st_wvestartread, st_wveread, st_rawstopread,
		st_wvestartwrite, st_wvewrite, st_wvestopwrite},
	{0, 0,
	 0, 0, 0, 0, 0, 0}
};

/* Effects handlers. */

extern LONG st_null_drain();		/* dummy drain routine */

extern void avg_getopts();
extern void avg_start();
extern void avg_flow();
extern void avg_stop();

extern void band_getopts();
extern void band_start();
extern void band_flow();
extern void band_stop();

extern void bandpass_getopts();
extern void bandpass_start();

extern void bandreject_getopts();
extern void bandreject_start();

extern void chorus_getopts();
extern void chorus_start();
extern void chorus_flow();
extern void chorus_drain();
extern void chorus_stop();

extern void compand_getopts();
extern void compand_start();
extern void compand_flow();

extern void copy_getopts();
extern void copy_start();
extern void copy_flow();
extern void copy_stop();

extern void cut_getopts();
extern void cut_start();
extern void cut_flow();
extern void cut_stop();

extern void deemph_getopts();
extern void deemph_start();
extern void deemph_flow();
extern void deemph_stop();

#ifdef	USE_DYN
extern void dyn_getopts();
extern void dyn_start();
extern void dyn_flow();
extern void dyn_stop();
#endif

extern void echo_getopts();
extern void echo_start();
extern void echo_flow();
extern void echo_drain();
extern void echo_stop();

extern void echos_getopts();
extern void echos_start();
extern void echos_flow();
extern void echos_drain();
extern void echos_stop();

extern void filter_getopts();
extern void filter_start();
extern void filter_flow();
extern void filter_drain();
extern void filter_stop();

extern void flanger_getopts();
extern void flanger_start();
extern void flanger_flow();
extern void flanger_drain();
extern void flanger_stop();

extern void highp_getopts();
extern void highp_start();
extern void highp_flow();
extern void highp_stop();

extern void highpass_getopts();
extern void highpass_start();

extern void lowp_getopts();
extern void lowp_start();
extern void lowp_flow();
extern void lowp_stop();

extern void lowpass_getopts();
extern void lowpass_start();

extern void map_getopts();
extern void map_start();
extern void map_flow();

extern void mask_getopts();
extern void mask_flow();

extern void phaser_getopts();
extern void phaser_start();
extern void phaser_flow();
extern void phaser_drain();
extern void phaser_stop();

extern void pick_getopts();
extern void pick_start();
extern void pick_flow();
extern void pick_stop();

extern void poly_getopts();
extern void poly_start();
extern void poly_flow();
extern void poly_drain();
extern void poly_stop();

extern void rate_getopts();
extern void rate_start();
extern void rate_flow();
extern void rate_stop();

extern void resample_getopts();
extern void resample_start();
extern void resample_flow();
extern void resample_drain();
extern void resample_stop();

extern void reverb_getopts();
extern void reverb_start();
extern void reverb_flow();
extern void reverb_drain();
extern void reverb_stop();

extern void reverse_getopts();
extern void reverse_start();
extern void reverse_flow();
extern void reverse_drain();
extern void reverse_stop();

extern void split_getopts();
extern void split_start();
extern void split_flow();
extern void split_stop();

extern void stat_getopts();
extern void stat_start();
extern void stat_flow();
extern void stat_stop();

extern void swap_getopts();
extern void swap_start();
extern void swap_flow();
extern void swap_drain();
extern void swap_stop();

extern void vibro_getopts();
extern void vibro_start();
extern void vibro_flow();
extern void vibro_stop();

/*
 * EFF_CHAN means that the number of channels can change.
 * EFF_RATE means that the sample rate can change.
 * The first effect which can handle a data rate change, stereo->mono, etc.
 * is the default handler for that problem.
 * 
 * EFF_MCHAN just means that the effect is coded for multiple channels.
 */

st_effect_t st_effects[] = {
	{"null", 0, 			/* stand-in, never gets called */
		st_nothing, st_nothing, st_nothing, 
		st_null_drain, st_nothing},
	{"avg", ST_EFF_CHAN | ST_EFF_MCHAN, 
		avg_getopts, avg_start, avg_flow, 
		st_null_drain, avg_stop},
	{"band", 0, 
		band_getopts, band_start, band_flow, 
		st_null_drain, band_stop},
	{"bandpass", 0, 
		bandpass_getopts, bandpass_start, butterworth_flow, 
		st_null_drain, st_nothing},
	{"bandreject", 0, 
		bandreject_getopts, bandreject_start, butterworth_flow, 
		st_null_drain, st_nothing},
	{"chorus", 0,
	        chorus_getopts, chorus_start, chorus_flow,
	 	chorus_drain, chorus_stop},
	{"compand", ST_EFF_MCHAN,
	        compand_getopts, compand_start, compand_flow,
		st_null_drain, st_nothing},
	{"copy", ST_EFF_MCHAN, 
		copy_getopts, copy_start, copy_flow, 
		st_null_drain, st_nothing},
	{"cut", ST_EFF_MCHAN, 
		cut_getopts, cut_start, cut_flow, 
		st_null_drain, st_nothing},
	{"deemph", ST_EFF_MCHAN,
	        deemph_getopts, deemph_start, deemph_flow,
	        st_null_drain, deemph_stop},
#ifdef	USE_DYN
	{"dyn", 0, 
		dyn_getopts, dyn_start, dyn_flow, 
		st_null_drain, dyn_stop},
#endif
	{"echo", 0, 
		echo_getopts, echo_start, echo_flow, 
		echo_drain, echo_stop},
	{"echos", 0, 
		echos_getopts, echos_start, echos_flow,
	        echos_drain, echos_stop},
	{ "filter", 0,
	    	filter_getopts, filter_start, filter_flow,
		filter_drain, filter_stop},
	{"flanger", 0,
	        flanger_getopts, flanger_start, flanger_flow,
	        flanger_drain, flanger_stop},
	{"highp", 0, 
		highp_getopts, highp_start, highp_flow, 
		st_null_drain,highp_stop},
	{"highpass", 0, 
		highpass_getopts, highpass_start, butterworth_flow, 
		st_null_drain, st_nothing},
	{"lowp", 0, 
		lowp_getopts, lowp_start, lowp_flow, 
		st_null_drain, lowp_stop},
	{"lowpass", 0, 
		lowpass_getopts, lowpass_start, butterworth_flow, 
		st_null_drain, st_nothing},
	{"map", ST_EFF_REPORT, 
		map_getopts, map_start, map_flow, 
		st_null_drain, st_nothing},
	{"mask", ST_EFF_MCHAN, 
		mask_getopts, st_nothing, mask_flow, 
		st_null_drain, st_nothing},
	{"phaser", 0,
	        phaser_getopts, phaser_start, phaser_flow,
	        phaser_drain, phaser_stop},
	{"pick", ST_EFF_CHAN | ST_EFF_MCHAN, 
		pick_getopts, pick_start, pick_flow, 
		st_null_drain, pick_stop},
	{"polyphase", ST_EFF_RATE,
	        poly_getopts, poly_start, poly_flow,
	        poly_drain, poly_stop},
	{"rate", ST_EFF_RATE, 
		rate_getopts, rate_start, rate_flow, 
		st_null_drain, st_nothing},
	{"resample", ST_EFF_RATE, 
		resample_getopts, resample_start, resample_flow, 
		resample_drain, resample_stop},
	{"reverb", 0,
	        reverb_getopts, reverb_start, reverb_flow,
	        reverb_drain, reverb_stop},
	{"reverse", 0, 
		reverse_getopts, reverse_start, 
		reverse_flow, reverse_drain, reverse_stop},
	{"split", ST_EFF_CHAN | ST_EFF_MCHAN, 
		split_getopts, split_start, split_flow, 
		st_null_drain,split_stop},
	{"stat", ST_EFF_MCHAN | ST_EFF_REPORT | ST_EFF_RATE | ST_EFF_CHAN,
		stat_getopts, stat_start, stat_flow, 
		st_null_drain, stat_stop},
	{"swap", ST_EFF_MCHAN,
		swap_getopts, swap_start, swap_flow, 
		swap_drain, swap_stop},
	{"vibro", 0, 
		vibro_getopts, vibro_start, vibro_flow, 
		st_null_drain, st_nothing},
	{0, 0, 0, 0, 0, 0, 0}
};

