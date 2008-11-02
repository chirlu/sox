/* libSoX Library Public Interface
 *
 * Copyright 1999-2008 Chris Bagwell and SoX Contributors.
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Chris Bagwell And SoX Contributors are not responsible for
 * the consequences of using this software.
 */

#ifndef SOX_H
#define SOX_H

#include <limits.h>
#include <stdarg.h>
#include <stddef.h> /* Ensure NULL etc. are available throughout SoX */
#include <stdio.h>
#include <stdlib.h>
#include "soxstdint.h"

/* Avoid warnings about unused parameters. */
#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#define PRINTF __attribute__ ((format (printf, 1, 2)))
#else
#define UNUSED
#define PRINTF
#endif

/* The following is the API version of libSoX.  It is not meant
 * to follow the version number of SoX but it has historically.
 * Please do not count on these numbers being in sync.
 */
#define SOX_LIB_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#define SOX_LIB_VERSION_CODE SOX_LIB_VERSION(14, 2, 0)

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

typedef int32_t sox_sample_t;

/* Minimum and maximum values a sample can hold. */
#define SOX_SAMPLE_PRECISION 32
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



#include <stddef.h>
#define SOX_SIZE_MAX (size_t)(-sizeof(char))

typedef void (*sox_output_message_handler_t)(unsigned level, const char *filename, const char *fmt, va_list ap);

typedef struct { /* Global parameters (for effects & formats) */
/* public: */
  unsigned     verbosity;
  sox_output_message_handler_t output_message_handler;
  sox_bool     repeatable;
/* The following is used at times in libSoX when alloc()ing buffers
 * to perform file I/O.  It can be useful to pass in similar sized
 * data to get max performance.
 */
  size_t   bufsiz, input_bufsiz;

/* private: */
  char const * stdin_in_use_by;
  char const * stdout_in_use_by;
  char const * subsystem;
} sox_globals_t;
extern sox_globals_t sox_globals;

typedef double sox_rate_t;

typedef struct { /* Signal parameters; 0 if unknown */
  sox_rate_t       rate;         /* sampling rate */
  unsigned         channels;     /* number of sound channels */
  unsigned         precision;    /* in bits */
  size_t       length;       /* samples * chans in file; 0 if unknown */
} sox_signalinfo_t;

typedef enum {
  SOX_ENCODING_UNKNOWN   ,

  SOX_ENCODING_SIGN2     , /* signed linear 2's comp: Mac */
  SOX_ENCODING_UNSIGNED  , /* unsigned linear: Sound Blaster */
  SOX_ENCODING_FLOAT     , /* floating point (binary format) */
  SOX_ENCODING_FLOAT_TEXT, /* floating point (text format) */
  SOX_ENCODING_FLAC      , /* FLAC compression */
  SOX_ENCODING_HCOM      , /*  */
  SOX_ENCODING_WAVPACK   , /*  */
  SOX_ENCODING_WAVPACKF  , /*  */
  SOX_ENCODING_ULAW      , /* u-law signed logs: US telephony, SPARC */
  SOX_ENCODING_ALAW      , /* A-law signed logs: non-US telephony, Psion */
  SOX_ENCODING_G721      , /* G.721 4-bit ADPCM */
  SOX_ENCODING_G723      , /* G.723 3 or 5 bit ADPCM */
  SOX_ENCODING_CL_ADPCM  , /* Creative Labs 8 --> 2,3,4 bit Compressed PCM */
  SOX_ENCODING_CL_ADPCM16, /* Creative Labs 16 --> 4 bit Compressed PCM */
  SOX_ENCODING_MS_ADPCM  , /* Microsoft Compressed PCM */
  SOX_ENCODING_IMA_ADPCM , /* IMA Compressed PCM */
  SOX_ENCODING_OKI_ADPCM , /* Dialogic/OKI Compressed PCM */
  SOX_ENCODING_DPCM      , /*  */
  SOX_ENCODING_DWVW      , /*  */
  SOX_ENCODING_DWVWN     , /*  */
  SOX_ENCODING_GSM       , /* GSM 6.10 33byte frame lossy compression */
  SOX_ENCODING_MP3       , /* MP3 compression */
  SOX_ENCODING_VORBIS    , /* Vorbis compression */
  SOX_ENCODING_AMR_WB    , /* AMR-WB compression */
  SOX_ENCODING_AMR_NB    , /* AMR-NB compression */
  SOX_ENCODING_CVSD      , /*  */
  SOX_ENCODING_LPC10     , /*  */

  SOX_ENCODINGS            /* End of list marker */
} sox_encoding_t;

typedef struct {
  unsigned flags;
  #define SOX_LOSSY1 1     /* encode, decode, encode, decode: lossy once */
  #define SOX_LOSSY2 2     /* encode, decode, encode, decode: lossy twice */

  char const * name;
  char const * desc;
} sox_encodings_info_t;

extern sox_encodings_info_t const sox_encodings_info[];

typedef enum {SOX_OPTION_NO, SOX_OPTION_YES, SOX_OPTION_DEFAULT} sox_option_t;

typedef struct { /* Encoding parameters */
  sox_encoding_t encoding; /* format of sample numbers */
  unsigned bits_per_sample;  /* 0 if unknown or variable; uncompressed value if lossless; compressed value if lossy */
  double compression;      /* compression factor (where applicable) */

  /* If these 3 variables are set to DEFAULT, then, during
   * sox_open_read or sox_open_write, libSoX will set them to either
   * NO or YES according to the machine or format default. */
  sox_option_t reverse_bytes;    /* endiannesses... */
  sox_option_t reverse_nibbles;
  sox_option_t reverse_bits;

  sox_bool opposite_endian;
} sox_encodinginfo_t;

void sox_init_encodinginfo(sox_encodinginfo_t * e);
unsigned sox_precision(sox_encoding_t encoding, unsigned pcm_size);

/* Defaults for common hardware */
#define SOX_DEFAULT_CHANNELS  2
#define SOX_DEFAULT_RATE      48000
#define SOX_DEFAULT_PRECISION 16
#define SOX_DEFAULT_ENCODING  SOX_ENCODING_SIGN2

/* Loop parameters */

typedef struct {
  size_t    start;          /* first sample */
  size_t    length;         /* length */
  unsigned int  count;          /* number of repeats, 0=forever */
  unsigned char type;           /* 0=no, 1=forward, 2=forward/back */
} sox_loopinfo_t;

/* Instrument parameters */

/* vague attempt at generic information for sampler-specific info */

typedef struct {
  int8_t MIDInote;       /* for unity pitch playback */
  int8_t MIDIlow, MIDIhi;/* MIDI pitch-bend range */
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

typedef struct {
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
  unsigned     sox_lib_version_code; /* Checked on load; must be 1st in struct*/
  char         const * description;
  char         const * const * names;
  unsigned int flags;
  int          (*startread)(sox_format_t * ft);
  size_t   (*read)(sox_format_t * ft, sox_sample_t *buf, size_t len);
  int          (*stopread)(sox_format_t * ft);
  int          (*startwrite)(sox_format_t * ft);
  size_t   (*write)(sox_format_t * ft, const sox_sample_t *buf, size_t len);
  int          (*stopwrite)(sox_format_t * ft);
  int          (*seek)(sox_format_t * ft, uint64_t offset);
  unsigned     const * write_formats;
  sox_rate_t   const * write_rates;
  size_t       priv_size;
} sox_format_handler_t;

/*
 *  Format information for input and output files.
 */

typedef char * * sox_comments_t;

size_t sox_num_comments(sox_comments_t comments);
void sox_append_comment(sox_comments_t * comments, char const * comment);
void sox_append_comments(sox_comments_t * comments, char const * comment);
sox_comments_t sox_copy_comments(sox_comments_t comments);
void sox_delete_comments(sox_comments_t * comments);
char const * sox_find_comment(sox_comments_t comments, char const * id);

#define SOX_MAX_NLOOPS           8

typedef struct {
  /* Decoded: */
  sox_comments_t   comments;              /* Comment strings */
  sox_instrinfo_t  instr;                 /* Instrument specification */
  sox_loopinfo_t   loops[SOX_MAX_NLOOPS]; /* Looping specification */

  /* TBD: Non-decoded chunks, etc: */
} sox_oob_t;                              /* Out Of Band data */

typedef enum {lsx_io_file, lsx_io_pipe, lsx_io_url} lsx_io_type;

struct sox_format {
  char             * filename;      /* File name */
  sox_signalinfo_t signal;          /* Signal specifications */
  sox_encodinginfo_t encoding;      /* Encoding specifications */
  char             * filetype;      /* Type of file */
  sox_oob_t        oob;             /* Out Of Band data */
  sox_bool         seekable;        /* Can seek on this file */
  char             mode;            /* Read or write mode ('r' or 'w') */
  size_t       olength;         /* Samples * chans written to file */
  size_t       clips;           /* Incremented if clipping occurs */
  int              sox_errno;       /* Failure error code */
  char             sox_errstr[256]; /* Failure error text */
  FILE             * fp;            /* File stream pointer */
  lsx_io_type      io_type;
  long             tell_off;
  long             data_start;
  sox_format_handler_t handler;     /* Format handler for this file */
  void             * priv;          /* Format handler's private data area */
};

/* File flags field */
#define SOX_FILE_NOSTDIO 0x0001 /* Does not use stdio routines */
#define SOX_FILE_DEVICE  0x0002 /* File is an audio device */
#define SOX_FILE_PHONY   0x0004 /* Phony file/device */
#define SOX_FILE_REWIND  0x0008 /* File should be rewound to write header */
#define SOX_FILE_BIT_REV 0x0010 /* Is file bit-reversed? */
#define SOX_FILE_NIB_REV 0x0020 /* Is file nibble-reversed? */
#define SOX_FILE_ENDIAN  0x0040 /* Is file format endian? */
#define SOX_FILE_ENDBIG  0x0080 /* If so, is it big endian? */
#define SOX_FILE_MONO    0x0100 /* Do channel restrictions allow mono? */
#define SOX_FILE_STEREO  0x0200 /* Do channel restrictions allow stereo? */
#define SOX_FILE_QUAD    0x0400 /* Do channel restrictions allow quad? */

#define SOX_FILE_CHANS   (SOX_FILE_MONO | SOX_FILE_STEREO | SOX_FILE_QUAD)
#define SOX_FILE_LIT_END (SOX_FILE_ENDIAN | 0)
#define SOX_FILE_BIG_END (SOX_FILE_ENDIAN | SOX_FILE_ENDBIG)

int sox_format_init(void);

typedef const sox_format_handler_t *(*sox_format_fn_t)(void);

typedef struct {
  char *name;
  sox_format_fn_t fn;
} sox_format_tab_t;

extern sox_format_tab_t sox_format_fns[];

sox_format_t * sox_open_read(
    char               const * path,
    sox_signalinfo_t   const * signal,
    sox_encodinginfo_t const * encoding,
    char               const * filetype);
sox_bool sox_format_supports_encoding(
    char               const * path,
    char               const * filetype,
    sox_encodinginfo_t const * encoding);
sox_format_t * sox_open_write(
    char               const * path,
    sox_signalinfo_t   const * signal,
    sox_encodinginfo_t const * encoding,
    char               const * filetype,
    sox_oob_t          const * oob,
    sox_bool           (*overwrite_permitted)(const char *filename));
size_t sox_read(sox_format_t * ft, sox_sample_t *buf, size_t len);
size_t sox_write(sox_format_t * ft, const sox_sample_t *buf, size_t len);
int sox_close(sox_format_t * ft);

#define SOX_SEEK_SET 0
int sox_seek(sox_format_t * ft, uint64_t offset, int whence);

sox_format_handler_t const * sox_find_format(char const * name, sox_bool no_dev);
void sox_format_quit(void);

/*
 * Structures for effects.
 */

#define SOX_MAX_EFFECTS 20

#define SOX_EFF_CHAN     1           /* Effect can alter # of channels */
#define SOX_EFF_RATE     2           /* Effect can alter sample rate */
#define SOX_EFF_PREC     4           /* Effect can alter sample precision */
#define SOX_EFF_LENGTH   8           /* Effect can alter audio length */
#define SOX_EFF_MCHAN    16          /* Effect can handle multi-channel */
#define SOX_EFF_NULL     32          /* Effect does nothing */
#define SOX_EFF_DEPRECATED 64        /* Effect is living on borrowed time */
#define SOX_EFF_GETOPT   128         /* Effect uses getopt */

typedef enum {sox_plot_off, sox_plot_octave, sox_plot_gnuplot} sox_plot_t;
typedef struct sox_effect sox_effect_t;
struct sox_effects_globals { /* Global parameters (for effects) */
  sox_plot_t plot;         /* To help the user choose effect & options */
  sox_globals_t * global_info;
};
typedef struct sox_effects_globals sox_effects_globals_t;
extern sox_effects_globals_t sox_effects_globals;

typedef struct {
  char const * name;
  char const * usage;
  unsigned int flags;

  int (*getopts)(sox_effect_t * effp, int argc, char *argv[]);
  int (*start)(sox_effect_t * effp);
  int (*flow)(sox_effect_t * effp, const sox_sample_t *ibuf,
      sox_sample_t *obuf, size_t *isamp, size_t *osamp);
  int (*drain)(sox_effect_t * effp, sox_sample_t *obuf, size_t *osamp);
  int (*stop)(sox_effect_t * effp);
  int (*kill)(sox_effect_t * effp);
  size_t       priv_size;
} sox_effect_handler_t;

struct sox_effect {
  sox_effects_globals_t    * global_info; /* global parameters */
  sox_signalinfo_t         in_signal;
  sox_signalinfo_t         out_signal;
  sox_encodinginfo_t       const * in_encoding;
  sox_encodinginfo_t       const * out_encoding;
  sox_effect_handler_t     handler;
  sox_sample_t             * obuf;        /* output buffer */
  size_t               obeg, oend;    /* consumed, total length */
  size_t               imin;          /* minimum input buffer size */
  size_t               clips;         /* increment if clipping occurs */
  size_t               flows;         /* 1 if MCHAN, # chans otherwise */
  size_t               flow;          /* flow # */
  void                     * priv;        /* Effect's private data area */
};

sox_effect_handler_t const * sox_find_effect(char const * name);
sox_effect_t * sox_create_effect(sox_effect_handler_t const * eh);
int sox_effect_options(sox_effect_t *effp, int argc, char * const argv[]);

/* Effects chain */

typedef const sox_effect_handler_t *(*sox_effect_fn_t)(void);
extern sox_effect_fn_t sox_effect_fns[];

struct sox_effects_chain {
  sox_effect_t * effects[SOX_MAX_EFFECTS];
  unsigned length;
  sox_sample_t **ibufc, **obufc; /* Channel interleave buffers */
  sox_effects_globals_t global_info;
  sox_encodinginfo_t const * in_enc;
  sox_encodinginfo_t const * out_enc;
};
typedef struct sox_effects_chain sox_effects_chain_t;
sox_effects_chain_t * sox_create_effects_chain(
    sox_encodinginfo_t const * in_enc, sox_encodinginfo_t const * out_enc);
void sox_delete_effects_chain(sox_effects_chain_t *ecp);
int sox_add_effect( sox_effects_chain_t * chain, sox_effect_t * effp, sox_signalinfo_t * in, sox_signalinfo_t const * out);
int sox_flow_effects(sox_effects_chain_t *, int (* callback)(sox_bool all_done));
size_t sox_effects_clips(sox_effects_chain_t *);
size_t sox_stop_effect(sox_effect_t *effp);
void sox_push_effect_last(sox_effects_chain_t *chain, sox_effect_t *effp);
sox_effect_t *sox_pop_effect_last(sox_effects_chain_t *chain);
void sox_delete_effect(sox_effect_t *effp);
void sox_delete_effect_last(sox_effects_chain_t *chain);
void sox_delete_effects(sox_effects_chain_t *chain);

/* The following routines are unique to the trim effect.
 * sox_trim_get_start can be used to find what is the start
 * of the trim operation as specified by the user.
 * sox_trim_clear_start will reset what ever the user specified
 * back to 0.
 * These two can be used together to find out what the user
 * wants to trim and use a sox_seek() operation instead.  After
 * sox_seek()'ing, you should set the trim option to 0.
 */
size_t sox_trim_get_start(sox_effect_t * effp);
void sox_trim_clear_start(sox_effect_t * effp);

typedef int (* sox_playlist_callback_t)(void *, char *);
sox_bool sox_is_playlist(char const * filename);
int sox_parse_playlist(sox_playlist_callback_t callback, void * p, char const * const listname);

void sox_output_message(FILE *file, const char *filename, const char *fmt, va_list ap);

/* WARNING BEGIN
 *
 * The items in this section are subject to instability.  They only exist
 * in public API because sox (the application) make use of them but
 * may not be supported and may change rapidly.
 */
void lsx_fail(const char *, ...) PRINTF;
void lsx_warn(const char *, ...) PRINTF;
void lsx_report(const char *, ...) PRINTF;
void lsx_debug(const char *, ...) PRINTF;

#define lsx_fail       sox_globals.subsystem=__FILE__,lsx_fail
#define lsx_warn       sox_globals.subsystem=__FILE__,lsx_warn
#define lsx_report     sox_globals.subsystem=__FILE__,lsx_report
#define lsx_debug      sox_globals.subsystem=__FILE__,lsx_debug

typedef struct {char const *text; unsigned value;} lsx_enum_item;
#define LSX_ENUM_ITEM(prefix, item) {#item, prefix##item},

lsx_enum_item const * lsx_find_enum_text(char const * text, lsx_enum_item const * lsx_enum_items);
lsx_enum_item const * lsx_find_enum_value(unsigned value, lsx_enum_item const * lsx_enum_items);
int lsx_enum_option(int c, lsx_enum_item const * items);
char const * lsx_find_file_extension(char const * pathname);
char const * lsx_sigfigs3(size_t number);
char const * lsx_sigfigs3p(double percentage);

/* WARNING END */
#endif
