#include "sox_i.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_IO_H
  #include <io.h>
#endif

#ifdef HAVE_LIBLTDL
  #include <ltdl.h>
#endif

static void output_message(unsigned level, const char *filename, const char *fmt, va_list ap);

sox_globals_t sox_globals = {2, 8192, NULL, NULL, output_message, NULL, sox_false};

static void output_message(unsigned level, const char *filename, const char *fmt, va_list ap)
{
  if (sox_globals.verbosity >= level) {
    sox_output_message(stderr, filename, fmt, ap);
    fprintf(stderr, "\n");
  }
}

/* Plugins */

#ifdef HAVE_LIBLTDL
static sox_bool plugins_initted = sox_false;
#endif

/* FIXME: Use vasprintf */
#ifdef HAVE_LIBLTDL
#define MAX_NAME_LEN 1024
static int init_format(const char *file, lt_ptr data)
{
  lt_dlhandle lth = lt_dlopenext(file);
  const char *end = file + strlen(file);
  const char prefix[] = "libsox_fmt_";
  char fnname[MAX_NAME_LEN];
  char *start = strstr(file, prefix);

  (void)data;
  if (start && (start += sizeof(prefix) - 1) < end) {
    int ret = snprintf(fnname, MAX_NAME_LEN, "sox_%.*s_format_fn", end - start, start);
    if (ret > 0 && ret < MAX_NAME_LEN) {
      union {sox_format_fn_t fn; lt_ptr ptr;} ltptr;
      ltptr.ptr = lt_dlsym(lth, fnname);
      sox_format_fns[sox_formats].fn = ltptr.fn;
      sox_debug("opening format plugin `%s': library %p, entry point %p\n", fnname, (void *)lth, ltptr.ptr);
      if (sox_format_fns[sox_formats].fn)
        sox_formats++;
    }
  }

  return 0;
}
#endif

/*
 * Initialize list of known format handlers.
 */
int sox_format_init(void)
{
#ifdef HAVE_LIBLTDL
  int ret;

  if ((ret = lt_dlinit()) != 0) {
    sox_fail("lt_dlinit failed with %d error(s): %s", ret, lt_dlerror());
    return SOX_EOF;
  }
  plugins_initted = sox_true;

  lt_dlforeachfile(PKGLIBDIR, init_format, NULL);
#endif
  return SOX_SUCCESS;
}

/*
 * Check that we have a known format suffix string.
 */
int sox_gettype(sox_format_t * ft, sox_bool is_file_extension)
{
  sox_format_handler_t const * handler;

  if (!ft->filetype) {
    sox_fail_errno(ft, SOX_EFMT, "unknown file type");
    return SOX_EFMT;
  }
  handler = sox_find_format(ft->filetype, is_file_extension);
  if (!handler) {
    sox_fail_errno(ft, SOX_EFMT, "unknown file type `%s'", ft->filetype);
    return SOX_EFMT;
  }
  ft->handler = *handler;
  if (ft->mode == 'r' && !ft->handler.startread && !ft->handler.read) {
    sox_fail_errno(ft, SOX_EFMT, "file type `%s' isn't readable", ft->filetype);
    return SOX_EFMT;
  }
  if (ft->mode == 'w' && !ft->handler.startwrite && !ft->handler.write) {
    sox_fail_errno(ft, SOX_EFMT, "file type `%s' isn't writable", ft->filetype);
    return SOX_EFMT;
  }
  return SOX_SUCCESS;
}

/*
 * Cleanup things.
 */
void sox_format_quit(void)
{
#ifdef HAVE_LIBLTDL
  {
    int ret;
    if (plugins_initted && (ret = lt_dlexit()) != 0) {
      sox_fail("lt_dlexit failed with %d error(s): %s", ret, lt_dlerror());
    }
  }
#endif
}

void set_signal_defaults(sox_signalinfo_t * signal)
{
  if (!signal->rate    ) signal->rate     = SOX_DEFAULT_RATE;
  if (!signal->precision    ) signal->precision     = SOX_DEFAULT_PRECISION;
  if (!signal->channels) signal->channels = SOX_DEFAULT_CHANNELS;
}

void set_endianness_if_not_already_set(sox_format_t * ft)
{
  if (ft->encoding.reverse_bytes == SOX_OPTION_DEFAULT) {
    if (ft->handler.flags & SOX_FILE_ENDIAN)
    {
        /* Set revere_bytes if we are running on opposite endian
         * machine compared to file format.
         */
        if (ft->handler.flags & SOX_FILE_ENDBIG)
            ft->encoding.reverse_bytes = SOX_IS_LITTLEENDIAN;
        else
            ft->encoding.reverse_bytes = SOX_IS_BIGENDIAN;
    }
    else
      ft->encoding.reverse_bytes = SOX_OPTION_NO;
  }
  if (ft->encoding.reverse_bits == SOX_OPTION_DEFAULT)
    ft->encoding.reverse_bits = !!(ft->handler.flags & SOX_FILE_BIT_REV);
  else if (ft->encoding.reverse_bits != !!(ft->handler.flags & SOX_FILE_BIT_REV))
      sox_report("'%s': Format options overriding file-type bit-order", ft->filename);

  if (ft->encoding.reverse_nibbles == SOX_OPTION_DEFAULT)
    ft->encoding.reverse_nibbles = !!(ft->handler.flags & SOX_FILE_NIB_REV);
  else if (ft->encoding.reverse_nibbles != !!(ft->handler.flags & SOX_FILE_NIB_REV))
      sox_report("'%s': Format options overriding file-type nibble-order", ft->filename);
}

static int is_seekable(sox_format_t * ft)
{
        struct stat st;

        fstat(fileno(ft->fp), &st);

        return ((st.st_mode & S_IFMT) == S_IFREG);
}

/* check that all settings have been given */
static int sox_checkformat(sox_format_t * ft)
{
  ft->sox_errno = SOX_SUCCESS;

  if (!ft->signal.rate) {
    sox_fail_errno(ft,SOX_EFMT,"sampling rate was not specified");
    return SOX_EOF;
  }
  if (!ft->signal.precision) {
    sox_fail_errno(ft,SOX_EFMT,"data encoding was not specified");
    return SOX_EOF;
  }
  return SOX_SUCCESS;
}

sox_format_t * sox_open_read(
    char               const * path,
    sox_signalinfo_t   const * signal,
    sox_encodinginfo_t const * encoding,
    char               const * filetype)
{
    sox_format_t * ft = xcalloc(sizeof(*ft), 1);

    ft->filename = xstrdup(path);

    /* Let auto type do the work if user is not overriding. */
    if (!filetype)
        ft->filetype = xstrdup("magic");
    else
        ft->filetype = xstrdup(filetype);

    ft->mode = 'r';
    if (sox_gettype(ft, sox_false) != SOX_SUCCESS) {
      sox_fail("Can't open input file `%s': %s", ft->filename, ft->sox_errstr);
      goto input_error;
    }
    if (signal)
      ft->signal = *signal;
    if (encoding)
      ft->encoding = *encoding;
    else sox_init_encodinginfo(&ft->encoding);

    if (!(ft->handler.flags & SOX_FILE_NOSTDIO))
    {
        /* Open file handler based on input name.  Used stdin file handler
         * if the filename is "-"
         */
        if (!strcmp(ft->filename, "-")) {
          if (sox_globals.stdin_in_use_by) {
            sox_fail("'-' (stdin) already in use by '%s'", sox_globals.stdin_in_use_by);
            goto input_error;
          }
          sox_globals.stdin_in_use_by = "audio input";
          SET_BINARY_MODE(stdin);
          ft->fp = stdin;
        }
        else if ((ft->fp = xfopen(ft->filename, "rb")) == NULL)
        {
            sox_fail("Can't open input file `%s': %s", ft->filename,
                    strerror(errno));
            goto input_error;
        }

        /* See if this file is seekable or not */
        ft->seekable = is_seekable(ft);
    }

    if (filetype)
      set_endianness_if_not_already_set(ft);

    /* Read and write starters can change their formats. */
    if (ft->handler.startread && (*ft->handler.startread)(ft) != SOX_SUCCESS)
    {
        sox_fail("Can't open input file `%s': %s", ft->filename, ft->sox_errstr);
        goto input_error;
    }

    if (!ft->signal.precision)
      ft->signal.precision = sox_precision(ft->encoding.encoding, ft->encoding.bits_per_sample);

    /* Go ahead and assume 1 channel audio if nothing is detected.
     * This is because libsox usually doesn't set this for mono file
     * formats (for historical reasons).
     */
    if (!(ft->handler.flags & SOX_FILE_PHONY) && !ft->signal.channels)
      ft->signal.channels = 1;

    if (sox_checkformat(ft) )
    {
        sox_fail("bad input format for file %s: %s", ft->filename,
                ft->sox_errstr);
        goto input_error;
    }
    return ft;

input_error:

    free(ft->filename);
    free(ft->filetype);
    free(ft);
    return NULL;
}

static void set_output_format(sox_format_t * ft)
{
  sox_encoding_t e;
  unsigned i, s;
  unsigned const * encodings = ft->handler.write_formats;
#define enc_arg(T) (T)encodings[i++]

  if (ft->handler.write_rates){
    if (!ft->signal.rate)
      ft->signal.rate = ft->handler.write_rates[0];
    else {
      sox_rate_t r;
      i = 0;
      while ((r = ft->handler.write_rates[i++])) {
        if (r == ft->signal.rate)
          break;
      }
      if (r != ft->signal.rate) {
        sox_rate_t given = ft->signal.rate, max = 0;
        ft->signal.rate = HUGE_VAL;
        i = 0;
        while ((r = ft->handler.write_rates[i++])) {
          if (r > given && r < ft->signal.rate)
            ft->signal.rate = r;
          else max = max(r, max);
        }
        if (ft->signal.rate == HUGE_VAL)
          ft->signal.rate = max;
        sox_warn("%s can't encode at %gHz; using %gHz", ft->handler.names[0], given, ft->signal.rate);
      }
    }
  }
  else if (!ft->signal.rate)
    ft->signal.rate = SOX_DEFAULT_RATE;

  if (ft->handler.flags & SOX_FILE_CHANS) {
    if (ft->signal.channels == 1 && !(ft->handler.flags & SOX_FILE_MONO)) {
      ft->signal.channels = (ft->handler.flags & SOX_FILE_STEREO)? 2 : 4;
      sox_warn("%s can't encode mono; setting channels to %u", ft->handler.names[0], ft->signal.channels);
    } else
    if (ft->signal.channels == 2 && !(ft->handler.flags & SOX_FILE_STEREO)) {
      ft->signal.channels = (ft->handler.flags & SOX_FILE_QUAD)? 4 : 1;
      sox_warn("%s can't encode stereo; setting channels to %u", ft->handler.names[0], ft->signal.channels);
    } else
    if (ft->signal.channels == 4 && !(ft->handler.flags & SOX_FILE_QUAD)) {
      ft->signal.channels = (ft->handler.flags & SOX_FILE_STEREO)? 2 : 1;
      sox_warn("%s can't encode quad; setting channels to %u", ft->handler.names[0], ft->signal.channels);
    }
  } else ft->signal.channels = max(ft->signal.channels, 1);

  if (!encodings)
    return;
  /* If an encoding has been given, check if it supported by this handler */
  if (ft->encoding.encoding) {
    i = 0;
    while ((e = enc_arg(sox_encoding_t))) {
      if (e == ft->encoding.encoding)
        break;
      while (enc_arg(unsigned));
    }
    if (e != ft->encoding.encoding) {
      sox_warn("%s can't encode %s", ft->handler.names[0], sox_encodings_str[ft->encoding.encoding]);
      ft->encoding.encoding = 0;
    }
    else {
      unsigned max_p = 0;
      unsigned max_p_s = 0;
      unsigned given_size = 0;
      sox_bool found = sox_false;
      if (ft->encoding.bits_per_sample)
        given_size = ft->encoding.bits_per_sample;
      ft->encoding.bits_per_sample = 65;
      while ((s = enc_arg(unsigned))) {
        if (s == given_size)
          found = sox_true;
        if (sox_precision(e, s) >= ft->signal.precision) {
          if (s < ft->encoding.bits_per_sample)
            ft->encoding.bits_per_sample = s;
        }
        else if (sox_precision(e, s) > max_p) {
          max_p = sox_precision(e, s);
          max_p_s = s;
        }
      }
      if (ft->encoding.bits_per_sample == 65)
        ft->encoding.bits_per_sample = max_p_s;
      if (given_size) {
        if (found)
          ft->encoding.bits_per_sample = given_size;
        else sox_warn("%s can't encode %s to %u-bit", ft->handler.names[0], sox_encodings_str[ft->encoding.encoding], given_size);
      }
    }
  }

  /* If a size has been given, check if it supported by this handler */
  if (!ft->encoding.encoding && ft->encoding.bits_per_sample) {
    i = 0;
    s= 0;
    while (s != ft->encoding.bits_per_sample && (e = enc_arg(sox_encoding_t)))
      while ((s = enc_arg(unsigned)) && s != ft->encoding.bits_per_sample);
    if (s != ft->encoding.bits_per_sample) {
      sox_warn("%s can't encode to %u-bit", ft->handler.names[0], ft->encoding.bits_per_sample);
      ft->encoding.bits_per_sample = 0;
    }
    else ft->encoding.encoding = e;
  }

  /* Find the smallest lossless encoding with precision >= signal.precision */
  if (!ft->encoding.encoding) {
    ft->encoding.bits_per_sample = 65;
    i = 0;
    while ((e = enc_arg(sox_encoding_t)))
      while ((s = enc_arg(unsigned)))
        if (e < SOX_ENCODING_LOSSLESS &&
            sox_precision(e, s) >= ft->signal.precision && s < ft->encoding.bits_per_sample) {
          ft->encoding.encoding = e;
          ft->encoding.bits_per_sample = s;
        }
  }

  /* Find the smallest lossy encoding with precision >= signal precision,
   * or, if none such, the highest precision encoding */
  if (!ft->encoding.encoding) {
    unsigned max_p = 0;
    sox_encoding_t max_p_e = 0;
    unsigned max_p_s = 0;
    i = 0;
    while ((e = enc_arg(sox_encoding_t)))
      do {
        s = enc_arg(unsigned);
        if (sox_precision(e, s) >= ft->signal.precision) {
          if (s < ft->encoding.bits_per_sample) {
            ft->encoding.encoding = e;
            ft->encoding.bits_per_sample = s;
          }
        }
        else if (sox_precision(e, s) > max_p) {
          max_p = sox_precision(e, s);
          max_p_e = e;
          max_p_s = s;
        }
      } while (s);
    if (!ft->encoding.encoding) {
      ft->encoding.encoding = max_p_e;
      ft->encoding.bits_per_sample = max_p_s;
    }
  }
  ft->signal.precision = min(ft->signal.precision, sox_precision(ft->encoding.encoding, ft->encoding.bits_per_sample));
  #undef enc_arg
}

sox_bool sox_format_supports_encoding(
    char               const * path,
    char               const * filetype,
    sox_encodinginfo_t const * encoding)
{
  #define enc_arg(T) (T)ft.handler.write_formats[i++]
  sox_encoding_t e;
  unsigned i = 0, s;
  sox_format_t ft;
  sox_bool no_filetype_given = filetype == NULL;

  assert(path);
  assert(encoding);
  ft.filetype = (char *)(filetype? filetype : find_file_extension(path));
  if (sox_gettype(&ft, no_filetype_given) != SOX_SUCCESS ||
      !ft.handler.write_formats)
    return sox_false;
  while ((e = enc_arg(sox_encoding_t))) {
    if (e == encoding->encoding) {
      while ((s = enc_arg(unsigned)))
        if (s == encoding->bits_per_sample)
          return sox_true;
      break;
    }
    while (enc_arg(unsigned));
  }
  return sox_false;
  #undef enc_arg
}

sox_format_t * sox_open_write(
    sox_bool (*overwrite_permitted)(const char *filename),
    char               const * path,
    sox_signalinfo_t   const * signal,
    sox_encodinginfo_t const * encoding,
    char               const * filetype,
    comments_t                 comments,
    sox_size_t                 length,
    sox_instrinfo_t    const * instr,
    sox_loopinfo_t     const * loops)
{
  sox_bool no_filetype_given = filetype == NULL;
  sox_format_t * ft = xcalloc(sizeof(*ft), 1);
  int i;

  if (!path || !signal) {
    free(ft);
    return NULL;
  }

  ft->filename = xstrdup(path);

  if (!filetype) {
    char const * extension = find_file_extension(ft->filename);
    if (extension)
      ft->filetype = xstrdup(extension);
  } else ft->filetype = xstrdup(filetype);

  ft->mode = 'w';
  if (sox_gettype(ft, no_filetype_given) != SOX_SUCCESS) {
    sox_fail("Can't open output file `%s': %s", ft->filename, ft->sox_errstr);
    goto output_error;
  }
  ft->signal = *signal;
  if (encoding)
    ft->encoding = *encoding;
  else sox_init_encodinginfo(&ft->encoding);

  if (!(ft->handler.flags & SOX_FILE_NOSTDIO))
  {
      /* Open file handler based on output name.  Used stdout file handler
       * if the filename is "-"
       */
      if (!strcmp(ft->filename, "-")) {
        if (sox_globals.stdout_in_use_by) {
          sox_fail("'-' (stdout) already in use by '%s'", sox_globals.stdout_in_use_by);
          goto output_error;
        }
        sox_globals.stdout_in_use_by = "audio output";
          SET_BINARY_MODE(stdout);
          ft->fp = stdout;
      }
      else {
        struct stat st;
        if (!stat(ft->filename, &st) && (st.st_mode & S_IFMT) == S_IFREG &&
            (overwrite_permitted && !overwrite_permitted(ft->filename))) {
          sox_fail("Permission to overwrite '%s' denied", ft->filename);
          goto output_error;
        }
        if ((ft->fp = fopen(ft->filename, "wb")) == NULL) {
          sox_fail("Can't open output file `%s': %s", ft->filename,
                  strerror(errno));
          goto output_error;
        }
      }

      /* stdout tends to be line-buffered.  Override this */
      /* to be Full Buffering. */
      if (setvbuf (ft->fp, NULL, _IOFBF, sizeof(char) * sox_globals.bufsiz))
      {
          sox_fail("Can't set write buffer");
          goto output_error;
      }

      /* See if this file is seekable or not */
      ft->seekable = is_seekable(ft);
  }

  ft->comments = copy_comments(comments);

  if (loops) for (i = 0; i < SOX_MAX_NLOOPS; i++)
    ft->loops[i] = loops[i];

  /* leave SMPTE # alone since it's absolute */
  if (instr)
    ft->instr = *instr;

  ft->length = length;
  set_endianness_if_not_already_set(ft);
  set_output_format(ft);

  /* FIXME: doesn't cover the situation where
   * codec changes audio length (e.g. 8svx, gsm): */
  if (signal->rate && signal->channels)
    ft->length = ft->length * ft->signal.rate / signal->rate *
      ft->signal.channels / signal->channels + .5;

  if ((ft->handler.flags & SOX_FILE_REWIND) && !ft->length && !ft->seekable)
    sox_warn("can't seek in output file `%s'; length in file header will be unspecified", ft->filename);

  /* Read and write starters can change their formats. */
  if (ft->handler.startwrite && (ft->handler.startwrite)(ft) != SOX_SUCCESS){
    sox_fail("can't open output file `%s': %s", ft->filename, ft->sox_errstr);
    goto output_error;
  }

  if (sox_checkformat(ft) == SOX_SUCCESS)
    return ft;
  sox_fail("bad format for output file `%s': %s", ft->filename, ft->sox_errstr);

output_error:
  free(ft->filename);
  free(ft->filetype);
  free(ft);
  return NULL;
}

sox_size_t sox_read(sox_format_t * ft, sox_sample_t * buf, sox_size_t len)
{
  sox_size_t actual = ft->handler.read? (*ft->handler.read)(ft, buf, len) : 0;
  return (actual > len? 0 : actual);
}

sox_size_t sox_write(sox_format_t * ft, const sox_sample_t *buf, sox_size_t len)
{
  ft->olength += len;
  return ft->handler.write? (*ft->handler.write)(ft, buf, len) : 0;
}

#define TWIDDLE_BYTE(ub, type) \
  do { \
    if (ft->encoding.reverse_bits) \
      ub = cswap[ub]; \
    if (ft->encoding.reverse_nibbles) \
      ub = ((ub & 15) << 4) | (ub >> 4); \
  } while (0);

#define TWIDDLE_WORD(uw, type) \
  if (ft->encoding.reverse_bytes) \
    uw = sox_swap ## type(uw);

#define TWIDDLE_FLOAT(f, type) \
  if (ft->encoding.reverse_bytes) \
    sox_swapf(&f);

/* N.B. This macro doesn't work for unaligned types (e.g. 3-byte
   types). */
#define READ_FUNC(type, size, ctype, twiddle) \
  sox_size_t sox_read_ ## type ## _buf( \
      sox_format_t * ft, ctype *buf, sox_size_t len) \
  { \
    sox_size_t n, nread; \
    if ((nread = sox_readbuf(ft, buf, len * size)) != len * size && sox_error(ft)) \
      sox_fail_errno(ft, errno, sox_readerr); \
    nread /= size; \
    for (n = 0; n < nread; n++) \
      twiddle(buf[n], type); \
    return nread; \
  }

/* Unpack a 3-byte value from a uint8_t * */
#define sox_unpack3(p) ((p)[0] | ((p)[1] << 8) | ((p)[2] << 16))

/* This (slower) macro works for unaligned types (e.g. 3-byte types)
   that need to be unpacked. */
#define READ_FUNC_UNPACK(type, size, ctype, twiddle) \
  sox_size_t sox_read_ ## type ## _buf( \
      sox_format_t * ft, ctype *buf, sox_size_t len) \
  { \
    sox_size_t n, nread; \
    uint8_t *data = xmalloc(size * len); \
    if ((nread = sox_readbuf(ft, data, len * size)) != len * size && sox_error(ft)) \
      sox_fail_errno(ft, errno, sox_readerr); \
    nread /= size; \
    for (n = 0; n < nread; n++) { \
      ctype datum = sox_unpack ## size(data + n * size); \
      twiddle(datum, type); \
      buf[n] = datum; \
    } \
    free(data); \
    return n; \
  }

READ_FUNC(b, 1, uint8_t, TWIDDLE_BYTE)
READ_FUNC(w, 2, uint16_t, TWIDDLE_WORD)
READ_FUNC_UNPACK(3, 3, uint24_t, TWIDDLE_WORD)
READ_FUNC(dw, 4, uint32_t, TWIDDLE_WORD)
READ_FUNC(f, sizeof(float), float, TWIDDLE_FLOAT)
READ_FUNC(df, sizeof(double), double, TWIDDLE_WORD)

/* N.B. This macro doesn't work for unaligned types (e.g. 3-byte
   types). */
#define WRITE_FUNC(type, size, ctype, twiddle) \
  sox_size_t sox_write_ ## type ## _buf( \
      sox_format_t * ft, ctype *buf, sox_size_t len) \
  { \
    sox_size_t n, nwritten; \
    for (n = 0; n < len; n++) \
      twiddle(buf[n], type); \
    if ((nwritten = sox_writebuf(ft, buf, len * size)) != len * size) \
      sox_fail_errno(ft, errno, sox_writerr); \
    return nwritten / size; \
  }

/* Pack a 3-byte value to a uint8_t * */
#define sox_pack3(p, v) \
  (p)[0] = v & 0xff; \
  (p)[1] = (v >> 8) & 0xff; \
  (p)[2] = (v >> 16) & 0xff;

/* This (slower) macro works for unaligned types (e.g. 3-byte types)
   that need to be packed. */
#define WRITE_FUNC_PACK(type, size, ctype, twiddle) \
  sox_size_t sox_write_ ## type ## _buf( \
      sox_format_t * ft, ctype *buf, sox_size_t len) \
  { \
    sox_size_t n, nwritten; \
    uint8_t *data = xmalloc(size * len); \
    for (n = 0; n < len; n++) { \
      ctype datum = buf[n]; \
      twiddle(datum, type); \
      sox_pack ## size(data + n * size, datum); \
    } \
    if ((nwritten = sox_writebuf(ft, data, len * size)) != len * size) \
      sox_fail_errno(ft, errno, sox_writerr); \
    free(data); \
    return nwritten / size; \
  }

WRITE_FUNC(b, 1, uint8_t, TWIDDLE_BYTE)
WRITE_FUNC(w, 2, uint16_t, TWIDDLE_WORD)
WRITE_FUNC_PACK(3, 3, uint24_t, TWIDDLE_WORD)
WRITE_FUNC(dw, 4, uint32_t, TWIDDLE_WORD)
WRITE_FUNC(f, sizeof(float), float, TWIDDLE_FLOAT)
WRITE_FUNC(df, sizeof(double), double, TWIDDLE_WORD)

#define WRITE1U_FUNC(type, ctype) \
  int sox_write ## type(sox_format_t * ft, unsigned d) \
  { ctype datum = (ctype)d; \
    return sox_write_ ## type ## _buf(ft, &datum, 1) == 1 ? SOX_SUCCESS : SOX_EOF; \
  }

#define WRITE1S_FUNC(type, ctype) \
  int sox_writes ## type(sox_format_t * ft, signed d) \
  { ctype datum = (ctype)d; \
    return sox_write_ ## type ## _buf(ft, &datum, 1) == 1 ? SOX_SUCCESS : SOX_EOF; \
  }

#define WRITE1_FUNC(type, ctype) \
  int sox_write ## type(sox_format_t * ft, ctype datum) \
  { \
    return sox_write_ ## type ## _buf(ft, &datum, 1) == 1 ? SOX_SUCCESS : SOX_EOF; \
  }

WRITE1U_FUNC(b, uint8_t)
WRITE1U_FUNC(w, uint16_t)
WRITE1U_FUNC(3, uint24_t)
WRITE1U_FUNC(dw, uint32_t)
WRITE1S_FUNC(b, uint8_t)
WRITE1S_FUNC(w, uint16_t)
WRITE1_FUNC(f, float)
WRITE1_FUNC(df, double)

/* N.B. The file (if any) may already have been deleted. */
int sox_close(sox_format_t * ft)
{
  int rc = SOX_SUCCESS;

  if (ft->mode == 'r')
    rc = ft->handler.stopread? (*ft->handler.stopread)(ft) : SOX_SUCCESS;
  else {
    if (ft->handler.flags & SOX_FILE_REWIND) {
      if (ft->olength != ft->length && ft->seekable) {
        rc = sox_seeki(ft, 0, 0);
        if (rc == SOX_SUCCESS)
          rc = ft->handler.stopwrite? (*ft->handler.stopwrite)(ft)
             : ft->handler.startwrite?(*ft->handler.startwrite)(ft) : SOX_SUCCESS;
      }
    }
    else rc = ft->handler.stopwrite? (*ft->handler.stopwrite)(ft) : SOX_SUCCESS;
  }

  if (!(ft->handler.flags & SOX_FILE_NOSTDIO))
    fclose(ft->fp);
  free(ft->filename);
  free(ft->filetype);
  delete_comments(&ft->comments);

  free(ft);
  return rc;
}

int sox_seek(sox_format_t * ft, sox_size_t offset, int whence)
{
    /* FIXME: Implement SOX_SEEK_CUR and SOX_SEEK_END. */
    if (whence != SOX_SEEK_SET)
        return SOX_EOF; /* FIXME: return SOX_EINVAL */

    /* If file is a seekable file and this handler supports seeking,
     * then invoke handler's function.
     */
    if (ft->seekable && ft->handler.seek)
      return (*ft->handler.seek)(ft, offset);
    return SOX_EOF; /* FIXME: return SOX_EBADF */
}

sox_bool sox_is_playlist(char const * filename)
{
  return strcaseends(filename, ".m3u") || strcaseends(filename, ".pls");
}

int sox_parse_playlist(sox_playlist_callback_t callback, void * p, char const * const listname)
{
  sox_bool const is_pls = strcaseends(listname, ".pls");
  int const comment_char = "#;"[is_pls];
  size_t text_length = 100;
  char * text = xmalloc(text_length + 1);
  char * dirname = xstrdup(listname);
  char * slash_pos = LAST_SLASH(dirname);
  FILE * file = xfopen(listname, "r");
  char * filename;
  int c, result = SOX_SUCCESS;

  if (!slash_pos)
    *dirname = '\0';
  else
    *slash_pos = '\0';

  if (file == NULL) {
    sox_fail("Can't open playlist file `%s': %s", listname, strerror(errno));
    result = SOX_EOF;
  }
  else do {
    size_t i = 0;
    size_t begin = 0, end = 0;

    while (isspace(c = getc(file)));
    if (c == EOF)
      break;
    while (c != EOF && !strchr("\r\n", c) && c != comment_char) {
      if (i == text_length)
        text = xrealloc(text, (text_length <<= 1) + 1);
      text[i++] = c;
      if (!strchr(" \t\f", c))
        end = i;
      c = getc(file);
    }
    if (ferror(file))
      break;
    if (c == comment_char) {
      do c = getc(file);
      while (c != EOF && !strchr("\r\n", c));
      if (ferror(file))
        break;
    }
    text[end] = '\0';
    if (is_pls) {
      char dummy;
      if (!strncasecmp(text, "file", 4) && sscanf(text + 4, "%*u=%c", &dummy) == 1)
        begin = strchr(text + 5, '=') - text + 1;
      else end = 0;
    }
    if (begin != end) {
      char const * id = text + begin;

      if (!dirname[0] || is_uri(id) || IS_ABSOLUTE(id))
        filename = xstrdup(id);
      else {
        filename = xmalloc(strlen(dirname) + strlen(id) + 2); 
        sprintf(filename, "%s/%s", dirname, id); 
      }
      if (sox_is_playlist(filename))
        sox_parse_playlist(callback, p, filename);
      else if (callback(p, filename))
        c = EOF;
      free(filename);
    }
  } while (c != EOF);

  if (ferror(file)) {
    sox_fail("Error reading playlist file `%s': %s", listname, strerror(errno));
    result = SOX_EOF;
  }
  if (file) fclose(file);
  free(text);
  free(dirname);
  return result;
}

