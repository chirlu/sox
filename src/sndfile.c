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

#include <stdio.h>
#include <string.h>
#include <sndfile.h>

/* Private data for sndfile files */
typedef struct sndfile
{
  SNDFILE *sf_file;
  SF_INFO *sf_info;
} *sndfile_t;

assert_static(sizeof(struct sndfile) <= ST_MAX_FILE_PRIVSIZE, 
              /* else */ sndfile_PRIVSIZE_too_big);

/*
 * Open file in sndfile.
 */
int st_sndfile_startread(ft_t ft)
{
  sndfile_t sf = (sndfile_t)ft->priv;

  sf->sf_info = (SF_INFO *)xcalloc(1, sizeof(SF_INFO));
  /* We'd like to use sf_open, but auto file typing has already
     invoked stdio buffering. */
  if ((sf->sf_file = sf_open(ft->filename, SFM_READ, sf->sf_info)) == NULL) {
    st_fail("sndfile cannot open file for reading: %s %x", sf_strerror(sf->sf_file), sf->sf_info->format);
    free(sf->sf_file);
    return ST_EOF;
  }

  /* Copy format info */
  ft->signal.rate = sf->sf_info->samplerate;
  ft->signal.size = ST_SIZE_32BIT;
  ft->signal.encoding = ST_ENCODING_UNSIGNED;
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
  { "aif",	3,	SF_FORMAT_AIFF	},
  { "wav",	0,	SF_FORMAT_WAV	},
  { "au",	0,	SF_FORMAT_AU	},
  { "caf",	0,	SF_FORMAT_CAF	},
  { "flac",	0,	SF_FORMAT_FLAC	},
  { "snd",	0,	SF_FORMAT_AU	},
  { "svx",	0,	SF_FORMAT_SVX	},
  { "paf",	0,	SF_ENDIAN_BIG | SF_FORMAT_PAF	},
  { "fap",	0,	SF_ENDIAN_LITTLE | SF_FORMAT_PAF },
  { "gsm",	0,	SF_FORMAT_RAW	},
  { "nist", 	0,	SF_FORMAT_NIST	},
  { "ircam",	0,	SF_FORMAT_IRCAM	},
  { "sf",	0, 	SF_FORMAT_IRCAM	},
  { "voc",	0, 	SF_FORMAT_VOC	},
  { "w64", 	0, 	SF_FORMAT_W64	},
  { "raw",	0,	SF_FORMAT_RAW	},
  { "mat4", 	0,	SF_FORMAT_MAT4	},
  { "mat5", 	0, 	SF_FORMAT_MAT5 	},
  { "mat",	0, 	SF_FORMAT_MAT4 	},
  { "pvf",	0, 	SF_FORMAT_PVF 	},
  { "sds",	0, 	SF_FORMAT_SDS 	},
  { "sd2",	0, 	SF_FORMAT_SD2 	},
  { "vox",	0, 	SF_FORMAT_RAW 	},
  { "xi",	0, 	SF_FORMAT_XI 	}
};

static int guess_output_file_type(const char *type, int format)
{
  int k;

  format &= SF_FORMAT_SUBMASK;

  if (strcmp(type, "gsm") == 0)
    return SF_FORMAT_RAW | SF_FORMAT_GSM610;

  if (strcmp(type, "vox") == 0)
    return SF_FORMAT_RAW | SF_FORMAT_VOX_ADPCM;

  for (k = 0; k < (int)(sizeof(format_map) / sizeof(format_map [0])); k++) {
    if (format_map[k].len > 0 && strncmp(type, format_map[k].ext, format_map[k].len) == 0)
      return format_map[k].format | format;
    else if (strcmp(type, format_map[k].ext) == 0)
      return format_map[k].format | format;
  }

  return 0;
}

int st_sndfile_startwrite(ft_t ft)
{
  sndfile_t sf = (sndfile_t)ft->priv;
  sf->sf_info = (SF_INFO *)xmalloc(sizeof(SF_INFO));

  /* Copy format info */
  /* FIXME: Need to have a table of suitable default subtypes */
  sf->sf_info->format = guess_output_file_type(ft->filetype, SF_FORMAT_PCM_16);
  sf->sf_info->samplerate = ft->signal.rate;
  sf->sf_info->channels = ft->signal.channels;
  sf->sf_info->frames = ft->length / ft->signal.channels;

  if (!sf_format_check(sf->sf_info)) {
    st_fail("invalid sndfile output format");
    return ST_EOF;
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
/* For now, comment out formats built-in to SoX */
static const char *names[] = {
  /* "aif", */
  /* "wav", */
  /* "au", */
  "caf",
  /* "flac", */
  /* "snd", */
  /* "svx", */
  "paf",
  "fap",
  /* "gsm", */
  "nist",
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
