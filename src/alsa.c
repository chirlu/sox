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
    snd_pcm_hw_params_t *hw_params;
    unsigned int min_rate, max_rate;
    unsigned int min_chan, max_chan;
    unsigned int rate;
    int dir;
    snd_pcm_format_mask_t *fmask;

    /* Reserve buffer for 16-bit data.  FIXME: Whats a good size? */
    alsa->buf_size = ST_BUFSIZ*2;

    if ((alsa->buf = malloc(alsa->buf_size)) == NULL) 
    {
        st_fail_errno(ft,ST_ENOMEM,
                      "unable to allocate output buffer of size %d", 
                      ft->file.size);
        return(ST_EOF);
    }

    if ((err = snd_pcm_open(&(alsa->pcm_handle), ft->filename, 
                            mode, 0)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot open audio device\n");
        return ST_EOF;
    }

    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) 
    {
        st_fail_errno(ft, ST_ENOMEM, 
                      "cannot allocate hardware parameter structure\n");
        return ST_EOF;
    }

    if ((err = snd_pcm_hw_params_any(alsa->pcm_handle, hw_params)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM,
                      "cannot initialize hardware parameter structure\n");
        return ST_EOF;
    }

    if ((err = snd_pcm_hw_params_set_access(alsa->pcm_handle, hw_params, 
                                            SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    {
        st_fail_errno(ft, ST_EPERM,
                      "cannot set access type\n");
        return ST_EOF;
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
        return (ST_EOF);

    snd_pcm_format_mask_free(fmask);

    if ((err = snd_pcm_hw_params_set_format(alsa->pcm_handle, 
                                            hw_params, fmt)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot set sample format\n");
        return ST_EOF;
    }

    snd_pcm_hw_params_get_rate_min(hw_params, &min_rate, &dir);
    snd_pcm_hw_params_get_rate_max(hw_params, &max_rate, &dir);

    rate = ft->info.rate;
    if (rate < min_rate) 
        rate = min_rate;
    else 
        if (rate > max_rate) 
            rate = max_rate;
    if (rate != ft->info.rate)
    {
        st_warn("alsa: Hardware does not support %d.  Forcing sample rate to %d.\n", ft->info.rate, rate);
    }

    dir = 0;
    if ((err = snd_pcm_hw_params_set_rate_near(alsa->pcm_handle, 
                                               hw_params, 
                                               &rate,
                                               &dir)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot set sample rate\n");
        return ST_EOF;
    }
    if (rate != ft->info.rate)
    {
        st_warn("Could not set exact rate of %d.  Approximating with %d\n",
                ft->info.rate, rate);
    }

    snd_pcm_hw_params_get_rate(hw_params, &rate, &dir);

    if ((err = snd_pcm_hw_params_set_channels(alsa->pcm_handle,
                                              hw_params, 
                                              ft->info.channels)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot set channel count\n");
        return ST_EOF;
    }

#if 0
    /* Set number of periods. Periods used to be called fragments. */ 
    if (snd_pcm_hw_params_set_periods(alsa->pcm_handle, hw_params, 2, 0) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "Error setting periods.\n");
        return ST_EOF;
    }

    /* Set buffer size (in frames). The resulting latency is given by */
    /* latency = periodsize * periods / (rate * bytes_per_frame)     */
    if (snd_pcm_hw_params_set_buffer_size(alsa->pcm_handle, hw_params, (ST_BUFSIZ * 8)>>2) < 0) {
      st_fail_errno(ft, ST_EPERM, "Error setting buffersize.\n");
      return ST_EOF;
    }
#endif

    if ((err = snd_pcm_hw_params(alsa->pcm_handle, hw_params)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot set parameters\n");
        return ST_EOF;
    }

    snd_pcm_hw_params_free(hw_params);

    if ((err = snd_pcm_prepare(alsa->pcm_handle)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot prepare audio interface for use\n");
        return ST_EOF;
    }

    sigintreg(ft);      /* Prepare to catch SIGINT */

    return (ST_SUCCESS);
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
    alsa_priv_t alsa = (alsa_priv_t)ft->priv;
    void (*read_buf)(st_sample_t *, char *, st_ssize_t, char) = 0;

    /* Check to see if user sent SIGINT and if so return ST_EOF to
     * stop playing.
     */
    if (ft->file.eof)
        return ST_EOF;


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

    len = snd_pcm_readi(alsa->pcm_handle, alsa->buf, nsamp);

    read_buf(buf, alsa->buf, len, ft->swap);

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

    int fmt = SND_PCM_FORMAT_S16;
    int err;
    alsa_priv_t alsa = (alsa_priv_t)ft->priv;
    snd_pcm_hw_params_t *hw_params;
    unsigned int min_rate, max_rate;
    unsigned int min_chan, max_chan;
    unsigned int rate;
    int dir;
    snd_pcm_format_mask_t *fmask;

    /* Reserve buffer for 16-bit data.  FIXME: Whats a good size? */
    alsa->buf_size = ST_BUFSIZ*2;

    if ((alsa->buf = malloc(alsa->buf_size)) == NULL) 
    {
        st_fail_errno(ft,ST_ENOMEM,
                      "unable to allocate output buffer of size %d", 
                      ft->file.size);
        return(ST_EOF);
    }

    if ((err = snd_pcm_open(&(alsa->pcm_handle), ft->filename, 
                            SND_PCM_STREAM_PLAYBACK, 0)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot open audio device\n");
        return ST_EOF;
    }

    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) 
    {
        st_fail_errno(ft, ST_ENOMEM, 
                      "cannot allocate hardware parameter structure\n");
        return ST_EOF;
    }

    if ((err = snd_pcm_hw_params_any(alsa->pcm_handle, hw_params)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM,
                      "cannot initialize hardware parameter structure\n");
        return ST_EOF;
    }

    if ((err = snd_pcm_hw_params_set_access(alsa->pcm_handle, hw_params, 
                                            SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    {
        st_fail_errno(ft, ST_EPERM,
                      "cannot set access type\n");
        return ST_EOF;
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
        return (ST_EOF);

    snd_pcm_format_mask_free(fmask);

    if ((err = snd_pcm_hw_params_set_format(alsa->pcm_handle, 
                                            hw_params, fmt)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot set sample format\n");
        return ST_EOF;
    }

    snd_pcm_hw_params_get_rate_min(hw_params, &min_rate, &dir);
    snd_pcm_hw_params_get_rate_max(hw_params, &max_rate, &dir);

    rate = ft->info.rate;
    if (rate < min_rate) 
        rate = min_rate;
    else 
        if (rate > max_rate) 
            rate = max_rate;
    if (rate != ft->info.rate)
    {
        st_warn("alsa: Hardware does not support %d.  Forcing sample rate to %d.\n", ft->info.rate, rate);
    }

    dir = 0;
    if ((err = snd_pcm_hw_params_set_rate_near(alsa->pcm_handle, 
                                               hw_params, 
                                               &rate,
                                               &dir)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot set sample rate\n");
        return ST_EOF;
    }
    if (rate != ft->info.rate)
    {
        st_warn("Could not set exact rate of %d.  Approximating with %d\n",
                ft->info.rate, rate);
    }

    snd_pcm_hw_params_get_rate(hw_params, &rate, &dir);

    if ((err = snd_pcm_hw_params_set_channels(alsa->pcm_handle,
                                              hw_params, 
                                              ft->info.channels)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot set channel count\n");
        return ST_EOF;
    }

#if 0
    /* Set number of periods. Periods used to be called fragments. */ 
    if (snd_pcm_hw_params_set_periods(alsa->pcm_handle, hw_params, 2, 0) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "Error setting periods.\n");
        return ST_EOF;
    }

    /* Set buffer size (in frames). The resulting latency is given by */
    /* latency = periodsize * periods / (rate * bytes_per_frame)     */
    if (snd_pcm_hw_params_set_buffer_size(alsa->pcm_handle, hw_params, (ST_BUFSIZ * 8)>>2) < 0) {
      st_fail_errno(ft, ST_EPERM, "Error setting buffersize.\n");
      return ST_EOF;
    }
#endif

    if ((err = snd_pcm_hw_params(alsa->pcm_handle, hw_params)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot set parameters\n");
        return ST_EOF;
    }

    snd_pcm_hw_params_free(hw_params);

    if ((err = snd_pcm_prepare(alsa->pcm_handle)) < 0) 
    {
        st_fail_errno(ft, ST_EPERM, "cannot prepare audio interface for use\n");
        return ST_EOF;
    }

    sigintreg(ft);      /* Prepare to catch SIGINT */

    return (ST_SUCCESS);
}

st_ssize_t st_alsawrite(ft_t ft, st_sample_t *buf, st_ssize_t nsamp)
{
    st_ssize_t len;
    alsa_priv_t alsa = (alsa_priv_t)ft->priv;
    void (*write_buf)(char *, st_sample_t *, st_ssize_t, char) = 0;

    /* Check to see if user sent SIGINT and if so return ST_EOF to
     * stop recording
     */
    if (ft->file.eof)
        return ST_EOF;

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

    write_buf(alsa->buf, buf, nsamp, ft->swap);

    if ((len = snd_pcm_writei(alsa->pcm_handle, alsa->buf, nsamp)) != nsamp) {
        snd_pcm_prepare(alsa->pcm_handle);
    }

    return len;
}


int st_alsastopwrite(ft)
ft_t ft;
{
    alsa_priv_t alsa = (alsa_priv_t)ft->priv;

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
        st_warn("ALSA driver only supports byte and word samples.  Changing to word.\n");
        ft->info.size = ST_SIZE_WORD;
    }

    if (ft->info.encoding != ST_ENCODING_SIGN2 &&
        ft->info.encoding != ST_ENCODING_UNSIGNED)
    {
        if (ft->info.size == ST_SIZE_WORD)
        {
            st_warn("ALSA driver only supports signed and unsigned samples.  Changing to signed.\n");
            ft->info.encoding = ST_ENCODING_SIGN2;
        }
        else
        {
            st_warn("ALSA driver only supports signed and unsigned samples.  Changing to unsigned.\n");
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
