/*
 * Sound Tools skeleton file format driver.
 *
 * Copyright 1999 Chris Bagwell And Sundry Contributors
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

#include <string.h>

/* Private data for SKEL file */
typedef struct skelform
{
  st_size_t remaining_samples;
} *skelform_t;

assert_static(sizeof(struct skelform) <= ST_MAX_FILE_PRIVSIZE, 
              /* else */ skel_PRIVSIZE_too_big);

/* Note that if any of your methods doesn't need to do anything, you
   can instead use the relevant st_*_nothing* method */

/*
 * Do anything required before you start reading samples.
 * Read file header.
 *      Find out sampling rate,
 *      size and encoding of samples,
 *      mono/stereo/quad.
 */
static int startread(ft_t ft)
{
  skelform_t sk = (skelform_t)ft->priv;
  st_size_t samples_in_file;

  /* If you need to seek around the input file. */
  if (!ft->seekable) {
    st_fail_errno(ft, ST_EOF, "skel inputfile must be a file");
    return ST_EOF;
  }

  /*
   * If your format is headerless and has fixed values for
   * the following items, you can hard code them here (see cdr.c).
   * If your format contains a header with format information
   * then you should set it here.
   */
  ft->signal.rate =  44100;
  ft->signal.size = ST_SIZE_BYTE; /* or WORD ... */
  ft->signal.encoding = ST_ENCODING_UNSIGNED; /* or SIGN2 ... */
  ft->signal.channels = 1; /* or 2 or 4 */
  {
    char *comment = "any comment in file header.";
    ft->comment = xmalloc(sizeof(comment));
    strcpy(ft->comment, comment);
  }

  /* If your format doesn't have a header then samples_in_file
   * can be determined by the file size.
   */
  samples_in_file = st_filelength(ft) / ft->signal.size;

  /* If you can detect the length of your file, record it here. */
  ft->length = samples_in_file;
  sk->remaining_samples = samples_in_file;

  return ST_SUCCESS;
}

/*
 * Read up to len samples of type st_sample_t from file into buf[].
 * Return number of samples read, or 0 if at end of file.
 */
static st_size_t read(ft_t ft, st_sample_t *buf, st_size_t len)
{
  skelform_t sk = (skelform_t)ft->priv;
  st_size_t done;
  unsigned char sample;

  for (done = 0; done < len; done++) {
    if (feof(ft->fp)) /* no more samples */
      break;
    sample = fgetc(ft->fp);
    switch (ft->signal.size) {
    case ST_SIZE_BYTE:
      switch (ft->signal.encoding) {
      case ST_ENCODING_UNSIGNED:
        *buf++ = ST_UNSIGNED_BYTE_TO_SAMPLE(sample,);
        break;
      default:
        st_fail("Undetected sample encoding in read!");
        exit(2);
      }
      break;
    default:
      st_fail("Undetected bad sample size in read!");
      exit(2);
    }
  }

  return done;
}

/*
 * Do anything required when you stop reading samples.
 * Don't close input file!
 */
static int stopread(ft_t ft)
{
  return ST_SUCCESS;
}

static int startwrite(ft_t ft)
{
  skelform_t sk = (skelform_t)ft->priv;

  /* If you have to seek around the output file. */
  /* If header contains a length value then seeking will be
   * required.  Instead of failing, it's sometimes nice to
   * just set the length to max value and not fail.
   */
  if (!ft->seekable) {
    st_fail("Output .skel file must be a file, not a pipe");
    return ST_EOF;
  }

  if (ft->signal.rate != 44100)
    st_fail("Output .skel file must have a sample rate of 44100Hz");

  if (ft->signal.size == -1) {
    st_fail("Did not specify a size for .skel output file");
    return ST_EOF;
  }

  /* error check ft->signal.encoding */
  /* error check ft->signal.channels */

  /* Write file header, if any */
  /* Write comment field, if any */

  return ST_SUCCESS;

}

/*
 * Write len samples of type st_sample_t from buf[] to file.
 * Return number of samples written.
 */
static st_size_t write(ft_t ft, const st_sample_t *buf, st_size_t len)
{
  skelform_t sk = (skelform_t)ft->priv;

  switch (ft->signal.size) {
  case ST_SIZE_BYTE:
    switch (ft->signal.encoding) {
    case ST_ENCODING_UNSIGNED:
      while (len--) {
        len = st_writeb(ft, ST_SAMPLE_TO_UNSIGNED_BYTE(*buf++, ft->clips));
        if (len == 0)
          break;
      }
      break;
    default:
      st_fail("Undetected bad sample encoding in write!");
      exit(2);
    }
    break;
  default:
    st_fail("Undetected bad sample size in write!");
    exit(2);
  }

  return len;
}

static int stopwrite(ft_t ft)
{
  /* All samples are already written out. */
  /* If file header needs fixing up, for example it needs the number
     of samples in a field, seek back and write them here. */
  return ST_SUCCESS;
}

static int seek(ft_t ft, st_size_t offset)
{
  /* Seek relative to current position. */
  return ST_SUCCESS;
}

/* Format file suffixes */
static const char *names[] = {
  "skel",
  NULL
};

/* Format descriptor */
static st_format_t st_skel_format = {
  names,
  NULL,
  ST_FILE_SEEK,
  startread,
  read,
  stopread,
  startwrite,
  write,
  stopwrite,
  seek
};

const st_format_t *st_skel_format_fn(void)
{
  return &st_skel_format;
}
