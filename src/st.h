#ifndef ST_H
#define ST_H
/*
 * Sound Tools Library - October 11, 1999
 *
 * Copyright 1999 Chris Bagwell
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Chris Bagwell And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

#include <stdio.h>

#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif

/* FIXME: Move to seperate header */
#ifdef __alpha__
#include <sys/types.h>   /* To get defines for 32-bit integers */
#define	LONG	int32_t
#define ULONG	u_int32_t
#else
#define	LONG	long
#define ULONG	unsigned long
#endif

#ifdef __GNUC__
#define NORET __attribute__((noreturn))
#define INLINE inline
#else
#define NORET
#define INLINE
#endif

/*
 * Handler structure for each format.
 */

typedef struct format {
	char	**names;	/* file type names */
	int	flags;		/* details about file type */
	void	(*startread)();			
	LONG	(*read)();			
	void	(*stopread)();		
	void	(*startwrite)();			
	void	(*write)();
	void	(*stopwrite)();		
} format_t;

/* FIXME: Does this need to be here? */ 
extern format_t formats[];

/* Signal parameters */

/* FIXME: Change to typedef */
struct  signalinfo {
	LONG		rate;		/* sampling rate */
	int		size;		/* word length of data */
	int		style;		/* format of sample numbers */
	int		channels;	/* number of sound channels */
};

/* Loop parameters */

/* FIXME: Change to typedef */
struct  loopinfo {
	int		start;		/* first sample */
	int		length;		/* length */
	int		count;		/* number of repeats, 0=forever */
	int		type;		/* 0=no, 1=forward, 2=forward/back */
};

/* Instrument parameters */

/* vague attempt at generic information for sampler-specific info */

/* FIXME: Change to typedef */
struct  instrinfo {
	char 		MIDInote;	/* for unity pitch playback */
	char		MIDIlow, MIDIhi;/* MIDI pitch-bend range */
	char		loopmode;	/* semantics of loop data */
	char		nloops;		/* number of active loops */
	unsigned char	smpte[4];	/* SMPTE offset (hour:min:sec:frame) */
					/* this is a film audio thing */
};


#define MIDI_UNITY 60		/* MIDI note number to play sample at unity */

/* Loop modes, upper 4 bits mask the loop blass, lower 4 bits describe */
/* the loop behaviour, ie. single shot, bidirectional etc. */
#define LOOP_NONE          0	
#define LOOP_8             32	/* 8 loops: don't know ?? */
#define LOOP_SUSTAIN_DECAY 64	/* AIFF style: one sustain & one decay loop */

/*
 * File buffer info.  Holds info so that data can be read in blocks.
 */

/* FIXME: Change to typedef */
struct fileinfo {
	char	*buf;			/* Pointer to data buffer */
	int	size;			/* Size of buffer */
	int	count;			/* Count read in to buffer */
	int	pos;			/* Position in buffer */
	int	eof;			/* Marker that EOF has been reached */
};


/*
 *  Format information for input and output files.
 */

#define	PRIVSIZE	330

#define NLOOPS		8

struct soundstream {
	struct	signalinfo info;	/* signal specifications */
	struct  instrinfo instr;	/* instrument specification */
	struct  loopinfo loops[NLOOPS];	/* Looping specification */
	char	swap;			/* do byte- or word-swap */
	char	seekable;		/* can seek on this file */
	char	*filename;		/* file name */
	char	*filetype;		/* type of file */
	char	*comment;		/* comment string */
	FILE	*fp;			/* File stream pointer */
	struct	fileinfo file;		/* File data block */
	format_t *h;			/* format struct for this file */
	/* FIXME: I perfer void * or char * */
	double	priv[PRIVSIZE/8];	/* format's private data area */
};

/* This shoul not be here.  Only needed in sox.c */
extern struct soundstream informat;
extern struct soundstream outformat;

typedef struct soundstream *ft_t;

/* FIXME: Prefix all #defines with ST_ */
/* flags field */
#define FILE_STEREO	1	/* does file format support stereo? */
#define FILE_LOOPS	2	/* does file format support loops? */
#define FILE_INSTR	4	/* does file format support instrument specificications? */

/* Size field */
#define	BYTE	1
#define	WORD	2
#define	DWORD	4
#define	FLOAT	5
#define DOUBLE	6
#define IEEE	7		/* IEEE 80-bit floats.  Is it necessary? */

/* Style field */
#define UNSIGNED	1	/* unsigned linear: Sound Blaster */
#define SIGN2		2	/* signed linear 2's comp: Mac */
#define	ULAW		3	/* U-law signed logs: US telephony, SPARC */
#define ALAW		4	/* A-law signed logs: non-US telephony */
#define ADPCM		5	/* Compressed PCM */
#define IMA_ADPCM		6	/* Compressed PCM */
#define GSM		7	/* GSM 6.10 33-byte frame lossy compression */

/* FIXME: This shouldn't be defined inside library.  Only needed
 * by sox.c itself.  Delete from raw.c and misc.c.
 */
extern char *sizes[];
extern char *styles[];

/*
 * Handler structure for each effect.
 */

typedef struct {
	char	*name;			/* effect name */
	int	flags;			/* this and that */
	void	(*getopts)();		/* process arguments */
	void	(*start)();		/* start off effect */
	void	(*flow)();		/* do a buffer */
	void	(*drain)();		/* drain out at end */
	void	(*stop)();		/* finish up effect */
} effect_t;

extern effect_t effects[];

#define	EFF_CHAN	1		/* Effect can mix channels up/down */
#define EFF_RATE	2		/* Effect can alter data rate */
#define EFF_MCHAN	4		/* Effect can handle multi-channel */
#define EFF_REPORT	8		/* Effect does nothing */

struct effect {
	char		*name;		/* effect name */
	struct signalinfo ininfo;	/* input signal specifications */
	struct loopinfo   loops[8];	/* input loops  specifications */
	struct instrinfo  instr;	/* input instrument  specifications */
	struct signalinfo outinfo;	/* output signal specifications */
	effect_t 	*h;		/* effects driver */
	LONG		*obuf;		/* output buffer */
	LONG		odone, olen;	/* consumed, total length */
	double		priv[PRIVSIZE];	/* private area for effect */
};

typedef struct effect *eff_t;

/* FIXME: Move to internal st header */
#if	defined(__STDC__)
#define	P1(a) a
#define	P2(a,b) a, b
#define	P3(a,b,c) a, b, c
#define	P4(a,b,c,d) a, b, c, d
#define	P5(a,b,c,d,e) a, b, c, d, e
#define	P6(a,b,c,d,e,f) a, b, c, d, e, f
#define	P7(a,b,c,d,e,f,g) a, b, c, d, e, f, g
#define	P8(a,b,c,d,e,f,g,h) a, b, c, d, e, f, g, h
#define	P9(a,b,c,d,e,f,g,h,i) a, b, c, d, e, f, g, h, i
#define	P10(a,b,c,d,e,f,g,h,i,j) a, b, c, d, e, f, g, h, i, j
#else
#define	P1(a)
#define	P2(a,b)
#define	P3(a,b,c)
#define	P4(a,b,c,d)
#define	P5(a,b,c,d,e)
#define	P6(a,b,c,d,e,f)
#define	P7(a,b,c,d,e,f,g)
#define	P8(a,b,c,d,e,f,g,h)
#define	P9(a,b,c,d,e,f,g,h,i)
#define	P10(a,b,c,d,e,f,g,h,i,j)
#endif

/* Read and write basic data types from "ft" stream.  Uses ft->swap for
 * possible byte swapping.
 */
unsigned short rshort(P1(ft_t ft));			
unsigned short wshort(P2(ft_t ft, unsigned short us));
ULONG          rlong(P1(ft_t ft));		
ULONG          wlong(P2(ft_t ft, ULONG ul));
float          rfloat(P1(ft_t ft));
void           wfloat(P2(ft_t ft, double f));
double         rdouble(P1(ft_t ft));
void           wdouble(P2(ft_t ft, double d));

/* FIXME: raw routines are used by so many formats their prototypes are defined
 * here for convience.  This wont last for long so application software
 * shouldn't make use of it.
 */
void rawstartread(P1(ft_t ft));
void rawstartwrite(P1(ft_t ft));
void rawstopread(P1(ft_t ft));
void rawstopwrite(P1(ft_t ft));
LONG rawread(P3(ft_t ft, LONG *buf, LONG nsamp));
void rawwrite(P3(ft_t ft, LONG *buf, LONG nsamp));

/* Utilities to byte-swap values, use libc optimized macro's if possible  */
#ifdef HAVE_BYTESWAP_H
#define swapw(x) bswap_16(x)
#define swapl(x) bswap_32(x)
#define swapf(x) (float)bswap_32((ULONG)(x))
#else
unsigned short swapw(P1(unsigned short us));		/* Swap short */
ULONG  	       swapl(P1(ULONG ul));			/* Swap long */
float  	       swapf(P1(float f));			/* Swap float */
#endif
double 	       swapd(P1(double d));			/* Swap double */

/* util.c */
void report(P2(char *, ...));
void warn(P2(char *, ...));
void fail(P2(char *, ...))NORET;

void geteffect(P1(eff_t));
void gettype(P1(ft_t));
void checkformat(P1(ft_t));
void copyformat(P2(ft_t, ft_t));
void cmpformats(P2(ft_t, ft_t));

/* FIXME: Recording hacks shouldn't display a "sigint" style interface.
 * Instead we should provide a function to call when done playing/recording.
 * sox.c should be responsible for registering to sigint.
 */
void sigintreg(P1(ft_t));

/* FIXME: Move to sox header file. */
typedef	unsigned int u_i;
typedef	ULONG u_l;
typedef	unsigned short u_s;

extern float volume;	/* expansion coefficient */
extern int dovolume;

extern int writing;	/* are we writing to a file? */

/* export flags */
extern int verbose;	/* be noisy on stderr */

extern char *myname;

extern int soxpreview;	/* Preview mode: be fast and ugly */

/* FIXME: Not externally visible currently.  Its a per-effect value. */
#define	MAXRATE	50L * 1024			/* maximum sample rate */

/* FIXME: Move to internal st header */
#define RIGHT(datum, bits)	((datum) >> bits)
#define LEFT(datum, bits)	((datum) << bits)

#ifndef	M_PI
#define M_PI	3.14159265358979323846
#endif

#define READBINARY	"rb"
#define WRITEBINARY	"wb"

#define REMOVE unlink

char *version();			/* return version number */

#endif
