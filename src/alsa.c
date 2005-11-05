/*
 * Copyright 1997-2005 Jimen Ching And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Jimen Ching And Sundry Contributors are not
 * responsible for the consequences of using this software.
 */

/* ALSA sound driver
 *
 * converted to alsalib cbagwell 20050914
 * added by Jimen Ching (jching@flex.com) 19990207
 * based on info grabed from aplay.c in alsa-utils package.
 * Updated for ALSA 0.9.X API 20020824.
 */

#include "st_i.h"

#if defined(HAVE_ALSA)

#include <alsa/asoundlib.h>

typedef struct alsa_priv
{
    snd_pcm_t *pcm_handle;
    void *buf;
    st_ssize_t buf_size;
} *alsa_priv_t;

static int get_format(ft_t ft, snd_pcm_format_mask_t *fmask, int *fmt);

extern void st_ub_write_buf(char* buf1, st_sample_t *buf2, st_ssize_t len, char swap);
extern void st_sb_write_buf(char *buf1, st_sample_t *buf2, st_ssize_t len, char swap);
extern void st_uw_write_buf(char *buf1, st_sample_t *buf2, st_ssize_t len, char swap);
extern void st_sw_write_buf(char *buf1, st_sample_t *buf2, st_ssize_t len, char swap);
extern void st_ub_read_buf(st_sample_t *buf1, char *buf2, st_ssize_t len, char swap);
extern void st_sb_read_buf(st_sample_t *buf1, char *buf2, st_ssize_t len, char swap);
extern void st_uw_read_buf(st_sample_t *buf1, char *buf2, st_ssize_t len, char swap);
extern void st_sw_read_buf(st_sample_t *buf1, char *buf2, st_ssize_t len, char swap);

int st_alsasetup(ft_t ft, snd_pcm_stream_t mode)
{
    int fmt = SND_PCM_FORMAT_S16;
    int err;
    alsa_priv_t alsa = (alsa_priv_t)ft->priv;
    snd_pcm_hw_params_t *hw_params = NULL;
#if 0
    snd_pcm_sw_params_t *sw_params;
#endif
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
    rate = 0;
    err = snd_pcm_hw_params_set_rate_resample(alsa->pcm_handle, hw_params, 
                                              rate);
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
    if (ft->info.channels == -1) 
        ft->info.channels = min_chan;
    else 
        if (ft->info.channels > max_chan) 
            ft->info.channels = max_chan;
        else if (ft->info.channels < min_chan) 
            ft->info.channels = min_chan;

    snd_pcm_format_mask_malloc(&fmask);
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

    rate = ft->info.rate;
    snd_pcm_hw_params_get_rate_min(hw_params, &min_rate, &dir);
    snd_pcm_hw_params_get_rate_max(hw_params, &max_rate, &dir);

    if (min_rate != -1 && rate < min_rate) 
        rate = min_rate;
    else 
        if (max_rate != -1 && rate > max_rate) 
            rate = max_rate;
    if (rate != ft->info.rate)
    {
        st_report("alsa: Hardware does not support %d.  Forcing sample rate to %d.", ft->info.rate, rate);
        ft->info.rate = rate;
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
 
    if (rate != ft->info.rate)
    {
        st_report("Could not set exact rate of %d.  Approximating with %d",
                ft->info.rate, rate);
    }

    snd_pcm_hw_params_get_rate(hw_params, &rate, &dir);

    if ((err = snd_pcm_hw_params_set_channels(alsa->pcm_handle,
                                              hw_params, 
                                              ft->info.channels)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot set channel count");
        goto open_error;
    }

    buffer_size = (ST_BUFSIZ / sizeof(st_sample_t) / ft->info.channels);

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
        st_fail_errno(ft, ST_EPERM, "Error setting buffersize.");
        goto open_error;
    }

    snd_pcm_hw_params_get_buffer_size(hw_params, &buffer_size);
    if (period_size*2 > buffer_size)
    {
        st_fail_errno(ft, ST_EPERM, "Buffer to small. Could not use.");
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

    alsa->buf_size = buffer_size * ft->info.size * ft->info.channels;

    if ((alsa->buf = malloc(alsa->buf_size)) == NULL) 
    {
        st_fail_errno(ft,ST_ENOMEM,
                      "unable to allocate output buffer of size %d", 
                      ft->file.size);
        return(ST_EOF);
    }

    return (ST_SUCCESS);

open_error:
    if (fmask)
        snd_pcm_format_mask_free(fmask);
    if (hw_params)
        snd_pcm_hw_params_free(hw_params);

    return ST_EOF;

}

/*
 *   Underrun and suspend recovery
 */
static int xrun_recovery(snd_pcm_t *handle, int err)
{
    if (err == -EPIPE) 
    {    /* under-run */
        err = snd_pcm_prepare(handle);
        if (err < 0)
            st_warn("Can't recovery from overrun, prepare failed: %s", snd_strerror(err));
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
int st_alsastartread(ft_t ft)
{
    return st_alsasetup(ft, SND_PCM_STREAM_CAPTURE);
}

st_ssize_t st_alsaread(ft_t ft, st_sample_t *buf, st_ssize_t nsamp)
{
    st_ssize_t len;
    int err;
    alsa_priv_t alsa = (alsa_priv_t)ft->priv;
    void (*read_buf)(st_sample_t *, char *, st_ssize_t, char) = 0;

    switch(ft->info.size) {
        case ST_SIZE_BYTE:
            switch(ft->info.encoding)
            {
                case ST_ENCODING_SIGN2:
                    read_buf = st_sb_read_buf;
                    break;
                case ST_ENCODING_UNSIGNED:
                    read_buf = st_ub_read_buf;
                    break;
                default:
                    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size");
                    return ST_EOF;
            }
            break;
        case ST_SIZE_WORD:
            switch(ft->info.encoding)
            {
                case ST_ENCODING_SIGN2:
                    read_buf = st_sw_read_buf;
                    break;
                case ST_ENCODING_UNSIGNED:
                    read_buf = st_uw_read_buf;
                    break;
                default:
                    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size");
                    return ST_EOF;
            }
            break;
        default:
            st_fail_errno(ft,ST_EFMT,"Do not support this data size for this handler");
            return ST_EOF;
    }

    /* Prevent overflow */
    if (nsamp > alsa->buf_size/ft->info.size)
        nsamp = (alsa->buf_size/ft->info.size);
    len = 0;

    while (len < nsamp)
    {
        /* ALSA library takes "frame" counts. */
        err = snd_pcm_readi(alsa->pcm_handle, alsa->buf, 
                            (nsamp-len)/ft->info.channels);
        if (err == -EAGAIN)
            continue;
        if (err < 0)
        {
            if (xrun_recovery(alsa->pcm_handle, err) < 0)
            {
                st_fail_errno(ft, ST_EPERM, "ALSA write error");
                return ST_EOF;
            }
        }
        else
        {
            read_buf(buf+(len*sizeof(st_sample_t)), alsa->buf, err, ft->swap);
            len += err * ft->info.channels;
        }
    }

    return len;
}

int st_alsastopread(ft)
ft_t ft;
{
    alsa_priv_t alsa = (alsa_priv_t)ft->priv;

    snd_pcm_close(alsa->pcm_handle);

    free(alsa->buf);

    return ST_SUCCESS;
}

int st_alsastartwrite(ft_t ft)
{
    return st_alsasetup(ft, SND_PCM_STREAM_PLAYBACK);
}

st_ssize_t st_alsawrite(ft_t ft, st_sample_t *buf, st_ssize_t nsamp)
{
    st_ssize_t len;
    int err;
    alsa_priv_t alsa = (alsa_priv_t)ft->priv;
    void (*write_buf)(char *, st_sample_t *, st_ssize_t, char) = 0;

    switch(ft->info.size) {
        case ST_SIZE_BYTE:
            switch(ft->info.encoding)
            {
                case ST_ENCODING_SIGN2:
                    write_buf = st_sb_write_buf;
                    break;
                case ST_ENCODING_UNSIGNED:
                    write_buf = st_ub_write_buf;
                    break;
                default:
                    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size");
                    return ST_EOF;
            }
            break;
        case ST_SIZE_WORD:
            switch(ft->info.encoding)
            {
                case ST_ENCODING_SIGN2:
                    write_buf = st_sw_write_buf;
                    break;
                case ST_ENCODING_UNSIGNED:
                    write_buf = st_uw_write_buf;
                    break;
                default:
                    st_fail_errno(ft,ST_EFMT,"Do not support this encoding for this data size");
                    return ST_EOF;
            }
            break;
        default:
            st_fail_errno(ft,ST_EFMT,"Do not support this data size for this handler");
            return ST_EOF;
    }

    /* Prevent overflow */
    if (nsamp > alsa->buf_size/ft->info.size)
        nsamp = (alsa->buf_size/ft->info.size);
    len = 0;

    write_buf(alsa->buf, buf, nsamp, ft->swap);

    while (len < nsamp)
    {
        err = snd_pcm_writei(alsa->pcm_handle, 
                             alsa->buf+(len*ft->info.size), 
                             (nsamp-len)/ft->info.channels);
        if (err == -EAGAIN)
            continue;
        if (err < 0)
        {
            if (xrun_recovery(alsa->pcm_handle, err) < 0)
            {
                st_fail_errno(ft, ST_EPERM, "ALSA write error\n");
                return ST_EOF;
            }
        }
        else
            len += err * ft->info.channels;
    }

    return len;
}


int st_alsastopwrite(ft)
ft_t ft;
{
    alsa_priv_t alsa = (alsa_priv_t)ft->priv;

    snd_pcm_drain(alsa->pcm_handle);
    snd_pcm_close(alsa->pcm_handle);

    free(alsa->buf);

    return ST_SUCCESS;
}

#define EMSGFMT "ALSA driver does not support %s %s output"

static int get_format(ft_t ft, snd_pcm_format_mask_t *fmask, int *fmt)
{
    if (ft->info.size == -1)
        ft->info.size = ST_SIZE_WORD;

    if (ft->info.encoding == -1)
    {
        if (ft->info.size == ST_SIZE_WORD)
            ft->info.encoding = ST_ENCODING_SIGN2;
        else
            ft->info.encoding = ST_ENCODING_UNSIGNED;
    }

    if (ft->info.size != ST_SIZE_WORD &&
        ft->info.size != ST_SIZE_BYTE)
    {
        st_report("ALSA driver only supports byte and word samples.  Changing to word.");
        ft->info.size = ST_SIZE_WORD;
    }

    if (ft->info.encoding != ST_ENCODING_SIGN2 &&
        ft->info.encoding != ST_ENCODING_UNSIGNED)
    {
        if (ft->info.size == ST_SIZE_WORD)
        {
            st_report("ALSA driver only supports signed and unsigned samples.  Changing to signed.");
            ft->info.encoding = ST_ENCODING_SIGN2;
        }
        else
        {
            st_report("ALSA driver only supports signed and unsigned samples.  Changing to unsigned.");
            ft->info.encoding = ST_ENCODING_UNSIGNED;
        }
    }

    /* Some hardware only wants to work with 8-bit or 16-bit data */
    if (ft->info.size == ST_SIZE_BYTE)
    {
        if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U8)) && 
            !(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S8)))
        {
            st_report("ALSA driver doesn't supported byte samples.  Changing to words.");
            ft->info.size = ST_SIZE_WORD;
        }
    }
    else if (ft->info.size == ST_SIZE_WORD)
    {
        if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U16)) && 
            !(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S16)))
        {
            st_report("ALSA driver doesn't supported word samples.  Changing to bytes.");
            ft->info.size = ST_SIZE_BYTE;
        }
    }
    else
    {
        if ((snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U16)) ||
            (snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S16)))
        {
            st_report("ALSA driver doesn't supported %s samples.  Changing to words.", st_sizes_str[(unsigned char)ft->info.size]);
            ft->info.size = ST_SIZE_WORD;
        }
        else
        {
            st_report("ALSA driver doesn't supported %s samples.  Changing to bytes.", st_sizes_str[(unsigned char)ft->info.size]);
            ft->info.size = ST_SIZE_BYTE;
        }
    }

    if (ft->info.size == ST_SIZE_BYTE) {
        switch (ft->info.encoding)
        {
            case ST_ENCODING_SIGN2:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S8)))
                {
                    st_report("ALSA driver doesn't supported signed byte samples.  Changing to unsigned bytes.");
                    ft->info.encoding = ST_ENCODING_UNSIGNED;
                }
                break;
            case ST_ENCODING_UNSIGNED:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U8)))
                {
                    st_report("ALSA driver doesn't supported unsigned byte samples.  Changing to signed bytes.");
                    ft->info.encoding = ST_ENCODING_SIGN2;
                }
                break;
        }
        switch (ft->info.encoding)
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
        }
    }
    else if (ft->info.size == ST_SIZE_WORD) {
        switch (ft->info.encoding)
        {
            case ST_ENCODING_SIGN2:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_S16)))
                {
                    st_report("ALSA driver does not support signed word samples.  Changing to unsigned words.");
                    ft->info.encoding = ST_ENCODING_UNSIGNED;
                }
                break;
            case ST_ENCODING_UNSIGNED:
                if (!(snd_pcm_format_mask_test(fmask, SND_PCM_FORMAT_U16)))
                {
                    st_report("ALSA driver does not support unsigned word samples.  Changing to signed words.");
                    ft->info.encoding = ST_ENCODING_SIGN2;
                }
                break;
        }
        switch (ft->info.encoding)
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
        }
    }
    else {
        st_fail_errno(ft,ST_EFMT,EMSGFMT,st_encodings_str[(unsigned char)ft->info.encoding], st_sizes_str[(unsigned char)ft->info.size]);
        return ST_EOF;
    }
    return 0;
}

#endif /* HAVE_ALSA */
