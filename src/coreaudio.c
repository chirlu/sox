/* AudioCore sound handler
 *
 * Copyright 2008 Chris Bagwell And Sundry Contributors
 */

#include "sox_i.h"

#include <CoreAudio/CoreAudio.h>

#include <assert.h>
#include <limits.h>
#include <pthread.h>

#define Buffactor 8

typedef struct {
    AudioDeviceID adid;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int device_started;
    size_t bufsize;
    size_t bufrd;
    size_t bufwr;
    size_t bufrdavail;
    float *buf;
    UInt32 strmidx;
} priv_t;

static OSStatus PlaybackIOProc(AudioDeviceID inDevice UNUSED,
                               const AudioTimeStamp *inNow UNUSED,
                               const AudioBufferList *inInputData UNUSED,
                               const AudioTimeStamp *inInputTime UNUSED,
                               AudioBufferList *outOutputData,
                               const AudioTimeStamp *inOutputTime UNUSED,
                               void *inClientData)
{
    priv_t *ac = (priv_t*)((sox_format_t*)inClientData)->priv;
    AudioBuffer *buf;
    size_t copylen, avail;
    UInt32 i;

    pthread_mutex_lock(&ac->mutex);

    assert(ac->strmidx < outOutputData->mNumberBuffers);

    for(i = 0; i < outOutputData->mNumberBuffers; i++) {
	buf = outOutputData->mBuffers + i;

	if(i != ac->strmidx) {
	    buf->mDataByteSize = 0;
	    continue;
	}

	assert(buf->mData != NULL);

	copylen = buf->mDataByteSize / sizeof(float);
	if(copylen > ac->bufrdavail)
	    copylen = ac->bufrdavail;

        avail = ac->bufsize - ac->bufrd;
        if(buf->mData == NULL){
            /*do nothing-hardware can't play audio*/
        }else if(copylen > avail){
            memcpy(buf->mData, ac->buf + ac->bufrd, avail * sizeof(float));
            memcpy((float*)buf->mData + avail, ac->buf,
		   (copylen - avail) * sizeof(float));
        }else{
            memcpy(buf->mData, ac->buf + ac->bufrd, copylen * sizeof(float));
        }

        buf->mDataByteSize = copylen * sizeof(float);
        ac->bufrd += copylen;
        if(ac->bufrd >= ac->bufsize)
            ac->bufrd -= ac->bufsize;
        ac->bufrdavail -= copylen;
    }

    pthread_cond_signal(&ac->cond);
    pthread_mutex_unlock(&ac->mutex);

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
    priv_t *ac = (priv_t *)((sox_format_t*)inClientData)->priv;
    const AudioBuffer *buf;
    size_t nfree, copylen, avail;

    pthread_mutex_lock(&ac->mutex);

    assert(ac->strmidx < inInputData->mNumberBuffers);
    buf = inInputData->mBuffers + ac->strmidx;
    assert(buf->mData != NULL);

    copylen = buf->mDataByteSize / sizeof(float);
    nfree = ac->bufsize - ac->bufrdavail - 1;
    if(nfree == 0)
	lsx_warn("coreaudio: unhandled buffer overrun.  Data discarded.");

    if(copylen > nfree)
	copylen = nfree;

    avail = ac->bufsize - ac->bufwr;
    if(copylen > avail){
	memcpy(ac->buf + ac->bufwr, buf->mData, avail * sizeof(float));
	memcpy(ac->buf, (float*)buf->mData + avail, (copylen - avail) * sizeof(float));
    }else{
	memcpy(ac->buf + ac->bufwr, buf->mData, copylen * sizeof(float));
    }

    ac->bufwr += copylen;
    if(ac->bufwr >= ac->bufsize)
	ac->bufwr -= ac->bufsize;
    ac->bufrdavail += copylen;

    pthread_cond_signal(&ac->cond);
    pthread_mutex_unlock(&ac->mutex);

    return kAudioHardwareNoError;
}

static int
distance(const struct AudioStreamRangedDescription *desc,
        const sox_format_t *ft)
{
    int distance = 0;

    if ((desc->mFormat.mFormatFlags & kLinearPCMFormatFlagIsFloat) == 0)
        return INT_MAX;
    
    if (desc->mSampleRateRange.mMinimum > ft->signal.rate)
        distance += (desc->mSampleRateRange.mMinimum - ft->signal.rate) / 1000;

    if (desc->mSampleRateRange.mMaximum < ft->signal.rate)
        distance += (ft->signal.rate - desc->mSampleRateRange.mMaximum) / 1000;

    distance += abs(ft->signal.channels - desc->mFormat.mChannelsPerFrame);

    return distance;
}

static int setup(sox_format_t *ft, int is_input)
{
    priv_t *ac = (priv_t *)ft->priv;
    OSStatus status;
    UInt32 property_size;
    struct AudioStreamBasicDescription stream_desc;
    int32_t buf_size;
    int rc, i , count, best;
    AudioStreamID *strms, strm;
    Boolean is_writable;
    struct AudioStreamRangedDescription *strmdescs;

    if (strncmp(ft->filename, "default", (size_t)7) == 0) {
        property_size = sizeof(ac->adid);
        status = AudioHardwareGetProperty(
                is_input? kAudioHardwarePropertyDefaultInputDevice :
                    kAudioHardwarePropertyDefaultOutputDevice,
                &property_size, &ac->adid);
        if (status != noErr || ac->adid == kAudioDeviceUnknown) {
            lsx_fail_errno(ft, SOX_EPERM, "can not open default audio device");
            return SOX_EOF;
        }
	
    } else {
        AudioDeviceID *devices;
        
        status = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices,
                &property_size, &is_writable);
        if (status != noErr) {
            lsx_fail_errno(ft, SOX_EPERM, "can not get hardware prop info");
            return SOX_EOF;
        }
        
        count = property_size / sizeof(AudioDeviceID);
        devices = malloc((size_t)property_size);
        status = AudioHardwareGetProperty(kAudioHardwarePropertyDevices,
                &property_size, devices);
        if (status != noErr) {
            lsx_fail_errno(ft, SOX_EPERM, "can not enum devices");
            free(devices);
            return SOX_EOF;
        }

        for (i = 0; i < count; i++) {
            char name[256];
            
            property_size = sizeof(name);
            status = AudioDeviceGetProperty(devices[i], 0, false,
                    kAudioDevicePropertyDeviceName, &property_size,
                    &name);
            if (status != noErr)
                continue;
            
            lsx_report("Found Audio Device \"%s\"\n",name);
            
            /* String returned from OS is truncated so only compare
             * as much as returned.
             */
            if (strncmp(name,ft->filename,strlen(name)) == 0) {
                ac->adid = devices[i];
                break;
            }
        }
        free(devices);
    }

    /* Find the stream */
    status = AudioDeviceGetPropertyInfo(ac->adid, 0, is_input,
            kAudioDevicePropertyStreams, &property_size,
            &is_writable);
    if (status != noErr) {
        lsx_fail_errno(ft, SOX_EPERM, "can not get audio device properties");
        return SOX_EOF;
    }

    count = property_size / sizeof(AudioStreamID);
    assert(count >= 1);
    strms = (AudioStreamID *)malloc(property_size);

    status = AudioDeviceGetProperty(ac->adid, 0, is_input,
            kAudioDevicePropertyStreams, &property_size,
            strms);
    if (status != noErr) {
        lsx_fail_errno(ft, SOX_EPERM, "can not enum streams");
        free(strms);
        return SOX_EOF;
    }

    /* TODO: allow the user to select the stream */
    lsx_report("Found %d Streams for selected audio device, defaulting to 0\n",
	       count);

    ac->strmidx = 0;
    strm = strms[0];
    free(strms);
    
    status = AudioStreamGetPropertyInfo(strm, 0,
            kAudioStreamPropertyAvailableVirtualFormats, &property_size,
            &is_writable);
    if (status != noErr) {
        lsx_fail_errno(ft, SOX_EPERM, "failed to get format size");
        return SOX_EOF;
    }

    count = property_size / sizeof(struct AudioStreamRangedDescription);
    assert(count > 0);

    strmdescs = (struct AudioStreamRangedDescription *) malloc(property_size);
    
    status = AudioStreamGetProperty(strm, 0,
            kAudioStreamPropertyAvailableVirtualFormats, &property_size,
            strmdescs);
    if (status != noErr) {
        lsx_fail_errno(ft, SOX_EPERM, "failed to get formats");
        free(strmdescs);
        return SOX_EOF;
    }

    best = 0;
    for (i = 0; i < count; i++) {
        lsx_report("Supported format: %lu bits, %lu nchan, %lf rate, %lf min"
                ", %lf max\n",
                strmdescs[i].mFormat.mBitsPerChannel,
                strmdescs[i].mFormat.mChannelsPerFrame,
                strmdescs[i].mFormat.mSampleRate,
                strmdescs[i].mSampleRateRange.mMinimum,
                strmdescs[i].mSampleRateRange.mMaximum);

        if (distance(&strmdescs[i], ft) < distance(&strmdescs[best], ft))
            best = i;
    }

    stream_desc = strmdescs[best].mFormat;
    if (ft->signal.rate < strmdescs[best].mSampleRateRange.mMinimum) {
        ft->signal.rate = strmdescs[best].mSampleRateRange.mMinimum;
        stream_desc.mSampleRate = strmdescs[best].mSampleRateRange.mMinimum;

    } else if (ft->signal.rate > strmdescs[best].mSampleRateRange.mMaximum) {
        ft->signal.rate = strmdescs[best].mSampleRateRange.mMaximum;
        stream_desc.mSampleRate = strmdescs[best].mSampleRateRange.mMaximum;

    } else {
        stream_desc.mSampleRate = ft->signal.rate;
    }

    ft->signal.channels = stream_desc.mChannelsPerFrame;
    free(strmdescs);

    if (!(stream_desc.mFormatFlags & kLinearPCMFormatFlagIsFloat)) {
        lsx_fail_errno(ft, SOX_EPERM, "audio device does not accept floats");
        return SOX_EOF;
    }

    /* Write them back */
    property_size = sizeof(stream_desc);
    status = AudioStreamSetProperty(strm, NULL, 0,
            kAudioStreamPropertyVirtualFormat, property_size, &stream_desc);
    if (status != noErr) {
        lsx_fail_errno(ft, SOX_EPERM, "can not set audio device properties");
        return SOX_EOF;
    }
    
    /* Query device to see if it worked */
    property_size = sizeof(struct AudioStreamBasicDescription);
    status = AudioStreamGetProperty(strm, 0,kAudioStreamPropertyPhysicalFormat,
            &property_size, &stream_desc);
    
    if (status) {
        lsx_fail_errno(ft, SOX_EPERM, "can not get audio device properties");
        return SOX_EOF;
    }
    
    assert(stream_desc.mChannelsPerFrame == ft->signal.channels);
    if (stream_desc.mChannelsPerFrame != ft->signal.channels) {
        lsx_debug("audio device did not accept %d channels. Use %d channels instead.", (int)ft->signal.channels,
            (int)stream_desc.mChannelsPerFrame);
        ft->signal.channels = stream_desc.mChannelsPerFrame;
    }
    
    assert(stream_desc.mSampleRate == ft->signal.rate);
    if (stream_desc.mSampleRate != ft->signal.rate) {
        lsx_debug("audio device did not accept %d sample rate. Use %d instead.", (int)ft->signal.rate,
            (int)stream_desc.mSampleRate);
        ft->signal.rate = stream_desc.mSampleRate;
    }
    
    ac->bufsize = sox_globals.bufsiz / sizeof(sox_sample_t) * Buffactor;
    ac->bufrd = 0;
    ac->bufwr = 0;
    ac->bufrdavail = 0;
    ac->buf = lsx_malloc(ac->bufsize * sizeof(float));
    
    buf_size = sox_globals.bufsiz / sizeof(sox_sample_t) /
        stream_desc.mChannelsPerFrame;
    property_size = sizeof(buf_size);
    status = AudioDeviceSetProperty(ac->adid, NULL, 0, is_input,
        kAudioDevicePropertyBufferFrameSize,
        property_size, &buf_size);
    
    rc = pthread_mutex_init(&ac->mutex, NULL);
    if (rc) {
        lsx_fail_errno(ft, SOX_EPERM, "failed initializing mutex");
        return SOX_EOF;
    }
    
    rc = pthread_cond_init(&ac->cond, NULL);
    if (rc) {
        lsx_fail_errno(ft, SOX_EPERM, "failed initializing condition");
        return SOX_EOF;
    }
    
    ac->device_started = 0;
    
    /* Registers callback with the device without activating it. */
    status = AudioDeviceAddIOProc(ac->adid,
            is_input? RecIOProc : PlaybackIOProc, ft);
    
    return SOX_SUCCESS;
}

static int startread(sox_format_t *ft)
{
    return setup(ft, 1);
}

static size_t read_samples(sox_format_t *ft, sox_sample_t *buf, size_t nsamp)
{
    priv_t *ac = (priv_t *)ft->priv;
    size_t len;
    SOX_SAMPLE_LOCALS;

    if (!ac->device_started) {
        AudioDeviceStart(ac->adid, RecIOProc);
        ac->device_started = 1;
    }

    pthread_mutex_lock(&ac->mutex);

    /* Wait until input buffer has been filled by device driver */
    while (ac->bufrdavail == 0)
        pthread_cond_wait(&ac->cond, &ac->mutex);

    len = 0;
    while(len < nsamp && ac->bufrdavail > 0){
        buf[len] = SOX_FLOAT_32BIT_TO_SAMPLE(ac->buf[ac->bufrd], ft->clips);
        len++;
        ac->bufrd++;
        if(ac->bufrd == ac->bufsize)
            ac->bufrd = 0;
        ac->bufrdavail--;
    }

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
  free(ac->buf);

  return SOX_SUCCESS;
}

static int startwrite(sox_format_t * ft)
{
    return setup(ft, 0);
}

static size_t write_samples(sox_format_t *ft, const sox_sample_t *buf, size_t nsamp)
{
    priv_t *ac = (priv_t *)ft->priv;
    size_t i;

    SOX_SAMPLE_LOCALS;

    pthread_mutex_lock(&ac->mutex);

    /* Wait to start until mutex is locked to help prevent callback
    * getting zero samples.
    */
    if(!ac->device_started){
        if(AudioDeviceStart(ac->adid, PlaybackIOProc)){
            pthread_mutex_unlock(&ac->mutex);
            return SOX_EOF;
        }
        ac->device_started = 1;
    }

    /* globals.bufsize is in samples
    * buf_offset is in bytes
    * buf_size is in bytes
    */
    for(i = 0; i < nsamp; i++){
        while(ac->bufrdavail == ac->bufsize - 1)
            pthread_cond_wait(&ac->cond, &ac->mutex);

        ac->buf[ac->bufwr] = SOX_SAMPLE_TO_FLOAT_32BIT(buf[i], ft->clips);
        ac->bufwr++;
        if(ac->bufwr == ac->bufsize)
            ac->bufwr = 0;
        ac->bufrdavail++;
    }

    pthread_mutex_unlock(&ac->mutex);
    return nsamp;
}


static int stopwrite(sox_format_t * ft)
{
    priv_t *ac = (priv_t *)ft->priv;

    if(ac->device_started){
        pthread_mutex_lock(&ac->mutex);

        while (ac->bufrdavail > 0)
            pthread_cond_wait(&ac->cond, &ac->mutex);

        pthread_mutex_unlock(&ac->mutex);

        AudioDeviceStop(ac->adid, PlaybackIOProc);
    }

    AudioDeviceRemoveIOProc(ac->adid, PlaybackIOProc);
    pthread_cond_destroy(&ac->cond);
    pthread_mutex_destroy(&ac->mutex);
    free(ac->buf);

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
