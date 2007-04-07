/*
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

/* File format: FLAC   (c) 2006 robs@users.sourceforge.net */



#include "sox_i.h"

#ifdef HAVE_LIBFLAC

#include <math.h>
#include <string.h>

#include <FLAC/all.h>

/* Workaround for flac versions < 1.1.2 */
#define FLAC__metadata_object_vorbiscomment_append_comment(object, entry, copy)\
  FLAC__metadata_object_vorbiscomment_insert_comment(object, object->data.vorbis_comment.num_comments, entry, copy)

#if !defined(FLAC_API_VERSION_CURRENT)
#define FLAC_API_VERSION_CURRENT 7
#define FLAC__StreamDecoder FLAC__FileDecoder
#define FLAC__stream_decoder_new FLAC__file_decoder_new
#define FLAC__stream_decoder_set_metadata_respond_all FLAC__file_decoder_set_metadata_respond_all
#define FLAC__stream_decoder_set_md5_checking FLAC__file_decoder_set_md5_checking
#define FLAC__stream_decoder_process_until_end_of_metadata FLAC__file_decoder_process_until_end_of_metadata
#define FLAC__stream_decoder_process_single FLAC__file_decoder_process_single
#define FLAC__stream_decoder_finish FLAC__file_decoder_finish
#define FLAC__stream_decoder_delete FLAC__file_decoder_delete
#endif


typedef struct {
  /* Info: */
  unsigned bits_per_sample;
  unsigned channels;
  unsigned sample_rate;
  unsigned total_samples;

  /* Decode buffer: */
  FLAC__int32 const * const * decoded_wide_samples;
  unsigned number_of_wide_samples;
  unsigned wide_sample_number;

  FLAC__StreamDecoder * flac;
  FLAC__bool eof;
} Decoder;



assert_static(sizeof(Decoder) <= SOX_MAX_FILE_PRIVSIZE, /* else */ Decoder__PRIVSIZE_too_big);



static void FLAC__decoder_metadata_callback(FLAC__StreamDecoder const * const flac, FLAC__StreamMetadata const * const metadata, void * const client_data)
{
  ft_t format = (ft_t) client_data;
  Decoder * decoder = (Decoder *) format->priv;

  (void) flac;

  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
    decoder->bits_per_sample = metadata->data.stream_info.bits_per_sample;
    decoder->channels = metadata->data.stream_info.channels;
    decoder->sample_rate = metadata->data.stream_info.sample_rate;
    decoder->total_samples = metadata->data.stream_info.total_samples;
  }
  else if (metadata->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
    size_t i, comment_size = 0;

    if (metadata->data.vorbis_comment.num_comments == 0)
      return;

    if (format->comment != NULL) {
      sox_warn("FLAC: multiple Vorbis comment block ignored");
      return;
    }

    for (i = 0; i < metadata->data.vorbis_comment.num_comments; ++i)
      comment_size += metadata->data.vorbis_comment.comments[i].length + 1;

    format->comment = (char *) xcalloc(comment_size, sizeof(char));

    for (i = 0; i < metadata->data.vorbis_comment.num_comments; ++i) {
      strcat(format->comment, (char const *) metadata->data.vorbis_comment.comments[i].entry);
      if (i != metadata->data.vorbis_comment.num_comments - 1)
        strcat(format->comment, "\n");
    }
  }
}



static void FLAC__decoder_error_callback(FLAC__StreamDecoder const * const flac, FLAC__StreamDecoderErrorStatus const status, void * const client_data)
{
  ft_t format = (ft_t) client_data;

  (void) flac;

  sox_fail_errno(format, SOX_EINVAL, "%s", FLAC__StreamDecoderErrorStatusString[status]);
}



static FLAC__StreamDecoderWriteStatus FLAC__frame_decode_callback(FLAC__StreamDecoder const * const flac, FLAC__Frame const * const frame, FLAC__int32 const * const buffer[], void * const client_data)
{
  ft_t format = (ft_t) client_data;
  Decoder * decoder = (Decoder *) format->priv;

  (void) flac;

  if (frame->header.bits_per_sample != decoder->bits_per_sample || frame->header.channels != decoder->channels || frame->header.sample_rate != decoder->sample_rate) {
    sox_fail_errno(format, SOX_EINVAL, "FLAC ERROR: parameters differ between frame and header");
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }

  decoder->decoded_wide_samples = buffer;
  decoder->number_of_wide_samples = frame->header.blocksize;
  decoder->wide_sample_number = 0;
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}



static int start_read(ft_t const format)
{
  Decoder * decoder = (Decoder *) format->priv;

  memset(decoder, 0, sizeof(*decoder));
  decoder->flac = FLAC__stream_decoder_new();
  if (decoder->flac == NULL) {
    sox_fail_errno(format, SOX_ENOMEM, "FLAC ERROR creating the decoder instance");
    return SOX_EOF;
  }

  FLAC__stream_decoder_set_md5_checking(decoder->flac, sox_true);
  FLAC__stream_decoder_set_metadata_respond_all(decoder->flac);
#if FLAC_API_VERSION_CURRENT <= 7
  FLAC__file_decoder_set_filename(decoder->flac, format->filename);
  FLAC__file_decoder_set_write_callback(decoder->flac, FLAC__frame_decode_callback);
  FLAC__file_decoder_set_metadata_callback(decoder->flac, FLAC__decoder_metadata_callback);
  FLAC__file_decoder_set_error_callback(decoder->flac, FLAC__decoder_error_callback);
  FLAC__file_decoder_set_client_data(decoder->flac, format);
  if (FLAC__file_decoder_init(decoder->flac) != FLAC__FILE_DECODER_OK) {
#else
  if (FLAC__stream_decoder_init_file(
    decoder->flac,
    format->filename,
    FLAC__frame_decode_callback,
    FLAC__decoder_metadata_callback,
    FLAC__decoder_error_callback,
    format) != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
#endif
    sox_fail_errno(format, SOX_EHDR, "FLAC ERROR initialising decoder");
    return SOX_EOF;
  }


  if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder->flac)) {
    sox_fail_errno(format, SOX_EHDR, "FLAC ERROR whilst decoding metadata");
    return SOX_EOF;
  }

#if FLAC_API_VERSION_CURRENT <= 7
  if (FLAC__file_decoder_get_state(decoder->flac) != FLAC__FILE_DECODER_OK && FLAC__file_decoder_get_state(decoder->flac) != FLAC__FILE_DECODER_END_OF_FILE) {
#else
  if (FLAC__stream_decoder_get_state(decoder->flac) > FLAC__STREAM_DECODER_END_OF_STREAM) {
#endif
    sox_fail_errno(format, SOX_EHDR, "FLAC ERROR during metadata decoding");
    return SOX_EOF;
  }

  format->signal.encoding = SOX_ENCODING_FLAC;
  format->signal.rate = decoder->sample_rate;
  format->signal.size = decoder->bits_per_sample >> 3;
  format->signal.channels = decoder->channels;
  format->length = decoder->total_samples * decoder->channels;
  return SOX_SUCCESS;
}


static sox_size_t read(ft_t const format, sox_sample_t * sampleBuffer, sox_size_t const requested)
{
  Decoder * decoder = (Decoder *) format->priv;
  size_t actual = 0;

  while (!decoder->eof && actual < requested) {
    if (decoder->wide_sample_number >= decoder->number_of_wide_samples)
      FLAC__stream_decoder_process_single(decoder->flac);
    if (decoder->wide_sample_number >= decoder->number_of_wide_samples)
      decoder->eof = sox_true;
    else {
      unsigned channel;

      for (channel = 0; channel < decoder->channels; channel++, actual++) {
        FLAC__int32 d = decoder->decoded_wide_samples[channel][decoder->wide_sample_number];
        switch (decoder->bits_per_sample) {
        case  8: *sampleBuffer++ = SOX_SIGNED_BYTE_TO_SAMPLE(d,); break;
        case 16: *sampleBuffer++ = SOX_SIGNED_WORD_TO_SAMPLE(d,); break;
        case 24: *sampleBuffer++ = SOX_SIGNED_24BIT_TO_SAMPLE(d,); break;
        case 32: *sampleBuffer++ = SOX_SIGNED_DWORD_TO_SAMPLE(d,); break;
        }
      }
      ++decoder->wide_sample_number;
    }
  }
  return actual;
}



static int stop_read(ft_t const format)
{
  Decoder * decoder = (Decoder *) format->priv;

  if (!FLAC__stream_decoder_finish(decoder->flac) && decoder->eof)
    sox_warn("FLAC decoder MD5 checksum mismatch.");
  FLAC__stream_decoder_delete(decoder->flac);
  return SOX_SUCCESS;
}



typedef struct {
  /* Info: */
  unsigned bits_per_sample;

  /* Encode buffer: */
  FLAC__int32 * decoded_samples;
  unsigned number_of_samples;

  FLAC__StreamEncoder * flac;
  FLAC__StreamMetadata *metadata;
} Encoder;



assert_static(sizeof(Encoder) <= SOX_MAX_FILE_PRIVSIZE, /* else */ Encoder__PRIVSIZE_too_big);



static FLAC__StreamEncoderWriteStatus flac_stream_encoder_write_callback(FLAC__StreamEncoder const * const flac, const FLAC__byte buffer[], unsigned const bytes, unsigned const samples, unsigned const current_frame, void * const client_data)
{
  ft_t const format = (ft_t) client_data;
  (void) flac, (void) samples, (void) current_frame;

  return sox_writebuf(format, buffer, bytes) == bytes ? FLAC__STREAM_ENCODER_WRITE_STATUS_OK : FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
}



static void flac_stream_encoder_metadata_callback(FLAC__StreamEncoder const * encoder, FLAC__StreamMetadata const * metadata, void * client_data)
{
  (void) encoder, (void) metadata, (void) client_data;
}



#if FLAC_API_VERSION_CURRENT >= 8
static FLAC__StreamEncoderSeekStatus flac_stream_encoder_seek_callback(FLAC__StreamEncoder const * encoder, FLAC__uint64 absolute_byte_offset, void * client_data)
{
  ft_t const format = (ft_t) client_data;
  (void) encoder;
  if (!format->seekable)
    return FLAC__STREAM_ENCODER_SEEK_STATUS_UNSUPPORTED;
  else if (sox_seeki(format, (sox_size_t)absolute_byte_offset, SEEK_SET) != SOX_SUCCESS)
    return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
  else
    return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}



static FLAC__StreamEncoderTellStatus flac_stream_encoder_tell_callback(FLAC__StreamEncoder const * encoder, FLAC__uint64 * absolute_byte_offset, void * client_data)
{
  ft_t const format = (ft_t) client_data;
  off_t pos;
  (void) encoder;
  if (!format->seekable)
    return FLAC__STREAM_ENCODER_TELL_STATUS_UNSUPPORTED;
  else if ((pos = ftello(format->fp)) < 0)
    return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;
  else {
    *absolute_byte_offset = (FLAC__uint64)pos;
    return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
  }
}
#endif



static int start_write(ft_t const format)
{
  Encoder * encoder = (Encoder *) format->priv;
  FLAC__StreamEncoderState status;

  memset(encoder, 0, sizeof(*encoder));
  encoder->flac = FLAC__stream_encoder_new();
  if (encoder->flac == NULL) {
    sox_fail_errno(format, SOX_ENOMEM, "FLAC ERROR creating the encoder instance");
    return SOX_EOF;
  }
  encoder->decoded_samples = xmalloc(SOX_BUFSIZ * sizeof(FLAC__int32));

  {     /* Select and set FLAC encoder options: */
    static struct {
      unsigned blocksize;
      FLAC__bool do_exhaustive_model_search;
      FLAC__bool do_mid_side_stereo;
      FLAC__bool loose_mid_side_stereo;
      unsigned max_lpc_order;
      unsigned max_residual_partition_order;
      unsigned min_residual_partition_order;
    } const options[] = {
      {1152, sox_false, sox_false, sox_false, 0, 2, 2},
      {1152, sox_false, sox_true, sox_true, 0, 2, 2},
      {1152, sox_false, sox_true, sox_false, 0, 3, 0},
      {4608, sox_false, sox_false, sox_false, 6, 3, 3},
      {4608, sox_false, sox_true, sox_true, 8, 3, 3},
      {4608, sox_false, sox_true, sox_false, 8, 3, 3},
      {4608, sox_false, sox_true, sox_false, 8, 4, 0},
      {4608, sox_true, sox_true, sox_false, 8, 6, 0},
      {4608, sox_true, sox_true, sox_false, 12, 6, 0},
    };
    unsigned compression_level = array_length(options) - 1; /* Default to "best" */

    if (format->signal.compression != HUGE_VAL) {
      compression_level = format->signal.compression;
      if (compression_level != format->signal.compression || 
          compression_level >= array_length(options)) {
        sox_fail_errno(format, SOX_EINVAL,
                      "FLAC compression level must be a whole number from 0 to %i",
                      array_length(options) - 1);
        return SOX_EOF;
      }
    }

#define SET_OPTION(x) do {\
  sox_report("FLAC "#x" = %i", options[compression_level].x); \
  FLAC__stream_encoder_set_##x(encoder->flac, options[compression_level].x);\
} while (0)
    SET_OPTION(blocksize);
    SET_OPTION(do_exhaustive_model_search);
    SET_OPTION(max_lpc_order);
    SET_OPTION(max_residual_partition_order);
    SET_OPTION(min_residual_partition_order);
    if (format->signal.channels == 2) {
      SET_OPTION(do_mid_side_stereo);
      SET_OPTION(loose_mid_side_stereo);
    }
#undef SET_OPTION
  }

  /* FIXME: FLAC should not need to know about this oddity */
  if (format->signal.encoding < SOX_ENCODING_SIZE_IS_WORD)
    format->signal.size = SOX_SIZE_16BIT;

  encoder->bits_per_sample = (format->signal.size > 4 ? 4 : format->signal.size) << 3;

  sox_report("FLAC encoding at %i bits per sample", encoder->bits_per_sample);

  FLAC__stream_encoder_set_channels(encoder->flac, format->signal.channels);
  FLAC__stream_encoder_set_bits_per_sample(encoder->flac, encoder->bits_per_sample);
  FLAC__stream_encoder_set_sample_rate(encoder->flac, format->signal.rate);

  { /* Check if rate is streamable: */
    static const unsigned streamable_rates[] =
      {8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000};
    size_t i;
    sox_bool streamable = sox_false;
    for (i = 0; !streamable && i < array_length(streamable_rates); ++i)
       streamable = (streamable_rates[i] == format->signal.rate);
    if (!streamable) {
      sox_report("FLAC: non-standard rate; output may not be streamable");
      FLAC__stream_encoder_set_streamable_subset(encoder->flac, sox_false);
    }
  }

  if (format->length != 0)
    FLAC__stream_encoder_set_total_samples_estimate(encoder->flac, (FLAC__uint64)format->length);

  if (format->comment != NULL && * format->comment != '\0') {
    FLAC__StreamMetadata * metadata[1];
    FLAC__StreamMetadata_VorbisComment_Entry entry;
    char * comments, * comment, * end_of_comment;

    encoder->metadata = metadata[0] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);

    /* Check if there is a FIELD=value pair already in the comment; if not, add one */
    if (strchr(format->comment, '=') == NULL) {
      static const char prepend[] = "COMMENT=";
      comments = xmalloc(strlen(format->comment) + sizeof(prepend));
      strcpy(comments, prepend);
      strcat(comments, format->comment);
    }
    else
      comments = strdup(format->comment);

    comment = comments;

    do {
      entry.entry = (FLAC__byte *) comment;
      end_of_comment = strchr(comment, '\n');
      if (end_of_comment != NULL) {
        *end_of_comment = '\0';
        comment = end_of_comment + 1;
      }
      entry.length = strlen((char const *) entry.entry);

      FLAC__metadata_object_vorbiscomment_append_comment(metadata[0], entry, /*copy= */ sox_true);
    } while (end_of_comment != NULL);

    FLAC__stream_encoder_set_metadata(encoder->flac, metadata, 1);
    free(comments);
  }

#if FLAC_API_VERSION_CURRENT <= 7
  FLAC__stream_encoder_set_write_callback(encoder->flac, flac_stream_encoder_write_callback);
  FLAC__stream_encoder_set_metadata_callback(encoder->flac, flac_stream_encoder_metadata_callback);
  FLAC__stream_encoder_set_client_data(encoder->flac, format);
  status = FLAC__stream_encoder_init(encoder->flac);
#else
  status = FLAC__stream_encoder_init_stream(encoder->flac, flac_stream_encoder_write_callback,
      flac_stream_encoder_seek_callback, flac_stream_encoder_tell_callback, flac_stream_encoder_metadata_callback, format);
#endif

  if (status != FLAC__STREAM_ENCODER_OK) {
    sox_fail_errno(format, SOX_EINVAL, "%s", FLAC__StreamEncoderStateString[status]);
    return SOX_EOF;
  }
  return SOX_SUCCESS;
}



static sox_size_t write(ft_t const format, sox_sample_t const * const sampleBuffer, sox_size_t const len)
{
  Encoder * encoder = (Encoder *) format->priv;
  unsigned i;

  for (i = 0; i < len; ++i) {
    switch (encoder->bits_per_sample) {
      case  8: encoder->decoded_samples[i] = SOX_SAMPLE_TO_SIGNED_BYTE(sampleBuffer[i], format->clips); break;
      case 16: encoder->decoded_samples[i] = SOX_SAMPLE_TO_SIGNED_WORD(sampleBuffer[i], format->clips); break;
      case 24: encoder->decoded_samples[i] = SOX_SAMPLE_TO_SIGNED_24BIT(sampleBuffer[i],format->clips); break;
      case 32: encoder->decoded_samples[i] = SOX_SAMPLE_TO_SIGNED_DWORD(sampleBuffer[i],format->clips); break;
    }
  }
  FLAC__stream_encoder_process_interleaved(encoder->flac, encoder->decoded_samples, len / format->signal.channels);
  return FLAC__stream_encoder_get_state(encoder->flac) == FLAC__STREAM_ENCODER_OK ? len : 0;
}



static int stop_write(ft_t const format)
{
  Encoder * encoder = (Encoder *) format->priv;
  FLAC__StreamEncoderState state = FLAC__stream_encoder_get_state(encoder->flac);

  FLAC__metadata_object_delete(encoder->metadata);
  FLAC__stream_encoder_finish(encoder->flac);
  FLAC__stream_encoder_delete(encoder->flac);
  free(encoder->decoded_samples);
  if (state != FLAC__STREAM_ENCODER_OK) {
    sox_fail_errno(format, SOX_EINVAL, "FLAC ERROR: failed to encode to end of stream");
    return SOX_EOF;
  }
  return SOX_SUCCESS;
}



sox_format_t const * sox_flac_format_fn(void)
{
  static char const * const names[] = {"flac", NULL};
  static sox_format_t const driver = {
    names, NULL, 0,
    start_read, read, stop_read,
    start_write, write, stop_write,
    0
  };
  return &driver;
}



#endif /* HAVE_LIBFLAC */
