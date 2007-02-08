/*
 * Sound Tools libsndfile formats.
 *
 * Copyright 2007 Reuben Thomas <rrt@sc3d.org>
 * Copyright 1999-2005 Erik de Castro Lopo <eridk@mega-nerd.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, write to the Free Software
 * Foundation, Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301,
 * USA.  */

#include "st_i.h"

#ifdef HAVE_SNDFILE_H

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sndfile.h>

/* Private data for sndfile files */
typedef struct sndfile
{
  SNDFILE *sf_file;
  SF_INFO *sf_info;
} *sndfile_t;

assert_static(sizeof(struct sndfile) <= ST_MAX_FILE_PRIVSIZE, 
              /* else */ sndfile_PRIVSIZE_too_big);

/* Get sample encoding and size from libsndfile subtype; return value
   is encoding if conversion was made, or ST_ENCODING_UNKNOWN for
   invalid input. If the libsndfile subtype can't be represented in
   SoX types, use 16-bit signed. */
static st_encoding_t sox_encoding_and_size(int format, int *size)
{
  *size = -1;                   /* Default */
  format &= SF_FORMAT_SUBMASK;
  
  switch (format) {
  case SF_FORMAT_PCM_S8:
    *size = ST_SIZE_8BIT;
    return ST_ENCODING_SIGN2;
  case SF_FORMAT_PCM_16:
    *size = ST_SIZE_16BIT;
    return ST_ENCODING_SIGN2;
  case SF_FORMAT_PCM_24:
    *size = ST_SIZE_24BIT;
    return ST_ENCODING_SIGN2;
  case SF_FORMAT_PCM_32:
    *size = ST_SIZE_32BIT;
    return ST_ENCODING_SIGN2;
  case SF_FORMAT_PCM_U8:
    *size = ST_SIZE_8BIT;
    return ST_ENCODING_UNSIGNED;
  case SF_FORMAT_FLOAT:
    *size = ST_SIZE_32BIT;
    return ST_ENCODING_FLOAT;
  case SF_FORMAT_DOUBLE:
    *size = ST_SIZE_64BIT;
    return ST_ENCODING_FLOAT;
  case SF_FORMAT_ULAW:
    *size = ST_SIZE_8BIT;
    return ST_ENCODING_ULAW;
  case SF_FORMAT_ALAW:
    *size = ST_SIZE_8BIT;
    return ST_ENCODING_ALAW;
  case SF_FORMAT_IMA_ADPCM:
    return ST_ENCODING_IMA_ADPCM;
  case SF_FORMAT_MS_ADPCM:
    return ST_ENCODING_MS_ADPCM;
  case SF_FORMAT_GSM610:
    return ST_ENCODING_GSM;
  case SF_FORMAT_VOX_ADPCM:
    return ST_ENCODING_ADPCM;

  /* For encodings we can't represent, have a sensible default */
  case SF_FORMAT_G721_32:
  case SF_FORMAT_G723_24:
  case SF_FORMAT_G723_40:
  case SF_FORMAT_DWVW_12:
  case SF_FORMAT_DWVW_16:
  case SF_FORMAT_DWVW_24:
  case SF_FORMAT_DWVW_N:
  case SF_FORMAT_DPCM_8:
  case SF_FORMAT_DPCM_16:
    return ST_ENCODING_SIGN2;

  default: /* Invalid libsndfile subtype */
    return ST_ENCODING_UNKNOWN;
  }

  assert(0); /* Should never reach here */
  return ST_ENCODING_UNKNOWN;
}

/*
 * Open file in sndfile.
 */
int st_sndfile_startread(ft_t ft)
{
  sndfile_t sf = (sndfile_t)ft->priv;

  sf->sf_info = (SF_INFO *)xcalloc(1, sizeof(SF_INFO));
  /* We'd like to use sf_open, but auto file typing has already
     invoked stdio buffering. */
  /* FIXME: If format parameters are set, assume file is raw. */
  if ((sf->sf_file = sf_open(ft->filename, SFM_READ, sf->sf_info)) == NULL) {
    st_fail("sndfile cannot open file for reading: %s", sf_strerror(sf->sf_file));
    free(sf->sf_file);
    return ST_EOF;
  }

  /* Copy format info */
  ft->signal.rate = sf->sf_info->samplerate;
  ft->signal.encoding = sox_encoding_and_size(sf->sf_info->format, &ft->signal.size);
  ft->signal.channels = sf->sf_info->channels;
  ft->length = sf->sf_info->frames * sf->sf_info->channels;

  return ST_SUCCESS;
}

/*
 * Read up to len samples of type st_sample_t from file into buf[].
 * Return number of samples read.
 */
st_size_t st_sndfile_read(ft_t ft, st_sample_t *buf, st_size_t len)
{
  sndfile_t sf = (sndfile_t)ft->priv;

  /* FIXME: We assume int == st_sample_t here */
  return (st_size_t)sf_read_int(sf->sf_file, (int *)buf, len);
}

/*
 * Close file for libsndfile (this doesn't close the file handle)
 */
int st_sndfile_stopread(ft_t ft)
{
  sndfile_t sf = (sndfile_t)ft->priv;
  sf_close(sf->sf_file);
  return ST_SUCCESS;
}

static struct {
  const char *ext;
  int len;
  int format;
} format_map[] =
{
  { "aif",	3, SF_FORMAT_AIFF },
  { "wav",	0, SF_FORMAT_WAV },
  { "au",	0, SF_FORMAT_AU },
  { "snd",	0, SF_FORMAT_AU },
#ifdef HAVE_SNDFILE_1_0_12
  { "caf",	0, SF_FORMAT_CAF },
  { "flac",	0, SF_FORMAT_FLAC },
#endif
  { "svx",	0, SF_FORMAT_SVX },
  { "8svx",     0, SF_FORMAT_SVX },
  { "paf",	0, SF_ENDIAN_BIG | SF_FORMAT_PAF },
  { "fap",	0, SF_ENDIAN_LITTLE | SF_FORMAT_PAF },
  { "gsm",	0, SF_FORMAT_RAW | SF_FORMAT_GSM610 },
  { "nist", 	0, SF_FORMAT_NIST },
  { "sph",      0, SF_FORMAT_NIST },
  { "ircam",	0, SF_FORMAT_IRCAM },
  { "sf",	0, SF_FORMAT_IRCAM },
  { "voc",	0, SF_FORMAT_VOC },
  { "w64", 	0, SF_FORMAT_W64 },
  { "raw",	0, SF_FORMAT_RAW },
  { "mat4", 	0, SF_FORMAT_MAT4 },
  { "mat5", 	0, SF_FORMAT_MAT5 },
  { "mat",	0, SF_FORMAT_MAT4 },
  { "pvf",	0, SF_FORMAT_PVF },
  { "sds",	0, SF_FORMAT_SDS },
  { "sd2",	0, SF_FORMAT_SD2 },
  { "vox",	0, SF_FORMAT_RAW | SF_FORMAT_VOX_ADPCM },
  { "xi",	0, SF_FORMAT_XI }
};

/* Convert file name or type to libsndfile format */
static int name_to_format(const char *name)
{
  int k;
#define FILE_TYPE_BUFLEN 15
  char buffer[FILE_TYPE_BUFLEN + 1], *cptr;

  if ((cptr = strrchr(name, '.')) != NULL) {
    strncpy(buffer, cptr + 1, FILE_TYPE_BUFLEN);
    buffer[FILE_TYPE_BUFLEN] = 0;
  
    for (k = 0; buffer[k]; k++)
      buffer[k] = tolower((buffer[k]));
  } else
    strncpy(buffer, name, FILE_TYPE_BUFLEN);
  
  for (k = 0; k < (int)(sizeof(format_map) / sizeof(format_map [0])); k++) {
    if (format_map[k].len > 0 && strncmp(name, format_map[k].ext, format_map[k].len) == 0)
      return format_map[k].format;
    else if (strcmp(buffer, format_map[k].ext) == 0)
      return format_map[k].format;
  }

  return 0;
}

/* Make libsndfile subtype from sample encoding and size */
static int sndfile_format(int encoding, int size)
{
  if (encoding < ST_ENCODING_SIZE_IS_WORD) {
    switch (encoding) {
    case ST_ENCODING_ULAW:
      return SF_FORMAT_ULAW;
    case ST_ENCODING_ALAW:
      return SF_FORMAT_ALAW;
    case ST_ENCODING_ADPCM:
    case ST_ENCODING_MS_ADPCM:
      return SF_FORMAT_MS_ADPCM;
    case ST_ENCODING_IMA_ADPCM:
      return SF_FORMAT_IMA_ADPCM;
    case ST_ENCODING_OKI_ADPCM:
      return SF_FORMAT_VOX_ADPCM;
    default: /* Should be impossible */
      return 0;
    }
  } else {
    switch (encoding) {
    case ST_ENCODING_UNSIGNED:
      if (size == ST_SIZE_8BIT)
        return SF_FORMAT_PCM_U8;
      else
        return 0;
    case ST_ENCODING_SIGN2:
    case ST_ENCODING_MP3:
    case ST_ENCODING_VORBIS:
#ifdef HAVE_SNDFILE_1_0_12
    case ST_ENCODING_FLAC:
      switch (size) {
      case ST_SIZE_8BIT:
        return SF_FORMAT_PCM_S8;
      case ST_SIZE_16BIT:
        return SF_FORMAT_PCM_16;
      case ST_SIZE_24BIT:
        return SF_FORMAT_PCM_24;
      case ST_SIZE_32BIT:
        return SF_FORMAT_PCM_32;
      default: /* invalid size */
        return 0;
      }
      break;
#endif
    case ST_ENCODING_FLOAT:
      return SF_FORMAT_FLOAT;
    case ST_ENCODING_GSM:
      return SF_FORMAT_GSM610;
    default: /* Bad encoding */
      return 0;
    }
  }
}

int st_sndfile_startwrite(ft_t ft)
{
  sndfile_t sf = (sndfile_t)ft->priv;
  int subtype = sndfile_format(ft->signal.encoding, ft->signal.size);
  sf->sf_info = (SF_INFO *)xmalloc(sizeof(SF_INFO));

  /* Copy format info */
  if (strcmp(ft->filetype, "sndfile") == 0)
    sf->sf_info->format = name_to_format(ft->filename) | subtype;
  else
    sf->sf_info->format = name_to_format(ft->filetype) | subtype;
  sf->sf_info->samplerate = ft->signal.rate;
  sf->sf_info->channels = ft->signal.channels;
  sf->sf_info->frames = ft->length / ft->signal.channels;

  /* If output format is invalid, try to find a sensible default */
  if (!sf_format_check(sf->sf_info)) {
    SF_FORMAT_INFO format_info;
    int i, count;

    st_warn("cannot use desired output encoding, choosing default");
    sf_command(sf->sf_file, SFC_GET_SIMPLE_FORMAT_COUNT, &count, sizeof(int));
    for (i = 0; i < count; i++) {
      format_info.format = i;
      sf_command(sf->sf_file, SFC_GET_SIMPLE_FORMAT, &format_info, sizeof(format_info));
      if ((format_info.format & SF_FORMAT_TYPEMASK) == (sf->sf_info->format & SF_FORMAT_TYPEMASK)) {
        sf->sf_info->format = format_info.format;
        /* FIXME: Print out exactly what we chose, needs sndfile ->
           sox encoding conversion functions */
        break;
      }
    }

    if (!sf_format_check(sf->sf_info)) {
      st_fail("cannot find a usable output encoding");
      return ST_EOF;
    }
  }

  if ((sf->sf_file = sf_open(ft->filename, SFM_WRITE, sf->sf_info)) == NULL) {
    st_fail("sndfile cannot open file for writing: %s", sf_strerror(sf->sf_file));
    return ST_EOF;
  }

  return ST_SUCCESS;
}

/*
 * Write len samples of type st_sample_t from buf[] to file.
 * Return number of samples written.
 */
st_size_t st_sndfile_write(ft_t ft, const st_sample_t *buf, st_size_t len)
{
  sndfile_t sf = (sndfile_t)ft->priv;

  /* FIXME: We assume int == st_sample_t here */
  return (st_size_t)sf_write_int(sf->sf_file, (int *)buf, len);
}

/*
 * Close file for libsndfile (this doesn't close the file handle)
 */
int st_sndfile_stopwrite(ft_t ft)
{
  sndfile_t sf = (sndfile_t)ft->priv;
  sf_close(sf->sf_file);
  return ST_SUCCESS;
}

int st_sndfile_seek(ft_t ft, st_size_t offset)
{
  sndfile_t sf = (sndfile_t)ft->priv;
  sf_seek(sf->sf_file, offset / ft->signal.channels, SEEK_CUR);
  return ST_SUCCESS;
}

/* Format file suffixes */
/* For now, comment out formats built in to SoX */
static const char *names[] = {
  "sndfile", /* special type to force use of sndfile */
  /* "aif", */
  /* "wav", */
  /* "au", */
#ifdef HAVE_SNDFILE_1_0_12
  "caf",
#endif
  /* "flac", */
  /* "snd", */
  /* "svx", */
  "paf",
  "fap",
  /* "gsm", */
  /* "nist", */
  /* "ircam", */
  /* "sf", */
  /* "voc", */
  "w64",
  /* "raw", */
  "mat4",
  "mat5",
  "mat",
  "pvf",
  "sds",
  "sd2",
  /* "vox", */
  "xi",
  NULL
};

/* Format descriptor */
static st_format_t st_sndfile_format = {
  names,
  NULL,
  ST_FILE_SEEK,
  st_sndfile_startread,
  st_sndfile_read,
  st_sndfile_stopread,
  st_sndfile_startwrite,
  st_sndfile_write,
  st_sndfile_stopwrite,
  st_sndfile_seek
};

const st_format_t *st_sndfile_format_fn(void)
{
  return &st_sndfile_format;
}

#endif
