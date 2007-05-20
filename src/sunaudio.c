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

#include "sox_i.h"

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
static int sox_sunstartread(ft_t ft)
{
    sox_fileinfo_t *file = (sox_fileinfo_t *)ft->priv;
    sox_size_t samplesize, encoding;
    audio_info_t audio_if;
#ifdef __SVR4
    audio_device_t audio_dev;
#endif
    char simple_hw=0;

    /* Hard-code for now. */
    file->count = 0;
    file->pos = 0;
    file->size = 1024;
    file->buf = xmalloc (file->size);

    if (ft->signal.rate == 0.0) ft->signal.rate = 8000;
    if (ft->signal.size == -1) ft->signal.size = SOX_SIZE_BYTE;
    if (ft->signal.encoding == SOX_ENCODING_UNKNOWN) ft->signal.encoding = SOX_ENCODING_ULAW;

#ifdef __SVR4
    /* Read in old values, change to what we need and then send back */
    if (ioctl(fileno(ft->fp), AUDIO_GETDEV, &audio_dev) < 0) {
        sox_fail_errno(ft,errno,"Unable to get device information.");
        return(SOX_EOF);
    }
    sox_report("Hardware detected:  %s",audio_dev.name);
    if (strcmp("SUNW,am79c30",audio_dev.name) == 0)
    {
        simple_hw = 1;
    }
#endif

    /* If simple hardware detected in force data to ulaw. */
    if (simple_hw)
    {
        if (ft->signal.size == SOX_SIZE_BYTE)
        {
            if (ft->signal.encoding != SOX_ENCODING_ULAW &&
                ft->signal.encoding != SOX_ENCODING_ALAW)
            {
                sox_report("Warning: Detected simple hardware.  Forcing output to ULAW");
                ft->signal.encoding = SOX_ENCODING_ULAW;
            }
        }
        else if (ft->signal.size == SOX_SIZE_16BIT)
        {
            sox_report("Warning: Detected simple hardware.  Forcing output to ULAW");
            ft->signal.size = SOX_SIZE_BYTE;
            ft->signal.encoding = SOX_ENCODING_ULAW;
        }
    }

    if (ft->signal.size == SOX_SIZE_BYTE) {
        samplesize = 8;
        if (ft->signal.encoding != SOX_ENCODING_ULAW &&
            ft->signal.encoding != SOX_ENCODING_ALAW &&
            ft->signal.encoding != SOX_ENCODING_SIGN2) {
            sox_fail_errno(ft,SOX_EFMT,"Sun audio driver only supports ULAW, ALAW, and signed linear for bytes.");
                return (SOX_EOF);
        }
        if ((ft->signal.encoding == SOX_ENCODING_ULAW ||
             ft->signal.encoding == SOX_ENCODING_ALAW) && 
            ft->signal.channels == 2)
        {
            sox_report("Warning: only support mono for ULAW and ALAW data.  Forcing to mono.");
            ft->signal.channels = 1;
        }
    }
    else if (ft->signal.size == SOX_SIZE_16BIT) {
        samplesize = 16;
        if (ft->signal.encoding != SOX_ENCODING_SIGN2) {
            sox_fail_errno(ft,SOX_EFMT,"Sun audio driver only supports signed linear for words.");
            return(SOX_EOF);
        }
    }
    else {
        sox_fail_errno(ft,SOX_EFMT,"Sun audio driver only supports bytes and words");
        return(SOX_EOF);
    }

    if (ft->signal.channels == 0) ft->signal.channels = 1;
    else if (ft->signal.channels > 1) {
        sox_report("Warning: some Sun audio devices can not play stereo");
        sox_report("at all or sometimes only with signed words.  If the");
        sox_report("sound seems sluggish then this is probably the case.");
        sox_report("Try forcing output to signed words or use the avg");
        sox_report("filter to reduce the number of channels.");
        ft->signal.channels = 2;
    }

    /* Read in old values, change to what we need and then send back */
    if (ioctl(fileno(ft->fp), AUDIO_GETINFO, &audio_if) < 0) {
        sox_fail_errno(ft,errno,"Unable to initialize /dev/audio");
        return(SOX_EOF);
    }
    audio_if.record.precision = samplesize;
    audio_if.record.channels = ft->signal.channels;
    audio_if.record.sample_rate = ft->signal.rate;
    if (ft->signal.encoding == SOX_ENCODING_ULAW)
        encoding = AUDIO_ENCODING_ULAW;
    else if (ft->signal.encoding == SOX_ENCODING_ALAW)
        encoding = AUDIO_ENCODING_ALAW;
    else
        encoding = AUDIO_ENCODING_LINEAR;
    audio_if.record.encoding = encoding;

    ioctl(fileno(ft->fp), AUDIO_SETINFO, &audio_if);
    if (audio_if.record.precision != samplesize) {
        sox_fail_errno(ft,errno,"Unable to initialize sample size for /dev/audio");
        return(SOX_EOF);
    }
    if (audio_if.record.channels != ft->signal.channels) {
        sox_fail_errno(ft,errno,"Unable to initialize number of channels for /dev/audio");
        return(SOX_EOF);
    }
    if (audio_if.record.sample_rate != ft->signal.rate) {
        sox_fail_errno(ft,errno,"Unable to initialize rate for /dev/audio");
        return(SOX_EOF);
    }
    if (audio_if.record.encoding != encoding) {
        sox_fail_errno(ft,errno,"Unable to initialize encoding for /dev/audio");
        return(SOX_EOF);
    }
    /* Flush any data in the buffers - its probably in the wrong format */
#if defined(__NetBSD__) || defined(__OpenBSD__)
    ioctl(fileno(ft->fp), AUDIO_FLUSH);
#else
    ioctl(fileno(ft->fp), I_FLUSH, FLUSHR);
#endif
    /* Change to non-buffered I/O*/
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * file->size);

    return (SOX_SUCCESS);
}

static int sox_sunstartwrite(ft_t ft)
{
    sox_fileinfo_t *file = (sox_fileinfo_t *)ft->priv;
    sox_size_t samplesize, encoding;
    audio_info_t audio_if;
#ifdef __SVR4
    audio_device_t audio_dev;
#endif
    char simple_hw=0;

    /* Hard-code for now. */
    file->count = 0;
    file->pos = 0;
    file->size = 1024;
    file->buf = xmalloc (file->size);

#ifdef __SVR4
    /* Read in old values, change to what we need and then send back */
    if (ioctl(fileno(ft->fp), AUDIO_GETDEV, &audio_dev) < 0) {
        sox_fail_errno(ft,errno,"Unable to get device information.");
        return(SOX_EOF);
    }
    sox_report("Hardware detected:  %s",audio_dev.name);
    if (strcmp("SUNW,am79c30",audio_dev.name) == 0)
    {
        simple_hw = 1;
    }
#endif

    if (simple_hw)
    {
        if (ft->signal.size == SOX_SIZE_BYTE)
        {
            if (ft->signal.encoding != SOX_ENCODING_ULAW &&
                ft->signal.encoding != SOX_ENCODING_ALAW)
            {
                sox_report("Warning: Detected simple hardware.  Forcing output to ULAW");
                ft->signal.encoding = SOX_ENCODING_ULAW;
            }
        }
        else if (ft->signal.size == SOX_SIZE_16BIT)
        {
            sox_report("Warning: Detected simple hardware.  Forcing output to ULAW");
            ft->signal.size = SOX_SIZE_BYTE;
            ft->signal.encoding = SOX_ENCODING_ULAW;
        }
    }

    if (ft->signal.rate == 0.0) ft->signal.rate = 8000;
    if (ft->signal.size == -1) ft->signal.size = SOX_SIZE_BYTE;
    if (ft->signal.encoding == SOX_ENCODING_UNKNOWN) 
        ft->signal.encoding = SOX_ENCODING_ULAW;

    if (ft->signal.size == SOX_SIZE_BYTE) 
    {
        samplesize = 8;
        if (ft->signal.encoding != SOX_ENCODING_ULAW &&
            ft->signal.encoding != SOX_ENCODING_ALAW &&
            ft->signal.encoding != SOX_ENCODING_SIGN2) {
            sox_report("Sun Audio driver only supports ULAW, ALAW, and Signed Linear for bytes.");
            sox_report("Forcing to ULAW");
            ft->signal.encoding = SOX_ENCODING_ULAW;
        }
        if ((ft->signal.encoding == SOX_ENCODING_ULAW ||
             ft->signal.encoding == SOX_ENCODING_ALAW) && 
            ft->signal.channels == 2)
        {
            sox_report("Warning: only support mono for ULAW and ALAW data.  Forcing to mono.");
            ft->signal.channels = 1;
        }

    }
    else if (ft->signal.size == SOX_SIZE_16BIT) {
        samplesize = 16;
        if (ft->signal.encoding != SOX_ENCODING_SIGN2) {
            sox_report("Sun Audio driver only supports Signed Linear for words.");
            sox_report("Forcing to Signed Linear");
            ft->signal.encoding = SOX_ENCODING_SIGN2;
        }
    }
    else {
        sox_report("Sun Audio driver only supports bytes and words");
        ft->signal.size = SOX_SIZE_16BIT;
        samplesize = 16;
    }

    if (ft->signal.channels == 0) ft->signal.channels = 1;
    else if (ft->signal.channels > 1) ft->signal.channels = 2;

    /* Read in old values, change to what we need and then send back */
    if (ioctl(fileno(ft->fp), AUDIO_GETINFO, &audio_if) < 0) {
        sox_fail_errno(ft,errno,"Unable to initialize /dev/audio");
        return(SOX_EOF);
    }
    audio_if.play.precision = samplesize;
    audio_if.play.channels = ft->signal.channels;
    audio_if.play.sample_rate = ft->signal.rate;
    if (ft->signal.encoding == SOX_ENCODING_ULAW)
        encoding = AUDIO_ENCODING_ULAW;
    else if (ft->signal.encoding == SOX_ENCODING_ALAW)
        encoding = AUDIO_ENCODING_ALAW;
    else
        encoding = AUDIO_ENCODING_LINEAR;
    audio_if.play.encoding = encoding;

    ioctl(fileno(ft->fp), AUDIO_SETINFO, &audio_if);
    if (audio_if.play.precision != samplesize) {
        sox_fail_errno(ft,errno,"Unable to initialize sample size for /dev/audio");
        return(SOX_EOF);
    }
    if (audio_if.play.channels != ft->signal.channels) {
        sox_fail_errno(ft,errno,"Unable to initialize number of channels for /dev/audio");
        return(SOX_EOF);
    }
    if (audio_if.play.sample_rate != ft->signal.rate) {
        sox_fail_errno(ft,errno,"Unable to initialize rate for /dev/audio");
        return(SOX_EOF);
    }
    if (audio_if.play.encoding != encoding) {
        sox_fail_errno(ft,errno,"Unable to initialize encoding for /dev/audio");
        return(SOX_EOF);
    }
    /* Change to non-buffered I/O */
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * file->size);

    return (SOX_SUCCESS);
}

/* Sun /dev/audio player */
static const char *names[] = {
  "sunau",
  NULL
};

static sox_format_t sox_sunau_format = {
  names,
  SOX_FILE_DEVICE,
  sox_sunstartread,
  sox_rawread,
  sox_rawstopread,
  sox_sunstartwrite,
  sox_rawwrite,
  sox_rawstopwrite,
  sox_format_nothing_seek
};

const sox_format_t *sox_sunau_format_fn(void);

const sox_format_t *sox_sunau_format_fn(void)
{
    return &sox_sunau_format;
}
