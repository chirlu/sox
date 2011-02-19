/* AudioCore sound handler
 *
 * Copyright 2008 Chris Bagwell And Sundry Contributors
 */

#include "sox_i.h"

#include <CoreAudio/CoreAudio.h>
#include <pthread.h>

typedef struct {
  AudioDeviceID adid;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int device_started;
  size_t buf_size;
  size_t buf_offset;
  float *buffer;
} priv_t;

static OSStatus PlaybackIOProc(AudioDeviceID inDevice UNUSED,
                               const AudioTimeStamp *inNow UNUSED,
                               const AudioBufferList *inInputData UNUSED,
                               const AudioTimeStamp *inInputTime UNUSED,
                               AudioBufferList *outOutputData,
                               const AudioTimeStamp *inOutputTime UNUSED,
                               void *inClientData)
{
  sox_format_t *ft = (sox_format_t *)inClientData;
  priv_t *ac = (priv_t *)ft->priv;
  float *buf = outOutputData->mBuffers[0].mData;
  unsigned int len;
  unsigned int buf_num;

  pthread_mutex_lock(&ac->mutex);

  for (buf_num = 0; buf_num <  outOutputData->mNumberBuffers; buf_num++)
  {
      /* TODO: Does more than 1 output buffer need to be handled? */
      if (buf_num > 0)
      {
    	  outOutputData->mBuffers[buf_num].mDataByteSize = 0;
	  continue;
      }

      buf = outOutputData->mBuffers[0].mData;

      /* mDataByteSize may be non-zero even when mData is NULL, but that is
       * not an error.
       */
      if (buf == NULL)
	  continue;

      len = ac->buf_offset;

      memcpy(buf, ac->buffer, len);
      outOutputData->mBuffers[buf_num].mDataByteSize = len;
      ac->buf_offset = 0;
  }

  pthread_mutex_unlock(&ac->mutex);
  pthread_cond_signal(&ac->cond);

  return kAudioHardwareNoError;
}

static OSStatus RecIOProc(AudioDeviceID inDevice UNUSED,
                          const AudioTimeStamp *inNow UNUSED,
                          const AudioBufferList *inInputData,
                          const AudioTimeStamp *inInputTime UNUSED,
                          AudioBufferList *outOutputData UNUSED,
                          const AudioTimeStamp *inOutputTime UNUSED,
                          void *inClientData)
{
  sox_format_t *ft = (sox_format_t *)inClientData;
  priv_t *ac = (priv_t *)ft->priv;
  float *buf;
  size_t buflen;
  float *destbuf = (float *)((unsigned char *)ac->buffer + ac->buf_offset);
  int i;
  unsigned int buf_num;

  pthread_mutex_lock(&ac->mutex);

  for (buf_num = 0; buf_num <  inInputData->mNumberBuffers; buf_num++)
  {
      /* TODO: Does more than 1 input buffer need to be handled? */
      if (buf_num > 0)
      {
	  lsx_warn("coreaudio: unhandled extra buffer.  Data discarded.");
	  continue;
      }

      buf = inInputData->mBuffers[buf_num].mData;
      buflen = inInputData->mBuffers[buf_num].mDataByteSize;

      /* mDataByteSize may be non-zero even when mData is NULL, but that is
       * not an error.
       */
      if (buf == NULL)
	  continue;

      if (buflen > (ac->buf_size - ac->buf_offset))
	  buflen = ac->buf_size - ac->buf_offset;

      /* FIXME: Handle buffer overrun. */
      if (buflen < ac->buf_size)
	  lsx_warn("coreaudio: unhandled buffer overrun.  Data discarded.");

      for (i = 0; i < (int)(buflen / sizeof(float)); i += 2) {
	  destbuf[i] = buf[i];
	  destbuf[i + 1] = buf[i + 1];
	  ac->buf_offset += sizeof(float) * 2;
      }
  }

  pthread_mutex_unlock(&ac->mutex);
  pthread_cond_signal(&ac->cond);

  return kAudioHardwareNoError;
}

static int setup(sox_format_t *ft, int is_input)
{
  priv_t *ac = (priv_t *)ft->priv;
  OSStatus status;
  UInt32 property_size;
  struct AudioStreamBasicDescription stream_desc;
  int32_t buf_size;
  int rc;

  property_size = sizeof(ac->adid);
  if (is_input)
    status = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice,
                                      &property_size, &ac->adid);
  else
    status = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
                                      &property_size, &ac->adid);

  if (status || ac->adid == kAudioDeviceUnknown)
  {
    lsx_fail_errno(ft, SOX_EPERM, "can not open audio device");
    return SOX_EOF;
  }

  /* Query device to get initial values */
  property_size = sizeof(struct AudioStreamBasicDescription);
  status = AudioDeviceGetProperty(ac->adid, 0, is_input,
                                  kAudioDevicePropertyStreamFormat,
                                  &property_size, &stream_desc);
  if (status)
  {
    lsx_fail_errno(ft, SOX_EPERM, "can not get audio device properties");
    return SOX_EOF;
  }

  if (!(stream_desc.mFormatFlags & kLinearPCMFormatFlagIsFloat))
  {
    lsx_fail_errno(ft, SOX_EPERM, "audio device does not accept floats");
    return SOX_EOF;
  }

  /* OS X effectively only supports these values. */
  ft->signal.channels = 2;
  ft->signal.rate = 44100;
  ft->encoding.bits_per_sample = 32;

  /* TODO: My limited experience with hardware can only get floats working which a fixed sample
   * rate and stereo.  I know that is a limitiation of audio device I have so this may not be
   * standard operating orders.  If some hardware supports setting sample rates and channel counts
   * then should do that over resampling and mixing.
   */
#if  0
  stream_desc.mSampleRate = ft->signal.rate;
  stream_desc.mChannelsPerFrame = ft->signal.channels;

  /* Write them back */
  property_size = sizeof(struct AudioStreamBasicDescription);
  status = AudioDeviceSetProperty(ac->adid, NULL, 0, is_input,
                                  kAudioDevicePropertyStreamFormat,
                                  property_size, &stream_desc);
  if (status)
  {
    lsx_fail_errno(ft, SOX_EPERM, "can not set audio device properties");
    return SOX_EOF;
  }

  /* Query device to see if it worked */
  property_size = sizeof(struct AudioStreamBasicDescription);
  status = AudioDeviceGetProperty(ac->adid, 0, is_input,
                                  kAudioDevicePropertyStreamFormat,
                                  &property_size, &stream_desc);

  if (status)
  {
    lsx_fail_errno(ft, SOX_EPERM, "can not get audio device properties");
    return SOX_EOF;
  }
#endif

  if (stream_desc.mChannelsPerFrame != ft->signal.channels)
  {
    lsx_debug("audio device did not accept %d channels. Use %d channels instead.", (int)ft->signal.channels,
              (int)stream_desc.mChannelsPerFrame);
    ft->signal.channels = stream_desc.mChannelsPerFrame;
  }

  if (stream_desc.mSampleRate != ft->signal.rate)
  {
    lsx_debug("audio device did not accept %d sample rate. Use %d instead.", (int)ft->signal.rate,
              (int)stream_desc.mSampleRate);
    ft->signal.rate = stream_desc.mSampleRate;
  }

  ac->buf_size = sox_globals.bufsiz * sizeof(float);
  ac->buf_offset = 0;
  ac->buffer = lsx_malloc(ac->buf_size);

  buf_size = ac->buf_size;
  property_size = sizeof(buf_size);
  status = AudioDeviceSetProperty(ac->adid, NULL, 0, is_input,
                                  kAudioDevicePropertyBufferSize,
                                  property_size, &buf_size);

  rc = pthread_mutex_init(&ac->mutex, NULL);
  if (rc)
  {
    lsx_fail_errno(ft, SOX_EPERM, "failed initializing mutex");
    return SOX_EOF;
  }

  rc = pthread_cond_init(&ac->cond, NULL);
  if (rc)
  {
    lsx_fail_errno(ft, SOX_EPERM, "failed initializing condition");
    return SOX_EOF;
  }

  ac->device_started = 0;

  /* Registers callback with the device without activating it. */
  if (is_input)
    status = AudioDeviceAddIOProc(ac->adid, RecIOProc, (void *)ft);
  else
    status = AudioDeviceAddIOProc(ac->adid, PlaybackIOProc, (void *)ft);

  return SOX_SUCCESS;
}

static int startread(sox_format_t *ft)
{
    return setup(ft, 1);
}

static size_t read_samples(sox_format_t *ft, sox_sample_t *buf, size_t nsamp)
{
  priv_t *ac = (priv_t *)ft->priv;
  size_t len = nsamp;
  size_t samp_left;
  OSStatus status;
  float *p;
  SOX_SAMPLE_LOCALS;

  if (!ac->device_started)
  {
    status = AudioDeviceStart(ac->adid, RecIOProc);
    ac->device_started = 1;
  }

  pthread_mutex_lock(&ac->mutex);

  /* Wait until input buffer has been filled by device driver */
  while (ac->buf_offset < ac->buf_size)
    pthread_cond_wait(&ac->cond, &ac->mutex);

  len = ac->buf_offset / sizeof(float);
  for (p = ac->buffer, samp_left = len; samp_left > 0; samp_left--, buf++, p++)
    *buf = SOX_FLOAT_32BIT_TO_SAMPLE(*p, ft->clips);
  ac->buf_offset = 0;

  pthread_mutex_unlock(&ac->mutex);

  return len;
}

static int stopread(sox_format_t * ft)
{
  priv_t *ac = (priv_t *)ft->priv;

  AudioDeviceStop(ac->adid, RecIOProc);

  return SOX_SUCCESS;
}

static int startwrite(sox_format_t * ft)
{
    return setup(ft, 0);
}

static size_t write_samples(sox_format_t *ft, const sox_sample_t *buf, size_t nsamp)
{
  priv_t *ac = (priv_t *)ft->priv;
  size_t len, written = 0;
  size_t samp_left;
  OSStatus status;
  float *p;
  SOX_SAMPLE_LOCALS;

  if (!ac->device_started)
  {
    status = AudioDeviceStart(ac->adid, PlaybackIOProc);
    ac->device_started = 1;
  }

  pthread_mutex_lock(&ac->mutex);

  /* globals.bufsize is in samples
   * buf_offset is in bytes
   * buf_size is in bytes
   */
  do {
    while (ac->buf_offset >= ac->buf_size)
      pthread_cond_wait(&ac->cond, &ac->mutex);

    len = nsamp - written;
    if (len > (ac->buf_size - ac->buf_offset) / sizeof(float))
      len = (ac->buf_size - ac->buf_offset) / sizeof(float);
    samp_left = len;

    p = ((unsigned char *)ac->buffer) + ac->buf_offset;

    while (samp_left--)
      *p++ = SOX_SAMPLE_TO_FLOAT_32BIT(*buf++, ft->clips);

    ac->buf_offset += len * sizeof(float);
    written += len;
  } while (written < nsamp);

  pthread_mutex_unlock(&ac->mutex);

  return written;
}


static int stopwrite(sox_format_t * ft)
{
  priv_t *ac = (priv_t *)ft->priv;

  AudioDeviceStop(ac->adid, PlaybackIOProc);

  return SOX_SUCCESS;
}

LSX_FORMAT_HANDLER(coreaudio)
{
  static char const *const names[] = { "coreaudio", NULL };
  static unsigned const write_encodings[] = {
    SOX_ENCODING_FLOAT, 32, 0,
    0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "Mac AudioCore device driver",
    names, SOX_FILE_DEVICE | SOX_FILE_NOSTDIO,
    startread, read_samples, stopread,
    startwrite, write_samples, stopwrite,
    NULL, write_encodings, NULL, sizeof(priv_t)
  };
  return &handler;
}
