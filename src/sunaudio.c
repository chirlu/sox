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
int s;
{
    fprintf(stderr,"Got SIGINT\n");
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
void sunstartread(ft)
ft_t ft;
{
    int samplesize, encoding;
    audio_info_t audio_if;

    /* Hard code for now. */
    abuf_size = 1024;
    if ((audiobuf = malloc (abuf_size)) == NULL) {
	fail("unable to allocate input buffer of size %d", abuf_size);
    }

    if (ft->info.rate == 0.0) ft->info.rate = 8000;
    if (ft->info.size == -1) ft->info.size = BYTE;
    if (ft->info.style == -1) ft->info.style = ULAW;

    if (ft->info.size == BYTE) {
	samplesize = 8;
	if (ft->info.style != ULAW &&
	    ft->info.style != ALAW &&
	    ft->info.style != SIGN2) {
	    fail("Sun Audio driver only supports ULAW, ALAW, and Signed Linear for bytes.");
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

LONG sunread(ft, buf, len)
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
		{
		    fprintf(stderr,"Returning early\n");
		    return(done);
		}
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
    fail("Drop through in sunread!");

    /* Return number of samples read */
    return(done);
}

/*
 * Do anything required when you stop reading samples.
 * Don't close input file!
 */
void sunstopread(ft)
ft_t ft;
{
}

void sunstartwrite(ft)
ft_t ft;
{
    int samplesize, encoding;
    audio_info_t audio_if;

    /* Hard code for now. */
    abuf_size = 1024;
    if ((audiobuf = malloc (abuf_size)) == NULL) {
	fail("unable to allocate output buffer of size %d", abuf_size);
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

void sunwrite(ft, buf, len)
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
	    while(done < len) {
		/* scale signed up to long's range */
		datum = (int) RIGHT(*buf++, 16);
		/* round up to 12 bits of data */
		datum += 0x8;	/* + 0b1000 */
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
	    fail("No U-Law support for words");
	    return;
	case ALAW:
	    fail("No A-Law support for words");
	    return;
	}
    }

    fail("Drop through in sunwrite!");
}

void sunstopwrite(ft)
ft_t ft;
{
    dspflush(ft);
}
#endif
