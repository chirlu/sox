#include "sox_i.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_IO_H
  #include <io.h>
#endif


void set_endianness_if_not_already_set(ft_t ft)
{
  if (ft->signal.reverse_bytes == SOX_OPTION_DEFAULT) {
    if (ft->h->flags & SOX_FILE_ENDIAN)
      /* Set reverse_bytes for big-endian formats; on a big-endian
         system this cancels out with the reversal made for being on a
         big-endian system. */
      ft->signal.reverse_bytes = (ft->h->flags & SOX_FILE_ENDBIG) != 0;
    else
      ft->signal.reverse_bytes = SOX_OPTION_NO;
  }
  if (ft->signal.reverse_bits == SOX_OPTION_DEFAULT)
    ft->signal.reverse_bits = !!(ft->h->flags & SOX_FILE_BIT_REV);
  else if (ft->signal.reverse_bits != !!(ft->h->flags & SOX_FILE_BIT_REV))
      sox_report("'%s': Format options overriding file-type bit-order", ft->filename);

  if (ft->signal.reverse_nibbles == SOX_OPTION_DEFAULT)
    ft->signal.reverse_nibbles = !!(ft->h->flags & SOX_FILE_NIB_REV);
  else if (ft->signal.reverse_nibbles != !!(ft->h->flags & SOX_FILE_NIB_REV))
      sox_report("'%s': Format options overriding file-type nibble-order", ft->filename);
}

static int is_seekable(ft_t ft)
{
        struct stat st;

        fstat(fileno(ft->fp), &st);

        return ((st.st_mode & S_IFMT) == S_IFREG);
}

/* check that all settings have been given */
static int sox_checkformat(ft_t ft)
{

        ft->sox_errno = SOX_SUCCESS;

        if (ft->signal.rate == 0)
        {
                sox_fail_errno(ft,SOX_EFMT,"sampling rate was not specified");
                return SOX_EOF;
        }

        if (ft->signal.size == -1)
        {
                sox_fail_errno(ft,SOX_EFMT,"data size was not specified");
                return SOX_EOF;
        }

        if (ft->signal.encoding == SOX_ENCODING_UNKNOWN)
        {
                sox_fail_errno(ft,SOX_EFMT,"data encoding was not specified");
                return SOX_EOF;
        }

        if ((ft->signal.size <= 0) || (ft->signal.size > SOX_INFO_SIZE_MAX))
        {
                sox_fail_errno(ft,SOX_EFMT,"data size %d is invalid", ft->signal.size);
                return SOX_EOF;
        }

        if (ft->signal.encoding <= 0  || ft->signal.encoding >= SOX_ENCODINGS)
        {
                sox_fail_errno(ft,SOX_EFMT,"data encoding %d is invalid", ft->signal.encoding);
                return SOX_EOF;
        }

        return SOX_SUCCESS;
}

ft_t sox_open_read(const char *path, const sox_signalinfo_t *info,
                  const char *filetype)
{
    ft_t ft = (ft_t)xcalloc(sizeof(struct sox_soundstream), 1);

    ft->filename = xstrdup(path);

    /* Let auto type do the work if user is not overriding. */
    if (!filetype)
        ft->filetype = xstrdup("auto");
    else
        ft->filetype = xstrdup(filetype);

    if (sox_gettype(ft, sox_false) != SOX_SUCCESS) {
        sox_fail("Unknown input file format for `%s': %s",
                ft->filename,
                ft->sox_errstr);
        goto input_error;
    }

    ft->signal.size = -1;
    ft->signal.encoding = SOX_ENCODING_UNKNOWN;
    ft->signal.channels = 0;
    if (info)
        ft->signal = *info;
    ft->mode = 'r';

    if (!(ft->h->flags & SOX_FILE_NOSTDIO))
    {
        /* Open file handler based on input name.  Used stdin file handler
         * if the filename is "-"
         */
        if (!strcmp(ft->filename, "-")) {
          if (sox_global_info.stdin_in_use_by) {
            sox_fail("'-' (stdin) already in use by '%s'", sox_global_info.stdin_in_use_by);
            goto input_error;
          }
          sox_global_info.stdin_in_use_by = "audio input";
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
    if (ft->h->startread && (*ft->h->startread)(ft) != SOX_SUCCESS)
    {
        sox_fail("Failed reading `%s': %s", ft->filename, ft->sox_errstr);
        goto input_error;
    }

    /* Go ahead and assume 1 channel audio if nothing is detected.
     * This is because libsox usually doesn't set this for mono file
     * formats (for historical reasons).
     */
    if (!(ft->h->flags & SOX_FILE_PHONY) && !ft->signal.channels)
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

ft_t sox_open_write(
    sox_bool (*overwrite_permitted)(const char *filename),
    const char *path,
    const sox_signalinfo_t *info,
    const char *filetype,
    const char *comment,
    sox_size_t length,
    const sox_instrinfo_t *instr,
    const sox_loopinfo_t *loops)
{
    ft_t ft = (ft_t)xcalloc(sizeof(struct sox_soundstream), 1);
    int i;
    sox_bool no_filetype_given = filetype == NULL;

    ft->filename = xstrdup(path);

    if (!filetype) {
      char const * extension = find_file_extension(ft->filename);
      if (extension)
        ft->filetype = xstrdup(extension);
    } else
      ft->filetype = xstrdup(filetype);

    if (!ft->filetype || sox_gettype(ft, no_filetype_given) != SOX_SUCCESS)
    {
        sox_fail("Unknown output file format for '%s':  %s",
                ft->filename,
                ft->sox_errstr);
        goto output_error;
    }

    ft->signal.size = -1;
    ft->signal.encoding = SOX_ENCODING_UNKNOWN;
    ft->signal.channels = 0;
    if (info)
        ft->signal = *info;
    ft->mode = 'w';

    if (!(ft->h->flags & SOX_FILE_NOSTDIO))
    {
        /* Open file handler based on output name.  Used stdout file handler
         * if the filename is "-"
         */
        if (!strcmp(ft->filename, "-")) {
          if (sox_global_info.stdout_in_use_by) {
            sox_fail("'-' (stdout) already in use by '%s'", sox_global_info.stdout_in_use_by);
            goto output_error;
          }
          sox_global_info.stdout_in_use_by = "audio output";
            SET_BINARY_MODE(stdout);
            ft->fp = stdout;
        }
        else {
          struct stat st;
          if (!stat(ft->filename, &st) && (st.st_mode & S_IFMT) == S_IFREG &&
              !overwrite_permitted(ft->filename)) {
            sox_fail("Permission to overwrite '%s' denied", ft->filename);
            goto output_error;
          }
          if ((ft->fp = fopen(ft->filename, "wb")) == NULL) {
            sox_fail("Can't open output file '%s': %s", ft->filename,
                    strerror(errno));
            goto output_error;
          }
        }

        /* stdout tends to be line-buffered.  Override this */
        /* to be Full Buffering. */
        if (setvbuf (ft->fp, NULL, _IOFBF, sizeof(char)*sox_bufsiz))
        {
            sox_fail("Can't set write buffer");
            goto output_error;
        }

        /* See if this file is seekable or not */
        ft->seekable = is_seekable(ft);
    }

    ft->comment = xstrdup(comment);

    if (loops)
        for (i = 0; i < SOX_MAX_NLOOPS; i++)
            ft->loops[i] = loops[i];

    /* leave SMPTE # alone since it's absolute */
    if (instr)
        ft->instr = *instr;

    ft->length = length;
    set_endianness_if_not_already_set(ft);

    /* Read and write starters can change their formats. */
    if (ft->h->startwrite && (*ft->h->startwrite)(ft) != SOX_SUCCESS)
    {
        sox_fail("Failed writing %s: %s", ft->filename, ft->sox_errstr);
        goto output_error;
    }

    if (sox_checkformat(ft) )
    {
        sox_fail("bad output format for file %s: %s", ft->filename,
                ft->sox_errstr);
        goto output_error;
    }

    return ft;

output_error:

    free(ft->filename);
    free(ft->filetype);
    free(ft);
    return NULL;
}

sox_size_t sox_read(ft_t f, sox_ssample_t * buf, sox_size_t len)
{
  sox_size_t actual = f->h->read? (*f->h->read)(f, buf, len) : 0;
  return (actual > len? 0 : actual);
}

sox_size_t sox_write(ft_t ft, const sox_ssample_t *buf, sox_size_t len)
{
    return ft->h->write? (*ft->h->write)(ft, buf, len) : 0;
}

#define TWIDDLE_BYTE(ub, type) \
  do { \
    if (ft->signal.reverse_bits) \
      ub = cswap[ub]; \
    if (ft->signal.reverse_nibbles) \
      ub = ((ub & 15) << 4) | (ub >> 4); \
  } while (0);

#define TWIDDLE_WORD(uw, type) \
  if (ft->signal.reverse_bytes ^ SOX_IS_BIGENDIAN) \
    uw = sox_swap ## type(uw);

/* N.B. This macro doesn't work for unaligned types (e.g. 3-byte
   types). */
#define READ_FUNC(type, size, ctype, twiddle) \
  sox_size_t sox_read_ ## type ## _buf( \
      ft_t ft, ctype *buf, sox_size_t len) \
  { \
    sox_size_t n, nread; \
    if ((nread = sox_readbuf(ft, buf, len * size)) != len * size) \
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
      ft_t ft, ctype *buf, sox_size_t len) \
  { \
    sox_size_t n, nread; \
    uint8_t *data = xmalloc(size * len); \
    if ((nread = sox_readbuf(ft, data, len * size)) != len * size) \
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
READ_FUNC(f, sizeof(float), float, TWIDDLE_WORD)
READ_FUNC(df, sizeof(double), double, TWIDDLE_WORD)

/* N.B. This macro doesn't work for unaligned types (e.g. 3-byte
   types). */
#define WRITE_FUNC(type, size, ctype, twiddle) \
  sox_size_t sox_write_ ## type ## _buf( \
      ft_t ft, ctype *buf, sox_size_t len) \
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
      ft_t ft, ctype *buf, sox_size_t len) \
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
WRITE_FUNC(f, sizeof(float), float, TWIDDLE_WORD)
WRITE_FUNC(df, sizeof(double), double, TWIDDLE_WORD)

#define WRITE1_FUNC(type, sign, ctype) \
  int sox_write ## type(ft_t ft, ctype datum) \
  { \
    return sox_write_ ## type ## _buf(ft, &datum, 1) == 1 ? SOX_SUCCESS : SOX_EOF; \
  }

WRITE1_FUNC(b, u, uint8_t)
WRITE1_FUNC(w, u, uint16_t)
WRITE1_FUNC(3, u, uint24_t)
WRITE1_FUNC(dw, u, uint32_t)
WRITE1_FUNC(f, su, float)
WRITE1_FUNC(df, su, double)

/* N.B. The file (if any) may already have been deleted. */
int sox_close(ft_t ft)
{
    int rc;

    if (ft->mode == 'r')
        rc = ft->h->stopread? (*ft->h->stopread)(ft) : SOX_SUCCESS;
    else
        rc = ft->h->stopwrite? (*ft->h->stopwrite)(ft) : SOX_SUCCESS;

    if (!(ft->h->flags & SOX_FILE_NOSTDIO))
        fclose(ft->fp);
    free(ft->filename);
    free(ft->filetype);
    /* Currently, since startread() mallocs comments, stopread
     * is expected to also free it. */
    if (ft->mode == 'w')
        free(ft->comment);

    return rc;
}

int sox_seek(ft_t ft, sox_size_t offset, int whence)
{
    /* FIXME: Implement SOX_SEEK_CUR and SOX_SEEK_END. */
    if (whence != SOX_SEEK_SET)
        return SOX_EOF; /* FIXME: return SOX_EINVAL */

    /* If file is a seekable file and this handler supports seeking,
     * the invoke handlers function.
     */
    if (ft->seekable  && (ft->h->flags & SOX_FILE_SEEK))
        return ft->h->seek? (*ft->h->seek)(ft, offset) : SOX_EOF;
    else
        return SOX_EOF; /* FIXME: return SOX_EBADF */
}
