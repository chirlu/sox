/*
 * Copyright 1997 Jimen Ching And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Jimen Ching And Sundry Contributors are not
 * responsible for the consequences of using this software.
 */

/* Direct to ALSA sound driver
 *
 * added by Jimen Ching (jching@flex.com) 19990207
 * based on info grabed from aplay.c in alsa-utils package.
 * Updated for ALSA 0.9.X API 20020824.
 */

#include "st_i.h"

#if defined(ALSA_PLAYER)

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

static int get_format(ft_t ft, int formats, int *fmt);

#if USE_ALSA9

#include <limits.h>
#include <sound/asound.h>

/* Backwards compatibility. */
#define SND_PCM_SFMT_S8     SNDRV_PCM_FORMAT_S8
#define SND_PCM_SFMT_U8     SNDRV_PCM_FORMAT_U8
#define SND_PCM_SFMT_S16_LE SNDRV_PCM_FORMAT_S16
#define SND_PCM_SFMT_U16_LE SNDRV_PCM_FORMAT_U16

#define SND_PCM_FMT_S8      (1 << SNDRV_PCM_FORMAT_S8)
#define SND_PCM_FMT_U8      (1 << SNDRV_PCM_FORMAT_U8)
#define SND_PCM_FMT_S16     (1 << SNDRV_PCM_FORMAT_S16)
#define SND_PCM_FMT_U16     (1 << SNDRV_PCM_FORMAT_U16)

#define alsa_params_masks(p, i) \
    (&((p)->masks[(i)-SNDRV_PCM_HW_PARAM_FIRST_MASK]))
#define alsa_params_intervals(p, i) \
    (&((p)->intervals[(i)-SNDRV_PCM_HW_PARAM_FIRST_INTERVAL]))

struct alsa_info
{
    unsigned int formats;
    unsigned int min_buffer_size;
    unsigned int max_buffer_size;
    unsigned int min_channels;
    unsigned int max_channels;
    unsigned int min_rate;
    unsigned int max_rate;
    unsigned int min_periods;
    unsigned int max_periods;
    unsigned int min_period_size;
    unsigned int max_period_size;
};

struct alsa_setup
{
    int format;
    char channels;
    st_rate_t rate;
    size_t buffer_size;
    int periods;
    size_t period_size;
};

int
alsa_hw_info_get(fd, a_info, params)
int fd;
struct alsa_info *a_info;
struct sndrv_pcm_hw_params *params;
{
    unsigned int i;
    unsigned int *msk;
    struct sndrv_interval *intr;

    memset(params, '\0', sizeof(struct sndrv_pcm_hw_params));
    for (i = 0; i <= SNDRV_PCM_HW_PARAM_LAST; ++i)
    {
	if (i >= SNDRV_PCM_HW_PARAM_FIRST_MASK &&
	    i <= SNDRV_PCM_HW_PARAM_LAST_MASK)
	{
	    msk = alsa_params_masks(params, i);
	    *msk = ~0UL;
	}
	else
	{
	    intr = alsa_params_intervals(params, i);
	    intr->min = 0;
	    intr->openmin = 0;
	    intr->max = UINT_MAX;
	    intr->openmax = 0;
	    intr->integer = 0;
	    intr->empty = 0;
	}
	params->cmask |= 1 << i;
	params->rmask |= 1 << i;
    }
    if (ioctl(fd, SNDRV_PCM_IOCTL_HW_REFINE, params) < 0) {
	return -1;
    }
    a_info->formats = *alsa_params_masks(params, SNDRV_PCM_HW_PARAM_FORMAT);
    a_info->min_buffer_size = alsa_params_intervals(params, SNDRV_PCM_HW_PARAM_BUFFER_BYTES)->min;
    a_info->max_buffer_size = alsa_params_intervals(params, SNDRV_PCM_HW_PARAM_BUFFER_BYTES)->max;
    a_info->min_channels = alsa_params_intervals(params, SNDRV_PCM_HW_PARAM_CHANNELS)->min;
    a_info->max_channels = alsa_params_intervals(params, SNDRV_PCM_HW_PARAM_CHANNELS)->max;
    a_info->min_rate = alsa_params_intervals(params, SNDRV_PCM_HW_PARAM_RATE)->min;
    a_info->max_rate = alsa_params_intervals(params, SNDRV_PCM_HW_PARAM_RATE)->max;
    a_info->min_periods = alsa_params_intervals(params, SNDRV_PCM_HW_PARAM_PERIODS)->min;
    a_info->max_periods = alsa_params_intervals(params, SNDRV_PCM_HW_PARAM_PERIODS)->max;
    a_info->min_period_size = alsa_params_intervals(params, SNDRV_PCM_HW_PARAM_PERIOD_BYTES)->min;
    a_info->max_period_size = alsa_params_intervals(params, SNDRV_PCM_HW_PARAM_PERIOD_BYTES)->max;
    return 0;
}

int
alsa_hw_info_set(fd, params, setup)
int fd;
struct sndrv_pcm_hw_params *params;
struct alsa_setup *setup;
{
    int i;
    unsigned int *msk;
    struct sndrv_interval *intr;

    i = SNDRV_PCM_HW_PARAM_ACCESS;
    msk = alsa_params_masks(params, i);
    *msk &= 1 << SNDRV_PCM_ACCESS_RW_INTERLEAVED;
    params->cmask = 1 << i;
    params->rmask = 1 << i;

    i = SNDRV_PCM_HW_PARAM_FORMAT;
    msk = alsa_params_masks(params, i);
    *msk &= 1 << setup->format;
    params->cmask = 1 << i;
    params->rmask = 1 << i;

    i = SNDRV_PCM_HW_PARAM_CHANNELS;
    intr = alsa_params_intervals(params, i);
    intr->empty = 0;
    intr->min = intr->max = setup->channels;
    intr->openmin = intr->openmax = 0;
    intr->integer = 1;
    params->cmask = 1 << i;
    params->rmask = 1 << i;

    i = SNDRV_PCM_HW_PARAM_RATE;
    intr = alsa_params_intervals(params, i);
    intr->empty = 0;
    intr->min = intr->max = setup->rate;
    intr->openmin = intr->openmax = 0;
    intr->integer = 1;
    params->cmask = 1 << i;
    params->rmask = 1 << i;

    i = SNDRV_PCM_HW_PARAM_BUFFER_BYTES;
    intr = alsa_params_intervals(params, i);
    intr->empty = 0;
    intr->min = intr->max = setup->buffer_size;
    intr->openmin = intr->openmax = 0;
    intr->integer = 1;
    params->cmask = 1 << i;
    params->rmask = 1 << i;

    i = SNDRV_PCM_HW_PARAM_PERIODS;
    intr = alsa_params_intervals(params, i);
    intr->empty = 0;
    intr->min = intr->max = setup->periods;
    intr->openmin = intr->openmax = 0;
    intr->integer = 1;
    params->cmask = 1 << i;
    params->rmask = 1 << i;

    i = SNDRV_PCM_HW_PARAM_PERIOD_BYTES;
    intr = alsa_params_intervals(params, i);
    intr->empty = 0;
    intr->min = intr->max = setup->period_size;
    intr->openmin = intr->openmax = 0;
    intr->integer = 1;
    params->cmask = 1 << i;
    params->rmask = 1 << i;

    if (ioctl(fd, SNDRV_PCM_IOCTL_HW_PARAMS, params) < 0) {
	return -1;
    }
    if (ioctl(fd, SNDRV_PCM_IOCTL_PREPARE) < 0) {
	return -1;
    }
    return 0;
}

/*
 * Do anything required before you start reading samples.
 * Read file header.
 *	Find out sampling rate,
 *	size and encoding of samples,
 *	mono/stereo/quad.
 */
int st_alsastartread(ft)
ft_t ft;
{
    int fmt;
    struct alsa_info a_info;
    struct alsa_setup a_setup;
    struct sndrv_pcm_hw_params params;

    if (alsa_hw_info_get(fileno(ft->fp), a_info, &params) < 0) {
	st_fail_errno(ft,ST_EPERM,"ioctl operation failed %d",errno);
	return(ST_EOF);
    }
    ft->file.count = 0;
    ft->file.pos = 0;
    ft->file.eof = 0;
    ft->file.size = a_info.max_buffer_size;
    if ((ft->file.buf = malloc (ft->file.size)) == NULL) {
	st_fail_errno(ft,ST_ENOMEM,"unable to allocate output buffer of size %d", ft->file.size);
	return(ST_EOF);
    }
    if (ft->info.rate < a_info.min_rate) ft->info.rate = 2 * a_info.min_rate;
    else if (ft->info.rate > a_info.max_rate) ft->info.rate = a_info.max_rate;
    if (ft->info.channels == -1) ft->info.channels = a_info.min_channels;
    else if (ft->info.channels > a_info.max_channels) ft->info.channels = a_info.max_channels;
    if (get_format(ft, a_info.formats, &fmt) < 0)
        return (ST_EOF);

    a_setup.format = fmt;
    a_setup.channels = ft->info.channels;
    a_setup.rate = ft->info.rate;
    a_setup.buffer_size = ft->file.size;
    a_setup.periods = 16;
    if (a_setup.periods < a_info.min_periods) a_setup.periods = a_info.min_periods;
    else if (a_setup.periods > a_info.max_periods) a_setup.periods = a_info.max_periods;
    a_setup.period_size = a_setup.buffer_size / a_setup.periods;
    if (a_setup.period_size < a_info.min_period_size) a_setup.period_size = a_info.min_period_size;
    else if (a_setup.period_size > a_info.max_period_size) a_setup.period_size = a_info.max_period_size;
    if (alsa_hw_info_set(fileno(ft->fp), &params, &a_setup) < 0) {
	st_fail_errno(ft,ST_EPERM,"ioctl operation failed %d",errno);
	return(ST_EOF);
    }

    /* Change to non-buffered I/O */
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * ft->file.size);

    sigintreg(ft);	/* Prepare to catch SIGINT */

    return (ST_SUCCESS);
}

int st_alsastartwrite(ft)
ft_t ft;
{
    int fmt;
    struct alsa_info a_info;
    struct alsa_setup a_setup;
    struct sndrv_pcm_hw_params params;

    if (alsa_hw_info_get(fileno(ft->fp), &a_info, &params) < 0) {
	st_fail_errno(ft,ST_EPERM,"ioctl operation failed %d",errno);
	return(ST_EOF);
    }
    ft->file.pos = 0;
    ft->file.eof = 0;
    ft->file.size = a_info.max_buffer_size;
    if ((ft->file.buf = malloc (ft->file.size)) == NULL) {
	st_fail_errno(ft,ST_ENOMEM,"unable to allocate output buffer of size %d", ft->file.size);
	return(ST_EOF);
    }
    if (ft->info.rate < a_info.min_rate) ft->info.rate = 2 * a_info.min_rate;
    else if (ft->info.rate > a_info.max_rate) ft->info.rate = a_info.max_rate;
    if (ft->info.channels == -1) ft->info.channels = a_info.min_channels;
    else if (ft->info.channels > a_info.max_channels) ft->info.channels = a_info.max_channels;
    if (get_format(ft, a_info.formats, &fmt) < 0)
        return (ST_EOF);

    a_setup.format = fmt;
    a_setup.channels = ft->info.channels;
    a_setup.rate = ft->info.rate;
    a_setup.buffer_size = ft->file.size;
    a_setup.periods = 16;
    if (a_setup.periods < a_info.min_periods) a_setup.periods = a_info.min_periods;
    else if (a_setup.periods > a_info.max_periods) a_setup.periods = a_info.max_periods;
    a_setup.period_size = a_setup.buffer_size / a_setup.periods;
    if (a_setup.period_size < a_info.min_period_size) a_setup.period_size = a_info.min_period_size;
    else if (a_setup.period_size > a_info.max_period_size) a_setup.period_size = a_info.max_period_size;
    if (alsa_hw_info_set(fileno(ft->fp), &params, &a_setup) < 0) {
	st_fail_errno(ft,ST_EPERM,"ioctl operation failed %d",errno);
	return(ST_EOF);
    }

    /* Change to non-buffered I/O */
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * ft->file.size);

    return(ST_SUCCESS);
}

int st_alsastopwrite(ft)
ft_t ft;
{
    ioctl(fileno(ft->fp), SNDRV_PCM_IOCTL_DRAIN);
    return(st_rawstopwrite(ft));
}

#else /* ! USE_ALSA9 */

#include <linux/asound.h>

#if USE_ALSA4 /* Start 0.4.x API */

int st_alsastartread(ft)
ft_t ft;
{
    int bps, fmt, size;
    snd_pcm_capture_info_t c_info;
    snd_pcm_format_t format;
    snd_pcm_capture_params_t c_params;

    memset(&c_info, 0, sizeof(c_info));
    if (ioctl(fileno(ft->fp), SND_PCM_IOCTL_CAPTURE_INFO, &c_info) < 0) {
	st_fail_errno(ft,ST_EPERM,"ioctl operation failed %d",errno);
	return(ST_EOF);
    }
    ft->file.count = 0;
    ft->file.pos = 0;
    ft->file.eof = 0;
    ft->file.size = c_info.buffer_size;
    if ((ft->file.buf = malloc (ft->file.size)) == NULL) {
	st_fail_errno(ft,ST_ENOMEM,"unable to allocate output buffer of size %d", ft->file.size);
	return(ST_EOF);
    }
    if (ft->info.rate < c_info.min_rate) ft->info.rate = 2 * c_info.min_rate;
    else if (ft->info.rate > c_info.max_rate) ft->info.rate = c_info.max_rate;
    if (ft->info.channels == -1) ft->info.channels = c_info.min_channels;
    else if (ft->info.channels > c_info.max_channels) ft->info.channels = c_info.max_channels;
    if (get_format(ft, c_info.hw_formats, &fmt) < 0)
	return(ST_EOF);

    memset(&format, 0, sizeof(format));
    format.format = fmt;
    format.rate = ft->info.rate;
    format.channels = ft->info.channels;
    if (ioctl(fileno(ft->fp), SND_PCM_IOCTL_CAPTURE_FORMAT, &format) < 0) {
	st_fail_errno(ft,ST_EPERM,"ioctl operation failed %d",errno);
	return(ST_EOF);
    }

    size = ft->file.size;
    bps = format.rate * format.channels;
    if (ft->info.size == ST_SIZE_WORD) bps <<= 1;
    bps >>= 2;
    while (size > bps) size >>= 1;
    if (size < 16) size = 16;
    memset(&c_params, 0, sizeof(c_params));
    c_params.fragment_size = size;
    c_params.fragments_min = 1;
    if (ioctl(fileno(ft->fp), SND_PCM_IOCTL_CAPTURE_PARAMS, &c_params) < 0) {
	st_fail_errno(ft,ST_EPERM,"ioctl operation failed %d",errno);
	return(ST_EOF);
    }

    /* Change to non-buffered I/O */
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * ft->file.size);

    sigintreg(ft);	/* Prepare to catch SIGINT */

    return (ST_SUCCESS);
}

int st_alsastartwrite(ft)
ft_t ft;
{
    int bps, fmt, size;
    snd_pcm_playback_info_t p_info;
    snd_pcm_format_t format;
    snd_pcm_playback_params_t p_params;

    memset(&p_info, 0, sizeof(p_info));
    if (ioctl(fileno(ft->fp), SND_PCM_IOCTL_PLAYBACK_INFO, &p_info) < 0) {
	st_fail_errno(ft,ST_EPERM,"ioctl operation failed %d",errno);
	return(ST_EOF);
    }
    ft->file.pos = 0;
    ft->file.eof = 0;
    ft->file.size = p_info.buffer_size;
    if ((ft->file.buf = malloc (ft->file.size)) == NULL) {
	st_fail_errno(ft,ST_ENOMEM,"unable to allocate output buffer of size %d", ft->file.size);
	return(ST_EOF);
    }
    if (ft->info.rate < p_info.min_rate) ft->info.rate = 2 * p_info.min_rate;
    else if (ft->info.rate > p_info.max_rate) ft->info.rate = p_info.max_rate;
    if (ft->info.channels == -1) ft->info.channels = p_info.min_channels;
    else if (ft->info.channels > p_info.max_channels) ft->info.channels = p_info.max_channels;
    if (get_format(ft, p_info.hw_formats, &fmt) < 0)
	return(ST_EOF);

    memset(&format, 0, sizeof(format));
    format.format = fmt;
    format.rate = ft->info.rate;
    format.channels = ft->info.channels;
    if (ioctl(fileno(ft->fp), SND_PCM_IOCTL_PLAYBACK_FORMAT, &format) < 0) {
	st_fail_errno(ft,ST_EPERM,"ioctl operation failed %d",errno);
	return(ST_EOF);
    }

    size = ft->file.size;
    bps = format.rate * format.channels;
    if (ft->info.size == ST_SIZE_WORD) bps <<= 1;
    bps >>= 2;
    while (size > bps) size >>= 1;
    if (size < 16) size = 16;
    memset(&p_params, 0, sizeof(p_params));
    p_params.fragment_size = size;
    p_params.fragments_max = -1;
    p_params.fragments_room = 1;
    if (ioctl(fileno(ft->fp), SND_PCM_IOCTL_PLAYBACK_PARAMS, &p_params) < 0) {
	st_fail_errno(ft,ST_EPERM,"ioctl operation failed %d",errno);
	return(ST_EOF);
    }

    /* Change to non-buffered I/O */
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * ft->file.size);

    return(ST_SUCCESS);
}

int st_alsastopwrite(ft)
ft_t ft;
{
    /* Is there a drain operation for ALSA 0.4.X? */
    return(st_rawstopwrite(ft));
}

#elif USE_ALSA5 /* Start 0.5.x API */

int st_alsastartread(ft)
ft_t ft;
{
    int bps, fmt, size;
    snd_pcm_channel_info_t c_info;
    snd_pcm_channel_params_t c_params;

    memset(&c_info, 0, sizeof(c_info));
    if (ioctl(fileno(ft->fp), SND_PCM_IOCTL_CHANNEL_INFO, &c_info) < 0) {
	st_fail_errno(ft,ST_EPERM,"ioctl operation failed %d",errno);
	return(ST_EOF);
    }
    ft->file.count = 0;
    ft->file.pos = 0;
    ft->file.eof = 0;
    ft->file.size = c_info.buffer_size;
    if ((ft->file.buf = malloc (ft->file.size)) == NULL) {
	st_fail_errno(ft,ST_ENOMEM,"unable to allocate output buffer of size %d", ft->file.size);
	return(ST_EOF);
    }
    if (ft->info.rate < c_info.min_rate) ft->info.rate = 2 * c_info.min_rate;
    else if (ft->info.rate > c_info.max_rate) ft->info.rate = c_info.max_rate;
    if (ft->info.channels == -1) ft->info.channels = c_info.min_voices;
    else if (ft->info.channels > c_info.max_voices) ft->info.channels = c_info.max_voices;
    if (get_format(ft, c_info.formats, &fmt) < 0)
	return (ST_EOF);

    memset(&c_params, 0, sizeof(c_params));
    c_params.format.format = fmt;
    c_params.format.rate = ft->info.rate;
    c_params.format.voices = ft->info.channels;
    c_params.format.interleave = 1;
    c_params.channel = SND_PCM_CHANNEL_CAPTURE;
    c_params.mode = SND_PCM_MODE_BLOCK;
    c_params.start_mode = SND_PCM_START_DATA;
    c_params.stop_mode = SND_PCM_STOP_STOP;
    bps = c_params.format.rate * c_params.format.voices;
    if (ft->info.size == ST_SIZE_WORD) bps <<= 1;
    bps >>= 2;
    size = 1;
    while ((size << 1) < bps) size <<= 1;
    if (size > ft->file.size) size = ft->file.size;
    if (size < c_info.min_fragment_size) size = c_info.min_fragment_size;
    else if (size > c_info.max_fragment_size) size = c_info.max_fragment_size;
    c_params.buf.block.frag_size = size;
    c_params.buf.block.frags_max = 32;
    c_params.buf.block.frags_min = 1;
    if (ioctl(fileno(ft->fp), SND_PCM_IOCTL_CHANNEL_PARAMS, &c_params) < 0) {
	st_fail_errno(ft,ST_EPERM,"ioctl operation failed %d",errno);
	return(ST_EOF);
    }
    if (ioctl(fileno(ft->fp), SND_PCM_IOCTL_CHANNEL_PREPARE) < 0) {
	st_fail_errno(ft,ST_EPERM,"ioctl operation failed %d",errno);
	return(ST_EOF);
    }

    /* Change to non-buffered I/O */
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * ft->file.size);

    sigintreg(ft);	/* Prepare to catch SIGINT */

    return (ST_SUCCESS);
}

int st_alsastartwrite(ft)
ft_t ft;
{
    int bps, fmt, size;
    snd_pcm_channel_info_t p_info;
    snd_pcm_channel_params_t p_params;

    memset(&p_info, 0, sizeof(p_info));
    if (ioctl(fileno(ft->fp), SND_PCM_IOCTL_CHANNEL_INFO, &p_info) < 0)	{
	st_fail_errno(ft,ST_EPERM,"ioctl operation failed %d",errno);
	return(ST_EOF);
    }
    ft->file.pos = 0;
    ft->file.eof = 0;
    ft->file.size = p_info.buffer_size;
    if ((ft->file.buf = malloc (ft->file.size)) == NULL) {
	st_fail_errno(ft,ST_ENOMEM,"unable to allocate output buffer of size %d", ft->file.size);
	return(ST_EOF);
    }
    if (ft->info.rate < p_info.min_rate) ft->info.rate = 2 * p_info.min_rate;
    else if (ft->info.rate > p_info.max_rate) ft->info.rate = p_info.max_rate;
    if (ft->info.channels == -1) ft->info.channels = p_info.min_voices;
    else if (ft->info.channels > p_info.max_voices) ft->info.channels = p_info.max_voices;
    if (get_format(ft, p_info.formats, &fmt) < 0)
	return(ST_EOF);

    memset(&p_params, 0, sizeof(p_params));
    p_params.format.format = fmt;
    p_params.format.rate = ft->info.rate;
    p_params.format.voices = ft->info.channels;
    p_params.format.interleave = 1;
    p_params.channel = SND_PCM_CHANNEL_PLAYBACK;
    p_params.mode = SND_PCM_MODE_BLOCK;
    p_params.start_mode = SND_PCM_START_DATA;
    p_params.stop_mode = SND_PCM_STOP_STOP;
    bps = p_params.format.rate * p_params.format.voices;
    if (ft->info.size == ST_SIZE_WORD) bps <<= 1;
    bps >>= 2;
    size = 1;
    while ((size << 1) < bps) size <<= 1;
    if (size > ft->file.size) size = ft->file.size;
    if (size < p_info.min_fragment_size) size = p_info.min_fragment_size;
    else if (size > p_info.max_fragment_size) size = p_info.max_fragment_size;
    p_params.buf.block.frag_size = size;
    p_params.buf.block.frags_max = -1; /* Little trick (playback only) */
    p_params.buf.block.frags_min = 1;
    if (ioctl(fileno(ft->fp), SND_PCM_IOCTL_CHANNEL_PARAMS, &p_params) < 0) {
	st_fail_errno(ft,ST_EPERM,"ioctl operation failed %d",errno);
	return(ST_EOF);
    }
    if (ioctl(fileno(ft->fp), SND_PCM_IOCTL_CHANNEL_PREPARE) < 0) {
	st_fail_errno(ft,ST_EPERM,"ioctl operation failed %d",errno);
	return(ST_EOF);
    }

    /* Change to non-buffered I/O */
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * ft->file.size);

    return(ST_SUCCESS);
}

int st_alsastopwrite(ft)
ft_t ft;
{
    ioctl(fileno(ft->fp), SND_PCM_IOCTL_CHANNEL_DRAIN);
    return(st_rawstopwrite(ft));
}

#endif /* USE_ALSA4/5 */

#endif /* USE_ALSA9 */

#define EMSGFMT "ALSA driver does not support %s %s output"

static int get_format(ft, formats, fmt)
ft_t ft;
int formats, *fmt;
{
    if (ft->info.size == -1) {
	if ((formats & SND_PCM_FMT_U8) || (formats & SND_PCM_FMT_S8))
	    ft->info.size = ST_SIZE_BYTE;
	else
	    ft->info.size = ST_SIZE_WORD;
    }
    if (ft->info.encoding == -1) {
	if ((formats & SND_PCM_FMT_S16) || (formats & SND_PCM_FMT_S8))
	    ft->info.encoding = ST_ENCODING_SIGN2;
	else
	    ft->info.encoding = ST_ENCODING_UNSIGNED;
    }
    if (ft->info.size == ST_SIZE_BYTE) {
	switch (ft->info.encoding)
	{
	    case ST_ENCODING_SIGN2:
		if (!(formats & SND_PCM_FMT_S8)) {
		    st_fail_errno(ft,ST_EFMT,"ALSA driver does not support signed byte samples");
		    return ST_EOF;
		}
		*fmt = SND_PCM_SFMT_S8;
		break;
	    case ST_ENCODING_UNSIGNED:
		if (!(formats & SND_PCM_FMT_U8)) {
		    st_fail_errno(ft,ST_EFMT,"ALSA driver does not support unsigned byte samples");
		    return ST_EOF;
		}
		*fmt = SND_PCM_SFMT_U8;
		break;
	    default:
		st_fail_errno(ft,ST_EFMT,EMSGFMT,st_encodings_str[(unsigned char)ft->info.encoding],"byte");
		return ST_EOF;
		break;
	}
    }
    else if (ft->info.size == ST_SIZE_WORD) {
	switch (ft->info.encoding)
	{
	    case ST_ENCODING_SIGN2:
		if (!(formats & SND_PCM_FMT_S16)) {
		    st_fail_errno(ft,ST_EFMT,"ALSA driver does not support signed word samples");
		    return ST_EOF;
		}
		*fmt = SND_PCM_SFMT_S16_LE;
		break;
	    case ST_ENCODING_UNSIGNED:
		if (!(formats & SND_PCM_FMT_U16)) {
		    st_fail_errno(ft,ST_EFMT,"ALSA driver does not support unsigned word samples");
		    return ST_EOF;
		}
		*fmt = SND_PCM_SFMT_U16_LE;
		break;
	    default:
		st_fail_errno(ft,ST_EFMT,EMSGFMT,st_encodings_str[(unsigned char)ft->info.encoding],"word");
		return ST_EOF;
		break;
	}
    }
    else {
	st_fail_errno(ft,ST_EFMT,EMSGFMT,st_encodings_str[(unsigned char)ft->info.encoding],st_sizes_str[(unsigned char)ft->info.size]);
	return ST_EOF;
    }
    return 0;
}

st_ssize_t st_alsaread(ft_t ft, st_sample_t *buf, st_ssize_t nsamp)
{
    st_ssize_t len;

    len = st_rawread(ft, buf, nsamp);

#if USE_ALSA9
    /* ALSA 0.9 and above require that we detects underruns and
     * reset the driver if it occurs.
     */
    if (len != nsamp)
    {
	/* Reset the driver.  A future enhancement would be to
	 * fill up the empty spots in the buffer (starting at
	 * nsamp - len).  But I'm being lazy (cbagwell) and just
	 * returning with a partial buffer.
	 */
	ioctl(fileno(ft->fp), SNDRV_PCM_IOCTL_PREPARE);

	/* Raw routines use eof flag to store when we've
	 * hit EOF or if an internal error occurs.  The
	 * above ioctl is much like calling the stdio clearerr() function
	 * and so we should reset libst's flag as well.  If the
	 * error condition is still really there, it will be
	 * detected on a future read.
	 */
	ft->file.eof = ST_SUCCESS;
    }
#endif

    return len;
}

st_ssize_t st_alsawrite(ft_t ft, st_sample_t *buf, st_ssize_t nsamp)
{
    st_ssize_t len;

    len = st_rawwrite(ft, buf, nsamp);

#if USE_ALSA9
    /* ALSA 0.9 and above require that we detects overruns and
     * reset the driver if it occurs.
     */
    if (len != nsamp)
    {
	/* Reset the driver.  A future enhancement would be to
	 * resend the remaining data (starting at (nsamp - len) in the buffer).
	 * But since we've already lost some data, I'm being lazy
	 * and letting a little more data be lost as well.
	 */
	ioctl(fileno(ft->fp), SNDRV_PCM_IOCTL_PREPARE);

	/* Raw routines use eof flag to store when an internal error
	 * the above ioctl is much like calling the stdio clearerr() function
	 * and so we should reset libst's flag as well.  If the
	 * error condition is still really there, it will be
	 * detected on a future write.
	 */
	ft->file.eof = ST_SUCCESS;
    }
#endif

    return len;
}

#endif /* ALSA_PLAYER */
