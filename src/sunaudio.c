/*
 * Copyright 1997 Chris Bagwell And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Rick Richardson, Lance Norskog And Sundry Contributors are not
 * responsible for the consequences of using this software.
 */

/* Direct to Sun Audio Driver
 *
 * Added by Chris Bagwell (cbagwell@sprynet.com) on 2/26/96
 * Based on oss driver.
 *
 * Cleaned up changes of format somewhat in sunstartwrite on 03/31/98
 *
 */

#include "st_i.h"

#if     defined(HAVE_SUNAUDIO)

#include <sys/ioctl.h>
#if defined(__SVR4) || defined(__NetBSD__) || defined(__OpenBSD__)
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/types.h>  /* This should be in audioio.h itself but its not */
#endif
#include <sys/audioio.h>
#else
#include <sun/audioio.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#include <stropts.h>
#endif
#ifdef _HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
/*
 * Do anything required before you start reading samples.
 * Read file header.
 *      Find out sampling rate,
 *      size and encoding of samples,
 *      mono/stereo/quad.
 */
int st_sunstartread(ft_t ft)
{
    int samplesize, encoding;
    audio_info_t audio_if;
#ifdef __SVR4
    audio_device_t audio_dev;
#endif
    char simple_hw=0;

    /* Hard code for now. */
    ft->file.count = 0;
    ft->file.pos = 0;
    ft->file.eof = 0;
    ft->file.size = 1024;
    if ((ft->file.buf = malloc (ft->file.size)) == NULL) {
        st_fail_errno(ft,ST_ENOMEM,"unable to allocate input buffer of size %d", ft->file.size);
        return ST_EOF;
    }

    if (ft->info.rate == 0.0) ft->info.rate = 8000;
    if (ft->info.size == -1) ft->info.size = ST_SIZE_BYTE;
    if (ft->info.encoding == -1) ft->info.encoding = ST_ENCODING_ULAW;

#ifdef __SVR4
    /* Read in old values, change to what we need and then send back */
    if (ioctl(fileno(ft->fp), AUDIO_GETDEV, &audio_dev) < 0) {
        st_fail_errno(ft,errno,"Unable to get device information.");
        return(ST_EOF);
    }
    st_report("Hardware detected:  %s\n",audio_dev.name);
    if (strcmp("SUNW,am79c30",audio_dev.name) == 0)
    {
        simple_hw = 1;
    }
#endif

    // If simple hardware detected in force data to ulaw.
    if (simple_hw)
    {
        if (ft->info.size == ST_SIZE_BYTE)
        {
            if (ft->info.encoding != ST_ENCODING_ULAW &&
                ft->info.encoding != ST_ENCODING_ALAW)
            {
                st_report("Warning: Detected simple hardware.  Forcing output to ULAW");
                ft->info.encoding = ST_ENCODING_ULAW;
            }
        }
        else if (ft->info.size == ST_SIZE_WORD)
        {
            st_report("Warning: Detected simple hardware.  Forcing output to ULAW");
            ft->info.size = ST_SIZE_BYTE;
            ft->info.encoding = ST_ENCODING_ULAW;
        }
    }

    if (ft->info.size == ST_SIZE_BYTE) {
        samplesize = 8;
        if (ft->info.encoding != ST_ENCODING_ULAW &&
            ft->info.encoding != ST_ENCODING_ALAW &&
            ft->info.encoding != ST_ENCODING_SIGN2) {
            st_fail_errno(ft,ST_EFMT,"Sun Audio driver only supports ULAW, ALAW, and Signed Linear for bytes.");
                return (ST_EOF);
        }
        if ((ft->info.encoding == ST_ENCODING_ULAW ||
             ft->info.encoding == ST_ENCODING_ALAW) && ft->info.channels == 2)
        {
            st_report("Warning: only support mono for ULAW and ALAW data.  Forcing to mono");
            ft->info.channels = 2;
        }
    }
    else if (ft->info.size == ST_SIZE_WORD) {
        samplesize = 16;
        if (ft->info.encoding != ST_ENCODING_SIGN2) {
            st_fail_errno(ft,ST_EFMT,"Sun Audio driver only supports Signed Linear for words.");
            return(ST_EOF);
        }
    }
    else {
        st_fail_errno(ft,ST_EFMT,"Sun Audio driver only supports bytes and words");
        return(ST_EOF);
    }

    if (ft->info.channels == -1) ft->info.channels = 1;
    else if (ft->info.channels > 1) {
        st_report("Warning: some sun audio devices can not play stereo");
        st_report("at all or sometime only with signed words.  If the");
        st_report("sound seems sluggish then this is probably the case.");
        st_report("Try forcing output to signed words or use the avg");
        st_report("filter to reduce the number of channels.");
        ft->info.channels = 2;
    }

    /* Read in old values, change to what we need and then send back */
    if (ioctl(fileno(ft->fp), AUDIO_GETINFO, &audio_if) < 0) {
        st_fail_errno(ft,errno,"Unable to initialize /dev/audio");
        return(ST_EOF);
    }
    audio_if.record.precision = samplesize;
    audio_if.record.channels = ft->info.channels;
    audio_if.record.sample_rate = ft->info.rate;
    if (ft->info.encoding == ST_ENCODING_ULAW)
        encoding = AUDIO_ENCODING_ULAW;
    else if (ft->info.encoding == ST_ENCODING_ALAW)
        encoding = AUDIO_ENCODING_ALAW;
    else
        encoding = AUDIO_ENCODING_LINEAR;
    audio_if.record.encoding = encoding;

    ioctl(fileno(ft->fp), AUDIO_SETINFO, &audio_if);
    if (audio_if.record.precision != samplesize) {
        st_fail_errno(ft,errno,"Unable to initialize sample size for /dev/audio");
        return(ST_EOF);
    }
    if (audio_if.record.channels != ft->info.channels) {
        st_fail_errno(ft,errno,"Unable to initialize number of channels for /dev/audio");
        return(ST_EOF);
    }
    if (audio_if.record.sample_rate != ft->info.rate) {
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
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * ft->file.size);

    return (ST_SUCCESS);
}

int st_sunstartwrite(ft_t ft)
{
    int samplesize, encoding;
    audio_info_t audio_if;
#ifdef __SVR4
    audio_device_t audio_dev;
#endif
    char simple_hw=0;

    /* Hard code for now. */
    ft->file.count = 0;
    ft->file.pos = 0;
    ft->file.eof = 0;
    ft->file.size = 1024;
    if ((ft->file.buf = malloc (ft->file.size)) == NULL) {
        st_fail_errno(ft,ST_ENOMEM,"unable to allocate output buffer of size %d", ft->file.size);
        return(ST_EOF);
    }

#ifdef __SVR4
    /* Read in old values, change to what we need and then send back */
    if (ioctl(fileno(ft->fp), AUDIO_GETDEV, &audio_dev) < 0) {
        st_fail_errno(ft,errno,"Unable to get device information.");
        return(ST_EOF);
    }
    st_report("Hardware detected:  %s\n",audio_dev.name);
    if (strcmp("SUNW,am79c30",audio_dev.name) == 0)
    {
        simple_hw = 1;
    }
#endif

    if (simple_hw)
    {
        if (ft->info.size == ST_SIZE_BYTE)
        {
            if (ft->info.encoding != ST_ENCODING_ULAW &&
                ft->info.encoding != ST_ENCODING_ALAW)
            {
                st_report("Warning: Detected simple hardware.  Forcing output to ULAW");
                ft->info.encoding = ST_ENCODING_ULAW;
            }
        }
        else if (ft->info.size == ST_SIZE_WORD)
        {
            st_report("Warning: Detected simple hardware.  Forcing output to ULAW");
            ft->info.size = ST_SIZE_BYTE;
            ft->info.encoding = ST_ENCODING_ULAW;
        }
    }

    if (ft->info.rate == 0.0) ft->info.rate = 8000;
    if (ft->info.size == -1) ft->info.size = ST_SIZE_BYTE;
    if (ft->info.encoding == -1) ft->info.encoding = ST_ENCODING_ULAW;

    if (ft->info.size == ST_SIZE_BYTE) {
        samplesize = 8;
        if (ft->info.encoding != ST_ENCODING_ULAW &&
            ft->info.encoding != ST_ENCODING_ALAW &&
            ft->info.encoding != ST_ENCODING_SIGN2) {
            st_report("Sun Audio driver only supports ULAW, ALAW, and Signed Linear for bytes.");
            st_report("Forcing to ULAW");
            ft->info.encoding = ST_ENCODING_ULAW;
        }
        if ((ft->info.encoding == ST_ENCODING_ULAW ||
             ft->info.encoding == ST_ENCODING_ALAW) && ft->info.channels == 2)
        {
            st_report("Warning: only support mono for ULAW and ALAW data.  Forcing to mono");
            ft->info.channels = 2;
        }

    }
    else if (ft->info.size == ST_SIZE_WORD) {
        samplesize = 16;
        if (ft->info.encoding != ST_ENCODING_SIGN2) {
            st_report("Sun Audio driver only supports Signed Linear for words.");
            st_report("Forcing to Signed Linear");
            ft->info.encoding = ST_ENCODING_SIGN2;
        }
    }
    else {
        st_report("Sun Audio driver only supports bytes and words");
        ft->info.size = ST_SIZE_WORD;
        samplesize = 16;
    }

    if (ft->info.channels == -1) ft->info.channels = 1;
    else if (ft->info.channels > 1) ft->info.channels = 2;

    /* Read in old values, change to what we need and then send back */
    if (ioctl(fileno(ft->fp), AUDIO_GETINFO, &audio_if) < 0) {
        st_fail_errno(ft,errno,"Unable to initialize /dev/audio");
        return(ST_EOF);
    }
    audio_if.play.precision = samplesize;
    audio_if.play.channels = ft->info.channels;
    audio_if.play.sample_rate = ft->info.rate;
    if (ft->info.encoding == ST_ENCODING_ULAW)
        encoding = AUDIO_ENCODING_ULAW;
    else if (ft->info.encoding == ST_ENCODING_ALAW)
        encoding = AUDIO_ENCODING_ALAW;
    else
        encoding = AUDIO_ENCODING_LINEAR;
    audio_if.play.encoding = encoding;

    ioctl(fileno(ft->fp), AUDIO_SETINFO, &audio_if);
    if (audio_if.play.precision != samplesize) {
        st_fail_errno(ft,errno,"Unable to initialize sample size for /dev/audio");
        return(ST_EOF);
    }
    if (audio_if.play.channels != ft->info.channels) {
        st_fail_errno(ft,errno,"Unable to initialize number of channels for /dev/audio");
        return(ST_EOF);
    }
    if (audio_if.play.sample_rate != ft->info.rate) {
        st_fail_errno(ft,errno,"Unable to initialize rate for /dev/audio");
        return(ST_EOF);
    }
    if (audio_if.play.encoding != encoding) {
        st_fail_errno(ft,errno,"Unable to initialize encoding for /dev/audio");
        return(ST_EOF);
    }
    /* Change to non-buffered I/O */
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * ft->file.size);

    return (ST_SUCCESS);
}

#endif
