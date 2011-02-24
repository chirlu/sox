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
  char *buf = outOutputData->mBuffers[0].mData;
  unsigned int len, output_len;

  if (outOutputData->mNumberBuffers != 1)
  {
	  lsx_warn("coreaudio: unhandled extra buffer.  Data discarded.");
	  return kAudioHardwareNoError;
  }

  buf = (char *)outOutputData->mBuffers[0].mData;
  output_len = outOutputData->mBuffers[0].mDataByteSize;

  pthread_mutex_lock(&ac->mutex);

  len = (ac->buf_offset < output_len) ? ac->buf_offset : output_len;

  /* Make sure to write 2 (stereo) floats at a time */
  if (len % 8)
      len -= len % 8;

  memcpy(buf, ac->buffer, len);

  /* Fill partial output buffers with silence */
  if (len < output_len)
      memset(buf+len, 0, output_len-len);

  ac->buf_offset -= len;

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
  size_t len, output_len;
  char *destbuf;
  char *buf;
  int i;

  pthread_mutex_lock(&ac->mutex);

  if (inInputData->mNumberBuffers != 1)
  {
    lsx_warn("coreaudio: unhandled extra buffer.  Data discarded.");
    return kAudioHardwareNoError;
  }

  destbuf = ((char *)ac->buffer + ac->buf_offset);
  buf = inInputData->mBuffers[0].mData;
  output_len = inInputData->mBuffers[0].mDataByteSize;

  /* mDataByteSize may be non-zero even when mData is NULL, but that is
   * not an error.
   */
  if (buf == NULL)
    return kAudioHardwareNoError;

  len = ac->buf_size - ac->buf_offset;

  /* Make sure to read 2 (stereo) floats at a time */
  if (len % 8)
      len -= len % 8;

  if (len > output_len)
    len = output_len;

  /* FIXME: Handle buffer overrun. */
  if (len < output_len)
      lsx_warn("coreaudio: unhandled buffer overrun.  Data discarded.");

  memcpy(destbuf, buf, len);
  ac->buf_offset += len;

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

  if (strncmp(ft->filename, "default", (size_t)7) == 0)
  {
      property_size = sizeof(ac->adid);
      if (is_input)
	  status = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice, &property_size, &ac->adid);
      else
	  status = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &property_size, &ac->adid);
  }
  else
  {
      Boolean is_writable;
      status = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &property_size, &is_writable);

      if (status == noErr)
      {
	  int device_count = property_size/sizeof(AudioDeviceID);
	  AudioDeviceID *devices;

	  devices = malloc(property_size);
    	  status = AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &property_size, devices);

	  if (status == noErr)
	  {
	      int i;
	      for (i = 0; i < property_size/sizeof(AudioDeviceID); i++)
	      {
		  char name[256];
		  status = AudioDeviceGetProperty(devices[i],0,false,kAudioDevicePropertyDeviceName,&property_size,&name);

		  lsx_report("Found Audio Device \"%s\"\n",name);

		  /* String returned from OS is truncated so only compare
		   * as much as returned.
		   */
		  if (strncmp(name,ft->filename,strlen(name)) == 0)
		  {
		      ac->adid = devices[i];
		      break;
		  }
	      }
	  }
	  free(devices);
      }
  }

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

  /* TODO: My limited experience with hardware can only get floats working
   * withh a fixed sample rate and stereo.  I know that is a limitiation of
   * audio device I have so this may not be standard operating orders.
   * If some hardware supports setting sample rates and channel counts
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
  AudioDeviceRemoveIOProc(ac->adid, RecIOProc);
  pthread_cond_destroy(&ac->cond);
  pthread_mutex_destroy(&ac->mutex);

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

  pthread_mutex_lock(&ac->mutex);

  /* Wait to start until mutex is locked to help prevent callback
   * getting zero samples.
   */
  if (!ac->device_started)
  {
      status = AudioDeviceStart(ac->adid, PlaybackIOProc);
      if (status)
      {
	  pthread_mutex_unlock(&ac->mutex);
	  return SOX_EOF;
      }
      ac->device_started = 1;
  }

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

  if (!ac->device_started)
  {
    pthread_mutex_lock(&ac->mutex);

    while (ac->buf_offset)
	pthread_cond_wait(&ac->cond, &ac->mutex);

    pthread_mutex_unlock(&ac->mutex);

    AudioDeviceStop(ac->adid, PlaybackIOProc);
  }

  AudioDeviceRemoveIOProc(ac->adid, PlaybackIOProc);
  pthread_cond_destroy(&ac->cond);
  pthread_mutex_destroy(&ac->mutex);

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
