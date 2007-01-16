/* Direct to Sun Audio Driver
 *
 * Added by Chris Bagwell (cbagwell@sprynet.com) on 2/26/96
 * Based on oss driver.
 *
 * Cleaned up changes of format somewhat in sunstartwrite on 03/31/98
 *
 */

/*
 * Copyright 1997 Chris Bagwell And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Rick Richardson, Lance Norskog And Sundry Contributors are not
 * responsible for the consequences of using this software.
 */

#include "st_i.h"

#ifdef HAVE_SUN_AUDIO

#include <sys/ioctl.h>
#include <sys/types.h>
#ifdef HAVE_SUN_AUDIOIO_H
#include <sun/audioio.h>
#else
#ifdef HAVE_SYS_AUDIOIO_H
#include <sys/audioio.h>
#endif
#endif
#include <errno.h>
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#include <stropts.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

/*
 * Do anything required before you start reading samples.
 * Read file header.
 *      Find out sampling rate,
 *      size and encoding of samples,
 *      mono/stereo/quad.
 */
static int st_sunstartread(ft_t ft)
{
    st_fileinfo_t *file = (st_fileinfo_t *)ft->priv;
    st_size_t samplesize, encoding;
    audio_info_t audio_if;
#ifdef __SVR4
    audio_device_t audio_dev;
#endif
    char simple_hw=0;

    /* Hard-code for now. */
    file->count = 0;
    file->pos = 0;
    file->eof = 0;
    file->size = 1024;
    file->buf = xmalloc (file->size);

    if (ft->signal.rate == 0.0) ft->signal.rate = 8000;
    if (ft->signal.size == -1) ft->signal.size = ST_SIZE_BYTE;
    if (ft->signal.encoding == ST_ENCODING_UNKNOWN) ft->signal.encoding = ST_ENCODING_ULAW;

#ifdef __SVR4
    /* Read in old values, change to what we need and then send back */
    if (ioctl(fileno(ft->fp), AUDIO_GETDEV, &audio_dev) < 0) {
        st_fail_errno(ft,errno,"Unable to get device information.");
        return(ST_EOF);
    }
    st_report("Hardware detected:  %s",audio_dev.name);
    if (strcmp("SUNW,am79c30",audio_dev.name) == 0)
    {
        simple_hw = 1;
    }
#endif

    /* If simple hardware detected in force data to ulaw. */
    if (simple_hw)
    {
        if (ft->signal.size == ST_SIZE_BYTE)
        {
            if (ft->signal.encoding != ST_ENCODING_ULAW &&
                ft->signal.encoding != ST_ENCODING_ALAW)
            {
                st_report("Warning: Detected simple hardware.  Forcing output to ULAW");
                ft->signal.encoding = ST_ENCODING_ULAW;
            }
        }
        else if (ft->signal.size == ST_SIZE_16BIT)
        {
            st_report("Warning: Detected simple hardware.  Forcing output to ULAW");
            ft->signal.size = ST_SIZE_BYTE;
            ft->signal.encoding = ST_ENCODING_ULAW;
        }
    }

    if (ft->signal.size == ST_SIZE_BYTE) {
        samplesize = 8;
        if (ft->signal.encoding != ST_ENCODING_ULAW &&
            ft->signal.encoding != ST_ENCODING_ALAW &&
            ft->signal.encoding != ST_ENCODING_SIGN2) {
            st_fail_errno(ft,ST_EFMT,"Sun audio driver only supports ULAW, ALAW, and signed linear for bytes.");
                return (ST_EOF);
        }
        if ((ft->signal.encoding == ST_ENCODING_ULAW ||
             ft->signal.encoding == ST_ENCODING_ALAW) && 
            ft->signal.channels == 2)
        {
            st_report("Warning: only support mono for ULAW and ALAW data.  Forcing to mono.");
            ft->signal.channels = 1;
        }
    }
    else if (ft->signal.size == ST_SIZE_16BIT) {
        samplesize = 16;
        if (ft->signal.encoding != ST_ENCODING_SIGN2) {
            st_fail_errno(ft,ST_EFMT,"Sun audio driver only supports signed linear for words.");
            return(ST_EOF);
        }
    }
    else {
        st_fail_errno(ft,ST_EFMT,"Sun audio driver only supports bytes and words");
        return(ST_EOF);
    }

    if (ft->signal.channels == 0) ft->signal.channels = 1;
    else if (ft->signal.channels > 1) {
        st_report("Warning: some Sun audio devices can not play stereo");
        st_report("at all or sometimes only with signed words.  If the");
        st_report("sound seems sluggish then this is probably the case.");
        st_report("Try forcing output to signed words or use the avg");
        st_report("filter to reduce the number of channels.");
        ft->signal.channels = 2;
    }

    /* Read in old values, change to what we need and then send back */
    if (ioctl(fileno(ft->fp), AUDIO_GETINFO, &audio_if) < 0) {
        st_fail_errno(ft,errno,"Unable to initialize /dev/audio");
        return(ST_EOF);
    }
    audio_if.record.precision = samplesize;
    audio_if.record.channels = ft->signal.channels;
    audio_if.record.sample_rate = ft->signal.rate;
    if (ft->signal.encoding == ST_ENCODING_ULAW)
        encoding = AUDIO_ENCODING_ULAW;
    else if (ft->signal.encoding == ST_ENCODING_ALAW)
        encoding = AUDIO_ENCODING_ALAW;
    else
        encoding = AUDIO_ENCODING_LINEAR;
    audio_if.record.encoding = encoding;

    ioctl(fileno(ft->fp), AUDIO_SETINFO, &audio_if);
    if (audio_if.record.precision != samplesize) {
        st_fail_errno(ft,errno,"Unable to initialize sample size for /dev/audio");
        return(ST_EOF);
    }
    if (audio_if.record.channels != ft->signal.channels) {
        st_fail_errno(ft,errno,"Unable to initialize number of channels for /dev/audio");
        return(ST_EOF);
    }
    if (audio_if.record.sample_rate != ft->signal.rate) {
        st_fail_errno(ft,errno,"Unable to initialize rate for /dev/audio");
        return(ST_EOF);
    }
    if (audio_if.record.encoding != encoding) {
        st_fail_errno(ft,errno,"Unable to initialize encoding for /dev/audio");
        return(ST_EOF);
    }
    /* Flush any data in the buffers - its probably in the wrong format */
#if defined(__NetBSD__) || defined(__OpenBSD__)
    ioctl(fileno(ft->fp), AUDIO_FLUSH);
#else
    ioctl(fileno(ft->fp), I_FLUSH, FLUSHR);
#endif
    /* Change to non-buffered I/O*/
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * file->size);

    return (ST_SUCCESS);
}

static int st_sunstartwrite(ft_t ft)
{
    st_fileinfo_t *file = (st_fileinfo_t *)ft->priv;
    st_size_t samplesize, encoding;
    audio_info_t audio_if;
#ifdef __SVR4
    audio_device_t audio_dev;
#endif
    char simple_hw=0;

    /* Hard-code for now. */
    file->count = 0;
    file->pos = 0;
    file->eof = 0;
    file->size = 1024;
    file->buf = xmalloc (file->size);

#ifdef __SVR4
    /* Read in old values, change to what we need and then send back */
    if (ioctl(fileno(ft->fp), AUDIO_GETDEV, &audio_dev) < 0) {
        st_fail_errno(ft,errno,"Unable to get device information.");
        return(ST_EOF);
    }
    st_report("Hardware detected:  %s",audio_dev.name);
    if (strcmp("SUNW,am79c30",audio_dev.name) == 0)
    {
        simple_hw = 1;
    }
#endif

    if (simple_hw)
    {
        if (ft->signal.size == ST_SIZE_BYTE)
        {
            if (ft->signal.encoding != ST_ENCODING_ULAW &&
                ft->signal.encoding != ST_ENCODING_ALAW)
            {
                st_report("Warning: Detected simple hardware.  Forcing output to ULAW");
                ft->signal.encoding = ST_ENCODING_ULAW;
            }
        }
        else if (ft->signal.size == ST_SIZE_16BIT)
        {
            st_report("Warning: Detected simple hardware.  Forcing output to ULAW");
            ft->signal.size = ST_SIZE_BYTE;
            ft->signal.encoding = ST_ENCODING_ULAW;
        }
    }

    if (ft->signal.rate == 0.0) ft->signal.rate = 8000;
    if (ft->signal.size == -1) ft->signal.size = ST_SIZE_BYTE;
    if (ft->signal.encoding == ST_ENCODING_UNKNOWN) 
        ft->signal.encoding = ST_ENCODING_ULAW;

    if (ft->signal.size == ST_SIZE_BYTE) 
    {
        samplesize = 8;
        if (ft->signal.encoding != ST_ENCODING_ULAW &&
            ft->signal.encoding != ST_ENCODING_ALAW &&
            ft->signal.encoding != ST_ENCODING_SIGN2) {
            st_report("Sun Audio driver only supports ULAW, ALAW, and Signed Linear for bytes.");
            st_report("Forcing to ULAW");
            ft->signal.encoding = ST_ENCODING_ULAW;
        }
        if ((ft->signal.encoding == ST_ENCODING_ULAW ||
             ft->signal.encoding == ST_ENCODING_ALAW) && 
            ft->signal.channels == 2)
        {
            st_report("Warning: only support mono for ULAW and ALAW data.  Forcing to mono.");
            ft->signal.channels = 1;
        }

    }
    else if (ft->signal.size == ST_SIZE_16BIT) {
        samplesize = 16;
        if (ft->signal.encoding != ST_ENCODING_SIGN2) {
            st_report("Sun Audio driver only supports Signed Linear for words.");
            st_report("Forcing to Signed Linear");
            ft->signal.encoding = ST_ENCODING_SIGN2;
        }
    }
    else {
        st_report("Sun Audio driver only supports bytes and words");
        ft->signal.size = ST_SIZE_16BIT;
        samplesize = 16;
    }

    if (ft->signal.channels == 0) ft->signal.channels = 1;
    else if (ft->signal.channels > 1) ft->signal.channels = 2;

    /* Read in old values, change to what we need and then send back */
    if (ioctl(fileno(ft->fp), AUDIO_GETINFO, &audio_if) < 0) {
        st_fail_errno(ft,errno,"Unable to initialize /dev/audio");
        return(ST_EOF);
    }
    audio_if.play.precision = samplesize;
    audio_if.play.channels = ft->signal.channels;
    audio_if.play.sample_rate = ft->signal.rate;
    if (ft->signal.encoding == ST_ENCODING_ULAW)
        encoding = AUDIO_ENCODING_ULAW;
    else if (ft->signal.encoding == ST_ENCODING_ALAW)
        encoding = AUDIO_ENCODING_ALAW;
    else
        encoding = AUDIO_ENCODING_LINEAR;
    audio_if.play.encoding = encoding;

    ioctl(fileno(ft->fp), AUDIO_SETINFO, &audio_if);
    if (audio_if.play.precision != samplesize) {
        st_fail_errno(ft,errno,"Unable to initialize sample size for /dev/audio");
        return(ST_EOF);
    }
    if (audio_if.play.channels != ft->signal.channels) {
        st_fail_errno(ft,errno,"Unable to initialize number of channels for /dev/audio");
        return(ST_EOF);
    }
    if (audio_if.play.sample_rate != ft->signal.rate) {
        st_fail_errno(ft,errno,"Unable to initialize rate for /dev/audio");
        return(ST_EOF);
    }
    if (audio_if.play.encoding != encoding) {
        st_fail_errno(ft,errno,"Unable to initialize encoding for /dev/audio");
        return(ST_EOF);
    }
    /* Change to non-buffered I/O */
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * file->size);

    return (ST_SUCCESS);
}

/* Sun /dev/audio player */
static const char *sunnames[] = {
  "sunau",
  NULL
};

static st_format_t st_sun_format = {
  sunnames,
  NULL,
  ST_FILE_DEVICE,
  st_sunstartread,
  st_rawread,
  st_rawstopread,
  st_sunstartwrite,
  st_rawwrite,
  st_rawstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_sun_format_fn(void)
{
    return &st_sun_format;
}

#endif
