#if	defined(SUNAUDIO_PLAYER)
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

#include <sys/ioctl.h>
#ifdef __SVR4
#include <sys/audioio.h>
#else
#include <sun/audioio.h>
#endif

#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include "st.h"

/*
 * Do anything required before you start reading samples.
 * Read file header.
 *	Find out sampling rate,
 *	size and style of samples,
 *	mono/stereo/quad.
 */
void sunstartread(ft)
ft_t ft;
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
	fail("unable to allocate input buffer of size %d", ft->file.size);
    }

    if (ft->info.rate == 0.0) ft->info.rate = 8000;
    if (ft->info.size == -1) ft->info.size = BYTE;
    if (ft->info.style == -1) ft->info.style = ULAW;

#ifdef __SVR4
    /* Read in old values, change to what we need and then send back */
    if (ioctl(fileno(ft->fp), AUDIO_GETDEV, &audio_dev) < 0) {
	fail("Unable to get device information.");
    }
    report("Hardware detected:  %s\n",audio_dev.name);
    if (strcmp("SUNW,am79c30",audio_dev.name) == 0)
    {
	simple_hw = 1;
    }
#endif

    // If simple hardware detected in force data to ulaw.
    if (simple_hw)
    {
	if (ft->info.size == BYTE)
	{
	    if (ft->info.style != ULAW && ft->info.style != ALAW)
	    {
		report("Warning: Detected simple hardware.  Forcing output to ULAW");
		ft->info.style = ULAW;
	    }
	}
	else if (ft->info.size == WORD)
	{
	    report("Warning: Detected simple hardware.  Forcing output to ULAW");
	    ft->info.size = BYTE;
	    ft->info.style = ULAW;
	}
    }
   
    if (ft->info.size == BYTE) {
	samplesize = 8;
	if (ft->info.style != ULAW &&
	    ft->info.style != ALAW &&
	    ft->info.style != SIGN2) {
	    fail("Sun Audio driver only supports ULAW, ALAW, and Signed Linear for bytes.");
	}
	if ((ft->info.style == ULAW || ft->info.style == ALAW) && ft->info.channels == 2)
	{
	    report("Warning: only support mono for ULAW and ALAW data.  Forcing to mono");
	    ft->info.channels = 2;
	}
    }
    else if (ft->info.size == WORD) {
	samplesize = 16;
	if (ft->info.style != SIGN2) {
	    fail("Sun Audio driver only supports Signed Linear for words.");
	}
    }
    else {
	fail("Sun Audio driver only supports bytes and words");
    }

    if (ft->info.channels == -1) ft->info.channels = 1;
    else if (ft->info.channels > 1) {
	report("Warning: some sun audio devices can not play stereo");
	report("at all or sometime only with signed words.  If the");
	report("sound seems sluggish then this is probably the case.");
	report("Try forcing output to signed words or use the avg");
	report("filter to reduce the number of channels.");
	ft->info.channels = 2;
    }

    /* Read in old values, change to what we need and then send back */
    if (ioctl(fileno(ft->fp), AUDIO_GETINFO, &audio_if) < 0) {
	fail("Unable to initialize /dev/audio");
    }
    audio_if.record.precision = samplesize;
    audio_if.record.channels = ft->info.channels;
    audio_if.record.sample_rate = ft->info.rate;
    if (ft->info.style == ULAW)
	encoding = AUDIO_ENCODING_ULAW;
    else if (ft->info.style == ALAW)
	encoding = AUDIO_ENCODING_ALAW;
    else
	encoding = AUDIO_ENCODING_LINEAR;
    audio_if.record.encoding = encoding;
    
    ioctl(fileno(ft->fp), AUDIO_SETINFO, &audio_if);
    if (audio_if.record.precision != samplesize) {
        fail("Unable to initialize sample size for /dev/audio");
    }
    if (audio_if.record.channels != ft->info.channels) {
	fail("Unable to initialize number of channels for /dev/audio");
    }
    if (audio_if.record.sample_rate != ft->info.rate) {
	fail("Unable to initialize rate for /dev/audio");
    }
    if (audio_if.record.encoding != encoding) {
	fail("Unable to initialize style for /dev/audio");
    }
    /* Change to non-buffered I/O*/
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * ft->file.size);
    sigintreg(ft);	/* Prepare to catch SIGINT */
}

void sunstartwrite(ft)
ft_t ft;
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
	fail("unable to allocate output buffer of size %d", ft->file.size);
    }

#ifdef __SVR4
    /* Read in old values, change to what we need and then send back */
    if (ioctl(fileno(ft->fp), AUDIO_GETDEV, &audio_dev) < 0) {
	fail("Unable to get device information.");
    }
    report("Hardware detected:  %s\n",audio_dev.name);
    if (strcmp("SUNW,am79c30",audio_dev.name) == 0)
    {
	simple_hw = 1;
    }
#endif

    if (simple_hw)
    {
	if (ft->info.size == BYTE)
	{
	    if (ft->info.style != ULAW && ft->info.style != ALAW)
	    {
		report("Warning: Detected simple hardware.  Forcing output to ULAW");
		ft->info.style = ULAW;
	    }
	}
	else if (ft->info.size == WORD)
	{
	    report("Warning: Detected simple hardware.  Forcing output to ULAW");
	    ft->info.size = BYTE;
	    ft->info.style = ULAW;
	}
    }
 
    if (ft->info.rate == 0.0) ft->info.rate = 8000;
    if (ft->info.size == -1) ft->info.size = BYTE;
    if (ft->info.style == -1) ft->info.style = ULAW;

    if (ft->info.size == BYTE) {
	samplesize = 8;
	if (ft->info.style != ULAW &&
	    ft->info.style != ALAW &&
	    ft->info.style != SIGN2) {
	    report("Sun Audio driver only supports ULAW, ALAW, and Signed Linear for bytes.");
	    report("Forcing to ULAW");
	    ft->info.style = ULAW;
	}
	if ((ft->info.style == ULAW || ft->info.style == ALAW) && ft->info.channels == 2)
	{
	    report("Warning: only support mono for ULAW and ALAW data.  Forcing to mono");
	    ft->info.channels = 2;
	}

    }
    else if (ft->info.size == WORD) {
	samplesize = 16;
	if (ft->info.style != SIGN2) {
	    report("Sun Audio driver only supports Signed Linear for words.");
	    report("Forcing to Signed Linear");
	    ft->info.style = SIGN2;
	}
    }
    else {
	report("Sun Audio driver only supports bytes and words");
	ft->info.size = WORD;
	samplesize = 16;
    }

    if (ft->info.channels == -1) ft->info.channels = 1;
    else if (ft->info.channels > 1) ft->info.channels = 2;

    /* Read in old values, change to what we need and then send back */
    if (ioctl(fileno(ft->fp), AUDIO_GETINFO, &audio_if) < 0) {
	fail("Unable to initialize /dev/audio");
    }
    audio_if.play.precision = samplesize;
    audio_if.play.channels = ft->info.channels;
    audio_if.play.sample_rate = ft->info.rate;
    if (ft->info.style == ULAW)
	encoding = AUDIO_ENCODING_ULAW;
    else if (ft->info.style == ALAW)
	encoding = AUDIO_ENCODING_ALAW;
    else
	encoding = AUDIO_ENCODING_LINEAR;
    audio_if.play.encoding = encoding;
    
    ioctl(fileno(ft->fp), AUDIO_SETINFO, &audio_if);
    if (audio_if.play.precision != samplesize) {
	fail("Unable to initialize sample size for /dev/audio");
    }
    if (audio_if.play.channels != ft->info.channels) {
	fail("Unable to initialize number of channels for /dev/audio");
    }
    if (audio_if.play.sample_rate != ft->info.rate) {
	fail("Unable to initialize rate for /dev/audio");
    }
    if (audio_if.play.encoding != encoding) {
	fail("Unable to initialize style for /dev/audio");
    }
    /* Change to non-buffered I/O */
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * ft->file.size);
}

#endif
