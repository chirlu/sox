/*
 * Originally created: July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

#include "st.h"

/*
 * Sound Tools file format and effect tables.
 */

/* File format handlers. */

char *aiffnames[] = {
	"aiff",
	"aif",
	(char *) 0
};
extern void aiffstartread();
extern LONG aiffread();
extern void aiffstopread();
extern void aiffstartwrite();
extern void aiffwrite();
extern void aiffstopwrite();

char *alnames[] = {
	"al",
	(char *) 0
};
extern void alstartread();
extern void alstartwrite();

#if	defined(ALSA_PLAYER)
char *alsanames[] = {
	"alsa",
	(char *) 0
};
extern void alsastartread();
extern void alsastartwrite();
#endif

char *aunames[] = {
	"au",
#ifdef	NeXT
	"snd",
#endif
	(char *) 0
};
extern void austartread();
extern LONG auread();
extern void austartwrite();
extern void auwrite();
extern void austopwrite();

char *autonames[] = {
	"auto",
	(char *) 0,
};

extern void autostartread();
extern void autostartwrite();

char *cdrnames[] = {
	"cdr",
	(char *) 0
};
extern void cdrstartread();
extern LONG cdrread();
extern void cdrstopread();
extern void cdrstartwrite();
extern void cdrwrite();
extern void cdrstopwrite();

char *cvsdnames[] = {
        "cvs",
	"cvsd",
	(char *)0
};
extern void cvsdstartread();
extern LONG cvsdread();
extern void cvsdstopread();
extern void cvsdstartwrite();
extern void cvsdwrite();
extern void cvsdstopwrite();

char *datnames[] = {
	"dat",
	(char *) 0
};
extern void datstartread();
extern LONG datread();
extern void datstartwrite();
extern void datwrite();

char *dvmsnames[] = {
        "vms",
	"dvms",
	(char *)0
};
extern void dvmsstartread();
extern void dvmsstartwrite();
extern void dvmsstopwrite();

#ifdef HAS_GSM
char *gsmnames[] = {
        "gsm",
	(char *) 0
};

extern void gsmstartread();
extern LONG gsmread();
extern void gsmstopread();
extern void gsmstartwrite();
extern void gsmwrite();
extern void gsmstopwrite();
#endif

char *hcomnames[] = {
	"hcom",
	(char *) 0
};
extern void hcomstartread();
extern LONG hcomread();
extern void hcomstopread();
extern void hcomstartwrite();
extern void hcomwrite();
extern void hcomstopwrite();

char *maudnames[] = {
        "maud",
        (char *) 0,
};
extern void maudstartread();
extern LONG maudread();
extern void maudstopread();
extern void maudwrite();
extern void maudstartwrite();
extern void maudstopwrite();

#if	defined(OSS_PLAYER)
char *ossdspnames[] = {
	"ossdsp",
	(char *) 0
};
extern void ossdspstartread();
extern void ossdspstartwrite();
#endif

char *rawnames[] = {
	"raw",
	(char *) 0
};
extern void rawstartread();
extern LONG rawread();
extern void rawstopread();
extern void rawstartwrite();
extern void rawwrite();
extern void rawstopwrite();

#if	defined(BLASTER) || defined(SBLAST)
char *sbdspnames[] = {
	"sbdsp",
	(char *) 0
};
extern void sbdspstartread();
extern LONG sbdspread();
extern void sbdspstopread();
extern void sbdspstartwrite();
extern void sbdspwrite();
extern void sbdspstopwrite();
#endif

char *sbnames[] = {
	"sb",
	(char *) 0
};
extern void sbstartread();
extern void sbstartwrite();

char *sfnames[] = {
	"sf",
	(char *) 0
};
extern void sfstartread();
extern void sfstartwrite();

char *smpnames[] = {
	"smp",
	(char *) 0,
};

extern void smpstartread();
extern LONG smpread();
extern void smpwrite();
extern void smpstartwrite();
extern void smpstopwrite();

char *sndrnames[] = {
	"sndr",
	(char *) 0
};
extern void sndrstartwrite();

char *sndtnames[] = {
	"sndt",
#ifdef	DOS
	"snd",
#endif
	(char *) 0
}; 
extern void sndtstartread();
extern void sndtstartwrite();
extern void sndtwrite();
extern void sndtstopwrite();

#if	defined(SUNAUDIO_PLAYER)
char *sunnames[] = {
	"sunau",
	(char *) 0
};
extern void sunstartread();
extern void sunstartwrite();
#endif

char *svxnames[] = {
	"8svx",
	(char *) 0
};
extern void svxstartread();
extern LONG svxread();
extern void svxstopread();
extern void svxstartwrite();
extern void svxwrite();
extern void svxstopwrite();

char *swnames[] = {
	"sw",
	(char *) 0
};
extern void swstartread();
extern void swstartwrite();

char *txwnames[] = {
    "txw",
    (char *)0
};
extern void txwstartread();
extern LONG txwread();
extern void txwstopread();
extern void txwstartwrite();
extern void txwwrite();
extern void txwstopwrite();

char *ubnames[] = {
	"ub",
	"sou",
	"fssd",
#ifdef	MAC
	"snd",
#endif
	(char *) 0
};
extern void ubstartread();
extern void ubstartwrite();

char *ulnames[] = {
	"ul",
	(char *) 0
};
extern void ulstartread();
extern void ulstartwrite();

char *uwnames[] = {
	"uw",
	(char *) 0
};
extern void uwstartread();
extern void uwstartwrite();

char *vocnames[] = {
	"voc",
	(char *) 0
};
extern void vocstartread();
extern LONG vocread();
extern void vocstopread();
extern void vocstartwrite();
extern void vocwrite();
extern void vocstopwrite();

char *wavnames[] = {
	"wav",
	(char *) 0
};
extern void wavstartread();
extern LONG wavread();
extern void wavstartwrite();
extern void wavwrite();
extern void wavstopwrite();

char *wvenames[] = {
      "wve",
      (char *) 0
};
extern void wvestartread();
extern LONG wveread();
extern void wvestartwrite();
extern void wvewrite();
extern void wvestopwrite();

extern void nothing();
extern LONG nothing_success();

EXPORT format_t formats[] = {
	{aiffnames, FILE_STEREO,
		aiffstartread, aiffread, aiffstopread,	   /* SGI/Apple AIFF */
		aiffstartwrite, aiffwrite, aiffstopwrite},
	{alnames, FILE_STEREO,
		alstartread, rawread, rawstopread, 	   /* a-law byte raw */
		alstartwrite, rawwrite, rawstopwrite},	
#if	defined(ALSA_PLAYER)
	{alsanames, FILE_STEREO,
		alsastartread, rawread, rawstopread,      /* /dev/snd/pcmXX */
		alsastartwrite, rawwrite, rawstopwrite},
#endif
	{aunames, FILE_STEREO,
		austartread, auread, rawstopread,      /* SPARC .AU w/header */
		austartwrite, auwrite, austopwrite},	
	{autonames, FILE_STEREO,
		autostartread, nothing_success, nothing,/* Guess from header */
		autostartwrite, nothing, nothing},	 /* patched run time */
	{cdrnames, FILE_STEREO,
		cdrstartread, cdrread, cdrstopread,	      /* CD-R format */
		cdrstartwrite, cdrwrite, cdrstopwrite},
	{cvsdnames, 0,
	        cvsdstartread, cvsdread, cvsdstopread,	   /* Cont. Variable */
	        cvsdstartwrite, cvsdwrite, cvsdstopwrite},    /* Slope Delta */
	{datnames, 0,
		datstartread, datread, nothing, 	/* Text data samples */
		datstartwrite, datwrite, nothing},
	{dvmsnames, 0,
	        dvmsstartread, cvsdread, cvsdstopread,	   /* Cont. Variable */
	        dvmsstartwrite, cvsdwrite, dvmsstopwrite},   /* Slope Delta */
#ifdef HAS_GSM
	{gsmnames, 0,
	        gsmstartread, gsmread, gsmstopread,            /* GSM 06.10 */
	        gsmstartwrite, gsmwrite, gsmstopwrite},
#endif
	{hcomnames, 0,
		hcomstartread, hcomread, hcomstopread,      /* Mac FSSD/HCOM */
		hcomstartwrite, hcomwrite, hcomstopwrite},
        {maudnames, FILE_STEREO,     			       /* Amiga MAUD */
		maudstartread, maudread, maudstopread,
		maudstartwrite, maudwrite, maudstopwrite},
#if	defined(OSS_PLAYER)
	/* OSS player. */
	{ossdspnames, FILE_STEREO,
		ossdspstartread, rawread, rawstopread, 	 /* /dev/dsp */
		ossdspstartwrite, rawwrite, rawstopwrite},
#endif
	{rawnames, FILE_STEREO,
		rawstartread, rawread, rawstopread, 	       /* Raw format */
		rawstartwrite, rawwrite, rawstopwrite},
#if	defined(BLASTER) || defined(SBLAST)
	/* 386 Unix sound blaster player. */
	{sbdspnames, FILE_STEREO,
		sbdspstartread, sbdspread, sbdspstopread,      /* /dev/sbdsp */
		sbdspstartwrite, sbdspwrite, sbdspstopwrite},	
#endif
	{sbnames, FILE_STEREO,
		sbstartread, rawread, rawstopread, 	  /* signed byte raw */
		sbstartwrite, rawwrite, rawstopwrite},	
	{sfnames, FILE_STEREO,
		sfstartread, rawread, rawstopread,       /* IRCAM Sound File */
		sfstartwrite, rawwrite, rawstopwrite},
	{smpnames, FILE_STEREO | FILE_LOOPS,
		smpstartread, smpread, nothing,	       /* SampleVision sound */
		smpstartwrite, smpwrite, smpstopwrite},	     /* Turtle Beach */
	{sndrnames, FILE_STEREO,
		sndtstartread, rawread, rawstopread,   /* Sounder Sound File */
		sndrstartwrite, rawwrite, rawstopwrite},
	{sndtnames, FILE_STEREO,
		sndtstartread, rawread, rawstopread,   /* Sndtool Sound File */
		sndtstartwrite, sndtwrite, sndtstopwrite},
#if	defined(SUNAUDIO_PLAYER)
	/* Sun /dev/audio player. */
	{sunnames, FILE_STEREO,
		sunstartread, rawread, rawstopread, 	       /* /dev/audio */
		sunstartwrite, rawwrite, rawstopwrite},
#endif
	{svxnames, FILE_STEREO,
		svxstartread, svxread, svxstopread,            /* Amiga 8SVX */
		svxstartwrite, svxwrite, svxstopwrite},
	{swnames, FILE_STEREO,
		swstartread, rawread, rawstopread,        /* signed word raw */
		swstartwrite, rawwrite, rawstopwrite},
	{txwnames, 0,
	        txwstartread, txwread, txwstopread,      /* Yamaha TX16W and */
	        txwstartwrite, txwwrite, txwstopwrite},        /* SY99 waves */
	{ubnames, FILE_STEREO,
		ubstartread, rawread, rawstopread, 	/* unsigned byte raw */
		ubstartwrite, rawwrite, rawstopwrite},
	{ulnames, FILE_STEREO,
		ulstartread, rawread, rawstopread, 	   /* u-law byte raw */
		ulstartwrite, rawwrite, rawstopwrite},	
	{uwnames, FILE_STEREO,
		uwstartread, rawread, rawstopread, 	/* unsigned word raw */
		uwstartwrite, rawwrite, rawstopwrite},	
	{vocnames, FILE_STEREO,
		vocstartread, vocread, vocstopread,    /* Sound Blaster .VOC */
		vocstartwrite, vocwrite, vocstopwrite},
	{wavnames, FILE_STEREO,
		wavstartread, wavread, nothing, 	   /* Microsoft .wav */
		wavstartwrite, wavwrite, wavstopwrite},	
	{wvenames, 0,
		wvestartread, wveread, rawstopread,            /* Psion .wve */
		wvestartwrite, wvewrite, wvestopwrite},
	{0, 0,
	 0, 0, 0, 0, 0, 0}
};

/* Effects handlers. */

extern void null_drain();		/* dummy drain routine */

extern void avg_getopts();
extern void avg_start();
extern void avg_flow();
extern void avg_stop();

extern void band_getopts();
extern void band_start();
extern void band_flow();
extern void band_stop();

extern void chorus_getopts();
extern void chorus_start();
extern void chorus_flow();
extern void chorus_drain();
extern void chorus_stop();

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

extern void flanger_getopts();
extern void flanger_start();
extern void flanger_flow();
extern void flanger_drain();
extern void flanger_stop();

extern void highp_getopts();
extern void highp_start();
extern void highp_flow();
extern void highp_stop();

extern void lowp_getopts();
extern void lowp_start();
extern void lowp_flow();
extern void lowp_stop();

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

EXPORT effect_t effects[] = {
	{"null", 0, 			/* stand-in, never gets called */
		nothing, nothing, nothing, null_drain, nothing},
	{"avg", EFF_CHAN | EFF_MCHAN, 
		avg_getopts, avg_start, avg_flow, null_drain, avg_stop},
	{"band", 0, 
		band_getopts, band_start, band_flow, null_drain, band_stop},
	{"chorus", 0,
	        chorus_getopts, chorus_start, chorus_flow,
	 chorus_drain, chorus_stop},
	{"copy", EFF_MCHAN, 
		copy_getopts, copy_start, copy_flow, null_drain, nothing},
	{"cut", EFF_MCHAN, 
		cut_getopts, cut_start, cut_flow, null_drain, nothing},
	{"deemph", EFF_MCHAN,
	        deemph_getopts, deemph_start, deemph_flow,
	        null_drain, deemph_stop},
#ifdef	USE_DYN
	{"dyn", 0, 
		dyn_getopts, dyn_start, dyn_flow, null_drain, dyn_stop},
#endif
	{"echo", 0, 
		echo_getopts, echo_start, echo_flow, echo_drain, echo_stop},
	{"echos", 0, 
		echos_getopts, echos_start, echos_flow,
	        echos_drain, echos_stop},
	{"flanger", 0,
	        flanger_getopts, flanger_start, flanger_flow,
	        flanger_drain, flanger_stop},
	{"highp", 0, 
		highp_getopts, highp_start, highp_flow, null_drain,highp_stop},
	{"lowp", 0, 
		lowp_getopts, lowp_start, lowp_flow, null_drain, lowp_stop},
	{"map", EFF_REPORT, 
		map_getopts, map_start, map_flow, null_drain, nothing},
	{"mask", EFF_MCHAN, 
		mask_getopts, nothing, mask_flow, null_drain, nothing},
	{"phaser", 0,
	        phaser_getopts, phaser_start, phaser_flow,
	        phaser_drain, phaser_stop},
	{"pick", EFF_CHAN | EFF_MCHAN, 
		pick_getopts, pick_start, pick_flow, null_drain, pick_stop},
	{"polyphase", EFF_RATE,
	        poly_getopts, poly_start, poly_flow,
	        poly_drain, poly_stop},
	{"rate", EFF_RATE, 
		rate_getopts, rate_start, rate_flow, null_drain, nothing},
	{"resample", EFF_RATE, 
		resample_getopts, resample_start, resample_flow, 
		resample_drain, resample_stop},
	{"reverb", 0,
	        reverb_getopts, reverb_start, reverb_flow,
	        reverb_drain, reverb_stop},
	{"reverse", 0, 
		reverse_getopts, reverse_start, 
		reverse_flow, reverse_drain, reverse_stop},
	{"split", EFF_CHAN | EFF_MCHAN, 
		split_getopts, split_start, split_flow, null_drain,split_stop},
	{"stat", EFF_MCHAN | EFF_REPORT | EFF_RATE | EFF_CHAN,
		stat_getopts, stat_start, stat_flow, null_drain, stat_stop},
	{"swap", EFF_MCHAN,
		swap_getopts, swap_start, swap_flow, swap_drain, swap_stop},
	{"vibro", 0, 
		vibro_getopts, vibro_start, vibro_flow, null_drain, nothing},
	{0, 0, 0, 0, 0, 0, 0}
};

