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

#include "st_i.h"

#if     defined(HAVE_OSS)

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
/* common r/w initialization code */
static int ossdspinit(ft_t ft)
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
        sampletype = (ST_IS_BIGENDIAN) ? AFMT_S16_BE : AFMT_S16_LE;
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
        sampletype = (ST_IS_BIGENDIAN) ? AFMT_S16_BE : AFMT_S16_LE;
        samplesize = 16;
        ft->info.size = ST_SIZE_WORD;
        ft->info.encoding = ST_ENCODING_SIGN2;
        st_report("OSS driver only supports bytes and words");
        st_report("Forcing to signed linear word");
    }

    if (ft->info.channels == -1) ft->info.channels = 1;
    else if (ft->info.channels > 2) ft->info.channels = 2;

    if (ioctl(fileno(ft->fp), SNDCTL_DSP_RESET, 0) < 0)
    {
        st_fail_errno(ft,ST_EOF,"Unable to reset OSS driver.  Possibly accessing an invalid file/device");
        return(ST_EOF);
    }

    /* Query the supported formats and find the best match
     */
    rc = ioctl(fileno(ft->fp), SNDCTL_DSP_GETFMTS, &tmp);
    if (rc == 0) {
        if ((tmp & sampletype) == 0)
        {
            /* is 16-bit supported? */
            if (samplesize == 16 && (tmp & (AFMT_S16_LE|AFMT_S16_BE)) == 0)
            {
                /* Must not like 16-bits, try 8-bits */
                ft->info.size = ST_SIZE_BYTE;
                ft->info.encoding = ST_ENCODING_UNSIGNED;
                st_report("OSS driver doesn't like signed words");
                st_report("Forcing to unsigned bytes");
                tmp = sampletype = AFMT_U8;
                samplesize = 8;
            }
            /* is 8-bit supported */
            else if (samplesize == 8 && (tmp & AFMT_U8) == 0)
            {
                ft->info.size = ST_SIZE_WORD;
                ft->info.encoding = ST_ENCODING_SIGN2;
                st_report("OSS driver doesn't like unsigned bytes");
                st_report("Forcing to signed words");
                sampletype = (ST_IS_BIGENDIAN) ? AFMT_S16_BE : AFMT_S16_LE;
                samplesize = 16;
            }
            /* determine which 16-bit format to use */
            if (samplesize == 16)
            {
                if ((tmp & sampletype) == 0)
                {
                    sampletype = (ST_IS_BIGENDIAN) ? AFMT_S16_LE : AFMT_S16_BE;
                    ft->swap = ft->swap ? 0 : 1;
                }
            }
        }
        tmp = sampletype;
        rc = ioctl(fileno(ft->fp), SNDCTL_DSP_SETFMT, &tmp);
    }
    /* Give up and exit */
    if (rc < 0 || tmp != sampletype)
    {
        st_fail_errno(ft,ST_EOF,"Unable to set the sample size to %d", samplesize);
        return (ST_EOF);
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
        (int)ft->info.rate != tmp) {
        /* If the rate the sound card is using is not within 1% of what
         * the user specified then override the user setting.
         * The only reason not to always override this is because of
         * clock-rounding problems. Sound cards will sometimes use
         * things like 44101 when you ask for 44100.  No need overriding
         * this and having strange output file rates for something that
         * we can't hear anyways.
         */
        if ((int)ft->info.rate - tmp > (tmp * .01) || 
            tmp - (int)ft->info.rate > (tmp * .01)) {
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
            st_fail_errno(ft,ST_EOF,"Invalid audio buffer size %d", ft->file.size);
            return (ST_EOF);
    }
    ft->file.count = 0;
    ft->file.pos = 0;
    ft->file.eof = 0;

    if ((ft->file.buf = (char *)malloc(ft->file.size)) == NULL) {
        st_fail_errno(ft,ST_EOF,"Unable to allocate input/output buffer of size %d", ft->file.size);
        return (ST_EOF);
    }

    if (ioctl(fileno(ft->fp), SNDCTL_DSP_SYNC, NULL) < 0) {
        st_fail_errno(ft,ST_EOF,"Unable to sync dsp");
        return (ST_EOF);
    }

    /* Change to non-buffered I/O */
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * ft->file.size);
    return(ST_SUCCESS);
}
/*
 * Do anything required before you start reading samples.
 * Read file header.
 *      Find out sampling rate,
 *      size and encoding of samples,
 *      mono/stereo/quad.
 */
int st_ossdspstartread(ft_t ft)
{
    int rc;
    rc = ossdspinit(ft);
    return rc;
}

int st_ossdspstartwrite(ft_t ft)
{
    return ossdspinit(ft);
}
#endif
