/* libSoX Library Public Interface
 *
 * Copyright 1999-2011 Chris Bagwell and SoX Contributors.
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

/* TODO: soxstdint.h can conflict with standard C headers -- needs work. */
#include "soxstdint.h"
typedef int32_t int24_t;   /* int24_t == int32_t (beware of the extra byte) */
typedef uint32_t uint24_t; /* uint24_t == uint32_t (beware of the extra byte) */

#if defined(__cplusplus)
extern "C" {
#endif

/* Avoid warnings about unused parameters. */
#ifdef __GNUC__
#define LSX_UNUSED __attribute__ ((unused))
#define LSX_PRINTF __attribute__ ((format (printf, 1, 2)))
#else
#define LSX_UNUSED
#define LSX_PRINTF
#endif
#ifdef _MSC_VER
#define LSX_UNUSED_VAR(x) ((void)(x))
#else
#define LSX_UNUSED_VAR(x) ((void)0)
#endif

/* The following is the API version of libSoX.  It is not meant
 * to follow the version number of SoX but it has historically.
 * Please do not count on these numbers being in sync. */
#define SOX_LIB_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#define SOX_LIB_VERSION_CODE   SOX_LIB_VERSION(14, 4, 0)

/* Returns version number */
const char *sox_version(void);

/* Flags indicating whether optional features are present in this build of SoX */
typedef enum sox_version_flags_t {
    sox_version_none = 0,
    sox_version_have_popen = 1,
    sox_version_have_magic = 2,
    sox_version_have_threads = 4,
    sox_version_have_memopen = 8
} sox_version_flags_t;

/* Information about this build of SoX */
typedef struct sox_version_info_t {
    size_t       size;         /* structure size = sizeof(sox_version_info_t) */
    sox_version_flags_t flags; /* feature flags = popen | magic | threads | memopen */
    uint32_t     version_code; /* version number = SOX_LIB_VERSION_CODE, i.e. 0x140400 */
    const char * version;      /* version string = sox_version(), i.e. "14.4.0" */
    const char * version_extra;/* version extra info or null = "PACKAGE_EXTRA", i.e. "beta" */
    const char * time;         /* build time = "__DATE__ __TIME__", i.e. "Jan  7 2010 03:31:50" */
    const char * distro;       /* distro or null = "DISTRO", i.e. "Debian" */
    const char * compiler;     /* compiler info or null, i.e. "msvc 160040219" */
    const char * arch;         /* arch, i.e. "1248 48 44 L OMP" */
    /* new info should be added at the end for version backwards-compatibility. */
} sox_version_info_t;

/* gets information about this build of libsox */
sox_version_info_t const * sox_version_info(void);

/* libSoX-specific error codes.  The rest directly map from errno. */
enum sox_error_t {
  SOX_SUCCESS = 0,     /* Function succeeded = 0 */
  SOX_EOF = -1,        /* End Of File or other error = -1 */
  SOX_EHDR = 2000,     /* Invalid Audio Header */
  SOX_EFMT,            /* Unsupported data format */
  SOX_ENOMEM,          /* Can't alloc memory */
  SOX_EPERM,           /* Operation not permitted */
  SOX_ENOTSUP,         /* Operation not supported */
  SOX_EINVAL           /* Invalid argument */
};

/* Boolean type, assignment (but not necessarily binary) compatible with C++ bool */
typedef enum sox_bool {
    sox_false,
    sox_true
} sox_bool;

#define SOX_INT_MIN(bits) (1 <<((bits)-1)) /* i.e. 0x80, 0x8000, 0x80000000 */
#define SOX_INT_MAX(bits) (((unsigned)-1)>>(33-(bits))) /* i.e. 0x7F, 0x7FFF, 0x7FFFFFFF */
#define SOX_UINT_MAX(bits) (SOX_INT_MIN(bits)|SOX_INT_MAX(bits)) /* i.e. 0xFF, 0xFFFF, 0xFFFFFFFF */

#define SOX_INT8_MAX  SOX_INT_MAX(8)  /* = 0x7F */
#define SOX_INT16_MAX SOX_INT_MAX(16) /* = 0x7FFF */
#define SOX_INT24_MAX SOX_INT_MAX(24) /* = 0x007FFFFF */
#define SOX_INT32_MAX SOX_INT_MAX(32) /* = 0x7FFFFFFF */

/* native SoX audio sample type */
typedef int32_t sox_sample_t;

/* Minimum and maximum values a sample can hold. */
#define SOX_SAMPLE_PRECISION 32                      /* bits in a sox_sample_t = 32 */
#define SOX_SAMPLE_MAX (sox_sample_t)SOX_INT_MAX(32) /* max value for sox_sample_t = 0x7FFFFFFF */
#define SOX_SAMPLE_MIN (sox_sample_t)SOX_INT_MIN(32) /* min value for sox_sample_t = 0x80000000 */


/*                Conversions: Linear PCM <--> sox_sample_t
 *
 *   I/O      Input    sox_sample_t Clips?   Input    sox_sample_t Clips?
 *  Format   Minimum     Minimum     I O    Maximum     Maximum     I O
 *  ------  ---------  ------------ -- --   --------  ------------ -- --
 *  Float     -inf         -1        y n      inf      1 - 5e-10    y n
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

#define SOX_SAMPLE_LOCALS sox_sample_t sox_macro_temp_sample LSX_UNUSED; \
  double sox_macro_temp_double LSX_UNUSED

#define SOX_SAMPLE_NEG SOX_INT_MIN(32) /* sign bit for sox_sample_t = 0x80000000 */
#define SOX_SAMPLE_TO_UNSIGNED(bits,d,clips) \
  (uint##bits##_t)(SOX_SAMPLE_TO_SIGNED(bits,d,clips)^SOX_INT_MIN(bits))
#define SOX_SAMPLE_TO_SIGNED(bits,d,clips) \
  (int##bits##_t)(LSX_UNUSED_VAR(sox_macro_temp_double),sox_macro_temp_sample=(d),sox_macro_temp_sample>SOX_SAMPLE_MAX-(1<<(31-bits))?++(clips),SOX_INT_MAX(bits):((uint32_t)(sox_macro_temp_sample+(1<<(31-bits))))>>(32-bits))
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
#define SOX_FLOAT_32BIT_TO_SAMPLE(d,clips) (sox_sample_t)(LSX_UNUSED_VAR(sox_macro_temp_sample),sox_macro_temp_double=(d)*(SOX_SAMPLE_MAX+1.),sox_macro_temp_double<SOX_SAMPLE_MIN?++(clips),SOX_SAMPLE_MIN:sox_macro_temp_double>=SOX_SAMPLE_MAX+1.?sox_macro_temp_double>SOX_SAMPLE_MAX+1.?++(clips),SOX_SAMPLE_MAX:SOX_SAMPLE_MAX:sox_macro_temp_double)
#define SOX_FLOAT_64BIT_TO_SAMPLE(d,clips) (sox_sample_t)(LSX_UNUSED_VAR(sox_macro_temp_sample),sox_macro_temp_double=(d)*(SOX_SAMPLE_MAX+1.),sox_macro_temp_double<0?sox_macro_temp_double<=SOX_SAMPLE_MIN-.5?++(clips),SOX_SAMPLE_MIN:sox_macro_temp_double-.5:sox_macro_temp_double>=SOX_SAMPLE_MAX+.5?sox_macro_temp_double>SOX_SAMPLE_MAX+1.?++(clips),SOX_SAMPLE_MAX:SOX_SAMPLE_MAX:sox_macro_temp_double+.5)
#define SOX_SAMPLE_TO_UNSIGNED_8BIT(d,clips) SOX_SAMPLE_TO_UNSIGNED(8,d,clips)
#define SOX_SAMPLE_TO_SIGNED_8BIT(d,clips) SOX_SAMPLE_TO_SIGNED(8,d,clips)
#define SOX_SAMPLE_TO_UNSIGNED_16BIT(d,clips) SOX_SAMPLE_TO_UNSIGNED(16,d,clips)
#define SOX_SAMPLE_TO_SIGNED_16BIT(d,clips) SOX_SAMPLE_TO_SIGNED(16,d,clips)
#define SOX_SAMPLE_TO_UNSIGNED_24BIT(d,clips) SOX_SAMPLE_TO_UNSIGNED(24,d,clips)
#define SOX_SAMPLE_TO_SIGNED_24BIT(d,clips) SOX_SAMPLE_TO_SIGNED(24,d,clips)
#define SOX_SAMPLE_TO_UNSIGNED_32BIT(d,clips) (uint32_t)((d)^SOX_SAMPLE_NEG)
#define SOX_SAMPLE_TO_SIGNED_32BIT(d,clips) (int32_t)(d)
#define SOX_SAMPLE_TO_FLOAT_32BIT(d,clips) (LSX_UNUSED_VAR(sox_macro_temp_double),sox_macro_temp_sample=(d),sox_macro_temp_sample>SOX_SAMPLE_MAX-128?++(clips),1:(((sox_macro_temp_sample+128)&~255)*(1./(SOX_SAMPLE_MAX+1.))))
#define SOX_SAMPLE_TO_FLOAT_64BIT(d,clips) ((d)*(1./(SOX_SAMPLE_MAX+1.)))


/* MACRO to clip a data type that is greater then sox_sample_t to
 * sox_sample_t's limits and increment a counter if clipping occurs.
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


#define SOX_SIZE_MAX ((size_t)(-1)) /* maximum value of size_t */

/* function-pointer type of globals.output_message_handler */
typedef void (*sox_output_message_handler_t)(
    unsigned level,
    const char *filename,
    const char *fmt, va_list ap);

/* Global parameters (for effects & formats) */
typedef struct sox_globals_t {
/* public: */
  unsigned     verbosity; /* messages are only written if globals.verbosity >= message.level */
  sox_output_message_handler_t output_message_handler; /* client-specified message output callback */
  sox_bool     repeatable; /* true to use pre-determined timestamps and PRNG seed */

/* The following is used at times in libSoX when alloc()ing buffers
 * to perform file I/O.  It can be useful to pass in similar sized
 * data to get max performance. */
  size_t       bufsiz;       /* default size (in bytes) used for blocks of sample data */
  size_t       input_bufsiz; /* default size (in bytes) used for blocks of input sample data */

  int32_t      ranqd1;       /* Can be used to re-seed libSoX's PRNG */

/* private: */
  char const * stdin_in_use_by;  /* tracks the name of the handler currently using stdin */
  char const * stdout_in_use_by; /* tracks the name of the handler currently using stdout */
  char const * subsystem;        /* tracks the name of the handler currently writing an output message */
  char       * tmp_path;         /* client-configured path to use for temporary files */
  sox_bool     use_magic;        /* true if client has requested use of 'magic' file-type detection */
  sox_bool     use_threads;      /* true if client has requested parallel effects processing */
} sox_globals_t;

/* the SoX global settings */
extern sox_globals_t sox_globals;

/* samples per second = double */
typedef double sox_rate_t;

#define SOX_UNSPEC 0 /* unknown value for signal parameter = 0 */
#define SOX_IGNORE_LENGTH (uint64_t)(-1) /* unspecified length for signal.length = -1 */

/* Signal parameters; SOX_UNSPEC (= 0) if unknown */
typedef struct sox_signalinfo_t {
  sox_rate_t       rate;         /* samples per second, 0 if unknown */
  unsigned         channels;     /* number of sound channels, 0 if unknown */
  unsigned         precision;    /* bits per sample, 0 if unknown */
  uint64_t         length;       /* samples * chans in file, 0 if unknown, -1 if unspecified */
  double           * mult;       /* Effects headroom multiplier; may be null */
} sox_signalinfo_t;

/* Format of sample data */
typedef enum sox_encoding_t {
  SOX_ENCODING_UNKNOWN   , /* encoding has not yet been determined */

  SOX_ENCODING_SIGN2     , /* signed linear 2's comp: Mac */
  SOX_ENCODING_UNSIGNED  , /* unsigned linear: Sound Blaster */
  SOX_ENCODING_FLOAT     , /* floating point (binary format) */
  SOX_ENCODING_FLOAT_TEXT, /* floating point (text format) */
  SOX_ENCODING_FLAC      , /* FLAC compression */
  SOX_ENCODING_HCOM      , /* Mac FSSD files with Huffman compression */
  SOX_ENCODING_WAVPACK   , /* WavPack with integer samples */
  SOX_ENCODING_WAVPACKF  , /* WavPack with float samples */
  SOX_ENCODING_ULAW      , /* u-law signed logs: US telephony, SPARC */
  SOX_ENCODING_ALAW      , /* A-law signed logs: non-US telephony, Psion */
  SOX_ENCODING_G721      , /* G.721 4-bit ADPCM */
  SOX_ENCODING_G723      , /* G.723 3 or 5 bit ADPCM */
  SOX_ENCODING_CL_ADPCM  , /* Creative Labs 8 --> 2,3,4 bit Compressed PCM */
  SOX_ENCODING_CL_ADPCM16, /* Creative Labs 16 --> 4 bit Compressed PCM */
  SOX_ENCODING_MS_ADPCM  , /* Microsoft Compressed PCM */
  SOX_ENCODING_IMA_ADPCM , /* IMA Compressed PCM */
  SOX_ENCODING_OKI_ADPCM , /* Dialogic/OKI Compressed PCM */
  SOX_ENCODING_DPCM      , /* Differential PCM: Fasttracker 2 (xi) */
  SOX_ENCODING_DWVW      , /* Delta Width Variable Word */
  SOX_ENCODING_DWVWN     , /* Delta Width Variable Word N-bit */
  SOX_ENCODING_GSM       , /* GSM 6.10 33byte frame lossy compression */
  SOX_ENCODING_MP3       , /* MP3 compression */
  SOX_ENCODING_VORBIS    , /* Vorbis compression */
  SOX_ENCODING_AMR_WB    , /* AMR-WB compression */
  SOX_ENCODING_AMR_NB    , /* AMR-NB compression */
  SOX_ENCODING_CVSD      , /* Continuously Variable Slope Delta modulation */
  SOX_ENCODING_LPC10     , /* Linear Predictive Coding */

  SOX_ENCODINGS            /* End of list marker */
} sox_encoding_t;

/* Flags for sox_encodings_info_t: lossless/lossy1/lossy2 */
typedef enum sox_encodings_flags_t {
  sox_encodings_none   = 0, /* no flags specified (implies lossless encoding) */
  sox_encodings_lossy1 = 1, /* encode, decode: lossy once */
  sox_encodings_lossy2 = 2  /* encode, decode, encode, decode: lossy twice */
} sox_encodings_flags_t;

#define SOX_LOSSY1 sox_encodings_lossy1 /* encode, decode: lossy once */
#define SOX_LOSSY2 sox_encodings_lossy2 /* encode, decode, encode, decode: lossy twice */

typedef struct sox_encodings_info_t {
  sox_encodings_flags_t flags; /* lossy once (lossy1), lossy twice (lossy2), or lossless (0) */
  char const * name;           /* encoding name */
  char const * desc;           /* encoding description */
} sox_encodings_info_t;

/* the list of available encodings */
extern sox_encodings_info_t const sox_encodings_info[];

/* yes, no, or default (auto-detect) */
typedef enum sox_option_t {
    SOX_OPTION_NO,
    SOX_OPTION_YES,
    SOX_OPTION_DEFAULT
} sox_option_t;

/* Encoding parameters */
typedef struct sox_encodinginfo_t {
  sox_encoding_t encoding; /* format of sample numbers */
  unsigned bits_per_sample;/* 0 if unknown or variable; uncompressed value if lossless; compressed value if lossy */
  double compression;      /* compression factor (where applicable) */

  /* If these 3 variables are set to SOX_OPTION_DEFAULT, then, during
   * sox_open_read or sox_open_write, libSoX will set them to either
   * NO or YES according to the machine or format default. */
  sox_option_t reverse_bytes;    /* endiannesses... */
  sox_option_t reverse_nibbles;
  sox_option_t reverse_bits;

  sox_bool opposite_endian;
} sox_encodinginfo_t;

/* fills in an encodinginfo with default values */
void sox_init_encodinginfo(sox_encodinginfo_t * e);

/* Given an encoding (i.e. SIGN2) and the encoded bits_per_sample (i.e. 16),
 * returns the number of useful bits per sample in the decoded data (i.e. 16).
 * Returns 0 to indicate that the value returned by the format handler should
 * be used instead of a pre-determined precision. */
unsigned sox_precision(
    sox_encoding_t encoding,
    unsigned bits_per_sample);

/* Defaults for common hardware */
#define SOX_DEFAULT_CHANNELS  2     /* = 2 (stereo) */
#define SOX_DEFAULT_RATE      48000 /* = 48000Hz */
#define SOX_DEFAULT_PRECISION 16    /* = 16 bits per sample */
#define SOX_DEFAULT_ENCODING  SOX_ENCODING_SIGN2 /* = SIGN2 (linear 2's complement PCM) */

/* Loop parameters */

/* Loop modes, upper 4 bits mask the loop blass, lower 4 bits describe */
/* the loop behaviour, ie. single shot, bidirectional etc. */
enum sox_loop_flags_t {
  sox_loop_none = 0,          /* single-shot = 0 */
  sox_loop_forward = 1,       /* forward loop = 1 */
  sox_loop_forward_back = 2,  /* forward/back loop = 2 */
  sox_loop_8 = 32,            /* 8 loops (??) = 32 */
  sox_loop_sustain_decay = 64 /* AIFF style, one sustain & one decay loop = 64 */
};
#define SOX_LOOP_NONE          ((uint8_t)sox_loop_none)          /* single-shot = 0 */
#define SOX_LOOP_8             ((uint8_t)sox_loop_8)             /* 8 loops (??) = 32 */
#define SOX_LOOP_SUSTAIN_DECAY ((uint8_t)sox_loop_sustain_decay) /* AIFF style, one sustain & one decay loop = 64 */

/* Looping parameters (out-of-band data) */
typedef struct sox_loopinfo_t {
  uint64_t  start;      /* first sample */
  uint64_t  length;     /* length */
  unsigned  count;      /* number of repeats, 0=forever */
  uint8_t   type;       /* 0=no, 1=forward, 2=forward/back (see sox_loop_... for valid values) */
} sox_loopinfo_t;

/* Instrument parameters */

/* vague attempt at generic information for sampler-specific info */

/* instrument information */
typedef struct sox_instrinfo_t{
  int8_t MIDInote;  /* for unity pitch playback */
  int8_t MIDIlow;   /* MIDI pitch-bend low range */
  int8_t MIDIhi;    /* MIDI pitch-bend high range */
  uint8_t loopmode; /* 0=no, 1=forward, 2=forward/back (see sox_loop_... values) */
  unsigned nloops;  /* number of active loops (max SOX_MAX_NLOOPS) */
} sox_instrinfo_t;

/* File buffer info.  Holds info so that data can be read in blocks. */
typedef struct sox_fileinfo_t {
  char          *buf;                 /* Pointer to data buffer */
  size_t        size;                 /* Size of buffer in bytes */
  size_t        count;                /* Count read into buffer */
  size_t        pos;                  /* Position in buffer */
} sox_fileinfo_t;


typedef struct sox_format_t sox_format_t; /* file format definition */

/* Handler structure defined by each format. */
typedef struct sox_format_handler_t {
  unsigned     sox_lib_version_code; /* Checked on load; must be 1st in struct*/
  char         const * description; /* short description of format */
  char         const * const * names; /* null-terminated array of filename extensions that are handled by this format */
  unsigned int flags; /* File flags (SOX_FILE_...) */
  int          (*startread)(sox_format_t * ft); /* called to initialize reader (decoder) */
  size_t   (*read)(sox_format_t * ft, sox_sample_t *buf, size_t len); /* called to read (decode) a block of samples */
  int          (*stopread)(sox_format_t * ft); /* called to close reader (decoder); may be null if no closing necessary */
  int          (*startwrite)(sox_format_t * ft); /* called to initialize writer (encoder) */
  size_t   (*write)(sox_format_t * ft, const sox_sample_t *buf, size_t len); /* called to write (encode) a block of samples */
  int          (*stopwrite)(sox_format_t * ft); /* called to close writer (decoder); may be null if no closing necessary */
  int          (*seek)(sox_format_t * ft, uint64_t offset); /* called to reposition reader; may be null if not supported */

  /* Array of values indicating the encodings and precisions supported for
   * writing (encoding). Precisions specified with default precision first.
   * Encoding, precision, precision, ..., 0, repeat. End with one more 0.
   * Example:
   * unsigned const* formats = {
   *   SOX_ENCODING_SIGN2, 16, 24, 0, // Support SIGN2 at 16 and 24 bits, default to 16 bits.
   *   SOX_ENCODING_UNSIGNED, 8, 0,   // Support UNSIGNED at 8 bits, default to 8 bits.
   *   0 // No more supported encodings.
   * };
   */
  unsigned     const * write_formats;

  /* Array of sample rates (samples per second) supported for writing (encoding).
   * NULL if all (or almost all) rates are supported. End with 0. */
  sox_rate_t   const * write_rates;

  /* SoX will automatically allocate a buffer in which the handler can store data.
   * Specify the size of the buffer needed here. Usually this will be sizeof(your_struct).
   * The buffer will be allocated and zeroed before the call to startread/startwrite.
   * The buffer will be freed after the call to stopread/stopwrite.
   * The buffer will be provided via format.priv in each call to the handler. */
  size_t       priv_size;
} sox_format_handler_t;

/*
 *  Format information for input and output files.
 */

/* File's metadata. Access via sox_..._comments functions. */
typedef char * * sox_comments_t;

size_t sox_num_comments(sox_comments_t comments); /* Returns the number of items in the metadata block. */
void sox_append_comment(sox_comments_t * comments, char const * comment); /* Adds a "id=value" item to the metadata block. */
void sox_append_comments(sox_comments_t * comments, char const * comment); /* Adds a newline-delimited list of "id=value" items to the metadata block. */
sox_comments_t sox_copy_comments(sox_comments_t comments); /* Duplicates the metadata block. */
void sox_delete_comments(sox_comments_t * comments); /* Frees the metadata block. */
char const * sox_find_comment(sox_comments_t comments, char const * id); /* If "id=value" is found, return value, else return null. */

#define SOX_MAX_NLOOPS           8

/* comments, instrument info, loop info (out-of-band data) */
typedef struct sox_oob_t{
  /* Decoded: */
  sox_comments_t   comments;              /* Comment strings in id=value format. */
  sox_instrinfo_t  instr;                 /* Instrument specification */
  sox_loopinfo_t   loops[SOX_MAX_NLOOPS]; /* Looping specification */

  /* TBD: Non-decoded chunks, etc: */
} sox_oob_t;

/* Is file a real file, a pipe, or a url? */
typedef enum lsx_io_type
{
    lsx_io_file,
    lsx_io_pipe,
    lsx_io_url
} lsx_io_type;

struct sox_format_t { /* Data passed to/from the format handler */
  char             * filename;      /* File name */
  
  /* Signal specifications for reader (decoder) or writer (encoder):
   * sample rate, #channels, precision, length, headroom multiplier.
   * Any info specified by the user is here on entry to startread or
   * startwrite. Info will be SOX_UNSPEC if the user provided no info.
   * At exit from startread, should be completely filled in, using
   * either data from the file's headers (if available) or whatever
   * the format is guessing/assuming (if header data is not available).
   * At exit from startwrite, should be completely filled in, using
   * either the data that was specified, or values chosen by the format
   * based on the format's defaults or capabilities. */
  sox_signalinfo_t signal;

  /* Encoding specifications for reader (decoder) or writer (encoder):
   * encoding (sample format), bits per sample, compression rate, endianness.
   * Should be filled in by startread. Values specified should be used
   * by startwrite when it is configuring the encoding parameters. */
  sox_encodinginfo_t encoding;

  char             * filetype;      /* Type of file, as determined by header inspection or libmagic. */
  sox_oob_t        oob;             /* comments, instrument info, loop info (out-of-band data) */
  sox_bool         seekable;        /* Can seek on this file */
  char             mode;            /* Read or write mode ('r' or 'w') */
  uint64_t         olength;         /* Samples * chans written to file */
  size_t           clips;           /* Incremented if clipping occurs */
  int              sox_errno;       /* Failure error code */
  char             sox_errstr[256]; /* Failure error text */
  FILE             * fp;            /* File stream pointer */
  lsx_io_type      io_type;         /* Stores whether this is a file, pipe or URL */
  uint64_t         tell_off;        /* Current offset within file */
  uint64_t         data_start;      /* Offset at which headers end and sound data begins (set by lsx_check_read_params) */
  sox_format_handler_t handler;     /* Format handler for this file */
  void             * priv;          /* Format handler's private data area */
};

/* File flags field */
#define SOX_FILE_NOSTDIO 0x0001 /* Does not use stdio routines */
#define SOX_FILE_DEVICE  0x0002 /* File is an audio device */
#define SOX_FILE_PHONY   0x0004 /* Phony file/device (i.e. nulfile) */
#define SOX_FILE_REWIND  0x0008 /* File should be rewound to write header */
#define SOX_FILE_BIT_REV 0x0010 /* Is file bit-reversed? */
#define SOX_FILE_NIB_REV 0x0020 /* Is file nibble-reversed? */
#define SOX_FILE_ENDIAN  0x0040 /* Is file format endian? */
#define SOX_FILE_ENDBIG  0x0080 /* For endian file format, is it big endian? */
#define SOX_FILE_MONO    0x0100 /* Do channel restrictions allow mono? */
#define SOX_FILE_STEREO  0x0200 /* Do channel restrictions allow stereo? */
#define SOX_FILE_QUAD    0x0400 /* Do channel restrictions allow quad? */

#define SOX_FILE_CHANS   (SOX_FILE_MONO | SOX_FILE_STEREO | SOX_FILE_QUAD) /* No channel restrictions */
#define SOX_FILE_LIT_END (SOX_FILE_ENDIAN | 0)                             /* File is little-endian */
#define SOX_FILE_BIG_END (SOX_FILE_ENDIAN | SOX_FILE_ENDBIG)               /* File is big-endian */

int sox_format_init(void);  /* Find & load format handler plugins. */
void sox_format_quit(void); /* Unload format handler plugins. */

int sox_init(void); /* Initialize effects library. */
int sox_quit(void); /* Close effects library and unload format handler plugins. */

/* callback to retrieve information about a format handler */
typedef const sox_format_handler_t *(*sox_format_fn_t)(void);

/* Information about a loaded format handler: name and function pointer */
typedef struct sox_format_tab_t {
  char *name;         /* Name of format handler */
  sox_format_fn_t fn; /* Function to call to get format handler's information */
} sox_format_tab_t;

/* the table of format handler names and functions */
extern sox_format_tab_t sox_format_fns[];

/* Opens a decoding session for a file. Returned handle must be closed with sox_close(). */
sox_format_t * sox_open_read(
    char               const * path,      /* Path to file to be opened (required). */
    sox_signalinfo_t   const * signal,    /* Information already known about audio stream, or NULL if none. */
    sox_encodinginfo_t const * encoding,  /* Information already known about sample encoding, or NULL if none. */
    char               const * filetype); /* Previously-determined file type, or NULL to auto-detect. */

/* Opens a decoding session for a memory buffer. Returned handle must be closed with sox_close(). */
sox_format_t * sox_open_mem_read(
    void                     * buffer,      /* Pointer to audio data buffer (required). */
    size_t                     buffer_size, /* Number of bytes to read from audio data buffer. */
    sox_signalinfo_t   const * signal,      /* Information already known about audio stream, or NULL if none. */
    sox_encodinginfo_t const * encoding,    /* Information already known about sample encoding, or NULL if none. */
    char               const * filetype);   /* Previously-determined file type, or NULL to auto-detect. */

/* Returns true if the format handler for the specified file type supports the specified encoding. */
sox_bool sox_format_supports_encoding(
    char               const * path,       /* Path to file to be examined (required if filetype is NULL). */
    char               const * filetype,   /* Previously-determined file type, or NULL to use extension from path. */
    sox_encodinginfo_t const * encoding);  /* Encoding for which format handler should be queried. */

/* Gets the format handler for a specified file type. */
sox_format_handler_t const * sox_write_handler(
    char               const * path,         /* Path to file (required if filetype is NULL). */
    char               const * filetype,     /* Filetype for which handler is needed, or NULL to use extension from path. */
    char               const * * filetype1); /* Receives the filetype that was detected. Pass NULL if not needed. */

/* Opens an encoding session for a file. Returned handle must be closed with sox_close(). */
sox_format_t * sox_open_write(
    char               const * path,     /* Path to file to be written (required). */
    sox_signalinfo_t   const * signal,   /* Information about desired audio stream (required). */
    sox_encodinginfo_t const * encoding, /* Information about desired sample encoding, or NULL to use defaults. */
    char               const * filetype, /* Previously-determined file type, or NULL to auto-detect. */
    sox_oob_t          const * oob,      /* Out-of-band data to add to file, or NULL if none. */
    sox_bool           (*overwrite_permitted)(const char *filename)); /* Called if file exists to determine whether overwrite is ok. */

/* Opens an encoding session for a memory buffer. Returned handle must be closed with sox_close(). */
sox_format_t * sox_open_mem_write(
    void                     * buffer,      /* Pointer to audio data buffer that will receive data (required). */
    size_t                     buffer_size, /* Maximum number of bytes to write to audio data buffer. */
    sox_signalinfo_t   const * signal,      /* Information about desired audio stream (required). */
    sox_encodinginfo_t const * encoding,    /* Information about desired sample encoding, or NULL to use defaults. */
    char               const * filetype,    /* Previously-determined file type, or NULL to auto-detect. */
    sox_oob_t          const * oob);        /* Out-of-band data to add to file, or NULL if none. */

/* Opens an encoding session for a memstream buffer. Returned handle must be closed with sox_close(). */
sox_format_t * sox_open_memstream_write(
    char                     * * buffer_ptr,    /* Receives pointer to audio data buffer that receives data (required). */
    size_t                   * buffer_size_ptr, /* Receives size of data written to audio data buffer (required). */
    sox_signalinfo_t   const * signal,          /* Information about desired audio stream (required). */
    sox_encodinginfo_t const * encoding,        /* Information about desired sample encoding, or NULL to use defaults. */
    char               const * filetype,        /* Previously-determined file type, or NULL to auto-detect. */
    sox_oob_t          const * oob);            /* Out-of-band data to add to file, or NULL if none. */

/* Reads samples from a decoding session into a sample buffer. Returns # of samples decoded, or 0 for EOF. */
size_t sox_read(sox_format_t * ft, sox_sample_t *buf, size_t len);

/* Writes samples to an encoding session from a sample buffer. Returns # of samples encoded. */
size_t sox_write(sox_format_t * ft, const sox_sample_t *buf, size_t len);

/* Closes an encoding or decoding session. */
int sox_close(sox_format_t * ft);

#define SOX_SEEK_SET 0

/* Sets the location at which next samples will be decoded. Returns SOX_SUCCESS if successful. */
int sox_seek(sox_format_t * ft, uint64_t offset, int whence);

/* Finds a format handler by name. */
sox_format_handler_t const * sox_find_format(char const * name, sox_bool ignore_devices);

/*
 * Structures for effects.
 */

#define SOX_MAX_EFFECTS 20

#define SOX_EFF_CHAN     1           /* Effect might alter the number of channels */
#define SOX_EFF_RATE     2           /* Effect might alter sample rate */
#define SOX_EFF_PREC     4           /* Effect might alter sample precision */
#define SOX_EFF_LENGTH   8           /* Effect might alter audio length */
#define SOX_EFF_MCHAN    16          /* Effect handles multiple channels internally */
#define SOX_EFF_NULL     32          /* Effect does nothing (can be optimized out of flow) */
#define SOX_EFF_DEPRECATED 64        /* Effect will soon be removed from SoX */
#define SOX_EFF_GAIN     128         /* Effect does not support gain -r */
#define SOX_EFF_MODIFY   256         /* Effect does not modify samples (just watches as data goes through) */
#define SOX_EFF_ALPHA    512         /* Effect is experimental/incomplete */
#define SOX_EFF_INTERNAL 1024        /* Effect present libSoX but not valid for use by SoX command-line tools */

typedef enum sox_plot_t {
    sox_plot_off,
    sox_plot_octave,
    sox_plot_gnuplot,
    sox_plot_data
} sox_plot_t;

typedef struct sox_effect_t sox_effect_t;

/* Global parameters for effects */
typedef struct sox_effects_globals_t {
  sox_plot_t plot;         /* To help the user choose effect & options */
  sox_globals_t * global_info; /* Pointer to associated SoX globals */
} sox_effects_globals_t;

/* Global parameters for effects */
extern sox_effects_globals_t sox_effects_globals;

/* Effect handler information */
typedef struct sox_effect_handler_t {
  char const * name;  /* Effect name */
  char const * usage; /* Short explanation of parameters accepted by effect */
  unsigned int flags; /* Combination of SOX_EFF_... flags */

  int (*getopts)(sox_effect_t * effp, int argc, char *argv[]); /* Called to parse command-line arguments (called once per effect) */
  int (*start)(sox_effect_t * effp);                           /* Called to initialize effect (called once per flow) */
  int (*flow)(sox_effect_t * effp, const sox_sample_t *ibuf,
      sox_sample_t *obuf, size_t *isamp, size_t *osamp);       /* Called to process samples */
  int (*drain)(sox_effect_t * effp, sox_sample_t *obuf, size_t *osamp); /* Called to finish getting output after input is complete */
  int (*stop)(sox_effect_t * effp);                            /* Called to shut down effect (called once per flow) */
  int (*kill)(sox_effect_t * effp);                            /* Called to shut down effect (called once per effect) */
  size_t       priv_size;                                      /* Size of private data SoX should pre-allocate for effect */
} sox_effect_handler_t;

/* Effect information */
struct sox_effect_t {
  sox_effects_globals_t    * global_info; /* global effect parameters */
  sox_signalinfo_t         in_signal;     /* Information about the incoming data stream */
  sox_signalinfo_t         out_signal;    /* Information about the outgoing data stream */
  sox_encodinginfo_t       const * in_encoding;  /* Information about the incoming data encoding */
  sox_encodinginfo_t       const * out_encoding; /* Information about the outgoing data encoding */
  sox_effect_handler_t     handler;   /* The handler for this effect */
  sox_sample_t             * obuf;    /* output buffer */
  size_t                   obeg;      /* output buffer consumed */
  size_t                   oend;      /* output buffer total length */
  size_t               imin;          /* minimum input buffer size */
  size_t               clips;         /* increment if clipping occurs */
  size_t               flows;         /* 1 if MCHAN, # chans otherwise */
  size_t               flow;          /* flow # */
  void                 * priv;        /* Effect's private data area */
};

/* Finds the effect handler with the given name */
sox_effect_handler_t const * sox_find_effect(char const * name);

/* Creates an effect using the given handler */
sox_effect_t * sox_create_effect(sox_effect_handler_t const * eh);

/* Applies the command-line options to the effect. Returns the number of arguments consumed. */
int sox_effect_options(sox_effect_t *effp, int argc, char * const argv[]);

/* Effects chain */

/* Function that returns information about an effect handler */
typedef const sox_effect_handler_t *(*sox_effect_fn_t)(void);

/* Array of known effect handlers */
extern sox_effect_fn_t sox_effect_fns[];

/* Chain of effects to be applied to a stream */
typedef struct sox_effects_chain_t {
  sox_effect_t * effects[SOX_MAX_EFFECTS]; /* Array of effects to be applied to a stream */
  unsigned length;                         /* Number of effects to be applied */
  sox_sample_t **ibufc;                    /* Channel interleave buffer */
  sox_sample_t **obufc;                    /* Channel interleave buffer */
  sox_effects_globals_t global_info;       /* Copy of global effects settings */
  sox_encodinginfo_t const * in_enc;       /* Input encoding */
  sox_encodinginfo_t const * out_enc;      /* Output encoding */
} sox_effects_chain_t;

/* Initializes an effects chain. Returned handle must be closed with sox_delete_effects_chain(). */
sox_effects_chain_t * sox_create_effects_chain(
    sox_encodinginfo_t const * in_enc, sox_encodinginfo_t const * out_enc);

/* Closes an effects chain. */
void sox_delete_effects_chain(sox_effects_chain_t *ecp);

/* Adds an effect to the effects chain, returns SOX_SUCCESS if successful. */
int sox_add_effect( sox_effects_chain_t * chain, sox_effect_t * effp, sox_signalinfo_t * in, sox_signalinfo_t const * out);

/* Runs the effects chain, returns SOX_SUCCESS if successful. */
int sox_flow_effects(sox_effects_chain_t *, int (* callback)(sox_bool all_done, void * client_data), void * client_data);

/* Gets the number of clips that occurred while running an effects chain */
size_t sox_effects_clips(sox_effects_chain_t *);

/* Shuts down an effect (calls stop on each of its flows) */
size_t sox_stop_effect(sox_effect_t *effp);

/* Adds an already-initialized effect to the end of the chain */
void sox_push_effect_last(sox_effects_chain_t *chain, sox_effect_t *effp);

/* Removes and returns an effect from the end of the chain */
sox_effect_t *sox_pop_effect_last(sox_effects_chain_t *chain);

/* Shut down and delete an effect */
void sox_delete_effect(sox_effect_t *effp);

/* Shut down and delete the last effect in the chain */
void sox_delete_effect_last(sox_effects_chain_t *chain);

/* Shut down and delete all effects in the chain */
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
uint64_t sox_trim_get_start(sox_effect_t * effp);
void sox_trim_clear_start(sox_effect_t * effp);
uint64_t sox_crop_get_start(sox_effect_t * effp);
void sox_crop_clear_start(sox_effect_t * effp);

typedef int (* sox_playlist_callback_t)(void *, char *);

/* Returns true if the specified file is a known playlist file type */
sox_bool sox_is_playlist(char const * filename);

/* Parses the specified playlist file */
int sox_parse_playlist(sox_playlist_callback_t callback, void * p, char const * const listname);

/* Converts a SoX error code into an error string. */
char const * sox_strerror(int sox_errno);

/* Writes an error message regarding the specified filename to the given output file stream */
void sox_output_message(FILE *file, const char *filename, const char *fmt, va_list ap);

/* WARNING BEGIN
 *
 * The items in this section are subject to instability.  They only exist
 * in public API because sox (the application) make use of them but
 * may not be supported and may change rapidly.
 */
void lsx_fail(const char *, ...) LSX_PRINTF;
void lsx_warn(const char *, ...) LSX_PRINTF;
void lsx_report(const char *, ...) LSX_PRINTF;
void lsx_debug(const char *, ...) LSX_PRINTF;

#define lsx_fail       sox_globals.subsystem=__FILE__,lsx_fail
#define lsx_warn       sox_globals.subsystem=__FILE__,lsx_warn
#define lsx_report     sox_globals.subsystem=__FILE__,lsx_report
#define lsx_debug      sox_globals.subsystem=__FILE__,lsx_debug

typedef struct lsx_enum_item {
    char const *text;
    unsigned value;
} lsx_enum_item;
#define LSX_ENUM_ITEM(prefix, item) {#item, prefix##item},

lsx_enum_item const * lsx_find_enum_text(char const * text, lsx_enum_item const * lsx_enum_items, unsigned flags);
#define LSX_FET_CASE 1
lsx_enum_item const * lsx_find_enum_value(unsigned value, lsx_enum_item const * lsx_enum_items);
int lsx_enum_option(int c, lsx_enum_item const * items);
sox_bool lsx_strends(char const * str, char const * end);
char const * lsx_find_file_extension(char const * pathname);
char const * lsx_sigfigs3(double number);
char const * lsx_sigfigs3p(double percentage);
void *lsx_realloc(void *ptr, size_t newsize);
int lsx_strcasecmp(const char * s1, const char * s2);
int lsx_strncasecmp(char const * s1, char const * s2, size_t n);

/* WARNING END */

#if defined(__cplusplus)
}
#endif

#endif /* SOX_H */
