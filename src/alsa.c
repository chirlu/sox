/* ALSA sound driver
 *
 * Copyright 1997-2005 Jimen Ching And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Jimen Ching And Sundry Contributors are not
 * responsible for the consequences of using this software.
 */

#include "st_i.h"

#ifdef HAVE_ALSA

#include <alsa/asoundlib.h>

typedef struct alsa_priv
{
    snd_pcm_t *pcm_handle;
    char *buf;
    st_size_t buf_size;
} *alsa_priv_t;

static int get_format(ft_t ft, snd_pcm_format_mask_t *fmask, int *fmt)
{
    if (ft->signal.size == -1)
        ft->signal.size = ST_SIZE_16BIT;

    if (ft->signal.size != ST_SIZE_16BIT)
    {
        st_report("trying for word samples.");
        ft->signal.size = ST_SIZE_16BIT;
    }

    if (ft->signal.encoding != ST_ENCODING_SIGN2 &&
        ft->signal.encoding != ST_ENCODING_UNSIGNED)
    {
        if (ft->signal.size == ST_SIZE_16BIT)
        {
            st_report("driver only supports signed and unsigned samples.  Changing to signed.");
            ft->signal.encoding = ST_ENCODING_SIGN2;
        }
        else
        {
            st_report("driver only supports signed and unsigned samples.  Changing to unsigned.");
            ft->signal.encoding = ST_ENCODING_UNSIGNED;
        }
    }

    /* Some hardware only wants to work with 8-bit or 16-bit data */
    if (ft->signal.size == ST_SIZE_BYTE)
    {
        if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U8)) && 
            !(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S8)))
        {
            st_report("driver doesn't supported byte samples.  Changing to words.");
            ft->signal.size = ST_SIZE_16BIT;
        }
    }
    else if (ft->signal.size == ST_SIZE_16BIT)
    {
        if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U16)) && 
            !(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S16)))
        {
            st_report("driver doesn't supported word samples.  Changing to bytes.");
            ft->signal.size = ST_SIZE_BYTE;
        }
    }
    else
    {
        if ((snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U16)) ||
            (snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S16)))
        {
            st_report("driver doesn't supported %s samples.  Changing to words.", st_sizes_str[(unsigned char)ft->signal.size]);
            ft->signal.size = ST_SIZE_16BIT;
        }
        else
        {
            st_report("driver doesn't supported %s samples.  Changing to bytes.", st_sizes_str[(unsigned char)ft->signal.size]);
            ft->signal.size = ST_SIZE_BYTE;
        }
    }

    if (ft->signal.size == ST_SIZE_BYTE) {
        switch (ft->signal.encoding)
        {
            case ST_ENCODING_SIGN2:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S8)))
                {
                    st_report("driver doesn't supported signed byte samples.  Changing to unsigned bytes.");
                    ft->signal.encoding = ST_ENCODING_UNSIGNED;
                }
                break;
            case ST_ENCODING_UNSIGNED:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U8)))
                {
                    st_report("driver doesn't supported unsigned byte samples.  Changing to signed bytes.");
                    ft->signal.encoding = ST_ENCODING_SIGN2;
                }
                break;
            default:
                    break;
        }
        switch (ft->signal.encoding)
        {
            case ST_ENCODING_SIGN2:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S8)))
                {
                    st_fail_errno(ft,ST_EFMT,"ALSA driver does not support signed byte samples");
                    return ST_EOF;
                }
                *fmt = SND_PCM_FORMAT_S8;
                break;
            case ST_ENCODING_UNSIGNED:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U8)))
                {
                    st_fail_errno(ft,ST_EFMT,"ALSA driver does not support unsigned byte samples");
                    return ST_EOF;
                }
                *fmt = SND_PCM_FORMAT_U8;
                break;
            default:
                    break;
        }
    }
    else if (ft->signal.size == ST_SIZE_16BIT) {
        switch (ft->signal.encoding)
        {
            case ST_ENCODING_SIGN2:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S16)))
                {
                    st_report("driver does not support signed word samples.  Changing to unsigned words.");
                    ft->signal.encoding = ST_ENCODING_UNSIGNED;
                }
                break;
            case ST_ENCODING_UNSIGNED:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U16)))
                {
                    st_report("driver does not support unsigned word samples.  Changing to signed words.");
                    ft->signal.encoding = ST_ENCODING_SIGN2;
                }
                break;
            default:
                    break;
        }
        switch (ft->signal.encoding)
        {
            case ST_ENCODING_SIGN2:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S16)))
                {
                    st_fail_errno(ft,ST_EFMT,"ALSA driver does not support signed word samples");
                    return ST_EOF;
                }
                *fmt = SND_PCM_FORMAT_S16_LE;
                break;
            case ST_ENCODING_UNSIGNED:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U16)))
                {
                    st_fail_errno(ft,ST_EFMT,"ALSA driver does not support unsigned word samples");
                    return ST_EOF;
                }
                *fmt = SND_PCM_FORMAT_U16_LE;
                break;
            default:
                    break;
        }
    }
    else {
        st_fail_errno(ft,ST_EFMT,"ALSA driver does not support %s %s output",
                      st_encodings_str[(unsigned char)ft->signal.encoding], st_sizes_str[(unsigned char)ft->signal.size]);
        return ST_EOF;
    }
    return 0;
}

static int st_alsasetup(ft_t ft, snd_pcm_stream_t mode)
{
    int fmt = SND_PCM_FORMAT_S16;
    int err;
    alsa_priv_t alsa = (alsa_priv_t)ft->priv;
    snd_pcm_hw_params_t *hw_params = NULL;
    unsigned int min_rate, max_rate;
    unsigned int min_chan, max_chan;
    unsigned int rate;
    snd_pcm_uframes_t buffer_size, buffer_size_min, buffer_size_max;
    snd_pcm_uframes_t period_size, period_size_min, period_size_max;
    int dir;
    snd_pcm_format_mask_t *fmask = NULL;

    if ((err = snd_pcm_open(&(alsa->pcm_handle), ft->filename, 
                            mode, 0)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot open audio device");
        goto open_error;
    }

    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) 
    {
        st_fail_errno(ft, ST_ENOMEM, 
                      "cannot allocate hardware parameter structure");
        goto open_error;
    }

    if ((err = snd_pcm_hw_params_any(alsa->pcm_handle, hw_params)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM,
                      "cannot initialize hardware parameter structure");
        goto open_error;
    }

#if SND_LIB_VERSION >= 0x010009
    /* Turn off software resampling */
    err = snd_pcm_hw_params_set_rate_resample(alsa->pcm_handle, hw_params, 0);
    if (err < 0) {
        st_fail_errno(ft, ST_EPERM, "Resampling setup failed for playback");
        goto open_error;
    }
#endif

    if ((err = snd_pcm_hw_params_set_access(alsa->pcm_handle, hw_params, 
                                            SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    {
        st_fail_errno(ft, ST_EPERM,
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
        st_fail_errno(ft, ST_EPERM, "cannot set sample format");
        goto open_error;
    }

    snd_pcm_hw_params_get_rate_min(hw_params, &min_rate, &dir);
    snd_pcm_hw_params_get_rate_max(hw_params, &max_rate, &dir);

    rate = range_limit(ft->signal.rate, min_rate, max_rate);
    if (rate != ft->signal.rate)
    {
        st_report("hardware does not support sample rate %i; changing to %i.", ft->signal.rate, rate);
        ft->signal.rate = rate;
    }
    dir = 0;
    if ((err = snd_pcm_hw_params_set_rate_near(alsa->pcm_handle, 
                                               hw_params, 
                                               &rate,
                                               &dir)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot set sample rate");
        goto open_error;
    }
    snd_pcm_hw_params_get_rate(hw_params, 
                               &rate,
                               &dir);
 
    if (rate != ft->signal.rate)
    {
        st_report("Could not set exact rate of %d.  Approximating with %d",
                ft->signal.rate, rate);
    }

    snd_pcm_hw_params_get_rate(hw_params, &rate, &dir);

    if ((err = snd_pcm_hw_params_set_channels(alsa->pcm_handle,
                                              hw_params, 
                                              ft->signal.channels)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot set channel count");
        goto open_error;
    }

    /* Have a much larger buffer than ST_BUFSIZ to avoid underruns */
    buffer_size = ST_BUFSIZ * 8 / ft->signal.size / ft->signal.channels;

    if (snd_pcm_hw_params_get_buffer_size_min(hw_params, &buffer_size_min) < 0)
    {
        st_fail_errno(ft, ST_EPERM, "Error getting min buffer size.");
        goto open_error;
    }

    if (snd_pcm_hw_params_get_buffer_size_max(hw_params, &buffer_size_max) < 0)
    {
        st_fail_errno(ft, ST_EPERM, "Error getting max buffer size.");
        goto open_error;
    }

    dir = 0;
    if (snd_pcm_hw_params_get_period_size_min(hw_params, 
                                              &period_size_min, &dir) < 0)
    {
        st_fail_errno(ft, ST_EPERM, "Error getting min period size.");
        goto open_error;
    }

    dir = 0;
    if (snd_pcm_hw_params_get_period_size_max(hw_params, 
                                              &period_size_max, &dir) < 0)
    {
        st_fail_errno(ft, ST_EPERM, "Error getting max buffer size.");
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
        st_fail_errno(ft, ST_EPERM, "Error setting periods.");
        goto open_error;
    }
    snd_pcm_hw_params_get_period_size(hw_params, &period_size, &dir);

    dir = 0;
    if (snd_pcm_hw_params_set_buffer_size_near(alsa->pcm_handle, hw_params, 
                                               &buffer_size) < 0) {
        st_fail_errno(ft, ST_EPERM, "Error setting buffer size.");
        goto open_error;
    }

    snd_pcm_hw_params_get_buffer_size(hw_params, &buffer_size);
    if (period_size * 2 > buffer_size)
    {
        st_fail_errno(ft, ST_EPERM, "Buffer too small. Could not use.");
        goto open_error;
    }

    if ((err = snd_pcm_hw_params(alsa->pcm_handle, hw_params)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot set parameters");
        goto open_error;
    }

    snd_pcm_hw_params_free(hw_params);
    hw_params = NULL;

    if ((err = snd_pcm_prepare(alsa->pcm_handle)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot prepare audio interface for use");
        goto open_error;
    }

    alsa->buf_size = buffer_size * ft->signal.size * ft->signal.channels;
    alsa->buf = xmalloc(alsa->buf_size);

    return (ST_SUCCESS);

open_error:
    if (fmask)
        snd_pcm_format_mask_free(fmask);
    if (hw_params)
        snd_pcm_hw_params_free(hw_params);

    return ST_EOF;

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
            st_warn("Can't recover from over/underrun, prepare failed: %s", snd_strerror(err));
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
                    st_warn("Can't recovery from suspend, prepare failed: %s", snd_strerror(err));
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
static int st_alsastartread(ft_t ft)
{
    return st_alsasetup(ft, SND_PCM_STREAM_CAPTURE);
}

static void st_ub_read_buf(st_sample_t *buf1, char const * buf2, st_size_t len, char swap UNUSED, st_size_t * clips UNUSED)
{
    while (len--)
        *buf1++ = ST_UNSIGNED_BYTE_TO_SAMPLE(*((unsigned char *)buf2++),);
}

static void st_sb_read_buf(st_sample_t *buf1, char const * buf2, st_size_t len, char swap UNUSED, st_size_t * clips UNUSED)
{
    while (len--)
        *buf1++ = ST_SIGNED_BYTE_TO_SAMPLE(*((int8_t *)buf2++),);
}

static void st_uw_read_buf(st_sample_t *buf1, char const * buf2, st_size_t len, char swap, st_size_t * clips UNUSED)
{
    while (len--)
    {
        uint16_t datum = *((uint16_t *)buf2);
        buf2++; buf2++;
        if (swap)
            datum = st_swapw(datum);

        *buf1++ = ST_UNSIGNED_WORD_TO_SAMPLE(datum,);
    }
}

static void st_sw_read_buf(st_sample_t *buf1, char const * buf2, st_size_t len, char swap, st_size_t * clips UNUSED)
{
    while (len--)
    {
        int16_t datum = *((int16_t *)buf2);
        buf2++; buf2++;
        if (swap)
            datum = st_swapw(datum);

        *buf1++ = ST_SIGNED_WORD_TO_SAMPLE(datum,);
    }
}

static st_size_t st_alsaread(ft_t ft, st_sample_t *buf, st_size_t nsamp)
{
    st_size_t len;
    int err;
    alsa_priv_t alsa = (alsa_priv_t)ft->priv;
    void (*read_buf)(st_sample_t *, char const *, st_size_t, char, st_size_t *) = 0;

    switch(ft->signal.size) {
        case ST_SIZE_BYTE:
            switch(ft->signal.encoding)
            {
                case ST_ENCODING_SIGN2:
                    read_buf = st_sb_read_buf;
                    break;
                case ST_ENCODING_UNSIGNED:
                    read_buf = st_ub_read_buf;
                    break;
                default:
                    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size");
                    return 0;
            }
            break;
        case ST_SIZE_16BIT:
            switch(ft->signal.encoding)
            {
                case ST_ENCODING_SIGN2:
                    read_buf = st_sw_read_buf;
                    break;
                case ST_ENCODING_UNSIGNED:
                    read_buf = st_uw_read_buf;
                    break;
                default:
                    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size");
                    return 0;
            }
            break;
        default:
            st_fail_errno(ft,ST_EFMT,"Do not support this data size for this handler");
            return 0;
    }

    /* Prevent overflow */
    if (nsamp > alsa->buf_size/ft->signal.size)
        nsamp = (alsa->buf_size/ft->signal.size);
    len = 0;

    while (len < nsamp)
    {
        /* ALSA library takes "frame" counts. */
        err = snd_pcm_readi(alsa->pcm_handle, alsa->buf, 
                            (nsamp - len)/ft->signal.channels);
        if (err < 0)
        {
            if (xrun_recovery(alsa->pcm_handle, err) < 0)
            {
                st_fail_errno(ft, ST_EPERM, "ALSA write error");
                return 0;
            }
        }
        else
        {
            read_buf(buf+(len*sizeof(st_sample_t)), alsa->buf, err, ft->signal.reverse_bytes, &ft->clips);
            len += err * ft->signal.channels;
        }
    }

    return len;
}

static int st_alsastopread(ft_t ft)
{
    alsa_priv_t alsa = (alsa_priv_t)ft->priv;

    snd_pcm_close(alsa->pcm_handle);

    free(alsa->buf);

    return ST_SUCCESS;
}

static int st_alsastartwrite(ft_t ft)
{
    return st_alsasetup(ft, SND_PCM_STREAM_PLAYBACK);
}

static void st_ub_write_buf(char* buf1, st_sample_t const * buf2, st_size_t len, char swap UNUSED, st_size_t * clips)
{
    while (len--)
        *(uint8_t *)buf1++ = ST_SAMPLE_TO_UNSIGNED_BYTE(*buf2++, *clips);
}

static void st_sb_write_buf(char *buf1, st_sample_t const * buf2, st_size_t len, char swap UNUSED, st_size_t * clips)
{
    while (len--)
        *(int8_t *)buf1++ = ST_SAMPLE_TO_SIGNED_BYTE(*buf2++, *clips);
}

static void st_uw_write_buf(char *buf1, st_sample_t const * buf2, st_size_t len, char swap, st_size_t * clips)
{
    while (len--)
    {
        uint16_t datum = ST_SAMPLE_TO_UNSIGNED_WORD(*buf2++, *clips);
        if (swap)
            datum = st_swapw(datum);
        *(uint16_t *)buf1 = datum;
        buf1++; buf1++;
    }
}

static void st_sw_write_buf(char *buf1, st_sample_t const * buf2, st_size_t len, char swap, st_size_t * clips)
{
    while (len--)
    {
        int16_t datum = ST_SAMPLE_TO_SIGNED_WORD(*buf2++, *clips);
        if (swap)
            datum = st_swapw(datum);
        *(int16_t *)buf1 = datum;
        buf1++; buf1++;
    }
}

static st_size_t st_alsawrite(ft_t ft, const st_sample_t *buf, st_size_t nsamp)
{
    st_size_t osamp, done;
    alsa_priv_t alsa = (alsa_priv_t)ft->priv;
    void (*write_buf)(char *, const st_sample_t *, st_size_t, char, st_size_t *) = 0;

    switch(ft->signal.size) {
        case ST_SIZE_BYTE:
            switch (ft->signal.encoding)
            {
                case ST_ENCODING_SIGN2:
                    write_buf = st_sb_write_buf;
                    break;
                case ST_ENCODING_UNSIGNED:
                    write_buf = st_ub_write_buf;
                    break;
                default:
                    st_fail_errno(ft,ST_EFMT,"this encoding is not supported for this data size");
                    return 0;
            }
            break;
        case ST_SIZE_16BIT:
            switch (ft->signal.encoding)
            {
                case ST_ENCODING_SIGN2:
                    write_buf = st_sw_write_buf;
                    break;
                case ST_ENCODING_UNSIGNED:
                    write_buf = st_uw_write_buf;
                    break;
                default:
                    st_fail_errno(ft,ST_EFMT,"this encoding is not supported for this data size");
                    return 0;
            }
            break;
        default:
            st_fail_errno(ft,ST_EFMT,"this data size is not supported by this handler");
            return 0;
    }

    for (done = 0; done < nsamp; done += osamp) {
      int err;
      st_size_t len;
      
      osamp = min(nsamp - done, alsa->buf_size / ft->signal.size);
      write_buf(alsa->buf, buf, osamp, ft->signal.reverse_bytes, &ft->clips);
      buf += osamp;

      for (len = 0; len < osamp;) {
        err = snd_pcm_writei(alsa->pcm_handle, 
                             alsa->buf + (len * ft->signal.size), 
                             (osamp - len) / ft->signal.channels);
        if (errno == EAGAIN) /* Happens naturally; don't report it */
          errno = 0;
        if (err < 0) {
          if (xrun_recovery(alsa->pcm_handle, err) < 0) {
            st_fail_errno(ft, ST_EPERM, "ALSA write error");
            return 0;
          }
        } else
          len += err * ft->signal.channels;
      }
    }

    return nsamp;
}


static int st_alsastopwrite(ft_t ft)
{
    alsa_priv_t alsa = (alsa_priv_t)ft->priv;

    snd_pcm_drain(alsa->pcm_handle);
    snd_pcm_close(alsa->pcm_handle);

    free(alsa->buf);

    return ST_SUCCESS;
}

static const char *alsanames[] = {
  "alsa",
  NULL
};

static st_format_t st_alsa_format = {
  alsanames,
  NULL,
  ST_FILE_DEVICE | ST_FILE_NOSTDIO,
  st_alsastartread,
  st_alsaread,
  st_alsastopread,
  st_alsastartwrite,
  st_alsawrite,
  st_alsastopwrite,
  st_format_nothing_seek
};

const st_format_t *st_alsa_format_fn(void)
{
    return &st_alsa_format;
}

#endif /* HAVE_ALSA */
