#if	defined(ALSA_PLAYER)
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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/asound.h>
#include <sys/ioctl.h>
#include "st.h"

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
    ioctl(fileno(ft->fp), SND_PCM_IOCTL_CAPTURE_INFO, &c_info);
    ft->file.count = 0;
    ft->file.pos = 0;
    ft->file.eof = 0;
    ft->file.size = c_info.buffer_size;
    if ((ft->file.buf = malloc (ft->file.size)) == NULL) {
	st_fail("unable to allocate output buffer of size %d", ft->file.size);
	return (ST_EOF);
    }
    if (ft->info.rate < c_info.min_rate) ft->info.rate = 2 * c_info.min_rate;
    else if (ft->info.rate > c_info.max_rate) ft->info.rate = c_info.max_rate;
    if (ft->info.channels == -1) ft->info.channels = c_info.min_channels;
    else if (ft->info.channels > c_info.max_channels) ft->info.channels = c_info.max_channels;
    if (ft->info.size == -1) {
	if ((c_info.hw_formats & SND_PCM_FMT_U8) || (c_info.hw_formats & SND_PCM_FMT_S8))
	    ft->info.size = ST_SIZE_BYTE;
	else
	    ft->info.size = ST_SIZE_WORD;
    }
    if (ft->info.encoding == -1) {
	if ((c_info.hw_formats & SND_PCM_FMT_S16_LE) || (c_info.hw_formats & SND_PCM_FMT_S8))
	    ft->info.encoding = ST_ENCODING_SIGN2;
	else
	    ft->info.encoding = ST_ENCODING_UNSIGNED;
    }
    if (ft->info.size == ST_SIZE_BYTE) {
	switch (ft->info.encoding)
	{
	    case ST_ENCODING_SIGN2:
		if (!(c_info.hw_formats & SND_PCM_FMT_S8))
		{
		    st_fail("ALSA driver does not support signed byte samples");
		    return(ST_EOF);
		}
		fmt = SND_PCM_SFMT_S8;
		break;
	    case ST_ENCODING_UNSIGNED:
		if (!(c_info.hw_formats & SND_PCM_FMT_U8))
		{
		    st_fail("ALSA driver does not support unsigned byte samples");
		    return(ST_EOF);
		}
		fmt = SND_PCM_SFMT_U8;
		break;
	    default:
		st_fail("Hardware does not support %s output", st_encodings_str[ft->info.encoding]);
		return(ST_EOF);
		break;
	}
    }
    else {
	switch (ft->info.encoding)
	{
	    case ST_ENCODING_SIGN2:
		if (!(c_info.hw_formats & SND_PCM_FMT_S16_LE))
		    st_fail("ALSA driver does not support signed word samples");
		fmt = SND_PCM_SFMT_S16_LE;
		break;
	    case ST_ENCODING_UNSIGNED:
		if (!(c_info.hw_formats & SND_PCM_FMT_U16_LE))
		    st_fail("ALSA driver does not support unsigned word samples");
		fmt = SND_PCM_SFMT_U16_LE;
		break;
	    default:
		st_fail("Hardware does not support %s output", st_encodings_str[ft->info.encoding]);
		return(ST_EOF);
		break;
	}
    }

    memset(&format, 0, sizeof(format));
    format.format = fmt;
    format.rate = ft->info.rate;
    format.channels = ft->info.channels;
    ioctl(fileno(ft->fp), SND_PCM_IOCTL_CAPTURE_FORMAT, &format);

    size = ft->file.size;
    bps = format.rate * format.channels;
    if (ft->info.size == ST_SIZE_WORD) bps <<= 1;
    bps >>= 2;
    while (size > bps) size >>= 1;
    if (size < 16) size = 16;
    memset(&c_params, 0, sizeof(c_params));
    c_params.fragment_size = size;
    c_params.fragments_min = 1;
    ioctl(fileno(ft->fp), SND_PCM_IOCTL_CAPTURE_PARAMS, &c_params);

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
    ioctl(fileno(ft->fp), SND_PCM_IOCTL_PLAYBACK_INFO, &p_info);
    ft->file.pos = 0;
    ft->file.eof = 0;
    ft->file.size = p_info.buffer_size;
    if ((ft->file.buf = malloc (ft->file.size)) == NULL) {
	st_fail("unable to allocate output buffer of size %d", ft->file.size);
	return(ST_EOF);
    }
    if (ft->info.rate < p_info.min_rate) ft->info.rate = 2 * p_info.min_rate;
    else if (ft->info.rate > p_info.max_rate) ft->info.rate = p_info.max_rate;
    if (ft->info.channels == -1) ft->info.channels = p_info.min_channels;
    else if (ft->info.channels > p_info.max_channels) ft->info.channels = p_info.max_channels;
    if (ft->info.size == -1) {
	if ((p_info.hw_formats & SND_PCM_FMT_U8) || (p_info.hw_formats & SND_PCM_FMT_S8))
	    ft->info.size = ST_SIZE_BYTE;
	else
	    ft->info.size = ST_SIZE_WORD;
    }
    if (ft->info.encoding == -1) {
	if ((p_info.hw_formats & SND_PCM_FMT_S16_LE) || (p_info.hw_formats & SND_PCM_FMT_S8))
	    ft->info.encoding = ST_ENCODING_SIGN2;
	else
	    ft->info.encoding = ST_ENCODING_UNSIGNED;
    }
    if (ft->info.size == ST_SIZE_BYTE) {
	switch (ft->info.encoding)
	{
	    case ST_ENCODING_SIGN2:
		if (!(p_info.hw_formats & SND_PCM_FMT_S8))
		{
		    st_fail("ALSA driver does not support signed byte samples");
		    return (ST_EOF);
		}
		fmt = SND_PCM_SFMT_S8;
		break;
	    case ST_ENCODING_UNSIGNED:
		if (!(p_info.hw_formats & SND_PCM_FMT_U8))
		{
		    st_fail("ALSA driver does not support unsigned byte samples");
		    return (ST_EOF);
		}
		fmt = SND_PCM_SFMT_U8;
		break;
	    default:
		st_fail("Hardware does not support %s output", st_encodings_str[ft->info.encoding]);
		return(ST_EOF);
		break;
	}
    }
    else {
	switch (ft->info.encoding)
	{
	    case ST_ENCODING_SIGN2:
		if (!(p_info.hw_formats & SND_PCM_FMT_S16_LE))
		{
		    st_fail("ALSA driver does not support signed word samples");
		    return (ST_EOF);
		}
		fmt = SND_PCM_SFMT_S16_LE;
		break;
	    case ST_ENCODING_UNSIGNED:
		if (!(p_info.hw_formats & SND_PCM_FMT_U16_LE))
		{
		    st_fail("ALSA driver does not support unsigned word samples");
		    return(ST_EOF);
		}
		fmt = SND_PCM_SFMT_U16_LE;
		break;
	    default:
		st_fail("Hardware does not support %s output", st_encodings_str[ft->info.encoding]);
		return(ST_EOF);
		break;
	}
    }

    memset(&format, 0, sizeof(format));
    format.format = fmt;
    format.rate = ft->info.rate;
    format.channels = ft->info.channels;
    ioctl(fileno(ft->fp), SND_PCM_IOCTL_PLAYBACK_FORMAT, &format);

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
    ioctl(fileno(ft->fp), SND_PCM_IOCTL_PLAYBACK_PARAMS, &p_params);

    /* Change to non-buffered I/O */
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * ft->file.size);

    return(ST_SUCCESS);
}

#endif
