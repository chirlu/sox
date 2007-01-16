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

#ifdef HAVE_OSS

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
    st_fileinfo_t *file = (st_fileinfo_t *)ft->priv;

    if (ft->signal.rate == 0.0) ft->signal.rate = 8000;
    if (ft->signal.size == -1) ft->signal.size = ST_SIZE_BYTE;
    if (ft->signal.size == ST_SIZE_BYTE) {
        sampletype = AFMT_U8;
        samplesize = 8;
        if (ft->signal.encoding == ST_ENCODING_UNKNOWN)
            ft->signal.encoding = ST_ENCODING_UNSIGNED;
        if (ft->signal.encoding != ST_ENCODING_UNSIGNED) {
            st_report("OSS driver only supports unsigned with bytes");
            st_report("Forcing to unsigned");
            ft->signal.encoding = ST_ENCODING_UNSIGNED;
        }
    }
    else if (ft->signal.size == ST_SIZE_16BIT) {
        sampletype = (ST_IS_BIGENDIAN) ? AFMT_S16_BE : AFMT_S16_LE;
        samplesize = 16;
        if (ft->signal.encoding == ST_ENCODING_UNKNOWN)
            ft->signal.encoding = ST_ENCODING_SIGN2;
        if (ft->signal.encoding != ST_ENCODING_SIGN2) {
            st_report("OSS driver only supports signed with words");
            st_report("Forcing to signed linear");
            ft->signal.encoding = ST_ENCODING_SIGN2;
        }
    }
    else {
        sampletype = (ST_IS_BIGENDIAN) ? AFMT_S16_BE : AFMT_S16_LE;
        samplesize = 16;
        ft->signal.size = ST_SIZE_16BIT;
        ft->signal.encoding = ST_ENCODING_SIGN2;
        st_report("OSS driver only supports bytes and words");
        st_report("Forcing to signed linear word");
    }

    if (ft->signal.channels == 0) ft->signal.channels = 1;
    else if (ft->signal.channels > 2) ft->signal.channels = 2;

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
                ft->signal.size = ST_SIZE_BYTE;
                ft->signal.encoding = ST_ENCODING_UNSIGNED;
                st_report("OSS driver doesn't like signed words");
                st_report("Forcing to unsigned bytes");
                tmp = sampletype = AFMT_U8;
                samplesize = 8;
            }
            /* is 8-bit supported */
            else if (samplesize == 8 && (tmp & AFMT_U8) == 0)
            {
                ft->signal.size = ST_SIZE_16BIT;
                ft->signal.encoding = ST_ENCODING_SIGN2;
                st_report("OSS driver doesn't like unsigned bytes");
                st_report("Forcing to signed words");
                sampletype = (ST_IS_BIGENDIAN) ? AFMT_S16_BE : AFMT_S16_LE;
                samplesize = 16;
            }
            /* determine which 16-bit format to use */
            if (samplesize == 16 && (tmp & sampletype) == 0)
              sampletype = (ST_IS_BIGENDIAN) ? AFMT_S16_LE : AFMT_S16_BE;
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

    if (samplesize == 16)
      ft->signal.reverse_bytes = ST_IS_BIGENDIAN != (sampletype == AFMT_S16_BE);

    if (ft->signal.channels == 2) dsp_stereo = 1;
    else dsp_stereo = 0;

    tmp = dsp_stereo;
    if (ioctl(fileno(ft->fp), SNDCTL_DSP_STEREO, &tmp) < 0)
    {
        st_warn("Couldn't set to %s", dsp_stereo?  "stereo":"mono");
        dsp_stereo = 0;
    }

    if (tmp != dsp_stereo)
    {
        st_warn("Sound card appears to only support %d channels.  Overriding format", tmp+1);
        ft->signal.channels = tmp + 1;
    }

    tmp = ft->signal.rate;
    if (ioctl (fileno(ft->fp), SNDCTL_DSP_SPEED, &tmp) < 0 || 
        (int)ft->signal.rate != tmp) {
        /* If the rate the sound card is using is not within 1% of what
         * the user specified then override the user setting.
         * The only reason not to always override this is because of
         * clock-rounding problems. Sound cards will sometimes use
         * things like 44101 when you ask for 44100.  No need overriding
         * this and having strange output file rates for something that
         * we can't hear anyways.
         */
        if ((int)ft->signal.rate - tmp > (tmp * .01) || 
            tmp - (int)ft->signal.rate > (tmp * .01)) {
            st_warn("Unable to set audio speed to %d (set to %d)",
                     ft->signal.rate, tmp);
            ft->signal.rate = tmp;
        }
    }

    /* Find out block size to use last because the driver could compute
     * its size based on specific rates/formats.
     */
    file->size = 0;
    ioctl (fileno(ft->fp), SNDCTL_DSP_GETBLKSIZE, &file->size);
    if (file->size < 4 || file->size > 65536) {
            st_fail_errno(ft,ST_EOF,"Invalid audio buffer size %d", file->size);
            return (ST_EOF);
    }
    file->count = 0;
    file->pos = 0;
    file->eof = 0;
    file->buf = (char *)xmalloc(file->size);

    if (ioctl(fileno(ft->fp), SNDCTL_DSP_SYNC, NULL) < 0) {
        st_fail_errno(ft,ST_EOF,"Unable to sync dsp");
        return (ST_EOF);
    }

    /* Change to non-buffered I/O */
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * file->size);
    return(ST_SUCCESS);
}

/*
 * Do anything required before you start reading samples.
 * Read file header.
 *      Find out sampling rate,
 *      size and encoding of samples,
 *      mono/stereo/quad.
 */
static int st_ossdspstartread(ft_t ft)
{
    int rc;
    rc = ossdspinit(ft);
    return rc;
}

static int st_ossdspstartwrite(ft_t ft)
{
    return ossdspinit(ft);
}

/* OSS /dev/dsp player */
static const char *ossdspnames[] = {
  "ossdsp",
  NULL
};

static st_format_t st_ossdsp_format = {
  ossdspnames,
  NULL,
  ST_FILE_DEVICE,
  st_ossdspstartread,
  st_rawread,
  st_rawstopread,
  st_ossdspstartwrite,
  st_rawwrite,
  st_rawstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_ossdsp_format_fn(void)
{
    return &st_ossdsp_format;
}
#endif
