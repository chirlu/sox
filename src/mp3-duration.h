/*
 * Determine MP3 duration
 * Copyright (c) 2007 robs@users.sourceforge.net
 * Based on original ideas by Regis Boudin, Thibaut Varene & Pascal Giard
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, write to the Free Software Foundation,
 * Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301, USA.
 */

#include <sys/stat.h>

#if HAVE_ID3TAG && HAVE_UNISTD_H

static id3_utf8_t * utf8_id3tag_findframe(
    struct id3_tag * tag, const char * const frameid, unsigned index)
{
  struct id3_frame const * frame = id3_tag_findframe(tag, frameid, index);
  if (frame) {
    union id3_field  const * field = id3_frame_field(frame, 1);
    unsigned nstrings = id3_field_getnstrings(field);
    while (nstrings--){
      id3_ucs4_t const * ucs4 = id3_field_getstrings(field, nstrings);
      if (ucs4)
        return id3_ucs4_utf8duplicate(ucs4); /* Must call free() on this */
    }
  }
  return NULL;
}

static sox_size_t id3tag_duration_ms(FILE * fp)
{
  struct id3_file   * id3struct;
  struct id3_tag    * tag;
  id3_utf8_t        * utf8;
  sox_size_t        duration_ms = 0;
  int               fd = dup(fileno(fp));

  if ((id3struct = id3_file_fdopen(fd, ID3_FILE_MODE_READONLY))) {
    if ((tag = id3_file_tag(id3struct)) && tag->frames)
      if ((utf8 = utf8_id3tag_findframe(tag, "TLEN", 0))) {
        if (atoi((char *)utf8) > 0)
          duration_ms = atoi((char *)utf8);
        free(utf8);
      }
    id3_file_close(id3struct);
  }
  else close(fd);
  return duration_ms;
}

#endif

static unsigned long xing_frames(struct mad_bitptr ptr, unsigned bitlen)
{
  #define XING_MAGIC ( ('X' << 24) | ('i' << 16) | ('n' << 8) | 'g' )
  if (bitlen >= 96 && mad_bit_read(&ptr, 32) == XING_MAGIC &&
      (mad_bit_read(&ptr, 32) & 1 )) /* XING_FRAMES */
    return mad_bit_read(&ptr, 32);
  return 0;
}

static void mad_timer_mult(mad_timer_t * t, double d)
{
  t->seconds = d *= (t->seconds + t->fraction * (1. / MAD_TIMER_RESOLUTION));
  t->fraction = (d - t->seconds) * MAD_TIMER_RESOLUTION + .5;
}

static sox_size_t mp3_duration_ms(FILE * fp, unsigned char *buffer)
{
  struct mad_stream   mad_stream;
  struct mad_header   mad_header;
  struct mad_frame    mad_frame;
  mad_timer_t         time = mad_timer_zero;
  sox_size_t          initial_bitrate, tagsize = 0, consumed = 0, frames = 0;
  sox_bool            vbr = sox_false, padded = sox_false;

#if HAVE_ID3TAG && HAVE_UNISTD_H
  sox_size_t duration_ms = id3tag_duration_ms(fp);
  if (duration_ms) {
    sox_debug("got exact duration from ID3 TLEN");
    return duration_ms;
  }
#endif

  mad_stream_init(&mad_stream);
  mad_header_init(&mad_header);
  mad_frame_init(&mad_frame);

  while (sox_true) {
    int read, padding = 0;
    size_t leftover = mad_stream.bufend - mad_stream.next_frame;
    memcpy(buffer, mad_stream.this_frame, leftover);
    read = fread(buffer + leftover, 1, INPUT_BUFFER_SIZE - leftover, fp);
    if (read <= 0) {
      sox_debug("got exact duration by scan to EOF (frames=%u leftover=%u)", frames, leftover);
      break;
    }
    for (; !padded && padding < read && !buffer[padding]; ++padding);
    padded = sox_true;
    mad_stream_buffer(&mad_stream, buffer + padding, leftover + read - padding);

    while (sox_true) {
      mad_stream.error = MAD_ERROR_NONE;
      if (mad_header_decode(&mad_header, &mad_stream) == -1) {
        if (mad_stream.error == MAD_ERROR_BUFLEN)
          break;  /* Get some more data from the file */
        if (!MAD_RECOVERABLE(mad_stream.error)) {
          sox_warn("unrecoverable MAD error");
          break;
        }
        if (mad_stream.error == MAD_ERROR_LOSTSYNC) {
          unsigned available = (mad_stream.bufend - mad_stream.this_frame);
          tagsize = tagtype(mad_stream.this_frame, available);
          if (tagsize > 0) {   /* It's just some ID3 tags, so skip */
            if (tagsize >= available) {
              fseeko(fp, (off_t)(tagsize - available), SEEK_CUR);
              padded = sox_false;
            }
            mad_stream_skip(&mad_stream, min(tagsize, available));
            continue;
          }
          sox_warn("MAD lost sync");
          continue;
        }
        sox_warn("recoverable MAD error");
        continue;
      }

      mad_timer_add(&time, mad_header.duration);
      consumed += mad_stream.next_frame - mad_stream.this_frame;

      if (!frames) {
        initial_bitrate = mad_header.bitrate;

        /* Get the precise frame count from the XING header if present */
        mad_frame.header = mad_header;
        if (mad_frame_decode(&mad_frame, &mad_stream) == -1)
          if (!MAD_RECOVERABLE(mad_stream.error)) {
            sox_warn("unrecoverable MAD error");
            break;
          }
        if ((frames = xing_frames(mad_stream.anc_ptr, mad_stream.anc_bitlen))) {
          mad_timer_multiply(&time, (signed long)frames);
          sox_debug("got exact duration from XING frame count (%u)", frames);
          break;
        }
      }
      else vbr |= initial_bitrate != mad_header.bitrate;

      /* If not VBR, we can time just a few frames then extrapolate */
      if (++frames == 10 && !vbr) {
        struct stat filestat;
        fstat(fileno(fp), &filestat);
        mad_timer_mult(&time, (double)(filestat.st_size - tagsize) / consumed);
        sox_debug("got approx. duration by FBR extrapolation");
        break;
      }
    }

    if (mad_stream.error != MAD_ERROR_BUFLEN)
      break;
  }
  mad_frame_finish(&mad_frame);
  mad_header_finish(&mad_header);
  mad_stream_finish(&mad_stream);
  rewind(fp);
  return mad_timer_count(time, MAD_UNITS_MILLISECONDS);
}
