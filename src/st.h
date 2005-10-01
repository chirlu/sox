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
#include "ststdint.h"

/* Release 12.17.9 of libst */
#define ST_LIB_VERSION_CODE 0x0c1109
#define ST_LIB_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

typedef int32_t st_sample_t;
typedef uint32_t st_size_t;
typedef int32_t st_ssize_t;
typedef uint32_t st_rate_t;

/* Minimum and maximum values a sample can hold. */
#define ST_SAMPLE_MAX 2147483647L
#define ST_SAMPLE_MIN (-ST_SAMPLE_MAX - 1L)
#define ST_SAMPLE_FLOAT_UNSCALE 2147483647.0
#define ST_SAMPLE_FLOAT_SCALE 2147483648.0

#define ST_UNSIGNED_BYTE_TO_SAMPLE(d) ((st_sample_t)((d) ^ 0x80) << 24)
#define ST_SIGNED_BYTE_TO_SAMPLE(d) ((st_sample_t)(d) << 24)
#define ST_UNSIGNED_WORD_TO_SAMPLE(d) ((st_sample_t)((d) ^ 0x8000) << 16)
#define ST_SIGNED_WORD_TO_SAMPLE(d) ((st_sample_t)(d) << 16)
#define ST_UNSIGNED_DWORD_TO_SAMPLE(d) ((st_sample_t)((d) ^ 0x80000000L))
#define ST_SIGNED_DWORD_TO_SAMPLE(d) ((st_sample_t)d)
/* FIXME: This is an approximation because it 
 * doesn't account for -1.0 mapping to -FLOAT_SCALE-1. */
#define ST_FLOAT_DWORD_TO_SAMPLE(d) ((st_sample_t)(d*ST_SAMPLE_FLOAT_UNSCALE))
#define ST_FLOAT_DDWORD_TO_SAMPLE(d) ((st_sample_t)(d*ST_SAMPLE_FLOAT_UNSCALE))
#define ST_SAMPLE_TO_UNSIGNED_BYTE(d) ((uint8_t)((d) >> 24) ^ 0x80)
#define ST_SAMPLE_TO_SIGNED_BYTE(d) ((int8_t)((d) >> 24))
#define ST_SAMPLE_TO_UNSIGNED_WORD(d) ((uint16_t)((d) >> 16) ^ 0x8000)
#define ST_SAMPLE_TO_SIGNED_WORD(d) ((int16_t)((d) >> 16))
#define ST_SAMPLE_TO_UNSIGNED_DWORD(d) ((uint32_t)(d) ^ 0x80000000L)
#define ST_SAMPLE_TO_SIGNED_DWORD(d) ((int32_t)(d))
/* FIXME: This is an approximation because its impossible to
 * to reach 1.
 */
#define ST_SAMPLE_TO_FLOAT_DWORD(d) ((float)(d/(ST_SAMPLE_FLOAT_SCALE)))
#define ST_SAMPLE_TO_FLOAT_DDWORD(d) ((double)((double)d/(ST_SAMPLE_FLOAT_SCALE)))

/* Maximum value size type can hold. (Minimum is 0). */
#define ST_SIZE_MAX 0xffffffffL

/* Minimum and maximum value signed size type can hold. */
#define ST_SSIZE_MAX 0x7fffffffL
#define ST_SSIZE_MIN (-ST_SSIZE_MAX - 1L)

/* Signal parameters */

typedef struct  st_signalinfo
{
    st_rate_t rate;       /* sampling rate */
    signed char size;     /* word length of data */
    signed char encoding; /* format of sample numbers */
    signed char channels; /* number of sound channels */
    char swap;            /* do byte- or word-swap */
} st_signalinfo_t;

/* Loop parameters */

typedef struct  st_loopinfo
{
    st_size_t    start;          /* first sample */
    st_size_t    length;         /* length */
    unsigned int count;          /* number of repeats, 0=forever */
    signed char  type;           /* 0=no, 1=forward, 2=forward/back */
} st_loopinfo_t;

/* Instrument parameters */

/* vague attempt at generic information for sampler-specific info */

typedef struct  st_instrinfo
{
    char MIDInote;       /* for unity pitch playback */
    char MIDIlow, MIDIhi;/* MIDI pitch-bend range */
    char loopmode;       /* semantics of loop data */
    signed char nloops;  /* number of active loops (max ST_MAX_NLOOPS) */
} st_instrinfo_t;

/* Loop modes, upper 4 bits mask the loop blass, lower 4 bits describe */
/* the loop behaviour, ie. single shot, bidirectional etc. */
#define ST_LOOP_NONE          0
#define ST_LOOP_8             32 /* 8 loops: don't know ?? */
#define ST_LOOP_SUSTAIN_DECAY 64 /* AIFF style: one sustain & one decay loop */

/*
 * File buffer info.  Holds info so that data can be read in blocks.
 */

typedef struct st_fileinfo
{
    char          *buf;                 /* Pointer to data buffer */
    size_t        size;                 /* Size of buffer */
    size_t        count;                /* Count read in to buffer */
    size_t        pos;                  /* Position in buffer */
    unsigned char eof;                  /* Marker that EOF has been reached */
} st_fileinfo_t;


/*
 *  Format information for input and output files.
 */

#define ST_MAX_FILE_PRIVSIZE    1000
#define ST_MAX_EFFECT_PRIVSIZE 1000

#define ST_MAX_NLOOPS           8

/*
 * Handler structure for each format.
 */

typedef struct st_soundstream *ft_t;

typedef struct st_format {
    char         **names;
    unsigned int flags;
    int          (*startread)(ft_t ft);
    st_ssize_t   (*read)(ft_t ft, st_sample_t *buf, st_ssize_t len);
    int          (*stopread)(ft_t ft);
    int          (*startwrite)(ft_t ft);
    st_ssize_t   (*write)(ft_t ft, st_sample_t *buf, st_ssize_t len);
    int          (*stopwrite)(ft_t ft);
    int          (*seek)(ft_t ft, st_size_t offset);
} st_format_t;

struct st_soundstream {
    st_signalinfo_t info;                 /* signal specifications */
    st_instrinfo_t  instr;                /* instrument specification */
    st_loopinfo_t   loops[ST_MAX_NLOOPS]; /* Looping specification */
    char            swap;                 /* do byte- or word-swap */
    char            seekable;             /* can seek on this file */
    char            mode;                 /* read or write mode */
    /* Total samples per channel of file.  Zero if unknown. */
    st_size_t       length;    
    char            *filename;            /* file name */
    char            *filetype;            /* type of file */
    char            *comment;             /* comment string */
    FILE            *fp;                  /* File stream pointer */
    st_fileinfo_t   file;                 /* File data block */
    int             st_errno;             /* Failure error codes */
    char            st_errstr[256];       /* Extend Failure text */
    st_format_t     *h;                   /* format struct for this file */
    /* The following is a portable trick to align this variable on
     * an 8-byte bounder.  Once this is done, the buffer alloced
     * after it should be align on an 8-byte boundery as well.
     * This lets you cast any structure over the private area
     * without concerns of alignment.
     */
    double priv1;
    char   priv[ST_MAX_FILE_PRIVSIZE]; /* format's private data area */
};

extern st_format_t st_formats[];

/* file flags field */
#define ST_FILE_STEREO  1  /* does file format support stereo? */
#define ST_FILE_LOOPS   2  /* does file format support loops? */
#define ST_FILE_INSTR   4  /* does file format support instrument specs? */
#define ST_FILE_SEEK    8  /* does file format support seeking? */
#define ST_FILE_NOSTDIO 16 /* does not use stdio routines */

/* Size field */
#define ST_SIZE_BYTE    1
#define ST_SIZE_8BIT    1
#define ST_SIZE_WORD    2
#define ST_SIZE_16BIT   2
#define ST_SIZE_24BIT   3
#define ST_SIZE_DWORD   4
#define ST_SIZE_32BIT   4
#define ST_SIZE_DDWORD  8
#define ST_SIZE_64BIT   8
#define ST_INFO_SIZE_MAX     8

/* Style field */
#define ST_ENCODING_UNSIGNED    1 /* unsigned linear: Sound Blaster */
#define ST_ENCODING_SIGN2       2 /* signed linear 2's comp: Mac */
#define ST_ENCODING_ULAW        3 /* u-law signed logs: US telephony, SPARC */
#define ST_ENCODING_ALAW        4 /* A-law signed logs: non-US telephony */
#define ST_ENCODING_FLOAT       5 /* 32-bit float */
#define ST_ENCODING_ADPCM       6 /* Compressed PCM */
#define ST_ENCODING_IMA_ADPCM   7 /* Compressed PCM */
#define ST_ENCODING_GSM         8 /* GSM 6.10 33byte frame lossy compression */
#define ST_ENCODING_INV_ULAW    9 /* Inversed bit-order u-law */
#define ST_ENCODING_INV_ALAW    10/* Inversed bit-order A-law */
#define ST_ENCODING_MP3         11/* MP3 compression */
#define ST_ENCODING_VORBIS      12/* Vorbis compression */
#define ST_ENCODING_MAX         12 

/* declared in misc.c */
extern const char *st_sizes_str[];
extern const char *st_size_bits_str[];
extern const char *st_encodings_str[];

#define ST_EFF_CHAN     1               /* Effect can mix channels up/down */
#define ST_EFF_RATE     2               /* Effect can alter data rate */
#define ST_EFF_MCHAN    4               /* Effect can handle multi-channel */
#define ST_EFF_REPORT   8               /* Effect does nothing */

/*
 * Handler structure for each effect.
 */

typedef struct st_effect *eff_t;

typedef struct
{
    char    *name;                  /* effect name */
    unsigned int flags;

    int (*getopts)(eff_t effp, int argc, char **argv);
    int (*start)(eff_t effp);
    int (*flow)(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf,
                st_size_t *isamp, st_size_t *osamp);
    int (*drain)(eff_t effp, st_sample_t *obuf, st_size_t *osamp);
    int (*stop)(eff_t effp);
} st_effect_t;

struct st_effect
{
    char            *name;          /* effect name */
    struct st_signalinfo ininfo;    /* input signal specifications */
    struct st_signalinfo outinfo;   /* output signal specifications */
    st_effect_t     *h;             /* effects driver */
    st_sample_t     *obuf;          /* output buffer */
    st_size_t       odone, olen;    /* consumed, total length */
    /* The following is a portable trick to align this variable on
     * an 8-byte bounder.  Once this is done, the buffer alloced
     * after it should be align on an 8-byte boundery as well.
     * This lets you cast any structure over the private area
     * without concerns of alignment.
     */
    double priv1;
    char priv[ST_MAX_EFFECT_PRIVSIZE]; /* private area for effect */
};

extern st_effect_t st_effects[]; /* declared in handlers.c */

extern ft_t st_open_read(const char *path, const st_signalinfo_t *info, 
                         const char *filetype);
extern ft_t st_open_write(const char *path, const st_signalinfo_t *info,
                          const char *filetype, const char *comment);
extern ft_t st_open_write_instr(const char *path, const st_signalinfo_t *info,
                                const char *filetype, const char *comment, 
                                const st_instrinfo_t *instr,
                                const st_loopinfo_t *loops);
extern st_ssize_t st_read(ft_t ft, st_sample_t *buf, st_ssize_t len);
extern st_ssize_t st_write(ft_t ft, st_sample_t *buf, st_ssize_t len);
extern int st_close(ft_t ft);

#define ST_SEEK_SET 0
extern int st_seek(ft_t ft, st_size_t offset, int whence);

int st_geteffect_opt(eff_t, int, char **);
int st_geteffect(eff_t, char *);
int st_checkeffect(char *);
int st_updateeffect(eff_t, st_signalinfo_t *in, st_signalinfo_t *out, int);
int st_gettype(ft_t);
ft_t st_initformat(void);
int st_parsesamples(st_rate_t rate, char *str, st_size_t *samples, char def);

/* FIXME: these declared in util.c, global is inappropriate for lib */
extern int verbose;     /* be noisy on stderr */
extern char *myname;

#define ST_EOF (-1)
#define ST_SUCCESS (0)

const char *st_version(void);                   /* return version number */

/* ST specific error codes.  The rest directly map from errno. */
#define ST_EHDR 2000            /* Invalid Audio Header */
#define ST_EFMT 2001            /* Unsupported data format */
#define ST_ERATE 2002           /* Unsupported rate for format */
#define ST_ENOMEM 2003          /* Can't alloc memory */
#define ST_EPERM 2004           /* Operation not permitted */
#define ST_ENOTSUP 2005         /* Operation not supported */
#define ST_EINVAL 2006          /* Invalid argument */
#define ST_EFFMT 2007           /* Unsupported file format */

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
