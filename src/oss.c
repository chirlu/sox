#if	defined(OSS_PLAYER)
/*
 * Copyright 1997 Chris Bagwell And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Chris Bagwell And Sundry Contributors are not
 * responsible for the consequences of using this software.
 */

/* Direct to Open Sound System (OSS) sound driver
 * OSS is a popular unix sound driver for Intel x86 unices (eg. Linux)
 * and several other unixes (such as SunOS/Solaris).
 * This driver is compatible with OSS original source that was called
 * USS, Voxware and TASD.
 *
 * added by Chris Bagwell (cbagwell@sprynet.com) on 2/19/96
 * based on info grabed from vplay.c in Voxware snd-utils-3.5 package.
 * and on LINUX_PLAYER patches added by Greg Lee
 * which was originally from Directo to Sound Blaster device driver (sbdsp.c).
 * SBLAST patches by John T. Kohl.
 */

#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <signal.h>
#include "st.h"
#include "libst.h"

static int got_int = 0;

static int abuf_size = 0;
static int abuf_cnt = 0;
static char *audiobuf;

/* This is how we know when to stop recording.  User sends interrupt
 * (eg. control-c) and then we mark a flag to show we are done.
 * Must call "sigint(0)" during init so that the OS can be notified
 * what to do.
 */
static void
sigint(s)
{
    if (s) got_int = 1;
    else signal(SIGINT, sigint);
}

/*
 * Do anything required before you start reading samples.
 * Read file header.
 *	Find out sampling rate,
 *	size and style of samples,
 *	mono/stereo/quad.
 */
void ossdspstartread(ft)
ft_t ft;
{
    int tmp;
    int samplesize = 8, dsp_stereo;

    if (ft->info.rate == 0.0) ft->info.rate = 8000;
    if (ft->info.size == -1) ft->info.size = BYTE;
    if (ft->info.size == BYTE) {
	samplesize = 8;
	if (ft->info.style == -1)
	    ft->info.style = UNSIGNED;
	if (ft->info.style != UNSIGNED) {
	    fail("OSS driver only supports unsigned with bytes");
	}
    }
    else if (ft->info.size == WORD) {
	samplesize = 16;
	if (ft->info.style == -1)
	    ft->info.style = SIGN2;
	if (ft->info.style != SIGN2) {
	    fail("OSS driver only supports signed with words");
	}
    }
    else {
	fail("OSS driver only supports bytes and words");
    }

    if (ft->info.channels == -1) ft->info.channels = 1;
    else if (ft->info.channels > 2) ft->info.channels = 2;

    ioctl(fileno(ft->fp), SNDCTL_DSP_RESET, 0);
    ioctl (fileno(ft->fp), SNDCTL_DSP_GETBLKSIZE, &abuf_size);
    if (abuf_size < 4 || abuf_size > 65536) {
	fail("Invalid audio buffer size %d", abuf_size);
    }

    if ((audiobuf = malloc (abuf_size)) == NULL) {
	fail("Unable to allocate input/output buffer of size %d", abuf_size);
    }

    if (ioctl(fileno(ft->fp), SNDCTL_DSP_SYNC, NULL) < 0) {
	fail("Unable to sync dsp");
    }

    tmp = samplesize;
    ioctl(fileno(ft->fp), SNDCTL_DSP_SAMPLESIZE, &tmp);
    if (tmp != samplesize) {
	fail("Unable to set the sample size to %d", samplesize);
    }

    if (ft->info.channels == 2) dsp_stereo = 1;
    else dsp_stereo = 0;

    tmp = dsp_stereo;
    ioctl(fileno(ft->fp), SNDCTL_DSP_STEREO, &tmp);
    if (tmp != dsp_stereo) {
	ft->info.channels = 1;
	warn("Couldn't set to %s", dsp_stereo?  "stereo":"mono");
	dsp_stereo = 0;
    }

    tmp = ft->info.rate;
    ioctl (fileno(ft->fp), SNDCTL_DSP_SPEED, &tmp);
    if (ft->info.rate != tmp) {
	if (ft->info.rate - tmp > tmp/10 || tmp - ft->info.rate > tmp/10)
	    warn("Unable to set audio speed to %d (set to %d)",
		     ft->info.rate, tmp);
	ft->info.rate = tmp;
    }

    sigint(0);	/* Prepare to catch SIGINT */
}

int dspget(ft)
ft_t ft;
{
    int rval;

    if (abuf_cnt < 1) {
	abuf_cnt = read (fileno(ft->fp), (char *)audiobuf, abuf_size);
	if (abuf_cnt == 0) {
	    got_int = 1; /* Act like user said end record */
	    return(0);
	}
    }
    rval = *(audiobuf + (abuf_size-abuf_cnt));
    abuf_cnt--;
    return(rval);
}

/* Read short. */
unsigned short dsprshort(ft)
ft_t ft;
{
    unsigned short rval;
    if (abuf_cnt < 2) {
	abuf_cnt = read (fileno(ft->fp), (char *)audiobuf, abuf_size);
	if (abuf_cnt == 0) {
	    got_int = 1;  /* act like user said end recording */
	    return(0);
	}
    }
    rval = *((unsigned short *)(audiobuf + (abuf_size-abuf_cnt)));
    abuf_cnt -= 2;
    return(rval);
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

LONG ossdspread(ft, buf, len)
ft_t ft;
LONG *buf, len;
{
    register int datum;
    int done = 0;

    if (got_int)
	return(0); /* Return with length 0 read so program will end */

    switch(ft->info.size) {
    case BYTE:
	switch(ft->info.style) {
	case SIGN2:
	    while(done < len) {
		datum = dspget(ft);
		if (got_int || feof(ft->fp))
		    return(done);
		/* scale signed up to long's range */
		*buf++ = LEFT(datum, 24);
		done++;
	    }
	    return done;
	case UNSIGNED:
	    while(done < len) {
		datum = dspget(ft);
		if (got_int || feof(ft->fp))
		    return(done);
		/* Convert to unsigned */
		datum ^= 128;
		/* scale signed up to long's range */
		*buf++ = LEFT(datum, 24);
		done++;
	    }
	    return done;
	case ULAW:
	    /* grab table from Posk stuff */
	    while(done < len) {
		datum = dspget(ft);
		if (got_int || feof(ft->fp))
		    return(done);
		datum = st_ulaw_to_linear(datum);
		/* scale signed up to long's range */
		*buf++ = LEFT(datum, 16);
		done++;
	    }
	    return done;
	case ALAW:
	    while(done < len) {
		datum = dspget(ft);
		if (got_int || feof(ft->fp))
		    return(done);
		datum = st_Alaw_to_linear(datum);
		/* scale signed up to long's range */
		*buf++ = LEFT(datum, 16);
		done++;
	    }
	    return done;
	}
    case WORD:
	switch(ft->info.style) {
	case SIGN2:
	    while(done < len) {
		datum = dsprshort(ft);
		if (got_int || feof(ft->fp))
		    return(done);
		/* scale signed up to long's range */
		*buf++ = LEFT(datum, 16);
		done++;
	    }
	    return done;
	case UNSIGNED:
	    while(done < len) {
		datum = dsprshort(ft);
		if (got_int || feof(ft->fp))
		    return(done);
		/* Convert to unsigned */
		datum ^= 0x8000;
		/* scale signed up to long's range */
		*buf++ = LEFT(datum, 16);
		done++;
	    }
	    return done;
	case ULAW:
	    fail("No U-Law support for shorts");
	    return done;
	case ALAW:
	    fail("No A-Law support");
	    return done;
	}
    }
    fail("Drop through in ossdspread!");

    /* Return number of samples read */
    return(done);
}

/*
 * Do anything required when you stop reading samples.
 * Don't close input file!
 */
void ossdspstopread(ft)
ft_t ft;
{
}

void ossdspstartwrite(ft)
ft_t ft;
{
    int samplesize = 8, dsp_stereo;
    int tmp;

    if (ft->info.rate == 0.0) ft->info.rate = 8000;
    if (ft->info.size == -1) ft->info.size = BYTE;
    if (ft->info.size == BYTE) {
	samplesize = 8;
	if (ft->info.style == -1)
	    ft->info.style = UNSIGNED;
	if (ft->info.style != UNSIGNED) {
	    report("OSS driver only supports unsigned with bytes");
	    report("Forcing to unsigned");
	    ft->info.style = UNSIGNED;
	}
    }
    else if (ft->info.size == WORD) {
	samplesize = 16;
	if (ft->info.style == -1)
	    ft->info.style = SIGN2;
	if (ft->info.style != SIGN2) {
	    report("OSS driver only supports signed with words");
	    report("Forcing to signed linear");
	    ft->info.style = SIGN2;
	}
    }
    else {
        ft->info.size = WORD;
	ft->info.style = SIGN2;
	report("OSS driver only supports bytes and words");
	report("Forcing to signed linear word");
    }

    if (ft->info.channels == -1) ft->info.channels = 1;
    else if (ft->info.channels > 2) ft->info.channels = 2;

    ioctl(fileno(ft->fp), SNDCTL_DSP_RESET, 0);
    ioctl (fileno(ft->fp), SNDCTL_DSP_GETBLKSIZE, &abuf_size);
    if (abuf_size < 4 || abuf_size > 65536) {
	    fail("Invalid audio buffer size %d", abuf_size);
    }

    if ((audiobuf = malloc (abuf_size)) == NULL) {
	fail("Unable to allocate input/output buffer of size %d", abuf_size);
    }

    if (ioctl(fileno(ft->fp), SNDCTL_DSP_SYNC, NULL) < 0) {
	fail("Unable to sync dsp");
    }

    tmp = samplesize;
    ioctl(fileno(ft->fp), SNDCTL_DSP_SAMPLESIZE, &tmp);
    if (tmp != samplesize) {
	fail("Unable to set the sample size to %d", samplesize);
    }

    if (ft->info.channels == 2) dsp_stereo = 1;
    else dsp_stereo = 0;

    tmp = dsp_stereo;
    ioctl(fileno(ft->fp), SNDCTL_DSP_STEREO, &tmp);
    if (tmp != dsp_stereo) {
	ft->info.channels = 1;
	warn("Couldn't set to %s", dsp_stereo?  "stereo":"mono");
	dsp_stereo = 0;
    }

    tmp = ft->info.rate;
    ioctl (fileno(ft->fp), SNDCTL_DSP_SPEED, &tmp);
    if (ft->info.rate != tmp) {
	if (ft->info.rate - tmp > tmp/10 || tmp - ft->info.rate > tmp/10)
	    warn("Unable to set audio speed to %d (set to %d)",
		     ft->info.rate, tmp);
	ft->info.rate = tmp;
    }
}

void dspflush(ft)
ft_t ft;
{
    if (write (fileno(ft->fp), audiobuf, abuf_cnt) != abuf_cnt) {
        fail("Error writing to sound driver");
    }
    abuf_cnt = 0;
}

void dspput(ft,c)
ft_t ft;
int c;
{
    if (abuf_cnt > abuf_size-1) dspflush(ft);
    *(audiobuf + abuf_cnt) = c;
    abuf_cnt++;
}

/* Write short. */
void
dspshort(ft,ui)
ft_t ft;
unsigned short ui;
{
    if (abuf_cnt > abuf_size-2) dspflush(ft);
    *((unsigned short *)(audiobuf + abuf_cnt)) = ui;
    abuf_cnt += 2;
}

void ossdspwrite(ft, buf, len)
ft_t ft;
LONG *buf, len;
{
    register int datum;
    int done = 0;

    switch(ft->info.size) {
    case BYTE:
	switch(ft->info.style) {
	case SIGN2:
	    while(done < len) {
		/* scale signed up to long's range */
		datum = RIGHT(*buf++, 24);
		dspput(ft,datum);
		done++;
	    }
	    return;
	case UNSIGNED:
	    while(done < len) {
		/* scale signed up to long's range */
		datum = RIGHT(*buf++, 24);
		/* Convert to unsigned */
		datum ^= 128;
		dspput(ft,datum);
		done++;
	    }
	    return;
	case ULAW:
	    /* grab table from Posk stuff */
	    while(done < len) {
		/* scale signed up to long's range */
		datum = RIGHT(*buf++, 16);
		datum = st_linear_to_ulaw(datum);
		dspput(ft,datum);
		done++;
	    }
	    return;
	case ALAW:
	    while(done < len) {
		/* scale signed up to long's range */
		datum = RIGHT(*buf++, 16);
		/* round up to 12 bits of data */
		datum += 0x8;	/* + 0b1000 */
		datum = st_linear_to_Alaw(datum);
		dspput(ft,datum);
		done++;
	    }
	    return;
	}
    case WORD:
	switch(ft->info.style) {
	case SIGN2:
	    while(done < len) {
		/* scale signed up to long's range */
		datum = RIGHT(*buf++, 16);
		dspshort(ft,datum);
		done++;
	    }
	    return;
	case UNSIGNED:
	    while(done < len) {
		/* scale signed up to long's range */
		datum = RIGHT(*buf++, 16);
		/* Convert to unsigned */
		datum ^= 0x8000;
		dspshort(ft,datum);
		done++;
	    }
	    return;
	case ULAW:
	    fail("No U-Law support for shorts");
	    return;
	case ALAW:
	    fail("No A-Law support");
	    return;
	}
    }
    fail("Drop through in ossdspwrite!");
}

void ossdspstopwrite(ft)
ft_t ft;
{
    dspflush(ft);
}
#endif
