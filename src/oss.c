#if	defined(OSS_PLAYER)
/*
 * Copyright 1997 Chris Bagwell And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Chris Bagwell And Sundry Contributors are not
 * responsible for the consequences of using this software.
 *
 * Direct to Open Sound System (OSS) sound driver
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
 *
 * Changes:
 *
 * Nov. 26, 1999 Stan Brooks <stabro@megsinet.net>
 *   Moved initialization code common to startread and startwrite
 *   into a single function ossdspinit().
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#endif
#ifdef HAVE_MACHINE_SOUNDCARD_H
#include <machine/soundcard.h>
#endif
#include <sys/ioctl.h>
#include "st.h"

/* common r/w initialization code */
static int ossdspinit(ft)
ft_t ft;
{
    int sampletype, samplesize, dsp_stereo;
    int tmp, rc;

    if (ft->info.rate == 0.0) ft->info.rate = 8000;
    if (ft->info.size == -1) ft->info.size = ST_SIZE_BYTE;
    if (ft->info.size == ST_SIZE_BYTE) {
	sampletype = AFMT_U8;
	samplesize = 8;
	if (ft->info.encoding == -1)
	    ft->info.encoding = ST_ENCODING_UNSIGNED;
	if (ft->info.encoding != ST_ENCODING_UNSIGNED) {
	    st_report("OSS driver only supports unsigned with bytes");
	    st_report("Forcing to unsigned");
	    ft->info.encoding = ST_ENCODING_UNSIGNED;
	}
    }
    else if (ft->info.size == ST_SIZE_WORD) {
	if (ST_IS_BIGENDIAN)
	    sampletype = AFMT_S16_BE;
	else
	    sampletype = AFMT_S16_LE;
	samplesize = 16;
	if (ft->info.encoding == -1)
	    ft->info.encoding = ST_ENCODING_SIGN2;
	if (ft->info.encoding != ST_ENCODING_SIGN2) {
	    st_report("OSS driver only supports signed with words");
	    st_report("Forcing to signed linear");
	    ft->info.encoding = ST_ENCODING_SIGN2;
	}
    }
    else {
        ft->info.size = ST_SIZE_WORD;
	ft->info.encoding = ST_ENCODING_SIGN2;
	st_report("OSS driver only supports bytes and words");
	st_report("Forcing to signed linear word");
    }

    if (ft->info.channels == -1) ft->info.channels = 1;
    else if (ft->info.channels > 2) ft->info.channels = 2;

    if (ioctl(fileno(ft->fp), SNDCTL_DSP_RESET, 0) < 0)
    {
	st_fail("Unable to reset OSS driver.  Possibly accessing an invalid file/device");
	return(ST_EOF);
    }

    if (ioctl(fileno(ft->fp), SNDCTL_DSP_SYNC, NULL) < 0) {
	st_fail("Unable to sync dsp");
	return (ST_EOF);
    }

#ifdef SNDCTL_DSP_SETFMT
    tmp = sampletype;
    rc = ioctl(fileno(ft->fp), SNDCTL_DSP_SAMPLESIZE, &tmp);

    /* If we are unable to set format type, then we should try a few
     * other format that we can handle.
     */
    if (rc < 0)
    {
	/* If using 16-bits, the sound card may just prefer to use
	 * an endian format different then the machine type.
	 * Try swaping data endian.
	 */
	if (sampletype == AFMT_S16_LE || sampletype == AFMT_S16_BE)
	{
	    if (sampletype == AFMT_S16_LE)
		sampletype = AFMT_S16_BE;
	    else
		sampletype = AFMT_S16_LE;

            rc = ioctl(fileno(ft->fp), SNDCTL_DSP_SAMPLESIZE, &tmp);
	    if (rc < 0)
	    {
		/* Must not like 16-bits, try 8-bits */
                ft->info.size = ST_SIZE_WORD;
	        ft->info.encoding = ST_ENCODING_SIGN2;
	        st_report("OSS driver doesn't like signed words");
	        st_report("Forcing to unsigned bytes");
	        sampletype = AFMT_U8;
                rc = ioctl(fileno(ft->fp), SNDCTL_DSP_SAMPLESIZE, &tmp);
	    }
	    else
		/* That was successful so store that we need to swap */
		ft->swap = ft->swap ? 0 : 1;

	}
	else
	{
            ft->info.size = ST_SIZE_WORD;
	    ft->info.encoding = ST_ENCODING_SIGNED2;
	    st_report("OSS driver doesn't like unsigned bytes");
	    st_report("Forcing to signed words");
            if (ST_IS_BIGENDIAN)
		sampletype = AFMT_S16_BE;
	    else
	        sampletype = AFMT_S16_LE;

            rc = ioctl(fileno(ft->fp), SNDCTL_DSP_SAMPLESIZE, &tmp);

	    /* If it doesn't like that, try swaping endians */
	    if (sampletype == AFMT_S16_LE || sampletype == AFMT_S16_BE)
	    {
	        if (sampletype == AFMT_S16_LE)
		    sampletype = AFMT_S16_BE;
	        else
		    sampletype = AFMT_S16_LE;

                rc = ioctl(fileno(ft->fp), SNDCTL_DSP_SAMPLESIZE, &tmp);
		if (rc >= 0)
		    /* That was successful so store that we need to swap */
		    ft->swap = ft->swap ? 0 : 1;
	    }

	}
	/* Give up and exit */
	if (rc < 0)
	{
	    st_fail("Unable to set the sample size to %d", samplesize);
	    return (ST_EOF);
	}
    }
#else
    /* Odd dumb interface */
    tmp = samplesize;
    if (ioctl(fileno(ft->fp), SNDCTL_DSP_SAMPLESIZE, &tmp) < 0)
    {
	st_fail("Unable to set the sample size to %d", samplesize);
	return (ST_EOF);
    }
#endif

    if (tmp != samplesize)
    {
	if (tmp == 16)
	{
	    st_warn("Sound card appears to only support singled word samples.  Overriding format");
	    ft->info.size = ST_SIZE_WORD;
	    ft->info.encoding = ST_ENCODING_SIGN2;
	}
	else if (tmp == 8)
	{
	    st_warn("Sound card appears to only support unsigned byte samples. Overriding format");
	    ft->info.size = ST_SIZE_BYTE;
	    ft->info.encoding = ST_ENCODING_UNSIGNED;
	}
    }

    if (ft->info.channels == 2) dsp_stereo = 1;
    else dsp_stereo = 0;

    tmp = dsp_stereo;
    if (ioctl(fileno(ft->fp), SNDCTL_DSP_STEREO, &tmp) < 0)
    {
	st_warn("Couldn't set to %s", dsp_stereo?  "stereo":"mono");
	dsp_stereo = 0;
    }

    if (tmp != dsp_stereo)
    {
	st_warn("Sound card appears to only support %d channels.  Overriding format\n", tmp+1);
	ft->info.channels = tmp + 1;
    }

    tmp = ft->info.rate;
    if (ioctl (fileno(ft->fp), SNDCTL_DSP_SPEED, &tmp) < 0 || 
    	ft->info.rate != tmp) {
	/* If the rate the sound card is using is not within 1% of what
	 * the user specified then override the user setting.
	 * The only reason not to always override this is because of
	 * clock-rounding problems. Sound cards will sometimes use
	 * things like 44101 when you ask for 44100.  No need overriding
	 * this and having strange output file rates for something that
	 * we can't hear anyways.
	 */
	if (ft->info.rate - tmp > (tmp * .01) || 
	    tmp - ft->info.rate > (tmp * .01)) {
	    st_warn("Unable to set audio speed to %d (set to %d)",
		     ft->info.rate, tmp);
	    ft->info.rate = tmp;
	}
    }

    /* Find out block size to use last because the driver could compute
     * its size based on specific rates/formats.
     */
    ft->file.size = 0;
    ioctl (fileno(ft->fp), SNDCTL_DSP_GETBLKSIZE, &ft->file.size);
    if (ft->file.size < 4 || ft->file.size > 65536) {
	    st_fail("Invalid audio buffer size %d", ft->file.size);
	    return (ST_EOF);
    }
    ft->file.count = 0;
    ft->file.pos = 0;
    ft->file.eof = 0;

    if ((ft->file.buf = malloc (ft->file.size)) == NULL) {
	st_fail("Unable to allocate input/output buffer of size %d", ft->file.size);
	return (ST_EOF);
    }

    /* Change to non-buffered I/O */
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * ft->file.size);
    return(ST_SUCCESS);
}
/*
 * Do anything required before you start reading samples.
 * Read file header.
 *	Find out sampling rate,
 *	size and encoding of samples,
 *	mono/stereo/quad.
 */
int st_ossdspstartread(ft)
ft_t ft;
{
    int rc;
    rc = ossdspinit(ft);
    sigintreg(ft);	/* Prepare to catch SIGINT */
    return rc;
}

int st_ossdspstartwrite(ft)
ft_t ft;
{
    return ossdspinit(ft);
}
#endif
