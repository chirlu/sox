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
 */

#include "st_i.h"

#if	defined(ALSA_PLAYER)

#include <string.h>
#include <fcntl.h>
#include <linux/asound.h>
#include <sys/ioctl.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

static int get_format(ft_t ft, int formats, int *fmt);

#ifdef USE_OLD_API /* Start 0.4.x API */

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

#else /* Start 0.5.x API */

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
    if (size > c_info.max_fragment_size) size = c_info.max_fragment_size;
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
    if (size > p_info.max_fragment_size) size = p_info.max_fragment_size;
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

#endif /* End API */

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
		st_fail_errno(ft,ST_EFMT,"Hardware does not support %s output", st_encodings_str[ft->info.encoding]);
		return ST_EOF;
		break;
	}
    }
    else {
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
		st_fail_errno(ft,ST_EFMT,"Hardware does not support %s output", st_encodings_str[ft->info.encoding]);
		return ST_EOF;
		break;
	}
    }
    return 0;
}

#endif
