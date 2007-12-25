/*
 * libSoX Library Public Interface
 *
 * Copyright 1999-2007 Chris Bagwell and SoX Contributors.
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Chris Bagwell And SoX Contributors are not responsible for
 * the consequences of using this software.
 */

#ifndef SOX_H
#define SOX_H

#include <stddef.h> /* Ensure NULL etc. are available throughout SoX */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "soxstdint.h"

/* Avoid warnings about unused parameters. */
#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

/* The following is the API version of libSoX.  It is not meant
 * to follow the version number of SoX but it has historically.
 * Please do not count on these numbers being in sync.
 * The following is at 14.0.1
 */
#define SOX_LIB_VERSION_CODE 0x0e0001
#define SOX_LIB_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

const char *sox_version(void);   /* Returns version number */

#define SOX_SUCCESS 0
#define SOX_EOF (-1)             /* End Of File or other error */

/* libSoX specific error codes.  The rest directly map from errno. */
#define SOX_EHDR 2000            /* Invalid Audio Header */
#define SOX_EFMT 2001            /* Unsupported data format */
#define SOX_ERATE 2002           /* Unsupported rate for format */
#define SOX_ENOMEM 2003          /* Can't alloc memory */
#define SOX_EPERM 2004           /* Operation not permitted */
#define SOX_ENOTSUP 2005         /* Operation not supported */
#define SOX_EINVAL 2006          /* Invalid argument */
#define SOX_EFFMT 2007           /* Unsupported file format */

/* Boolean type, assignment (but not necessarily binary) compatible with
 * C++ bool */
typedef enum {sox_false, sox_true} sox_bool;

typedef int32_t int24_t;     /* But beware of the extra byte. */
typedef uint32_t uint24_t;   /* ditto */

#define SOX_INT_MIN(bits) (1 <<((bits)-1))
#define SOX_INT_MAX(bits) (-1U>>(33-(bits)))
#define SOX_UINT_MAX(bits) (SOX_INT_MIN(bits)|SOX_INT_MAX(bits))

#define SOX_INT8_MAX  SOX_INT_MAX(8)
#define SOX_INT16_MAX SOX_INT_MAX(16)
#define SOX_INT24_MAX SOX_INT_MAX(24)
#define SOX_INT32_MAX SOX_INT_MAX(32)
#define SOX_INT64_MAX 0x7fffffffffffffffLL /* Not in use yet */

typedef int32_t sox_sample_t;

/* Minimum and maximum values a sample can hold. */
#define SOX_SAMPLE_MAX (sox_sample_t)SOX_INT_MAX(32)
#define SOX_SAMPLE_MIN (sox_sample_t)SOX_INT_MIN(32)



/*                Conversions: Linear PCM <--> sox_sample_t
 *
 *   I/O       I/O     sox_sample_t Clips?    I/O     sox_sample_t Clips? 
 *  Format   Minimum     Minimum     I O    Maximum     Maximum     I O      
 *  ------  ---------  ------------ -- --   --------  ------------ -- --  
 *  Float      -1     -1.00000000047 y y       1           1        y n         
 *  Int8      -128        -128       n n      127     127.9999999   n y   
 *  Int16    -32768      -32768      n n     32767    32767.99998   n y   
 *  Int24   -8388608    -8388608     n n    8388607   8388607.996   n y   
 *  Int32  -2147483648 -2147483648   n n   2147483647 2147483647    n n   
 *
 * Conversions are as accurate as possible (with rounding).
 *
 * Rounding: halves toward +inf, all others to nearest integer.
 *
 * Clips? shows whether on not there is the possibility of a conversion
 * clipping to the minimum or maximum value when inputing from or outputing 
 * to a given type.
 *
 * Unsigned integers are converted to and from signed integers by flipping
 * the upper-most bit then treating them as signed integers.
 */

/* Temporary variables to prevent multiple evaluation of macro arguments: */
static sox_sample_t sox_macro_temp_sample UNUSED;
static double sox_macro_temp_double UNUSED;

#define SOX_SAMPLE_NEG SOX_INT_MIN(32)
#define SOX_SAMPLE_TO_UNSIGNED(bits,d,clips) \
  (uint##bits##_t)( \
    sox_macro_temp_sample=(d), \
    sox_macro_temp_sample>(sox_sample_t)(SOX_SAMPLE_MAX-(1U<<(31-bits)))? \
      ++(clips),SOX_UINT_MAX(bits): \
      ((uint32_t)(sox_macro_temp_sample^SOX_SAMPLE_NEG)+(1U<<(31-bits)))>>(32-bits))
#define SOX_SAMPLE_TO_SIGNED(bits,d,clips) \
  (int##bits##_t)(SOX_SAMPLE_TO_UNSIGNED(bits,d,clips)^SOX_INT_MIN(bits))
#define SOX_SIGNED_TO_SAMPLE(bits,d)((sox_sample_t)(d)<<(32-bits))
#define SOX_UNSIGNED_TO_SAMPLE(bits,d)(SOX_SIGNED_TO_SAMPLE(bits,d)^SOX_SAMPLE_NEG)

#define SOX_UNSIGNED_8BIT_TO_SAMPLE(d,clips) SOX_UNSIGNED_TO_SAMPLE(8,d)
#define SOX_SIGNED_8BIT_TO_SAMPLE(d,clips) SOX_SIGNED_TO_SAMPLE(8,d)
#define SOX_UNSIGNED_16BIT_TO_SAMPLE(d,clips) SOX_UNSIGNED_TO_SAMPLE(16,d)
#define SOX_SIGNED_16BIT_TO_SAMPLE(d,clips) SOX_SIGNED_TO_SAMPLE(16,d)
#define SOX_UNSIGNED_24BIT_TO_SAMPLE(d,clips) SOX_UNSIGNED_TO_SAMPLE(24,d)
#define SOX_SIGNED_24BIT_TO_SAMPLE(d,clips) SOX_SIGNED_TO_SAMPLE(24,d)
#define SOX_UNSIGNED_32BIT_TO_SAMPLE(d,clips) ((sox_sample_t)(d)^SOX_SAMPLE_NEG)
#define SOX_SIGNED_32BIT_TO_SAMPLE(d,clips) (sox_sample_t)(d)
#define SOX_FLOAT_32BIT_TO_SAMPLE SOX_FLOAT_64BIT_TO_SAMPLE
#define SOX_FLOAT_64BIT_TO_SAMPLE(d,clips) (sox_macro_temp_double=(d),sox_macro_temp_double<-1?++(clips),(-SOX_SAMPLE_MAX):sox_macro_temp_double>1?++(clips),SOX_SAMPLE_MAX:(sox_sample_t)((uint32_t)((double)(sox_macro_temp_double)*SOX_SAMPLE_MAX+(SOX_SAMPLE_MAX+.5))-SOX_SAMPLE_MAX))
#define SOX_SAMPLE_TO_UNSIGNED_8BIT(d,clips) SOX_SAMPLE_TO_UNSIGNED(8,d,clips)
#define SOX_SAMPLE_TO_SIGNED_8BIT(d,clips) SOX_SAMPLE_TO_SIGNED(8,d,clips)
#define SOX_SAMPLE_TO_UNSIGNED_16BIT(d,clips) SOX_SAMPLE_TO_UNSIGNED(16,d,clips)
#define SOX_SAMPLE_TO_SIGNED_16BIT(d,clips) SOX_SAMPLE_TO_SIGNED(16,d,clips)
#define SOX_SAMPLE_TO_UNSIGNED_24BIT(d,clips) SOX_SAMPLE_TO_UNSIGNED(24,d,clips)
#define SOX_SAMPLE_TO_SIGNED_24BIT(d,clips) SOX_SAMPLE_TO_SIGNED(24,d,clips)
#define SOX_SAMPLE_TO_UNSIGNED_32BIT(d,clips) (uint32_t)((d)^SOX_SAMPLE_NEG)
#define SOX_SAMPLE_TO_SIGNED_32BIT(d,clips) (int32_t)(d)
#define SOX_SAMPLE_TO_FLOAT_32BIT SOX_SAMPLE_TO_FLOAT_64BIT
#define SOX_SAMPLE_TO_FLOAT_64BIT(d,clips) (sox_macro_temp_sample=(d),sox_macro_temp_sample==SOX_SAMPLE_MIN?++(clips),-1.0:((double)(sox_macro_temp_sample)*(1.0/SOX_SAMPLE_MAX)))



/* MACRO to clip a data type that is greater then sox_sample_t to
 * sox_sample_t's limits and increment a counter if clipping occurs..
 */
#define SOX_SAMPLE_CLIP_COUNT(samp, clips) \
  do { \
    if (samp > SOX_SAMPLE_MAX) \
      { samp = SOX_SAMPLE_MAX; clips++; } \
    else if (samp < SOX_SAMPLE_MIN) \
      { samp = SOX_SAMPLE_MIN; clips++; } \
  } while (0)

/* Rvalue MACRO to round and clip a double to a sox_sample_t,
 * and increment a counter if clipping occurs.
 */
#define SOX_ROUND_CLIP_COUNT(d, clips) \
  ((d) < 0? (d) <= SOX_SAMPLE_MIN - 0.5? ++(clips), SOX_SAMPLE_MIN: (d) - 0.5 \
        : (d) >= SOX_SAMPLE_MAX + 0.5? ++(clips), SOX_SAMPLE_MAX: (d) + 0.5)

/* Rvalue MACRO to clip an integer to a given number of bits
 * and increment a counter if clipping occurs.
 */
#define SOX_INTEGER_CLIP_COUNT(bits,i,clips) ( \
  (i) >(1 << ((bits)-1))- 1? ++(clips),(1 << ((bits)-1))- 1 : \
  (i) <-1 << ((bits)-1)    ? ++(clips),-1 << ((bits)-1) : (i))
#define SOX_16BIT_CLIP_COUNT(i,clips) SOX_INTEGER_CLIP_COUNT(16,i,clips)
#define SOX_24BIT_CLIP_COUNT(i,clips) SOX_INTEGER_CLIP_COUNT(24,i,clips)



typedef uint32_t sox_size_t;
/* Maximum value size type can hold. (Minimum is 0). */
#define SOX_SIZE_MAX 0xffffffff
#define SOX_SAMPLE_BITS (sizeof(sox_size_t) * CHAR_BIT)

typedef int32_t sox_ssize_t;
/* Minimum and maximum value signed size type can hold. */
#define SOX_SSIZE_MAX 0x7fffffff
#define SOX_SSIZE_MIN (-SOX_SSIZE_MAX - 1)

typedef double sox_rate_t;
/* Warning, this is a MAX value used in the library.  Each format and
 * effect may have its own limitations of rate.
 */
#define SOX_MAXRATE      (50U * 1024) /* maximum sample rate in library */

typedef enum {
  SOX_ENCODING_UNKNOWN   ,

  SOX_ENCODING_ULAW      , /* u-law signed logs: US telephony, SPARC */
  SOX_ENCODING_ALAW      , /* A-law signed logs: non-US telephony */
  SOX_ENCODING_ADPCM     , /* G72x Compressed PCM */
  SOX_ENCODING_MS_ADPCM  , /* Microsoft Compressed PCM */
  SOX_ENCODING_IMA_ADPCM , /* IMA Compressed PCM */
  SOX_ENCODING_OKI_ADPCM , /* Dialogic/OKI Compressed PCM */

  SOX_ENCODING_SIZE_IS_WORD, /* FIXME: marks raw types (above) that mis-report size. sox_signalinfo_t really needs a precision_in_bits item */

  SOX_ENCODING_UNSIGNED  , /* unsigned linear: Sound Blaster */
  SOX_ENCODING_SIGN2     , /* signed linear 2's comp: Mac */
  SOX_ENCODING_FLOAT     , /* 32-bit float */
  SOX_ENCODING_GSM       , /* GSM 6.10 33byte frame lossy compression */
  SOX_ENCODING_MP3       , /* MP3 compression */
  SOX_ENCODING_VORBIS    , /* Vorbis compression */
  SOX_ENCODING_FLAC      , /* FLAC compression */
  SOX_ENCODING_AMR_WB    , /* AMR-WB compression */
  SOX_ENCODING_AMR_NB    , /* AMR-NB compression */

  SOX_ENCODINGS            /* End of list marker */
} sox_encoding_t;

typedef enum {sox_plot_off, sox_plot_octave, sox_plot_gnuplot} sox_plot_t;
typedef enum {SOX_OPTION_NO, SOX_OPTION_YES, SOX_OPTION_DEFAULT} sox_option_t;

/* Signal parameters */

typedef struct sox_signalinfo
{
    sox_rate_t rate;       /* sampling rate */
    int size;             /* compressed or uncompressed datum size */
    sox_encoding_t encoding; /* format of sample numbers */
    unsigned channels;    /* number of sound channels */
    double compression;   /* compression factor (where applicable) */

    /* There is a delineation between these vars being tri-state and
     * effectively boolean.  Logically the line falls between setting
     * them up (could be done in libSoX, or by the libSoX client) and
     * using them (in libSoX).  libSoX's logic to set them up includes
     * knowledge of the machine default and the format default.  (The
     * sox client logic adds to this a layer of overridability via user
     * options.)  The physical delineation is in the somewhat
     * snappily-named libSoX function `set_endianness_if_not_already_set'
     * which is called at the right times (as files are openned) by the
     * libSoX core, not by the file handlers themselves.  The file handlers
     * indicate to the libSoX core if they have a preference using
     * SOX_FILE_xxx flags.
     */
    sox_option_t reverse_bytes;    /* endiannesses... */
    sox_option_t reverse_nibbles;
    sox_option_t reverse_bits;
} sox_signalinfo_t;

/* Loop parameters */

typedef struct  sox_loopinfo
{
    sox_size_t    start;          /* first sample */
    sox_size_t    length;         /* length */
    unsigned int  count;          /* number of repeats, 0=forever */
    unsigned char type;           /* 0=no, 1=forward, 2=forward/back */
} sox_loopinfo_t;

/* Instrument parameters */

/* vague attempt at generic information for sampler-specific info */

typedef struct  sox_instrinfo
{
    char MIDInote;       /* for unity pitch playback */
    char MIDIlow, MIDIhi;/* MIDI pitch-bend range */
    char loopmode;       /* semantics of loop data */
    unsigned nloops;     /* number of active loops (max SOX_MAX_NLOOPS) */
} sox_instrinfo_t;

/* Loop modes, upper 4 bits mask the loop blass, lower 4 bits describe */
/* the loop behaviour, ie. single shot, bidirectional etc. */
#define SOX_LOOP_NONE          0
#define SOX_LOOP_8             32 /* 8 loops: don't know ?? */
#define SOX_LOOP_SUSTAIN_DECAY 64 /* AIFF style: one sustain & one decay loop */

/*
 * File buffer info.  Holds info so that data can be read in blocks.
 */

typedef struct sox_fileinfo
{
    char          *buf;                 /* Pointer to data buffer */
    size_t        size;                 /* Size of buffer */
    size_t        count;                /* Count read in to buffer */
    size_t        pos;                  /* Position in buffer */
} sox_fileinfo_t;


/*
 * Handler structure for each format.
 */

typedef struct sox_format sox_format_t;

typedef struct {
    const char   * const *names;
    unsigned int flags;
    int          (*startread)(sox_format_t * ft);
    sox_size_t   (*read)(sox_format_t * ft, sox_sample_t *buf, sox_size_t len);
    int          (*stopread)(sox_format_t * ft);
    int          (*startwrite)(sox_format_t * ft);
    sox_size_t   (*write)(sox_format_t * ft, const sox_sample_t *buf, sox_size_t len);
    int          (*stopwrite)(sox_format_t * ft);
    int          (*seek)(sox_format_t * ft, sox_size_t offset);
} sox_format_handler_t;

/*
 *  Format information for input and output files.
 */

#define SOX_MAX_FILE_PRIVSIZE    1000
#define SOX_MAX_NLOOPS           8

struct sox_format {
  /* Placing priv at the start of this structure ensures that it gets aligned
   * in memory in the optimal way for any structure to be cast over it. */
  char   priv[SOX_MAX_FILE_PRIVSIZE];    /* format's private data area */

  sox_signalinfo_t signal;               /* signal specifications */
  sox_instrinfo_t  instr;                /* instrument specification */
  sox_loopinfo_t   loops[SOX_MAX_NLOOPS];/* Looping specification */
  sox_bool         seekable;             /* can seek on this file */
  char             mode;                 /* read or write mode */
  sox_size_t       length;               /* frames in file, or 0 if unknown. */
  sox_size_t       clips;                /* increment if clipping occurs */
  char             *filename;            /* file name */
  char             *filetype;            /* type of file */
  char             *comment;             /* comment string */
  FILE             *fp;                  /* File stream pointer */
  int              sox_errno;            /* Failure error codes */
  char             sox_errstr[256];      /* Extend Failure text */
  const sox_format_handler_t * handler;  /* format struct for this file */
};

/* file flags field */
#define SOX_FILE_LOOPS   1  /* does file format support loops? */
#define SOX_FILE_INSTR   2  /* does file format support instrument specs? */
#define SOX_FILE_SEEK    4  /* does file format support seeking? */
#define SOX_FILE_NOSTDIO 8  /* does not use stdio routines */
#define SOX_FILE_DEVICE  16 /* file is an audio device */
#define SOX_FILE_PHONY   32 /* phony file/device */
/* These two for use by the libSoX core or libSoX clients: */
#define SOX_FILE_ENDIAN  64 /* is file format endian? */
#define SOX_FILE_ENDBIG  128/* if so, is it big endian? */
/* These two for use by libSoX handlers: */
#define SOX_FILE_LIT_END  (0   + 64)
#define SOX_FILE_BIG_END  (128 + 64)
#define SOX_FILE_BIT_REV 256
#define SOX_FILE_NIB_REV 512

/* Size field */
#define SOX_SIZE_BYTE    1
#define SOX_SIZE_8BIT    1
#define SOX_SIZE_16BIT   2
#define SOX_SIZE_24BIT   3
#define SOX_SIZE_32BIT   4
#define SOX_SIZE_64BIT   8
#define SOX_INFO_SIZE_MAX     8

/* declared in misc.c */
extern const char * const sox_sizes_str[];
extern const char * const sox_size_bits_str[];
extern const char * const sox_encodings_str[];

int sox_format_init(void);
sox_format_t * sox_open_read(const char *path, const sox_signalinfo_t *info, 
                         const char *filetype);
sox_format_t * sox_open_write(
    sox_bool (*overwrite_permitted)(const char *filename),
    const char *path,
    const sox_signalinfo_t *info,
    const char *filetype,
    const char *comment,
    sox_size_t length,
    const sox_instrinfo_t *instr,
    const sox_loopinfo_t *loops);
sox_size_t sox_read(sox_format_t * ft, sox_sample_t *buf, sox_size_t len);
sox_size_t sox_write(sox_format_t * ft, const sox_sample_t *buf, sox_size_t len);
int sox_close(sox_format_t * ft);

#define SOX_SEEK_SET 0
int sox_seek(sox_format_t * ft, sox_size_t offset, int whence);

sox_format_handler_t const * sox_find_format(char const * name, sox_bool no_dev);
int sox_gettype(sox_format_t *, sox_bool);
void sox_format_quit(void);

/*
 * Structures for effects.
 */

#define SOX_MAX_EFFECT_PRIVSIZE (2 * SOX_MAX_FILE_PRIVSIZE)

#define SOX_EFF_CHAN     1           /* Effect can alter # of channels */
#define SOX_EFF_RATE     2           /* Effect can alter sample rate */
#define SOX_EFF_PREC     4           /* Effect can alter sample precision */
#define SOX_EFF_LENGTH   8           /* Effect can alter audio length */
#define SOX_EFF_MCHAN    16          /* Effect can handle multi-channel */
#define SOX_EFF_NULL     32          /* Effect does nothing */
#define SOX_EFF_DEPRECATED 64        /* Effect is living on borrowed time */

typedef struct sox_effect sox_effect_t;
typedef struct sox_effects_globals sox_effects_globals_t;

typedef struct {
  char const * name;
  char const * usage;
  unsigned int flags;

  int (*getopts)(sox_effect_t * effp, int argc, char *argv[]);
  int (*start)(sox_effect_t * effp);
  int (*flow)(sox_effect_t * effp, const sox_sample_t *ibuf,
      sox_sample_t *obuf, sox_size_t *isamp, sox_size_t *osamp);
  int (*drain)(sox_effect_t * effp, sox_sample_t *obuf, sox_size_t *osamp);
  int (*stop)(sox_effect_t * effp);
  int (*kill)(sox_effect_t * effp);
} sox_effect_handler_t;

struct sox_effect {
  /* Placing priv at the start of this structure ensures that it gets aligned
   * in memory in the optimal way for any structure to be cast over it. */
  char priv[SOX_MAX_EFFECT_PRIVSIZE];    /* private area for effect */

  sox_effects_globals_t    *global_info; /* global parameters */
  struct sox_signalinfo    ininfo;       /* input signal specifications */
  struct sox_signalinfo    outinfo;      /* output signal specifications */
  sox_effect_handler_t     handler;
  sox_sample_t            *obuf;        /* output buffer */
  sox_size_t               obeg, oend;   /* consumed, total length */
  sox_size_t               imin;         /* minimum input buffer size */
  sox_size_t               clips;        /* increment if clipping occurs */
  sox_size_t               flows;        /* 1 if MCHAN, # chans otherwise */
  sox_size_t               flow;         /* flow # */
};

sox_effect_handler_t const *sox_find_effect(char const * name);
void sox_create_effect(sox_effect_t * effp, sox_effect_handler_t const *e);

/* Effects chain */

int sox_effect_set_imin(sox_effect_t * effp, sox_size_t imin);

struct sox_effects_chain;
typedef struct sox_effects_chain sox_effects_chain_t;
sox_effects_chain_t * sox_create_effects_chain(void);

int sox_add_effect(sox_effects_chain_t *, sox_effect_t * effp, sox_signalinfo_t * in, sox_signalinfo_t const * out);
int sox_flow_effects(sox_effects_chain_t *, int (* callback)(sox_bool all_done));
sox_size_t sox_effects_clips(sox_effects_chain_t *);
sox_size_t sox_stop_effect(sox_effects_chain_t *, sox_size_t e);
void sox_delete_effects(sox_effects_chain_t *);

char const * sox_parsesamples(sox_rate_t rate, const char *str, sox_size_t *samples, int def);

/* The following routines are unique to the trim effect.
 * sox_trim_get_start can be used to find what is the start
 * of the trim operation as specified by the user.
 * sox_trim_clear_start will reset what ever the user specified
 * back to 0.
 * These two can be used together to find out what the user
 * wants to trim and use a sox_seek() operation instead.  After
 * sox_seek()'ing, you should set the trim option to 0.
 */
sox_size_t sox_trim_get_start(sox_effect_t * effp);
void sox_trim_clear_start(sox_effect_t * effp);

#endif
