/* libSoX ffmpeg formats.
 *
 * Copyright 2007, 2011 Reuben Thomas <rrt@sc3d.org>
 *
 * Based on ffplay.c and output_example.c Copyright 2003 Fabrice Bellard
 * Note: ffplay.c is distributed under the LGPL 2.1 or later;
 * output_example.c is under the MIT license:
 *-----------------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *-----------------------------------------------------------------------
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

#ifdef HAVE_FFMPEG

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "ffmpeg.h"

#ifndef CODEC_TYPE_AUDIO
#define CODEC_TYPE_AUDIO AVMEDIA_TYPE_AUDIO
#endif
#ifndef PKT_FLAG_KEY
#define PKT_FLAG_KEY AV_PKT_FLAG_KEY
#endif

/* Private data for ffmpeg files */
typedef struct {
  int audio_index;
  int audio_stream;
  AVStream *audio_st;
  uint8_t *audio_buf_aligned;
  int audio_buf_index, audio_buf_size;
  int16_t *samples;
  int samples_index;
  AVOutputFormat *fmt;
  AVFormatContext *ctxt;
  int audio_input_frame_size;
  AVPacket audio_pkt;
  uint8_t *audio_buf_raw;
} priv_t;

/* open a given stream. Return 0 if OK */
static int stream_component_open(priv_t * ffmpeg, int stream_index)
{
  AVFormatContext *ic = ffmpeg->ctxt;
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
#if LIBAVCODEC_VERSION_INT < ((52<<16)+(0<<8)+0)
  enc->error_resilience = 1;
#else
  enc->error_recognition = 1;
#endif

  if (!codec || avcodec_open(enc, codec) < 0)
    return -1;
  if (enc->codec_type != AVMEDIA_TYPE_AUDIO) {
    lsx_fail("ffmpeg CODEC %x is not an audio CODEC", enc->codec_type);
    return -1;
  }

  ffmpeg->audio_stream = stream_index;
  ffmpeg->audio_st = ic->streams[stream_index];
  ffmpeg->audio_buf_size = 0;
  ffmpeg->audio_buf_index = 0;

  memset(&ffmpeg->audio_pkt, 0, sizeof(ffmpeg->audio_pkt));

  return 0;
}

static void stream_component_close(priv_t * ffmpeg, int stream_index)
{
  AVFormatContext *ic = ffmpeg->ctxt;
  AVCodecContext *enc;

  if (stream_index < 0 || stream_index >= (int)ic->nb_streams)
    return;
  enc = ic->streams[stream_index]->codec;

  avcodec_close(enc);
}

/* Decode one audio frame and returns its uncompressed size */
static int audio_decode_frame(priv_t * ffmpeg, uint8_t *audio_buf, int buf_size)
{
  AVPacket *pkt = &ffmpeg->audio_pkt;
  int len1, data_size;

  for (;;) {
    /* NOTE: the audio packet can contain several frames */
    while (ffmpeg->audio_pkt.size > 0) {
      data_size = buf_size;
      len1 = avcodec_decode_audio3(ffmpeg->audio_st->codec,
				   (int16_t *)audio_buf, &data_size,
				   pkt);
      if (len1 < 0) /* if error, we skip the rest of the packet */
	return 0;

      ffmpeg->audio_pkt.data += len1;
      ffmpeg->audio_pkt.size -= len1;
      if (data_size <= 0)
	continue;
      return data_size;
    }
  }
}

/* On some platforms, libavcodec wants the output buffer aligned to 16
 * bytes (because it uses SSE/Altivec internally). */
#define ALIGN16(p) ((uint8_t *)(p) + (16 - (size_t)(p) % 16))

static int startread(sox_format_t * ft)
{
  priv_t * ffmpeg = (priv_t *)ft->priv;
  AVFormatParameters params;
  int ret;
  int i;

  ffmpeg->audio_buf_raw = lsx_calloc(1, (size_t)AVCODEC_MAX_AUDIO_FRAME_SIZE + 32);
  ffmpeg->audio_buf_aligned = ALIGN16(ffmpeg->audio_buf_raw);

  /* Signal audio stream not found */
  ffmpeg->audio_index = -1;

  /* register all CODECs, demux and protocols */
  av_register_all();

  /* Open file and get format */
  memset(&params, 0, sizeof(params));
  if ((ret = av_open_input_file(&ffmpeg->ctxt, ft->filename, NULL, 0, &params)) < 0) {
    lsx_fail("ffmpeg cannot open file for reading: %s (code %d)", ft->filename, ret);
    return SOX_EOF;
  }

  /* Get CODEC parameters */
  if ((ret = av_find_stream_info(ffmpeg->ctxt)) < 0) {
    lsx_fail("ffmpeg could not find CODEC parameters for %s", ft->filename);
    return SOX_EOF;
  }

  /* Now we can begin to play (RTSP stream only) */
  av_read_play(ffmpeg->ctxt);

  /* Find audio stream (FIXME: allow different stream to be selected) */
  for (i = 0; (unsigned)i < ffmpeg->ctxt->nb_streams; i++) {
    AVCodecContext *enc = ffmpeg->ctxt->streams[i]->codec;
    if (enc->codec_type == AVMEDIA_TYPE_AUDIO && ffmpeg->audio_index < 0) {
      ffmpeg->audio_index = i;
      break;
    }
  }

  /* Open the stream */
  if (ffmpeg->audio_index < 0 ||
      stream_component_open(ffmpeg, ffmpeg->audio_index) < 0 ||
      ffmpeg->audio_stream < 0) {
    lsx_fail("ffmpeg could not open CODECs for %s", ft->filename);
    return SOX_EOF;
  }

  /* Copy format info */
  ft->signal.rate = ffmpeg->audio_st->codec->sample_rate;
  ft->encoding.bits_per_sample = 16;
  ft->encoding.encoding = SOX_ENCODING_SIGN2;
  ft->signal.channels = ffmpeg->audio_st->codec->channels;
  ft->signal.length = 0; /* Currently we can't seek; no idea how to get this
		     info from ffmpeg anyway (in time, yes, but not in
		     samples); but ffmpeg *can* seek */

  return SOX_SUCCESS;
}

/*
 * Read up to len samples of type sox_sample_t from file into buf[].
 * Return number of samples read.
 */
static size_t read_samples(sox_format_t * ft, sox_sample_t *buf, size_t len)
{
  priv_t * ffmpeg = (priv_t *)ft->priv;
  AVPacket *pkt = &ffmpeg->audio_pkt;
  int ret;
  size_t nsamp = 0, nextra;

  /* Read data repeatedly until buf is full or no more can be read */
  do {
    /* If input buffer empty, read more data */
    if (ffmpeg->audio_buf_index * 2 >= ffmpeg->audio_buf_size) {
      if ((ret = av_read_frame(ffmpeg->ctxt, pkt)) < 0 &&
	  (ret == AVERROR_EOF || url_ferror(ffmpeg->ctxt->pb)))
	break;
      ffmpeg->audio_buf_size = audio_decode_frame(ffmpeg, ffmpeg->audio_buf_aligned, AVCODEC_MAX_AUDIO_FRAME_SIZE);
      ffmpeg->audio_buf_index = 0;
    }

    /* Convert data into SoX samples up to size of buffer */
    nextra = min((ffmpeg->audio_buf_size - ffmpeg->audio_buf_index) / 2, (int)(len - nsamp));
    for (; nextra > 0; nextra--)
      buf[nsamp++] = SOX_SIGNED_16BIT_TO_SAMPLE(((int16_t *)ffmpeg->audio_buf_aligned)[ffmpeg->audio_buf_index++], ft->clips);
  } while (nsamp < len && nextra > 0);

  return nsamp;
}

/*
 * Close file for ffmpeg (this doesn't close the file handle)
 */
static int stopread(sox_format_t * ft)
{
  priv_t * ffmpeg = (priv_t *)ft->priv;

  if (ffmpeg->audio_stream >= 0)
    stream_component_close(ffmpeg, ffmpeg->audio_stream);
  if (ffmpeg->ctxt) {
    av_close_input_file(ffmpeg->ctxt);
    ffmpeg->ctxt = NULL; /* safety */
  }

  free(ffmpeg->audio_buf_raw);
  return SOX_SUCCESS;
}

/*
 * add an audio output stream
 */
static AVStream *add_audio_stream(sox_format_t * ft, AVFormatContext *oc, enum CodecID codec_id)
{
  AVCodecContext *c;
  AVStream *st;

  st = av_new_stream(oc, 1);
  if (!st) {
    lsx_fail("ffmpeg could not alloc stream");
    return NULL;
  }

  c = st->codec;
  c->codec_id = codec_id;
  c->codec_type = AVMEDIA_TYPE_AUDIO;

  /* put sample parameters */
  c->bit_rate = 256000;  /* FIXME: allow specification */
  /* FIXME: currently mplayer says files do not have a specified
     compressed bit-rate */
  c->sample_rate = ft->signal.rate;
  c->channels = ft->signal.channels;
  return st;
}

static int open_audio(priv_t * ffmpeg, AVStream *st)
{
  AVCodecContext *c;
  AVCodec *codec;

  c = st->codec;

  /* find the audio encoder */
  codec = avcodec_find_encoder(c->codec_id);
  if (!codec) {
    lsx_fail("ffmpeg CODEC not found");
    return SOX_EOF;
  }

  /* open it */
  if (avcodec_open(c, codec) < 0) {
    lsx_fail("ffmpeg could not open CODEC");
    return SOX_EOF;
  }

  ffmpeg->audio_buf_raw = lsx_malloc((size_t)AVCODEC_MAX_AUDIO_FRAME_SIZE + 32);
  ffmpeg->audio_buf_aligned = ALIGN16(ffmpeg->audio_buf_raw);

  /* ugly hack for PCM codecs (will be removed ASAP with new PCM
     support to compute the input frame size in samples */
  if (c->frame_size <= 1) {
    ffmpeg->audio_input_frame_size = AVCODEC_MAX_AUDIO_FRAME_SIZE / c->channels;
    switch(st->codec->codec_id) {
    case CODEC_ID_PCM_S16LE:
    case CODEC_ID_PCM_S16BE:
    case CODEC_ID_PCM_U16LE:
    case CODEC_ID_PCM_U16BE:
      ffmpeg->audio_input_frame_size >>= 1;
      break;
    default:
      break;
    }
  } else
    ffmpeg->audio_input_frame_size = c->frame_size;

  ffmpeg->samples = lsx_malloc((size_t)(ffmpeg->audio_input_frame_size * 2 * c->channels));

  return SOX_SUCCESS;
}

static int startwrite(sox_format_t * ft)
{
  priv_t * ffmpeg = (priv_t *)ft->priv;

  /* initialize libavcodec, and register all codecs and formats */
  av_register_all();

  /* auto detect the output format from the name. default is
     mpeg. */
  ffmpeg->fmt = av_guess_format(NULL, ft->filename, NULL);
  if (!ffmpeg->fmt) {
    lsx_warn("ffmpeg could not deduce output format from file extension; using MPEG");
    ffmpeg->fmt = av_guess_format("mpeg", NULL, NULL);
    if (!ffmpeg->fmt) {
      lsx_fail("ffmpeg could not find suitable output format");
      return SOX_EOF;
    }
  }

  /* allocate the output media context */
  ffmpeg->ctxt = avformat_alloc_context();
  if (!ffmpeg->ctxt) {
    fprintf(stderr, "ffmpeg out of memory error");
    return SOX_EOF;
  }
  ffmpeg->ctxt->oformat = ffmpeg->fmt;
  snprintf(ffmpeg->ctxt->filename, sizeof(ffmpeg->ctxt->filename), "%s", ft->filename);

  /* add the audio stream using the default format codecs
     and initialize the codecs */
  ffmpeg->audio_st = NULL;
  if (ffmpeg->fmt->audio_codec != CODEC_ID_NONE) {
    ffmpeg->audio_st = add_audio_stream(ft, ffmpeg->ctxt, ffmpeg->fmt->audio_codec);
    if (ffmpeg->audio_st == NULL)
      return SOX_EOF;
  }

  /* set the output parameters (must be done even if no
     parameters). */
  if (av_set_parameters(ffmpeg->ctxt, NULL) < 0) {
    lsx_fail("ffmpeg invalid output format parameters");
    return SOX_EOF;
  }

  /* Next line for debugging */
  /* dump_format(ffmpeg->ctxt, 0, ft->filename, 1); */

  /* now that all the parameters are set, we can open the audio and
     codec and allocate the necessary encode buffers */
  if (ffmpeg->audio_st)
    if (open_audio(ffmpeg, ffmpeg->audio_st) == SOX_EOF)
      return SOX_EOF;

  /* open the output file, if needed */
  if (!(ffmpeg->fmt->flags & AVFMT_NOFILE)) {
    if (url_fopen(&ffmpeg->ctxt->pb, ft->filename, URL_WRONLY) < 0) {
      lsx_fail("ffmpeg could not open `%s'", ft->filename);
      return SOX_EOF;
    }
  }

  /* write the stream header, if any */
  av_write_header(ffmpeg->ctxt);

  return SOX_SUCCESS;
}

/*
 * Write up to len samples of type sox_sample_t from buf[] into file.
 * Return number of samples written.
 */
static size_t write_samples(sox_format_t * ft, const sox_sample_t *buf, size_t len)
{
  priv_t * ffmpeg = (priv_t *)ft->priv;
  size_t nread = 0, nwritten = 0;

  /* Write data repeatedly until buf is empty */
  do {
    /* If output frame is not full, copy data into it */
    if (ffmpeg->samples_index < ffmpeg->audio_input_frame_size) {
      SOX_SAMPLE_LOCALS;
      for (; nread < len && ffmpeg->samples_index < ffmpeg->audio_input_frame_size; nread++)
	ffmpeg->samples[ffmpeg->samples_index++] = SOX_SAMPLE_TO_SIGNED_16BIT(buf[nread], ft->clips);
    }

    /* If output frame full or no more data to read, write it out */
    if (ffmpeg->samples_index == ffmpeg->audio_input_frame_size ||
	(len == 0 && ffmpeg->samples_index > 0)) {
      AVCodecContext *c = ffmpeg->audio_st->codec;
      AVPacket pkt;

      av_init_packet(&pkt);
      pkt.size = avcodec_encode_audio(c, ffmpeg->audio_buf_aligned, AVCODEC_MAX_AUDIO_FRAME_SIZE, ffmpeg->samples);
      pkt.pts = av_rescale_q(c->coded_frame->pts, c->time_base, ffmpeg->audio_st->time_base);
      pkt.flags |= AV_PKT_FLAG_KEY;
      pkt.stream_index = ffmpeg->audio_st->index;
      pkt.data = ffmpeg->audio_buf_aligned;

      /* write the compressed frame to the media file */
      if (av_write_frame(ffmpeg->ctxt, &pkt) != 0)
	lsx_fail("ffmpeg had error while writing audio frame");

      /* Increment nwritten whether write succeeded or not; we have to
	 get rid of the input! */
      nwritten += ffmpeg->samples_index;
      ffmpeg->samples_index = 0;
    }
  } while (nread < len);

  return nwritten;
}

/*
 * Close file for ffmpeg (this doesn't close the file handle)
 */
static int stopwrite(sox_format_t * ft)
{
  priv_t * ffmpeg = (priv_t *)ft->priv;
  int i;

  /* Close CODEC */
  if (ffmpeg->audio_st) {
    avcodec_close(ffmpeg->audio_st->codec);
  }

  free(ffmpeg->samples);
  free(ffmpeg->audio_buf_raw);

  /* Write the trailer, if any */
  av_write_trailer(ffmpeg->ctxt);

  /* Free the streams */
  for (i = 0; (unsigned)i < ffmpeg->ctxt->nb_streams; i++) {
    av_freep(&ffmpeg->ctxt->streams[i]->codec);
    av_freep(&ffmpeg->ctxt->streams[i]);
  }

  if (!(ffmpeg->fmt->flags & AVFMT_NOFILE)) {
    /* close the output file */
#if (LIBAVFORMAT_VERSION_INT < 0x340000)
    url_fclose(&ffmpeg->ctxt->pb);
#else
    url_fclose(ffmpeg->ctxt->pb);
#endif
  }

  /* Free the output context */
  av_free(ffmpeg->ctxt);

  return SOX_SUCCESS;
}

LSX_FORMAT_HANDLER(ffmpeg)
{
  /* Format file suffixes */
  /* For now, comment out formats built in to SoX */
  static char const * const names[] = {
    "ffmpeg", /* special type to force use of ffmpeg */
    "mp4",
    "m4a",
    "m4b",
    "avi",
    "wmv",
    "mpg",
    NULL
  };

  static unsigned const write_encodings[] = {SOX_ENCODING_SIGN2, 16, 0, 0};

  static sox_format_handler_t handler = {SOX_LIB_VERSION_CODE,
    "Pseudo format to use libffmpeg", names, SOX_FILE_NOSTDIO,
    startread, read_samples, stopread,
    startwrite, write_samples, stopwrite,
    NULL, write_encodings, NULL, sizeof(priv_t)
  };

  return &handler;
}

#endif /* HAVE_FFMPEG */
