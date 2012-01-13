/* libSoX MP3 utilities  Copyright (c) 2007-9 SoX contributors
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

#include <sys/stat.h>

#ifdef USING_ID3TAG

static char const * id3tagmap[][2] =
{
  {"TIT2", "Title"},
  {"TPE1", "Artist"},
  {"TALB", "Album"},
  {"TRCK", "Tracknumber"},
  {"TDRC", "Year"},
  {"TCON", "Genre"},
  {"COMM", "Comment"},
  {"TPOS", "Discnumber"},
  {NULL, NULL}
};

#endif /* USING_ID3TAG */

#if defined(HAVE_LAME)

static void write_comments(sox_format_t * ft)
{
  priv_t *p = (priv_t *) ft->priv;
  const char* comment;

  p->id3tag_init(p->gfp);
  p->id3tag_set_pad(p->gfp, (size_t)ID3PADDING);

  /* Note: id3tag_set_fieldvalue is not present in LAME 3.97, so we're using
     the 3.97-compatible methods for all of the tags that 3.97 supported. */
  /* FIXME: This is no more necessary, since support for LAME 3.97 has ended. */
  if ((comment = sox_find_comment(ft->oob.comments, "Title")))
    p->id3tag_set_title(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Artist")))
    p->id3tag_set_artist(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Album")))
    p->id3tag_set_album(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Tracknumber")))
    p->id3tag_set_track(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Year")))
    p->id3tag_set_year(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Comment")))
    p->id3tag_set_comment(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Genre")))
  {
    if (p->id3tag_set_genre(p->gfp, comment))
      lsx_warn("\"%s\" is not a recognized ID3v1 genre.", comment);
  }

  if ((comment = sox_find_comment(ft->oob.comments, "Discnumber")))
  {
    char* id3tag_buf = lsx_malloc(strlen(comment) + 6);
    if (id3tag_buf)
    {
      sprintf(id3tag_buf, "TPOS=%s", comment);
      p->id3tag_set_fieldvalue(p->gfp, id3tag_buf);
      free(id3tag_buf);
    }
  }
}

#endif /* HAVE_LAME */

#ifdef USING_ID3TAG

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

static void read_comments(sox_format_t * ft)
{
  struct id3_file   * id3struct;
  struct id3_tag    * tag;
  id3_utf8_t        * utf8;
  int               i, fd = dup(fileno((FILE*)ft->fp));

  if ((id3struct = id3_file_fdopen(fd, ID3_FILE_MODE_READONLY))) {
    if ((tag = id3_file_tag(id3struct)) && tag->frames)
      for (i = 0; id3tagmap[i][0]; ++i)
        if ((utf8 = utf8_id3tag_findframe(tag, id3tagmap[i][0], 0))) {
          char * comment = lsx_malloc(strlen(id3tagmap[i][1]) + 1 + strlen((char *)utf8) + 1);
          sprintf(comment, "%s=%s", id3tagmap[i][1], utf8);
          sox_append_comment(&ft->oob.comments, comment);
          free(comment);
          free(utf8);
        }
      if ((utf8 = utf8_id3tag_findframe(tag, "TLEN", 0))) {
        unsigned long tlen = strtoul((char *)utf8, NULL, 10);
        if (tlen > 0 && tlen < ULONG_MAX) {
          ft->signal.length= tlen; /* In ms; convert to samples later */
          lsx_debug("got exact duration from ID3 TLEN");
        }
        free(utf8);
      }
    id3_file_close(id3struct);
  }
  else close(fd);
}

#endif /* USING_ID3TAG */

#ifdef HAVE_MAD_H

static unsigned long xing_frames(priv_t * p, struct mad_bitptr ptr, unsigned bitlen)
{
  #define XING_MAGIC ( ('X' << 24) | ('i' << 16) | ('n' << 8) | 'g' )
  if (bitlen >= 96 && p->mad_bit_read(&ptr, 32) == XING_MAGIC &&
      (p->mad_bit_read(&ptr, 32) & 1 )) /* XING_FRAMES */
    return p->mad_bit_read(&ptr, 32);
  return 0;
}

static void mad_timer_mult(mad_timer_t * t, double d)
{
  t->seconds = (signed long)(d *= (t->seconds + t->fraction * (1. / MAD_TIMER_RESOLUTION)));
  t->fraction = (unsigned long)((d - t->seconds) * MAD_TIMER_RESOLUTION + .5);
}

static size_t mp3_duration_ms(sox_format_t * ft)
{
  priv_t              * p = (priv_t *) ft->priv;
  FILE                * fp = ft->fp;
  struct mad_stream   mad_stream;
  struct mad_header   mad_header;
  struct mad_frame    mad_frame;
  mad_timer_t         time = mad_timer_zero;
  size_t              initial_bitrate = 0; /* Initialised to prevent warning */
  size_t              tagsize = 0, consumed = 0, frames = 0;
  sox_bool            vbr = sox_false, depadded = sox_false;

  p->mad_stream_init(&mad_stream);
  p->mad_header_init(&mad_header);
  p->mad_frame_init(&mad_frame);

  do {  /* Read data from the MP3 file */
    int read, padding = 0;
    size_t leftover = mad_stream.bufend - mad_stream.next_frame;

    memcpy(p->mp3_buffer, mad_stream.this_frame, leftover);
    read = fread(p->mp3_buffer + leftover, (size_t) 1, p->mp3_buffer_size - leftover, fp);
    if (read <= 0) {
      lsx_debug("got exact duration by scan to EOF (frames=%" PRIuPTR " leftover=%" PRIuPTR ")", frames, leftover);
      break;
    }
    for (; !depadded && padding < read && !p->mp3_buffer[padding]; ++padding);
    depadded = sox_true;
    p->mad_stream_buffer(&mad_stream, p->mp3_buffer + padding, leftover + read - padding);

    while (sox_true) {  /* Decode frame headers */
      mad_stream.error = MAD_ERROR_NONE;
      if (p->mad_header_decode(&mad_header, &mad_stream) == -1) {
        if (mad_stream.error == MAD_ERROR_BUFLEN)
          break;  /* Normal behaviour; get some more data from the file */
        if (!MAD_RECOVERABLE(mad_stream.error)) {
          lsx_warn("unrecoverable MAD error");
          break;
        }
        if (mad_stream.error == MAD_ERROR_LOSTSYNC) {
          unsigned available = (mad_stream.bufend - mad_stream.this_frame);
          tagsize = tagtype(mad_stream.this_frame, (size_t) available);
          if (tagsize) {   /* It's some ID3 tags, so just skip */
            if (tagsize >= available) {
              fseeko(fp, (off_t)(tagsize - available), SEEK_CUR);
              depadded = sox_false;
            }
            p->mad_stream_skip(&mad_stream, min(tagsize, available));
          }
          else lsx_warn("MAD lost sync");
        }
        else lsx_warn("recoverable MAD error");
        continue; /* Not an audio frame */
      }

      p->mad_timer_add(&time, mad_header.duration);
      consumed += mad_stream.next_frame - mad_stream.this_frame;

      lsx_debug_more("bitrate=%lu", mad_header.bitrate);
      if (!frames) {
        initial_bitrate = mad_header.bitrate;

        /* Get the precise frame count from the XING header if present */
        mad_frame.header = mad_header;
        if (p->mad_frame_decode(&mad_frame, &mad_stream) == -1)
          if (!MAD_RECOVERABLE(mad_stream.error)) {
            lsx_warn("unrecoverable MAD error");
            break;
          }
        if ((frames = xing_frames(p, mad_stream.anc_ptr, mad_stream.anc_bitlen))) {
          p->mad_timer_multiply(&time, (signed long)frames);
          lsx_debug("got exact duration from XING frame count (%" PRIuPTR ")", frames);
          break;
        }
      }
      else vbr |= mad_header.bitrate != initial_bitrate;

      /* If not VBR, we can time just a few frames then extrapolate */
      if (++frames == 25 && !vbr) {
        struct stat filestat;
        fstat(fileno(fp), &filestat);
        mad_timer_mult(&time, (double)(filestat.st_size - tagsize) / consumed);
        lsx_debug("got approx. duration by CBR extrapolation");
        break;
      }
    }
  } while (mad_stream.error == MAD_ERROR_BUFLEN);

  p->mad_frame_finish(&mad_frame);
  mad_header_finish(&mad_header);
  p->mad_stream_finish(&mad_stream);
  rewind(fp);
  return p->mad_timer_count(time, MAD_UNITS_MILLISECONDS);
}

#endif /* HAVE_MAD_H */
