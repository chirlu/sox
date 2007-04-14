/*
 * libSoX ffmpeg formats.
 *
 * Copyright 2007 Reuben Thomas <rrt@sc3d.org>
 *
 * Based on ffplay.c Copyright 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include "sox_i.h"

#ifdef HAVE_LIBAVFORMAT

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <ffmpeg/avformat.h>

/* Private data for ffmpeg files */
typedef struct ffmpeg
{
  uint8_t *audio_buf;
  int audio_index;
  int audio_stream;
  AVStream *audio_st;
  AVFormatContext *ic;
  int audio_buf_index;
  int audio_buf_size;
  AVPacket audio_pkt;
  uint8_t *audio_pkt_data;
  int audio_pkt_size;
} *ffmpeg_t;

assert_static(sizeof(struct ffmpeg) <= SOX_MAX_FILE_PRIVSIZE, 
              /* else */ ffmpeg_PRIVSIZE_too_big);

/* open a given stream. Return 0 if OK */
static int stream_component_open(ffmpeg_t ffmpeg, int stream_index)
{
  AVFormatContext *ic = ffmpeg->ic;
  AVCodecContext *enc;
  AVCodec *codec;

  if (stream_index < 0 || stream_index >= (int)(ic->nb_streams))
    return -1;
  enc = ic->streams[stream_index]->codec;

  /* hack for AC3. XXX: suppress that */
  if (enc->channels > 2)
    enc->channels = 2;

  codec = avcodec_find_decoder(enc->codec_id);
  enc->workaround_bugs = 1;
  enc->error_resilience= 1;
  if (!codec || avcodec_open(enc, codec) < 0)
    return -1;
  if (enc->codec_type != CODEC_TYPE_AUDIO) {
    sox_fail("ffmpeg CODEC %x is not an audio CODEC", enc->codec_type);
    return -1;
  }

  ffmpeg->audio_stream = stream_index;
  ffmpeg->audio_st = ic->streams[stream_index];
  ffmpeg->audio_buf_size = 0;
  ffmpeg->audio_buf_index = 0;

  memset(&ffmpeg->audio_pkt, 0, sizeof(ffmpeg->audio_pkt));

  return 0;
}

static void stream_component_close(ffmpeg_t ffmpeg, int stream_index)
{
  AVFormatContext *ic = ffmpeg->ic;
  AVCodecContext *enc;

  if (stream_index < 0 || stream_index >= (int)ic->nb_streams)
    return;
  enc = ic->streams[stream_index]->codec;

  avcodec_close(enc);
}

/* decode one audio frame and returns its uncompressed size */
static int audio_decode_frame(ffmpeg_t ffmpeg, uint8_t *audio_buf, int buf_size)
{
  AVPacket *pkt = &ffmpeg->audio_pkt;
  int len1, data_size;

  for(;;) {
    /* NOTE: the audio packet can contain several frames */
    while (ffmpeg->audio_pkt_size > 0) {
      data_size = buf_size;
      len1 = avcodec_decode_audio2(ffmpeg->audio_st->codec,
                                   (int16_t *)audio_buf, &data_size,
                                   ffmpeg->audio_pkt_data, ffmpeg->audio_pkt_size);
      if (len1 < 0) {
        /* if error, we skip the frame */
        ffmpeg->audio_pkt_size = 0;
        break;
      }

      ffmpeg->audio_pkt_data += len1;
      ffmpeg->audio_pkt_size -= len1;
      if (data_size <= 0)
        continue;
      return data_size;
    }

    ffmpeg->audio_pkt_data = pkt->data;
    ffmpeg->audio_pkt_size = pkt->size;
  }
}

/*
 * Open file in ffmpeg.
 */
static int sox_ffmpeg_startread(ft_t ft)
{
  ffmpeg_t ffmpeg = (ffmpeg_t)ft->priv;
  AVFormatParameters params;
  int ret;
  unsigned i;

  ffmpeg->audio_buf = xcalloc(1, AVCODEC_MAX_AUDIO_FRAME_SIZE);

  /* Signal audio stream not found */
  ffmpeg->audio_index = -1;
  
  /* register all CODECs, demux and protocols */
  av_register_all();

  /* Open file and get format */
  memset(&params, 0, sizeof(params));
  if ((ret = av_open_input_file(&ffmpeg->ic, ft->filename, NULL, 0, &params)) < 0) {
    sox_fail("ffmpeg cannot open file for reading: %s (code %d)", ft->filename, ret);
    return SOX_EOF;
  }

  /* Get CODEC parameters */
  if ((ret = av_find_stream_info(ffmpeg->ic)) < 0) {
    sox_fail("ffmpeg could not find CODEC parameters for %s", ft->filename);
    return SOX_EOF;
  }

  /* Now we can begin to play (RTSP stream only) */
  av_read_play(ffmpeg->ic);

  /* Find audio stream (assume we're using the first) */
  for (i = 0; i < ffmpeg->ic->nb_streams; i++) {
    AVCodecContext *enc = ffmpeg->ic->streams[i]->codec;
    if (enc->codec_type == CODEC_TYPE_AUDIO && ffmpeg->audio_index < 0) {
      ffmpeg->audio_index = i;
      break;
    }
  }

  /* Open the stream */
  if (ffmpeg->audio_index < 0 ||
      stream_component_open(ffmpeg, ffmpeg->audio_index) < 0 ||
      ffmpeg->audio_stream < 0) {
    sox_fail("could not open CODECs for %s", ft->filename);
    return SOX_EOF;
  }

  /* Copy format info */
  ft->signal.rate = ffmpeg->audio_st->codec->sample_rate;
  ft->signal.size = SOX_SIZE_16BIT;
  ft->signal.encoding = SOX_ENCODING_SIGN2;
  ft->signal.channels = ffmpeg->audio_st->codec->channels;
  ft->length = 0; /* Currently we can't seek; no idea how to get this
                     info from ffmpeg anyway (in time, yes, but not in
                     samples); but ffmpeg *can* seek */

  return SOX_SUCCESS;
}

/*
 * Read up to len samples of type sox_sample_t from file into buf[].
 * Return number of samples read.
 */
static sox_size_t sox_ffmpeg_read(ft_t ft, sox_ssample_t *buf, sox_size_t len)
{
  ffmpeg_t ffmpeg = (ffmpeg_t)ft->priv;
  AVPacket *pkt = &ffmpeg->audio_pkt;
  int ret;
  sox_size_t nsamp = 0, nextra;

  /* Read data repeatedly until buf is full or no more can be read */
  do {
    /* If buffer empty, read more data */
    if (ffmpeg->audio_buf_index * 2 >= ffmpeg->audio_buf_size) {
      if ((ret = av_read_frame(ffmpeg->ic, pkt)) < 0)
        break;
      ffmpeg->audio_buf_size = audio_decode_frame(ffmpeg, ffmpeg->audio_buf, AVCODEC_MAX_AUDIO_FRAME_SIZE);
      ffmpeg->audio_buf_index = 0;
    }

    nextra = min((ffmpeg->audio_buf_size - ffmpeg->audio_buf_index) / 2, (int)(len - nsamp));
    for (; nextra > 0; nextra--)
      buf[nsamp++] = SOX_SIGNED_16BIT_TO_SAMPLE(((int16_t *)ffmpeg->audio_buf)[ffmpeg->audio_buf_index++],);
  } while (nsamp < len && nextra > 0);

  return nsamp;
}

/*
 * Close file for ffmpeg (this doesn't close the file handle)
 */
static int sox_ffmpeg_stopread(ft_t ft)
{
  ffmpeg_t ffmpeg = (ffmpeg_t)ft->priv;

  if (ffmpeg->audio_stream >= 0)
    stream_component_close(ffmpeg, ffmpeg->audio_stream);
  if (ffmpeg->ic) {
    av_close_input_file(ffmpeg->ic);
    ffmpeg->ic = NULL; /* safety */
  }
  
  return SOX_SUCCESS;
}

static int sox_ffmpeg_startwrite(ft_t ft)
{
  (void)ft;
  sox_fail("Cannot (yet) write with ffmpeg");
  return SOX_EOF;
}

/* Format file suffixes */
/* For now, comment out formats built in to SoX */
static const char *names[] = {
  "ffmpeg", /* special type to force use of ffmpeg */
  "mp4",
  "m4a",
  "avi",
  "wmv",
  "mpg",
  NULL
};

/* Format descriptor */
static sox_format_t sox_ffmpeg_format = {
  names,
  SOX_FILE_NOSTDIO,
  sox_ffmpeg_startread,
  sox_ffmpeg_read,
  sox_ffmpeg_stopread,
  sox_ffmpeg_startwrite,
  sox_format_nothing_write,
  sox_format_nothing,
  sox_format_nothing_seek
};

const sox_format_t *sox_ffmpeg_format_fn(void)
{
  return &sox_ffmpeg_format;
}

#endif
