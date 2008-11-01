/* Implements the public API for using libSoX file formats.
 * All public functions & data are prefixed with sox_ .
 *
 * (c) 2005-8 Chris Bagwell and SoX contributors
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "sox_i.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_IO_H
  #include <io.h>
#endif

#if HAVE_MAGIC
  #include <magic.h>
#endif

static char const * detect_magic(sox_format_t * ft, char const * ext)
{
  char data[256];
  size_t len = lsx_readbuf(ft, data, sizeof(data));
  #define MAGIC(type, p2, l2, d2, p1, l1, d1) if (len >= p1 + l1 && \
      !memcmp(data + p1, d1, (size_t)l1) && !memcmp(data + p2, d2, (size_t)l2)) return #type;
  MAGIC(voc   , 0, 0, ""     , 0, 20, "Creative Voice File\x1a")
  MAGIC(smp   , 0, 0, ""     , 0, 17, "SOUND SAMPLE DATA")
  MAGIC(wve   , 0, 0, ""     , 0, 15, "ALawSoundFile**")
  MAGIC(amr-wb, 0, 0, ""     , 0,  9, "#!AMR-WB\n")
  MAGIC(prc   , 0, 0, ""     , 0,  8, "\x37\x00\x00\x10\x6d\x00\x00\x10")
  MAGIC(sph   , 0, 0, ""     , 0,  7, "NIST_1A")
  MAGIC(amr-nb, 0, 0, ""     , 0,  6, "#!AMR\n")
  MAGIC(txw   , 0, 0, ""     , 0,  6, "LM8953")
  MAGIC(sndt  , 0, 0, ""     , 0,  6, "SOUND\x1a")
  MAGIC(vorbis, 0, 4, "OggS" , 29, 6, "vorbis")
  MAGIC(speex , 0, 4, "OggS" , 28, 6, "Speex")
  MAGIC(hcom  ,65, 4, "FSSD" , 128,4, "HCOM")
  MAGIC(wav   , 0, 4, "RIFF" , 8,  4, "WAVE")
  MAGIC(wav   , 0, 4, "RIFX" , 8,  4, "WAVE")
  MAGIC(aiff  , 0, 4, "FORM" , 8,  4, "AIFF")
  MAGIC(aifc  , 0, 4, "FORM" , 8,  4, "AIFC")
  MAGIC(8svx  , 0, 4, "FORM" , 8,  4, "8SVX")
  MAGIC(maud  , 0, 4, "FORM" , 8,  4, "MAUD")
  MAGIC(xa    , 0, 0, ""     , 0,  4, "XA\0\0")
  MAGIC(xa    , 0, 0, ""     , 0,  4, "XAI\0")
  MAGIC(xa    , 0, 0, ""     , 0,  4, "XAJ\0")
  MAGIC(au    , 0, 0, ""     , 0,  4, ".snd")
  MAGIC(au    , 0, 0, ""     , 0,  4, "dns.")
  MAGIC(au    , 0, 0, ""     , 0,  4, "\0ds.")
  MAGIC(au    , 0, 0, ""     , 0,  4, ".sd\0")
  MAGIC(flac  , 0, 0, ""     , 0,  4, "fLaC")
  MAGIC(avr   , 0, 0, ""     , 0,  4, "2BIT")
  MAGIC(caf   , 0, 0, ""     , 0,  4, "caff")
  MAGIC(wv    , 0, 0, ""     , 0,  4, "wvpk")
  MAGIC(paf   , 0, 0, ""     , 0,  4, " paf")
  MAGIC(sf    , 0, 0, ""     , 0,  4, "\144\243\001\0")
  MAGIC(sf    , 0, 0, ""     , 0,  4, "\0\001\243\144")
  MAGIC(sf    , 0, 0, ""     , 0,  4, "\144\243\002\0")
  MAGIC(sf    , 0, 0, ""     , 0,  4, "\0\002\243\144")
  MAGIC(sf    , 0, 0, ""     , 0,  4, "\144\243\003\0")
  MAGIC(sf    , 0, 0, ""     , 0,  4, "\0\003\243\144")
  MAGIC(sf    , 0, 0, ""     , 0,  4, "\144\243\004\0")
  MAGIC(sox   , 0, 0, ""     , 0,  4, ".SoX")
  MAGIC(sox   , 0, 0, ""     , 0,  4, "XoS.")

  if (ext && !strcasecmp(ext, "snd"))
  MAGIC(sndr  , 7, 1, ""     , 0,  2, "\0")
  #undef MAGIC
  return NULL;
}

sox_encodings_info_t const sox_encodings_info[] = {
  {0         , "n/a"          , "Unknown or not applicable"},
  {0         , "Signed PCM"   , "Signed Integer PCM"},
  {0         , "Unsigned PCM" , "Unsigned Integer PCM"},
  {0         , "F.P. PCM"     , "Floating Point PCM"},
  {0         , "F.P. PCM"     , "Floating Point (text) PCM"},
  {0         , "FLAC"         , "FLAC"},
  {0         , "HCOM"         , "HCOM"},
  {0         , "WavPack"      , "WavPack"},
  {0         , "F.P. WavPack" , "Floating Point WavPack"},
  {SOX_LOSSY1, "u-law"        , "u-law"},
  {SOX_LOSSY1, "A-law"        , "A-law"},
  {SOX_LOSSY1, "G.721 ADPCM"  , "G.721 ADPCM"},
  {SOX_LOSSY1, "G.723 ADPCM"  , "G.723 ADPCM"},
  {SOX_LOSSY1, "CL ADPCM (8)" , "CL ADPCM (from 8-bit)"},
  {SOX_LOSSY1, "CL ADPCM (16)", "CL ADPCM (from 16-bit)"},
  {SOX_LOSSY1, "MS ADPCM"     , "MS ADPCM"},
  {SOX_LOSSY1, "IMA ADPCM"    , "IMA ADPCM"},
  {SOX_LOSSY1, "OKI ADPCM"    , "OKI ADPCM"},
  {SOX_LOSSY1, "DPCM"         , "DPCM"},
  {0         , "DWVW"         , "DWVW"},
  {0         , "DWVWN"        , "DWVWN"},
  {SOX_LOSSY2, "GSM"          , "GSM"},
  {SOX_LOSSY2, "MPEG audio"   , "MPEG audio (layer I, II or III)"},
  {SOX_LOSSY2, "Vorbis"       , "Vorbis"},
  {SOX_LOSSY2, "AMR-WB"       , "AMR-WB"},
  {SOX_LOSSY2, "AMR-NB"       , "AMR-NB"},
  {SOX_LOSSY2, "CVSD"         , "CVSD"},
  {SOX_LOSSY2, "LPC10"        , "LPC10"},
};

assert_static(array_length(sox_encodings_info) == SOX_ENCODINGS,
    SIZE_MISMATCH_BETWEEN_sox_encodings_t_AND_sox_encodings_info);

unsigned sox_precision(sox_encoding_t encoding, unsigned bits_per_sample)
{
  switch (encoding) {
    case SOX_ENCODING_DWVW:       return bits_per_sample;
    case SOX_ENCODING_DWVWN:      return !bits_per_sample? 16: 0; /* ? */
    case SOX_ENCODING_HCOM:       return !(bits_per_sample & 7) && (bits_per_sample >> 3) - 1 < 1? bits_per_sample: 0;
    case SOX_ENCODING_WAVPACK:
    case SOX_ENCODING_FLAC:       return !(bits_per_sample & 7) && (bits_per_sample >> 3) - 1 < 4? bits_per_sample: 0;
    case SOX_ENCODING_SIGN2:      return bits_per_sample <= 32? bits_per_sample : 0;
    case SOX_ENCODING_UNSIGNED:   return !(bits_per_sample & 7) && (bits_per_sample >> 3) - 1 < 4? bits_per_sample: 0;

    case SOX_ENCODING_ALAW:       return bits_per_sample == 8? 13: 0;
    case SOX_ENCODING_ULAW:       return bits_per_sample == 8? 14: 0;

    case SOX_ENCODING_CL_ADPCM:   return bits_per_sample? 8: 0;
    case SOX_ENCODING_CL_ADPCM16: return bits_per_sample == 4? 13: 0;
    case SOX_ENCODING_MS_ADPCM:   return bits_per_sample == 4? 14: 0;
    case SOX_ENCODING_IMA_ADPCM:  return bits_per_sample == 4? 13: 0;
    case SOX_ENCODING_OKI_ADPCM:  return bits_per_sample == 4? 12: 0;
    case SOX_ENCODING_G721:       return bits_per_sample == 4? 12: 0;
    case SOX_ENCODING_G723:       return bits_per_sample == 3? 8:
                                         bits_per_sample == 5? 14: 0;
    case SOX_ENCODING_CVSD:       return bits_per_sample == 1? 16: 0;
    case SOX_ENCODING_DPCM:       return bits_per_sample; /* ? */

    case SOX_ENCODING_GSM:
    case SOX_ENCODING_MP3:
    case SOX_ENCODING_VORBIS:
    case SOX_ENCODING_AMR_WB:
    case SOX_ENCODING_AMR_NB:
    case SOX_ENCODING_LPC10:      return !bits_per_sample? 16: 0;

    case SOX_ENCODING_WAVPACKF:
    case SOX_ENCODING_FLOAT:      return bits_per_sample == 32 ? 24: bits_per_sample == 64 ? 53: 0;
    case SOX_ENCODING_FLOAT_TEXT: return !bits_per_sample? 53: 0;

    case SOX_ENCODINGS:
    case SOX_ENCODING_UNKNOWN:    break;
  }
  return 0;
}

void sox_init_encodinginfo(sox_encodinginfo_t * e)
{
  e->reverse_bytes = SOX_OPTION_DEFAULT;
  e->reverse_nibbles = SOX_OPTION_DEFAULT;
  e->reverse_bits = SOX_OPTION_DEFAULT;
  e->compression = HUGE_VAL;
}

/*--------------------------------- Comments ---------------------------------*/

size_t sox_num_comments(sox_comments_t comments)
{
  size_t result = 0;
  if (!comments)
    return 0;
  while (*comments++)
    ++result;
  return result;
}

void sox_append_comment(sox_comments_t * comments, char const * comment)
{
  size_t n = sox_num_comments(*comments);
  *comments = lsx_realloc(*comments, (n + 2) * sizeof(**comments));
  assert(comment);
  (*comments)[n++] = lsx_strdup(comment);
  (*comments)[n] = 0;
}

void sox_append_comments(sox_comments_t * comments, char const * comment)
{
  char * end;
  if (comment) {
    while ((end = strchr(comment, '\n'))) {
      size_t len = end - comment;
      char * c = lsx_malloc((len + 1) * sizeof(*c));
      strncpy(c, comment, len);
      c[len] = '\0';
      sox_append_comment(comments, c);
      comment += len + 1;
      free(c);
    }
    if (*comment)
      sox_append_comment(comments, comment);
  }
}

sox_comments_t sox_copy_comments(sox_comments_t comments)
{
  sox_comments_t result = 0;

  if (comments) while (*comments)
    sox_append_comment(&result, *comments++);
  return result;
}

void sox_delete_comments(sox_comments_t * comments)
{
  sox_comments_t p = *comments;

  if (p) while (*p)
    free(*p++);
  free(*comments);
  *comments = 0;
}

char * lsx_cat_comments(sox_comments_t comments)
{
  sox_comments_t p = comments;
  size_t len = 0;
  char * result;

  if (p) while (*p)
    len += strlen(*p++) + 1;

  result = lsx_calloc(len? len : 1, sizeof(*result));

  if ((p = comments) && *p) {
    strcpy(result, *p);
    while (*++p)
      strcat(strcat(result, "\n"), *p);
  }
  return result;
}

char const * sox_find_comment(sox_comments_t comments, char const * id)
{
  size_t len = strlen(id);

  if (comments) for (;*comments; ++comments)
    if (!strncasecmp(*comments, id, len) && (*comments)[len] == '=')
      return *comments + len + 1;
  return NULL;
}

static void set_endiannesses(sox_format_t * ft)
{
  if (ft->encoding.opposite_endian)
    ft->encoding.reverse_bytes = (ft->handler.flags & SOX_FILE_ENDIAN)?
      !(ft->handler.flags & SOX_FILE_ENDBIG) != MACHINE_IS_BIGENDIAN : sox_true;
  else if (ft->encoding.reverse_bytes == SOX_OPTION_DEFAULT)
    ft->encoding.reverse_bytes = (ft->handler.flags & SOX_FILE_ENDIAN)?
      !(ft->handler.flags & SOX_FILE_ENDBIG) == MACHINE_IS_BIGENDIAN : sox_false;

  /* FIXME: Change reports to suitable warnings if trying
   * to override something that can't be overridden. */

  if (ft->handler.flags & SOX_FILE_ENDIAN) {
    if (ft->encoding.reverse_bytes == (sox_option_t)
        (!(ft->handler.flags & SOX_FILE_ENDBIG) != MACHINE_IS_BIGENDIAN))
      lsx_report("`%s': overriding file-type byte-order", ft->filename);
  } else if (ft->encoding.reverse_bytes == sox_true)
    lsx_report("`%s': overriding machine byte-order", ft->filename);

  if (ft->encoding.reverse_bits == SOX_OPTION_DEFAULT)
    ft->encoding.reverse_bits = !!(ft->handler.flags & SOX_FILE_BIT_REV);
  else if (ft->encoding.reverse_bits == !(ft->handler.flags & SOX_FILE_BIT_REV))
      lsx_report("`%s': overriding file-type bit-order", ft->filename);

  if (ft->encoding.reverse_nibbles == SOX_OPTION_DEFAULT)
    ft->encoding.reverse_nibbles = !!(ft->handler.flags & SOX_FILE_NIB_REV);
  else
    if (ft->encoding.reverse_nibbles == !(ft->handler.flags & SOX_FILE_NIB_REV))
      lsx_report("`%s': overriding file-type nibble-order", ft->filename);
}

static sox_bool is_seekable(sox_format_t const * ft)
{
  struct stat st;

  assert(ft);
  if (!ft->fp)
    return sox_false;
  fstat(fileno(ft->fp), &st);
  return ((st.st_mode & S_IFMT) == S_IFREG);
}

/* check that all settings have been given */
static int sox_checkformat(sox_format_t * ft)
{
  ft->sox_errno = SOX_SUCCESS;

  if (!ft->signal.rate) {
    lsx_fail_errno(ft,SOX_EFMT,"sampling rate was not specified");
    return SOX_EOF;
  }
  if (!ft->signal.precision) {
    lsx_fail_errno(ft,SOX_EFMT,"data encoding was not specified");
    return SOX_EOF;
  }
  return SOX_SUCCESS;
}

static sox_bool is_url(char const * text) /* detects only wget-supported URLs */
{
  return !(
      strncasecmp(text, "http:" , (size_t)5) &&
      strncasecmp(text, "https:", (size_t)6) &&
      strncasecmp(text, "ftp:"  , (size_t)4));
}

static int xfclose(FILE * file, lsx_io_type io_type)
{
  return
#ifdef HAVE_POPEN
    io_type != lsx_io_file? pclose(file) :
#endif
    fclose(file);
}

static FILE * xfopen(char const * identifier, char const * mode, lsx_io_type * io_type)
{
  *io_type = lsx_io_file;

  if (*identifier == '|') {
    FILE * f = NULL;
#ifdef HAVE_POPEN
    f = popen(identifier + 1, "r");
    *io_type = lsx_io_pipe;
#else
    lsx_fail("this build of SoX cannot open pipes");
#endif
    return f;
  }
  else if (is_url(identifier)) {
    FILE * f = NULL;
#ifdef HAVE_POPEN
    char const * const command_format = "wget --no-check-certificate -q -O- \"%s\"";
    char * command = lsx_malloc(strlen(command_format) + strlen(identifier));
    sprintf(command, command_format, identifier);
    f = popen(command, "r");
    free(command);
    *io_type = lsx_io_url;
#else
    lsx_fail("this build of SoX cannot open URLs");
#endif
    return f;
  }
  return fopen(identifier, mode);
}

sox_format_t * sox_open_read(
    char               const * path,
    sox_signalinfo_t   const * signal,
    sox_encodinginfo_t const * encoding,
    char               const * filetype)
{
  sox_format_t * ft = lsx_calloc(1, sizeof(*ft));
  sox_format_handler_t const * handler;
  char const * const io_types[] = {"file", "pipe", "file URL"};
  char const * type = "";

  if (filetype) {
    if (!(handler = sox_find_format(filetype, sox_false))) {
      lsx_fail("no handler for given file type `%s'", filetype);
      goto error;
    }
    ft->handler = *handler;
  }

  if (!(ft->handler.flags & SOX_FILE_NOSTDIO)) {
    size_t   input_bufsiz = sox_globals.input_bufsiz?
        sox_globals.input_bufsiz : sox_globals.bufsiz;

    if (!strcmp(path, "-")) { /* Use stdin if the filename is "-" */
      if (sox_globals.stdin_in_use_by) {
        lsx_fail("`-' (stdin) already in use by `%s'", sox_globals.stdin_in_use_by);
        goto error;
      }
      sox_globals.stdin_in_use_by = "audio input";
      SET_BINARY_MODE(stdin);
      ft->fp = stdin;
    }
    else {
      ft->fp = xfopen(path, "rb", &ft->io_type);
      type = io_types[ft->io_type];
      if (ft->fp == NULL) {
        lsx_fail("can't open input %s `%s': %s", type, path, strerror(errno));
        goto error;
      }
    }
    if (setvbuf (ft->fp, NULL, _IOFBF, sizeof(char) * input_bufsiz)) {
      lsx_fail("Can't set read buffer");
      goto error;
    }
    ft->seekable = is_seekable(ft);
  }

  if (!filetype) {
    if (ft->seekable) {
      filetype = detect_magic(ft, lsx_find_file_extension(path));
      lsx_rewind(ft);
#if HAVE_MAGIC
      if (!filetype) {
        static magic_t magic;
        if (!magic) {
          magic = magic_open(MAGIC_MIME | MAGIC_SYMLINK);
          if (magic)
            magic_load(magic, NULL);
        }
        if (magic)
          filetype = magic_file(magic, path);
        if (filetype && (
              !strcmp(filetype, "application/octet-stream") ||
              !strncmp(filetype, "text/plain", (size_t)10) ))
          filetype = NULL;
      }
#endif
    }
    if (filetype) {
      lsx_report("detected file format type `%s'", filetype);
      if (!(handler = sox_find_format(filetype, sox_false))) {
        lsx_fail("no handler for detected file type `%s'", filetype);
        goto error;
      }
    }
    else {
      if (ft->io_type == lsx_io_pipe) {
        filetype = "sox";
        lsx_report("assuming input pipe `%s' has file-type `sox'", path);
      }
      else if (!(filetype = lsx_find_file_extension(path))) {
        lsx_fail("can't determine type of %s `%s'", type, path);
        goto error;
      }
      if (!(handler = sox_find_format(filetype, sox_true))) {
        lsx_fail("no handler for file extension `%s'", filetype);
        goto error;
      }
    }
    ft->handler = *handler;
    if (ft->handler.flags & SOX_FILE_NOSTDIO) {
      xfclose(ft->fp, ft->io_type);
      ft->fp = NULL;
    }
  }
  if (!ft->handler.startread && !ft->handler.read) {
    lsx_fail("file type `%s' isn't readable", filetype);
    goto error;
  }

  ft->mode = 'r';
  ft->filetype = lsx_strdup(filetype);
  ft->filename = lsx_strdup(path);
  if (signal)
    ft->signal = *signal;

  if (encoding)
    ft->encoding = *encoding;
  else sox_init_encodinginfo(&ft->encoding);
  set_endiannesses(ft);

  ft->priv = lsx_calloc(1, ft->handler.priv_size);
  /* Read and write starters can change their formats. */
  if (ft->handler.startread && (*ft->handler.startread)(ft) != SOX_SUCCESS) {
    lsx_fail("can't open input %s `%s': %s", type, ft->filename, ft->sox_errstr);
    goto error;
  }

  /* Fill in some defaults: */
  if (!ft->signal.precision)
    ft->signal.precision = sox_precision(ft->encoding.encoding, ft->encoding.bits_per_sample);
  if (!(ft->handler.flags & SOX_FILE_PHONY) && !ft->signal.channels)
    ft->signal.channels = 1;

  if (sox_checkformat(ft) == SOX_SUCCESS)
    return ft;
  lsx_fail("bad input format for %s `%s': %s", type, ft->filename, ft->sox_errstr);

error:
  if (ft->fp && ft->fp != stdin)
    xfclose(ft->fp, ft->io_type);
  free(ft->priv);
  free(ft->filename);
  free(ft->filetype);
  free(ft);
  return NULL;
}

sox_bool sox_format_supports_encoding(
    char               const * path,
    char               const * filetype,
    sox_encodinginfo_t const * encoding)
{
  #define enc_arg(T) (T)handler->write_formats[i++]
  sox_bool is_file_extension = filetype == NULL;
  sox_format_handler_t const * handler;
  unsigned i = 0, s;
  sox_encoding_t e;

  assert(path);
  assert(encoding);
  if (!filetype)
    filetype = lsx_find_file_extension(path);

  if (!filetype || !(handler = sox_find_format(filetype, is_file_extension)) ||
      !handler->write_formats)
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
        lsx_warn("%s can't encode at %gHz; using %gHz", ft->handler.names[0], given, ft->signal.rate);
      }
    }
  }
  else if (!ft->signal.rate)
    ft->signal.rate = SOX_DEFAULT_RATE;

  if (ft->handler.flags & SOX_FILE_CHANS) {
    if (ft->signal.channels == 1 && !(ft->handler.flags & SOX_FILE_MONO)) {
      ft->signal.channels = (ft->handler.flags & SOX_FILE_STEREO)? 2 : 4;
      lsx_warn("%s can't encode mono; setting channels to %u", ft->handler.names[0], ft->signal.channels);
    } else
    if (ft->signal.channels == 2 && !(ft->handler.flags & SOX_FILE_STEREO)) {
      ft->signal.channels = (ft->handler.flags & SOX_FILE_QUAD)? 4 : 1;
      lsx_warn("%s can't encode stereo; setting channels to %u", ft->handler.names[0], ft->signal.channels);
    } else
    if (ft->signal.channels == 4 && !(ft->handler.flags & SOX_FILE_QUAD)) {
      ft->signal.channels = (ft->handler.flags & SOX_FILE_STEREO)? 2 : 1;
      lsx_warn("%s can't encode quad; setting channels to %u", ft->handler.names[0], ft->signal.channels);
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
      lsx_warn("%s can't encode %s", ft->handler.names[0], sox_encodings_info[ft->encoding.encoding].desc);
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
        else lsx_warn("%s can't encode %s to %u-bit", ft->handler.names[0], sox_encodings_info[ft->encoding.encoding].desc, given_size);
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
      lsx_warn("%s can't encode to %u-bit", ft->handler.names[0], ft->encoding.bits_per_sample);
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
        if (!(sox_encodings_info[e].flags & (SOX_LOSSY1 | SOX_LOSSY2)) &&
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

sox_format_t * sox_open_write(
    char               const * path,
    sox_signalinfo_t   const * signal,
    sox_encodinginfo_t const * encoding,
    char               const * filetype,
    sox_oob_t    const * oob,
    sox_bool           (*overwrite_permitted)(const char *filename))
{
  sox_format_t * ft = lsx_calloc(sizeof(*ft), 1);
  sox_format_handler_t const * handler;

  if (!path || !signal) {
    lsx_fail("must specify file name and signal parameters to write file");
    goto error;
  }

  if (filetype) {
    if (!(handler = sox_find_format(filetype, sox_false))) {
      lsx_fail("no handler for given file type `%s'", filetype);
      goto error;
    }
    ft->handler = *handler;
  }
  else {
    if (!(filetype = lsx_find_file_extension(path))) {
      lsx_fail("can't determine type of `%s'", path);
      goto error;
    }
    if (!(handler = sox_find_format(filetype, sox_true))) {
      lsx_fail("no handler for file extension `%s'", filetype);
      goto error;
    }
    ft->handler = *handler;
  }
  if (!ft->handler.startwrite && !ft->handler.write) {
    lsx_fail("file type `%s' isn't writeable", filetype);
    goto error;
  }

  if (!(ft->handler.flags & SOX_FILE_NOSTDIO)) {
    if (!strcmp(path, "-")) { /* Use stdout if the filename is "-" */
      if (sox_globals.stdout_in_use_by) {
        lsx_fail("`-' (stdout) already in use by `%s'", sox_globals.stdout_in_use_by);
        goto error;
      }
      sox_globals.stdout_in_use_by = "audio output";
      SET_BINARY_MODE(stdout);
      ft->fp = stdout;
    }
    else {
      struct stat st;
      if (!stat(path, &st) && (st.st_mode & S_IFMT) == S_IFREG &&
          (overwrite_permitted && !overwrite_permitted(path))) {
        lsx_fail("permission to overwrite '%s' denied", path);
        goto error;
      }
      if ((ft->fp = fopen(path, "wb")) == NULL) {
        lsx_fail("can't open output file `%s': %s", path, strerror(errno));
        goto error;
      }
    }

    /* stdout tends to be line-buffered.  Override this */
    /* to be Full Buffering. */
    if (setvbuf (ft->fp, NULL, _IOFBF, sizeof(char) * sox_globals.bufsiz)) {
      lsx_fail("Can't set write buffer");
      goto error;
    }
    ft->seekable = is_seekable(ft);
  }

  ft->filetype = lsx_strdup(filetype);
  ft->filename = lsx_strdup(path);
  ft->mode = 'w';
  ft->signal = *signal;

  if (encoding)
    ft->encoding = *encoding;
  else sox_init_encodinginfo(&ft->encoding);
  set_endiannesses(ft);

  if (oob) {
    ft->oob = *oob;
    /* deep copy: */
    ft->oob.comments = sox_copy_comments(oob->comments);
  }

  set_output_format(ft);

  /* FIXME: doesn't cover the situation where
   * codec changes audio length due to block alignment (e.g. 8svx, gsm): */
  if (signal->rate && signal->channels)
    ft->signal.length = ft->signal.length * ft->signal.rate / signal->rate *
      ft->signal.channels / signal->channels + .5;

  if ((ft->handler.flags & SOX_FILE_REWIND) && strcmp(ft->filetype, "sox") && !ft->signal.length && !ft->seekable)
    lsx_warn("can't seek in output file `%s'; length in file header will be unspecified", ft->filename);

  ft->priv = lsx_calloc(1, ft->handler.priv_size);
  /* Read and write starters can change their formats. */
  if (ft->handler.startwrite && (ft->handler.startwrite)(ft) != SOX_SUCCESS){
    lsx_fail("can't open output file `%s': %s", ft->filename, ft->sox_errstr);
    goto error;
  }

  if (sox_checkformat(ft) == SOX_SUCCESS)
    return ft;
  lsx_fail("bad format for output file `%s': %s", ft->filename, ft->sox_errstr);

error:
  if (ft->fp && ft->fp != stdout)
    xfclose(ft->fp, ft->io_type);
  free(ft->priv);
  free(ft->filename);
  free(ft->filetype);
  free(ft);
  return NULL;
}

size_t sox_read(sox_format_t * ft, sox_sample_t * buf, size_t len)
{
  size_t actual = ft->handler.read? (*ft->handler.read)(ft, buf, len) : 0;
  return (actual > len? 0 : actual);
}

size_t sox_write(sox_format_t * ft, const sox_sample_t *buf, size_t len)
{
  size_t ret = ft->handler.write? (*ft->handler.write)(ft, buf, len) : 0;
  ft->olength += ret;
  return ret;
}

int sox_close(sox_format_t * ft)
{
  int result = SOX_SUCCESS;

  if (ft->mode == 'r')
    result = ft->handler.stopread? (*ft->handler.stopread)(ft) : SOX_SUCCESS;
  else {
    if (ft->handler.flags & SOX_FILE_REWIND) {
      if (ft->olength != ft->signal.length && ft->seekable) {
        result = lsx_seeki(ft, (off_t)0, 0);
        if (result == SOX_SUCCESS)
          result = ft->handler.stopwrite? (*ft->handler.stopwrite)(ft)
             : ft->handler.startwrite?(*ft->handler.startwrite)(ft) : SOX_SUCCESS;
      }
    }
    else result = ft->handler.stopwrite? (*ft->handler.stopwrite)(ft) : SOX_SUCCESS;
  }

  if (ft->fp && ft->fp != stdin && ft->fp != stdout)
    xfclose(ft->fp, ft->io_type);
  free(ft->filename);
  free(ft->filetype);
  sox_delete_comments(&ft->oob.comments);

  free(ft);
  return result;
}

int sox_seek(sox_format_t * ft, uint64_t offset, int whence)
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

static int strcaseends(char const * str, char const * end)
{
  size_t str_len = strlen(str), end_len = strlen(end);
  return str_len >= end_len && !strcasecmp(str + str_len - end_len, end);
}

sox_bool sox_is_playlist(char const * filename)
{
  return *filename != '|' && 
    (strcaseends(filename, ".m3u") || strcaseends(filename, ".pls"));
}

int sox_parse_playlist(sox_playlist_callback_t callback, void * p, char const * const listname)
{
  sox_bool const is_pls = strcaseends(listname, ".pls");
  int const comment_char = "#;"[is_pls];
  size_t text_length = 100;
  char * text = lsx_malloc(text_length + 1);
  char * dirname = lsx_strdup(listname);
  char * slash_pos = LAST_SLASH(dirname);
  lsx_io_type io_type;
  FILE * file = xfopen(listname, "r", &io_type);
  char * filename;
  int c, result = SOX_SUCCESS;

  if (!slash_pos)
    *dirname = '\0';
  else
    *slash_pos = '\0';

  if (file == NULL) {
    lsx_fail("Can't open playlist file `%s': %s", listname, strerror(errno));
    result = SOX_EOF;
  }
  else {
    do {
      size_t i = 0;
      size_t begin = 0, end = 0;

      while (isspace(c = getc(file)));
      if (c == EOF)
        break;
      while (c != EOF && !strchr("\r\n", c) && c != comment_char) {
        if (i == text_length)
          text = lsx_realloc(text, (text_length <<= 1) + 1);
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
        if (!strncasecmp(text, "file", (size_t) 4) && sscanf(text + 4, "%*u=%c", &dummy) == 1)
          begin = strchr(text + 5, '=') - text + 1;
        else end = 0;
      }
      if (begin != end) {
        char const * id = text + begin;

        if (!dirname[0] || is_url(id) || IS_ABSOLUTE(id))
          filename = lsx_strdup(id);
        else {
          filename = lsx_malloc(strlen(dirname) + strlen(id) + 2);
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
      lsx_fail("error reading playlist file `%s': %s", listname, strerror(errno));
      result = SOX_EOF;
    }
    if (xfclose(file, io_type) && io_type == lsx_io_url) {
      lsx_fail("error reading playlist file URL `%s'", listname);
      result = SOX_EOF;
    }
  }
  free(text);
  free(dirname);
  return result;
}

#ifdef HAVE_LIBLTDL /* Plugin format handlers */

#include <ltdl.h>
#define MAX_FORMATS 256 /* FIXME: Use a vector, not a fixed-size array */
#define MAX_NAME_LEN (size_t)1024 /* FIXME: Use vasprintf */

static sox_bool plugins_initted = sox_false;

sox_format_tab_t sox_format_fns[MAX_FORMATS + 1];

static unsigned nformats = 0;

static int init_format(const char *file, lt_ptr data)
{
  lt_dlhandle lth = lt_dlopenext(file);
  const char *end = file + strlen(file);
  const char prefix[] = "sox_fmt_";
  char fnname[MAX_NAME_LEN];
  char *start = strstr(file, prefix);

  (void)data;
  if (start && (start += sizeof(prefix) - 1) < end) {
    int ret = snprintf(fnname, MAX_NAME_LEN, "sox_%.*s_format_fn", (int)(end - start), start);
    if (ret > 0 && ret < (int)MAX_NAME_LEN) {
      union {sox_format_fn_t fn; lt_ptr ptr;} ltptr;
      ltptr.ptr = lt_dlsym(lth, fnname);
      lsx_debug("opening format plugin `%s': library %p, entry point %p\n", fnname, (void *)lth, ltptr.ptr);
      if (nformats < MAX_FORMATS && ltptr.fn && (ltptr.fn()->sox_lib_version_code & ~255) == (SOX_LIB_VERSION_CODE & ~255))
        sox_format_fns[nformats++].fn = ltptr.fn;
    }
  }
  return SOX_SUCCESS;
}

int sox_format_init(void) /* Find & load format handlers.  */
{
  sox_format_handler_t const * sox_sox_format_fn(void);
  int ret;

  sox_format_fns[nformats++].fn = sox_sox_format_fn;

  if ((ret = lt_dlinit()) != 0) {
    lsx_fail("lt_dlinit failed with %d error(s): %s", ret, lt_dlerror());
    return SOX_EOF;
  }
  plugins_initted = sox_true;

  lt_dlforeachfile(PKGLIBDIR, init_format, NULL);
  return SOX_SUCCESS;
}

void sox_format_quit(void) /* Cleanup things.  */
{
  int ret;
  if (plugins_initted && (ret = lt_dlexit()) != 0)
    lsx_fail("lt_dlexit failed with %d error(s): %s", ret, lt_dlerror());
}

#else /* Static format handlers */

#define FORMAT(f) extern sox_format_handler_t const * sox_##f##_format_fn(void);
#include "formats.h"
#undef FORMAT

sox_format_tab_t sox_format_fns[] = {
  #define FORMAT(f) {NULL, sox_##f##_format_fn},
  #include "formats.h"
  #undef FORMAT
  {NULL, NULL}
};

int sox_format_init(void) {return SOX_SUCCESS;}
void sox_format_quit(void) {}

#endif

/* Find a named format in the formats library.
 *
 * (c) 2005-8 Chris Bagwell and SoX contributors.
 * Copyright 1991 Lance Norskog And Sundry Contributors.
 *
 * This source code is freely redistributable and may be used for any
 * purpose.  This copyright notice must be maintained.
 *
 * Lance Norskog, Sundry Contributors, Chris Bagwell and SoX contributors
 * are not responsible for the consequences of using this software.
 */
sox_format_handler_t const * sox_find_format(char const * name, sox_bool no_dev)
{
  size_t f, n;

  if (name) for (f = 0; sox_format_fns[f].fn; ++f) {
    sox_format_handler_t const * handler = sox_format_fns[f].fn();

    if (!(no_dev && (handler->flags & SOX_FILE_DEVICE)))
      for (n = 0; handler->names[n]; ++n)
        if (!strcasecmp(handler->names[n], name))
          return handler;                 /* Found it. */
  }
  return NULL;
}
