/* ALSA sound handler
 *
 * Copyright 1997-2005 Jimen Ching And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Jimen Ching And Sundry Contributors are not
 * responsible for the consequences of using this software.
 */

#include "sox_i.h"

#include <alsa/asoundlib.h>

typedef struct {
  snd_pcm_t *pcm_handle;
  char *buf;
  sox_size_t buf_size;
  snd_pcm_uframes_t period_size;
  snd_pcm_uframes_t frames_this_period;
} priv_t;

static int get_format(sox_format_t * ft, snd_pcm_format_mask_t *fmask, int *fmt)
{
    if (ft->encoding.bits_per_sample != 16)
    {
        sox_report("trying for word samples.");
        ft->encoding.bits_per_sample = 16;
    }

    if (ft->encoding.encoding != SOX_ENCODING_SIGN2 &&
        ft->encoding.encoding != SOX_ENCODING_UNSIGNED)
    {
        if (ft->encoding.bits_per_sample == 16)
        {
          if (ft->encoding.encoding != SOX_ENCODING_UNKNOWN)
            sox_report("driver supports only signed and unsigned samples.  Changing to signed.");
            ft->encoding.encoding = SOX_ENCODING_SIGN2;
        }
        else
        {
          if (ft->encoding.encoding != SOX_ENCODING_UNKNOWN)
            sox_report("driver supports only signed and unsigned samples.  Changing to unsigned.");
            ft->encoding.encoding = SOX_ENCODING_UNSIGNED;
        }
    }

    /* Some hardware only wants to work with 8-bit or 16-bit data */
    if (ft->encoding.bits_per_sample == 8)
    {
        if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U8)) &&
            !(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S8)))
        {
            sox_report("driver doesn't supported byte samples.  Changing to words.");
            ft->encoding.bits_per_sample = 16;
        }
    }
    else if (ft->encoding.bits_per_sample == 16)
    {
        if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U16)) &&
            !(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S16)))
        {
            sox_report("driver doesn't supported word samples.  Changing to bytes.");
            ft->encoding.bits_per_sample = 8;
        }
    }
    else
    {
        if ((snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U16)) ||
            (snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S16)))
        {
            sox_report("driver doesn't supported %u-bit samples.  Changing to 16-bit.", ft->encoding.bits_per_sample);
            ft->encoding.bits_per_sample = 16;
        }
        else
        {
            sox_report("driver doesn't supported %u-bit samples.  Changing to 8-bit.", ft->encoding.bits_per_sample);
            ft->encoding.bits_per_sample = 8;
        }
    }

    if (ft->encoding.bits_per_sample == 8) {
        switch (ft->encoding.encoding)
        {
            case SOX_ENCODING_SIGN2:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S8)))
                {
                    sox_report("driver doesn't supported signed byte samples.  Changing to unsigned bytes.");
                    ft->encoding.encoding = SOX_ENCODING_UNSIGNED;
                }
                break;
            case SOX_ENCODING_UNSIGNED:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U8)))
                {
                    sox_report("driver doesn't supported unsigned byte samples.  Changing to signed bytes.");
                    ft->encoding.encoding = SOX_ENCODING_SIGN2;
                }
                break;
            default:
                    break;
        }
        switch (ft->encoding.encoding)
        {
            case SOX_ENCODING_SIGN2:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S8)))
                {
                    lsx_fail_errno(ft,SOX_EFMT,"ALSA driver does not support signed byte samples");
                    return SOX_EOF;
                }
                *fmt = SND_PCM_FORMAT_S8;
                break;
            case SOX_ENCODING_UNSIGNED:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U8)))
                {
                    lsx_fail_errno(ft,SOX_EFMT,"ALSA driver does not support unsigned byte samples");
                    return SOX_EOF;
                }
                *fmt = SND_PCM_FORMAT_U8;
                break;
            default:
                    break;
        }
    }
    else if (ft->encoding.bits_per_sample == 16) {
        switch (ft->encoding.encoding)
        {
            case SOX_ENCODING_SIGN2:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S16)))
                {
                    sox_report("driver does not support signed word samples.  Changing to unsigned words.");
                    ft->encoding.encoding = SOX_ENCODING_UNSIGNED;
                }
                break;
            case SOX_ENCODING_UNSIGNED:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U16)))
                {
                    sox_report("driver does not support unsigned word samples.  Changing to signed words.");
                    ft->encoding.encoding = SOX_ENCODING_SIGN2;
                }
                break;
            default:
                    break;
        }
        switch (ft->encoding.encoding)
        {
            case SOX_ENCODING_SIGN2:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S16)))
                {
                    lsx_fail_errno(ft,SOX_EFMT,"ALSA driver does not support signed word samples");
                    return SOX_EOF;
                }
                *fmt = SND_PCM_FORMAT_S16;
                break;
            case SOX_ENCODING_UNSIGNED:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U16)))
                {
                    lsx_fail_errno(ft,SOX_EFMT,"ALSA driver does not support unsigned word samples");
                    return SOX_EOF;
                }
                *fmt = SND_PCM_FORMAT_U16;
                break;
            default:
                    break;
        }
    }
    else {
        lsx_fail_errno(ft,SOX_EFMT,"ALSA driver does not support %s %u-bit output",
                      sox_encodings_info[(unsigned char)ft->encoding.encoding].desc, ft->encoding.bits_per_sample);
        return SOX_EOF;
    }
    return 0;
}

static int setup(sox_format_t * ft, snd_pcm_stream_t mode)
{
    int fmt = SND_PCM_FORMAT_S16;
    int err;
    priv_t * alsa = (priv_t *)ft->priv;
    snd_pcm_hw_params_t *hw_params = NULL;
    unsigned int min_rate, max_rate;
    unsigned int min_chan, max_chan;
    unsigned int rate;
    snd_pcm_uframes_t buffer_size, buffer_size_min, buffer_size_max;
    snd_pcm_uframes_t period_size, period_size_min, period_size_max;
    int dir;
    snd_pcm_format_mask_t *fmask = NULL;
    sox_signalinfo_t client_signal = ft->signal;

    lsx_set_signal_defaults(&ft->signal);

    if ((err = snd_pcm_open(&(alsa->pcm_handle), ft->filename,
                            mode, 0)) < 0)
    {
        lsx_fail_errno(ft, SOX_EPERM, "cannot open audio device");
        goto open_error;
    }

    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0)
    {
        lsx_fail_errno(ft, SOX_ENOMEM,
                      "cannot allocate hardware parameter structure");
        goto open_error;
    }

    if ((err = snd_pcm_hw_params_any(alsa->pcm_handle, hw_params)) < 0)
    {
        lsx_fail_errno(ft, SOX_EPERM,
                      "cannot initialize hardware parameter structure");
        goto open_error;
    }

#if SND_LIB_VERSION >= 0x010009
    /* Turn off software resampling */
    err = snd_pcm_hw_params_set_rate_resample(alsa->pcm_handle, hw_params, 0);
    if (err < 0) {
        lsx_fail_errno(ft, SOX_EPERM, "Resampling setup failed for playback");
        goto open_error;
    }
#endif

    if ((err = snd_pcm_hw_params_set_access(alsa->pcm_handle, hw_params,
                                            SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    {
        lsx_fail_errno(ft, SOX_EPERM,
                      "cannot set access type");
        goto open_error;
    }

    snd_pcm_hw_params_get_channels_min(hw_params, &min_chan);
    snd_pcm_hw_params_get_channels_max(hw_params, &max_chan);
    if (ft->signal.channels == 0)
        ft->signal.channels = min_chan;
    else
        if (ft->signal.channels > max_chan)
            ft->signal.channels = max_chan;
        else if (ft->signal.channels < min_chan)
            ft->signal.channels = min_chan;

    if (snd_pcm_format_mask_malloc(&fmask) < 0)
        goto open_error;
    snd_pcm_hw_params_get_format_mask(hw_params, fmask);

    if (get_format(ft, fmask, &fmt) < 0)
        goto open_error;

    snd_pcm_format_mask_free(fmask);
    fmask = NULL;

    if ((err = snd_pcm_hw_params_set_format(alsa->pcm_handle,
                                            hw_params, fmt)) < 0)
    {
        lsx_fail_errno(ft, SOX_EPERM, "cannot set sample format");
        goto open_error;
    }

    snd_pcm_hw_params_get_rate_min(hw_params, &min_rate, &dir);
    snd_pcm_hw_params_get_rate_max(hw_params, &max_rate, &dir);

    rate = range_limit(ft->signal.rate, min_rate, max_rate);
    if (rate != ft->signal.rate)
    {
      if (client_signal.rate != 0)
        sox_report("hardware does not support sample rate %g; changing to %i.", ft->signal.rate, rate);
        ft->signal.rate = rate;
    }
    dir = 0;
    if ((err = snd_pcm_hw_params_set_rate_near(alsa->pcm_handle,
                                               hw_params,
                                               &rate,
                                               &dir)) < 0)
    {
        lsx_fail_errno(ft, SOX_EPERM, "cannot set sample rate");
        goto open_error;
    }
    snd_pcm_hw_params_get_rate(hw_params,
                               &rate,
                               &dir);

    if (rate != ft->signal.rate)
    {
        sox_report("Could not set exact rate of %g.  Approximating with %i",
                ft->signal.rate, rate);
    }

    snd_pcm_hw_params_get_rate(hw_params, &rate, &dir);

    if ((err = snd_pcm_hw_params_set_channels(alsa->pcm_handle,
                                              hw_params,
                                              ft->signal.channels)) < 0)
    {
        lsx_fail_errno(ft, SOX_EPERM, "cannot set channel count");
        goto open_error;
    }

    /* Have a much larger buffer than sox_globals.bufsiz to avoid underruns */
    buffer_size = sox_globals.bufsiz * 8 / (ft->encoding.bits_per_sample >> 3) / ft->signal.channels;

    if (snd_pcm_hw_params_get_buffer_size_min(hw_params, &buffer_size_min) < 0)
    {
        lsx_fail_errno(ft, SOX_EPERM, "Error getting min buffer size.");
        goto open_error;
    }

    if (snd_pcm_hw_params_get_buffer_size_max(hw_params, &buffer_size_max) < 0)
    {
        lsx_fail_errno(ft, SOX_EPERM, "Error getting max buffer size.");
        goto open_error;
    }

    dir = 0;
    if (snd_pcm_hw_params_get_period_size_min(hw_params,
                                              &period_size_min, &dir) < 0)
    {
        lsx_fail_errno(ft, SOX_EPERM, "Error getting min period size.");
        goto open_error;
    }

    dir = 0;
    if (snd_pcm_hw_params_get_period_size_max(hw_params,
                                              &period_size_max, &dir) < 0)
    {
        lsx_fail_errno(ft, SOX_EPERM, "Error getting max buffer size.");
        goto open_error;
    }

    if (buffer_size_max < buffer_size)
        buffer_size = buffer_size_max;
    else if (buffer_size_min > buffer_size)
        buffer_size = buffer_size_min;

    period_size = buffer_size / 8;
    buffer_size = period_size * 8;

    dir = 0;
    if (snd_pcm_hw_params_set_period_size_near(alsa->pcm_handle, hw_params,
                                               &period_size, &dir) < 0)
    {
        lsx_fail_errno(ft, SOX_EPERM, "Error setting periods.");
        goto open_error;
    }
    snd_pcm_hw_params_get_period_size(hw_params, &period_size, &dir);

    dir = 0;
    if (snd_pcm_hw_params_set_buffer_size_near(alsa->pcm_handle, hw_params,
                                               &buffer_size) < 0) {
        lsx_fail_errno(ft, SOX_EPERM, "Error setting buffer size.");
        goto open_error;
    }

    snd_pcm_hw_params_get_buffer_size(hw_params, &buffer_size);
    if (period_size * 2 > buffer_size)
    {
        lsx_fail_errno(ft, SOX_EPERM, "Buffer too small. Could not use.");
        goto open_error;
    }

    if ((err = snd_pcm_hw_params(alsa->pcm_handle, hw_params)) < 0)
    {
        lsx_fail_errno(ft, SOX_EPERM, "cannot set parameters");
        goto open_error;
    }

    snd_pcm_hw_params_free(hw_params);
    hw_params = NULL;

    if ((err = snd_pcm_prepare(alsa->pcm_handle)) < 0)
    {
        lsx_fail_errno(ft, SOX_EPERM, "cannot prepare audio interface for use");
        goto open_error;
    }

    alsa->buf_size = buffer_size * (ft->encoding.bits_per_sample >> 3) * ft->signal.channels;
    alsa->period_size = period_size;
    alsa->frames_this_period = 0;
    alsa->buf = lsx_malloc(alsa->buf_size);

    return (SOX_SUCCESS);

open_error:
    if (fmask)
        snd_pcm_format_mask_free(fmask);
    if (hw_params)
        snd_pcm_hw_params_free(hw_params);

    return SOX_EOF;

}

/*
 *   Over/underrun and suspend recovery
 */
static int xrun_recovery(snd_pcm_t *handle, int err)
{
    if (err == -EPIPE)
    {   /* over/under-run */
        err = snd_pcm_prepare(handle);
        if (err < 0)
            sox_warn("Can't recover from over/underrun, prepare failed: %s", snd_strerror(err));
        return 0;
    }
    else
    {
        if (err == -ESTRPIPE)
        {
            /* wait until the suspend flag is released */
            while ((err = snd_pcm_resume(handle)) == -EAGAIN)
                sleep(1);
            if (err < 0)
            {
                err = snd_pcm_prepare(handle);
                if (err < 0)
                    sox_warn("Can't recovery from suspend, prepare failed: %s", snd_strerror(err));
            }
        }
        return 0;
    }
    return err;
}

/*
 * Do anything required before you start reading samples.
 * Read file header.
 *      Find out sampling rate,
 *      size and encoding of samples,
 *      mono/stereo/quad.
 */
static int startread(sox_format_t * ft)
{
    return setup(ft, SND_PCM_STREAM_CAPTURE);
}

static void ub_read_buf(sox_sample_t *buf1, char const * buf2, sox_size_t len, sox_bool swap UNUSED, sox_size_t * clips UNUSED)
{
    while (len--)
        *buf1++ = SOX_UNSIGNED_8BIT_TO_SAMPLE(*((unsigned char *)buf2++),);
}

static void sb_read_buf(sox_sample_t *buf1, char const * buf2, sox_size_t len, sox_bool swap UNUSED, sox_size_t * clips UNUSED)
{
    while (len--)
        *buf1++ = SOX_SIGNED_8BIT_TO_SAMPLE(*((int8_t *)buf2++),);
}

static void uw_read_buf(sox_sample_t *buf1, char const * buf2, sox_size_t len, sox_bool swap, sox_size_t * clips UNUSED)
{
    while (len--)
    {
        uint16_t datum = *((uint16_t *)buf2);
        buf2++; buf2++;
        if (swap)
            datum = lsx_swapw(datum);

        *buf1++ = SOX_UNSIGNED_16BIT_TO_SAMPLE(datum,);
    }
}

static void sw_read_buf(sox_sample_t *buf1, char const * buf2, sox_size_t len, sox_bool swap, sox_size_t * clips UNUSED)
{
    while (len--)
    {
        int16_t datum = *((int16_t *)buf2);
        buf2++; buf2++;
        if (swap)
            datum = lsx_swapw(datum);

        *buf1++ = SOX_SIGNED_16BIT_TO_SAMPLE(datum,);
    }
}

static sox_size_t read_samples(sox_format_t * ft, sox_sample_t *buf, sox_size_t nsamp)
{
    priv_t * alsa = (priv_t *)ft->priv;
    void (*read_buf)(sox_sample_t *, char const *, sox_size_t, sox_bool, sox_size_t *) = 0;
    sox_size_t len;

    switch(ft->encoding.bits_per_sample) {
      case 8:
        switch(ft->encoding.encoding) {
          case SOX_ENCODING_SIGN2:    read_buf = sb_read_buf; break;
          case SOX_ENCODING_UNSIGNED: read_buf = ub_read_buf; break;
          default:
            lsx_fail_errno(ft,SOX_EFMT,"Do not support this encoding for this data size");
            return 0;
        }
        break;
      case 16:
        switch(ft->encoding.encoding) {
          case SOX_ENCODING_SIGN2:    read_buf = sw_read_buf; break;
          case SOX_ENCODING_UNSIGNED: read_buf = uw_read_buf; break;
          default:
            lsx_fail_errno(ft,SOX_EFMT,"Do not support this encoding for this data size");
            return 0;
        }
        break;
      default:
        lsx_fail_errno(ft,SOX_EFMT,"Do not support this data size for this handler");
        return 0;
    }

    /* Prevent overflow */
    if (nsamp > alsa->buf_size/(ft->encoding.bits_per_sample >> 3))
      nsamp = (alsa->buf_size/(ft->encoding.bits_per_sample >> 3));

    len = 0;
    while (len < nsamp) {
      sox_size_t n = snd_pcm_readi(alsa->pcm_handle, alsa->buf,
          (nsamp - len)/ft->signal.channels); /* ALSA takes "frame" counts. */
      if ((int)n < 0) {
        if (xrun_recovery(alsa->pcm_handle, (int)n) < 0) {
          lsx_fail_errno(ft, SOX_EPERM, "ALSA read error");
          return 0;
        }
      } else {
        n *= ft->signal.channels;
        read_buf(buf + len, alsa->buf, n, ft->encoding.reverse_bytes, &ft->clips);
        len += n;
      }
    }
    return len;
}

static int stopread(sox_format_t * ft)
{
    priv_t * alsa = (priv_t *)ft->priv;

    snd_pcm_close(alsa->pcm_handle);

    free(alsa->buf);

    return SOX_SUCCESS;
}

static int startwrite(sox_format_t * ft)
{
    return setup(ft, SND_PCM_STREAM_PLAYBACK);
}

static void sox_ub_write_buf(char* buf1, sox_sample_t const * buf2, sox_size_t len, sox_bool swap UNUSED, sox_size_t * clips)
{
    while (len--)
        *(uint8_t *)buf1++ = SOX_SAMPLE_TO_UNSIGNED_8BIT(*buf2++, *clips);
}

static void sox_sb_write_buf(char *buf1, sox_sample_t const * buf2, sox_size_t len, sox_bool swap UNUSED, sox_size_t * clips)
{
    while (len--)
        *(int8_t *)buf1++ = SOX_SAMPLE_TO_SIGNED_8BIT(*buf2++, *clips);
}

static void sox_uw_write_buf(char *buf1, sox_sample_t const * buf2, sox_size_t len, sox_bool swap, sox_size_t * clips)
{
    while (len--)
    {
        uint16_t datum = SOX_SAMPLE_TO_UNSIGNED_16BIT(*buf2++, *clips);
        if (swap)
            datum = lsx_swapw(datum);
        *(uint16_t *)buf1 = datum;
        buf1++; buf1++;
    }
}

static void sox_sw_write_buf(char *buf1, sox_sample_t const * buf2, sox_size_t len, sox_bool swap, sox_size_t * clips)
{
    while (len--)
    {
        int16_t datum = SOX_SAMPLE_TO_SIGNED_16BIT(*buf2++, *clips);
        if (swap)
            datum = lsx_swapw(datum);
        *(int16_t *)buf1 = datum;
        buf1++; buf1++;
    }
}

static sox_size_t write_samples(sox_format_t * ft, const sox_sample_t *buf, sox_size_t nsamp)
{
    sox_size_t osamp, done;
    priv_t * alsa = (priv_t *)ft->priv;
    void (*write_buf)(char *, const sox_sample_t *, sox_size_t, sox_bool, sox_size_t *) = 0;

    switch(ft->encoding.bits_per_sample) {
        case 8:
            switch (ft->encoding.encoding)
            {
                case SOX_ENCODING_SIGN2:
                    write_buf = sox_sb_write_buf;
                    break;
                case SOX_ENCODING_UNSIGNED:
                    write_buf = sox_ub_write_buf;
                    break;
                default:
                    lsx_fail_errno(ft,SOX_EFMT,"this encoding is not supported for this data size");
                    return 0;
            }
            break;
        case 16:
            switch (ft->encoding.encoding)
            {
                case SOX_ENCODING_SIGN2:
                    write_buf = sox_sw_write_buf;
                    break;
                case SOX_ENCODING_UNSIGNED:
                    write_buf = sox_uw_write_buf;
                    break;
                default:
                    lsx_fail_errno(ft,SOX_EFMT,"this encoding is not supported for this data size");
                    return 0;
            }
            break;
        default:
            lsx_fail_errno(ft,SOX_EFMT,"this data size is not supported by this handler");
            return 0;
    }

    for (done = 0; done < nsamp; done += osamp) {
      int err;
      sox_size_t len;

      osamp = min(nsamp - done, alsa->buf_size / (ft->encoding.bits_per_sample >> 3));
      write_buf(alsa->buf, buf, osamp, ft->encoding.reverse_bytes, &ft->clips);
      buf += osamp;

      for (len = 0; len < osamp;) {
        err = snd_pcm_writei(alsa->pcm_handle,
                             alsa->buf + (len * (ft->encoding.bits_per_sample >> 3)),
                             (osamp - len) / ft->signal.channels);
        if (errno == EAGAIN) /* Happens naturally; don't report it */
          errno = 0;
        if (err < 0) {
          if (xrun_recovery(alsa->pcm_handle, err) < 0) {
            lsx_fail_errno(ft, SOX_EPERM, "ALSA write error");
            return 0;
          }
        } else
          len += err * ft->signal.channels;
      }
    }

    /* keep track of how many frames have been played this period, so we know
     * how many frames of silence to append at the end of playback */
    alsa->frames_this_period = (alsa->frames_this_period + nsamp / ft->signal.channels) % alsa->period_size;
    return nsamp;
}


static int stopwrite(sox_format_t * ft)
{
  priv_t * alsa = (priv_t *)ft->priv;

  /* Pad to hardware period: */
  sox_size_t npad = (alsa->period_size - alsa->frames_this_period) * ft->signal.channels;
  sox_sample_t * buf = lsx_calloc(npad, sizeof(*buf)); /* silent samples */
  write_samples(ft, buf, npad);
  free(buf);

  snd_pcm_drain(alsa->pcm_handle);
  snd_pcm_close(alsa->pcm_handle);
  free(alsa->buf);
  return SOX_SUCCESS;
}

SOX_FORMAT_HANDLER(alsa)
{
  static char const * const names[] = { "alsa", NULL };
  static unsigned const write_encodings[] = {
    SOX_ENCODING_SIGN2, 16, 8, 0,
    SOX_ENCODING_UNSIGNED, 16, 8, 0,
    0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "Advanced Linux Sound Architecture device driver",
    names, SOX_FILE_DEVICE | SOX_FILE_NOSTDIO,
    startread, read_samples, stopread,
    startwrite, write_samples, stopwrite,
    NULL, write_encodings, NULL, sizeof(priv_t)
  };
  return &handler;
}
