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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif


/* SJB: these may be changed to assist fail-recovery in libST */
#define st_malloc malloc
#define st_free free

/* FIXME: Move to separate header */
#ifdef __alpha__
#include <sys/types.h>   /* To get defines for 32-bit integers */
#define	LONG	int32_t
#define ULONG	u_int32_t
#else
#define	LONG	long
#define ULONG	unsigned long
#endif

#define MAXLONG 0x7fffffffL
#define MAXULONG 0xffffffffL

/* various gcc optimizations */
#ifdef __GNUC__
#define NORET __attribute__((noreturn))
#define INLINE inline
#else
#define NORET
#define INLINE
#endif

#ifdef USE_REGPARM
#define REGPARM(n) __attribute__((regparm(n)))
#else
#define REGPARM(n)
#endif

/* FIXME: Move to internal st header */
#if	defined(__STDC__) || defined(__cplusplus)
#define	P0 void
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
#define	P0
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

/* Signal parameters */

/* FIXME: Change to typedef */
struct  st_signalinfo {
	LONG		rate;		/* sampling rate */
	int		size;		/* word length of data */
	int		encoding;	/* format of sample numbers */
	int		channels;	/* number of sound channels */
	unsigned short	bs;		/* requested blocksize, eg for output .wav's */
	unsigned char	dovol;		/* has volume factor */
	double		vol;		/* volume factor */
	ULONG		x;		/* current sample number */
	ULONG		x0;		/* 1st sample (if source) */
	ULONG		x1;		/* top sample (if source) */
};

/* Loop parameters */

/* FIXME: Change to typedef */
struct  st_loopinfo {
	int		start;		/* first sample */
	int		length;		/* length */
	int		count;		/* number of repeats, 0=forever */
	int		type;		/* 0=no, 1=forward, 2=forward/back */
};

/* Instrument parameters */

/* vague attempt at generic information for sampler-specific info */

/* FIXME: Change to typedef */
struct  st_instrinfo {
	char 		MIDInote;	/* for unity pitch playback */
	char		MIDIlow, MIDIhi;/* MIDI pitch-bend range */
	char		loopmode;	/* semantics of loop data */
	char		nloops;		/* number of active loops */
	unsigned char	smpte[4];	/* SMPTE offset (hour:min:sec:frame) */
					/* this is a film audio thing */
};


#define ST_MIDI_UNITY 60	/* MIDI note number to play sample at unity */

/* Loop modes, upper 4 bits mask the loop blass, lower 4 bits describe */
/* the loop behaviour, ie. single shot, bidirectional etc. */
#define ST_LOOP_NONE          0	
#define ST_LOOP_8             32 /* 8 loops: don't know ?? */
#define ST_LOOP_SUSTAIN_DECAY 64 /* AIFF style: one sustain & one decay loop */

/*
 * File buffer info.  Holds info so that data can be read in blocks.
 */

/* FIXME: Change to typedef */
struct st_fileinfo {
	char	*buf;			/* Pointer to data buffer */
	int	size;			/* Size of buffer */
	int	count;			/* Count read in to buffer */
	int	pos;			/* Position in buffer */
	int	eof;			/* Marker that EOF has been reached */
};


/*
 *  Format information for input and output files.
 */

#define	ST_MAX_PRIVSIZE	330

#define ST_MAX_NLOOPS		8

/*
 * Handler structure for each format.
 */

typedef struct st_soundstream *ft_t;

typedef struct st_format {
	char	**names;	/* file type names */
	int	flags;		/* details about file type */
	int	(*startread)(P1(ft_t ft));			
	LONG	(*read)(P3(ft_t ft, LONG *buf, LONG len));			
	int	(*stopread)(P1(ft_t ft));		
	int	(*startwrite)(P1(ft_t ft));			
	LONG	(*write)(P3(ft_t ft, LONG *buf, LONG len));
	int	(*stopwrite)(P1(ft_t ft));		
} st_format_t;

struct st_soundstream {
	struct	st_signalinfo info;	/* signal specifications */
	struct  st_instrinfo instr;	/* instrument specification */
	struct  st_loopinfo loops[ST_MAX_NLOOPS]; /* Looping specification */
	char	swap;			/* do byte- or word-swap */
	char	seekable;		/* can seek on this file */
	char	*filename;		/* file name */
	char	*filetype;		/* type of file */
	char	*comment;		/* comment string */
	FILE	*fp;			/* File stream pointer */
	struct	st_fileinfo file;	/* File data block */
	int     st_errno;		/* Failure error codes */
	char	st_errstr[256];		/* Extend Failure text */
	st_format_t *h;			/* format struct for this file */
	/* FIXME: I perfer void * or char * */
	double	priv[ST_MAX_PRIVSIZE/8]; /* format's private data area */
};

extern st_format_t st_formats[];

/* file flags field */
#define ST_FILE_STEREO	1	/* does file format support stereo? */
#define ST_FILE_LOOPS	2	/* does file format support loops? */
#define ST_FILE_INSTR	4	/* does file format support instrument specificications? */

/* Size field */ 
/* SJB: note that the 1st 3 are sometimes used as sizeof(type) */
#define	ST_SIZE_BYTE	1
#define ST_SIZE_8BIT	1
#define	ST_SIZE_WORD	2
#define ST_SIZE_16BIT	2
#define	ST_SIZE_DWORD	4
#define ST_SIZE_32BIT	4
#define	ST_SIZE_FLOAT	5
#define ST_SIZE_DOUBLE	6
#define ST_SIZE_IEEE	7	/* IEEE 80-bit floats. */

/* Style field */
#define ST_ENCODING_UNSIGNED	1 /* unsigned linear: Sound Blaster */
#define ST_ENCODING_SIGN2	2 /* signed linear 2's comp: Mac */
#define	ST_ENCODING_ULAW	3 /* U-law signed logs: US telephony, SPARC */
#define ST_ENCODING_ALAW	4 /* A-law signed logs: non-US telephony */
#define ST_ENCODING_ADPCM	5 /* Compressed PCM */
#define ST_ENCODING_IMA_ADPCM	6 /* Compressed PCM */
#define ST_ENCODING_GSM		7 /* GSM 6.10 33-byte frame lossy compression */

/* declared in misc.c */
extern const char *st_sizes_str[];
extern const char *st_encodings_str[];

#define	ST_EFF_CHAN	1		/* Effect can mix channels up/down */
#define ST_EFF_RATE	2		/* Effect can alter data rate */
#define ST_EFF_MCHAN	4		/* Effect can handle multi-channel */
#define ST_EFF_REPORT	8		/* Effect does nothing */

/*
 * Handler structure for each effect.
 */

typedef struct st_effect *eff_t;

typedef struct {
	char	*name;			/* effect name */
	int	flags;			/* this and that */
					/* process arguments */
	int	(*getopts)(P3(eff_t effp, int argc, char **argv));
					/* start off effect */
	int	(*start)(P1(eff_t effp));
					/* do a buffer */
	int	(*flow)(P5(eff_t effp, LONG *ibuf, LONG *obuf,
			   LONG *isamp, LONG *osamp));
					/* drain out at end */
	int	(*drain)(P3(eff_t effp, LONG *obuf, LONG *osamp));
	int	(*stop)(P1(eff_t effp));/* finish up effect */
} st_effect_t;

struct st_effect {
	char		*name;		/* effect name */
	struct st_signalinfo ininfo;	/* input signal specifications */
	struct st_loopinfo   loops[8];	/* input loops  specifications */
	struct st_instrinfo  instr;	/* input instrument  specifications */
	struct st_signalinfo outinfo;	/* output signal specifications */
	st_effect_t 	*h;		/* effects driver */
	LONG		*obuf;		/* output buffer */
	LONG		odone, olen;	/* consumed, total length */
	/* FIXME: I perfer void * or char * */
	double		priv[ST_MAX_PRIVSIZE]; /* private area for effect */
};

extern st_effect_t st_effects[]; /* declared in handlers.c */

/* declared in misc.c */
extern LONG st_clip24(P1(LONG)) REGPARM(1);
extern void st_sine(P4(int *, LONG, int, int));
extern void st_triangle(P4(int *, LONG, int, int));

extern LONG st_gcd(P2(LONG,LONG)) REGPARM(2);
extern LONG st_lcm(P2(LONG,LONG)) REGPARM(2);

/****************************************************/
/* Prototypes for internal cross-platform functions */
/****************************************************/
/* SJB: shouldn't these be elsewhere, exported from misc.c */
/* CB: Yep, we need to create something like a "platform.h" file for
 * these type functions.
 */
#ifndef HAVE_RAND
extern int rand(P0);
extern void srand(P1(ULONG seed));
#endif
extern void st_initrand(P0);

#ifndef HAVE_STRERROR
char *strerror(P1(int errorcode));
#endif

/* Read and write basic data types from "ft" stream.  Uses ft->swap for
 * possible byte swapping.
 */
/* declared in misc.c */
int	st_reads(P3(ft_t ft, char *c, int len));
int	st_writes(P2(ft_t ft, char *c));
int	st_readb(P2(ft_t ft, unsigned char *uc));
int	st_writeb(P2(ft_t ft, unsigned char uc));
int	st_readw(P2(ft_t ft, unsigned short *us));
int	st_writew(P2(ft_t ft, unsigned short us));
int	st_readdw(P2(ft_t ft, ULONG *ul));		
int	st_writedw(P2(ft_t ft, ULONG ul));
int	st_readf(P2(ft_t ft, float *f));
int	st_writef(P2(ft_t ft, double f));
int	st_readdf(P2(ft_t ft, double *d));
int	st_writedf(P2(ft_t ft, double d));

/* FIXME: raw routines are used by so many formats their prototypes are defined
 * here for convience.  This wont last for long so application software
 * shouldn't make use of it.
 */
/* declared in raw.c */
int st_rawstartread(P1(ft_t ft));
int st_rawstartwrite(P1(ft_t ft));
int st_rawstopread(P1(ft_t ft));
int st_rawstopwrite(P1(ft_t ft));
LONG st_rawread(P3(ft_t ft, LONG *buf, LONG nsamp));
LONG st_rawwrite(P3(ft_t ft, LONG *buf, LONG nsamp));

/* Utilities to byte-swap values, use libc optimized macro's if possible  */
#ifdef HAVE_BYTESWAP_H
#define st_swapw(x) bswap_16(x)
#define st_swapl(x) bswap_32(x)
#define st_swapf(x) (float)bswap_32((ULONG)(x))
#else
unsigned short st_swapw(P1(unsigned short us));		/* Swap short */
ULONG  	       st_swapl(P1(ULONG ul));			/* Swap long */
float  	       st_swapf(P1(float f));			/* Swap float */
#endif
double 	       st_swapd(P1(double d));			/* Swap double */

/* util.c */
void st_report(P2(const char *, ...));
void st_warn(P2(const char *, ...));
void st_fail(P2(const char *, ...))NORET;
void st_fail_errno(P4(ft_t, int, const char *, ...));

void st_geteffect(P1(eff_t));
void st_gettype(P1(ft_t));
void st_checkformat(P1(ft_t));
void st_copyformat(P2(ft_t, ft_t));
void st_cmpformats(P2(ft_t, ft_t));

/* FIXME: Recording hacks shouldn't display a "sigint" style interface.
 * Instead we should provide a function to call when done playing/recording.
 * sox.c should be responsible for registering to sigint.
 */
void sigintreg(P1(ft_t));

/* export flags */
/* FIXME: these declared in util.c, inappropriate for lib */
extern int verbose;	/* be noisy on stderr */
extern char *myname;

/* Warning, this is a MAX value used in the library.  Each format and
 * effect may have its own limitations of rate.
 */
#define	ST_MAXRATE	50L * 1024 /* maximum sample rate in library */

/* FIXME: Move to internal st header */
#define RIGHT(datum, bits)	((datum) >> bits)
#define LEFT(datum, bits)	((datum) << bits)

#ifndef	M_PI
#define M_PI	3.14159265358979323846
#endif

/* FIXME: Move to platform header file */
#define READBINARY	"rb"
#define WRITEBINARY	"wb"
#define REMOVE unlink

#define ST_EOF (-1)
#define ST_SUCCESS (0)

const char *st_version(P0);			/* return version number */

/* ST specific error codes.  The rest directly map from errno. */
#define ST_EHDR 2000		/* Invalid Audio Header */
#define ST_EFMT 2001		/* Unsupported data format */
#define ST_ERATE 20002		/* Unsupported rate for format */
#define ST_ENOMEM 2003		/* Can't alloc memory */

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
