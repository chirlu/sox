/* Copyright 1997 Chris Bagwell And Sundry Contributors
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

#include "sox_i.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#ifdef HAVE_SYS_SOUNDCARD_H
  #include <sys/soundcard.h>
#endif
#ifdef HAVE_MACHINE_SOUNDCARD_H
  #include <machine/soundcard.h>
#endif

/* these appear in the sys/soundcard.h of OSS 4.x, and in Linux's
 * sound/core/oss/pcm_oss.c (2.6.24 and later), but are typically
 * not included in system header files.
 */
#ifndef AFMT_S32_LE
#define AFMT_S32_LE 0x00001000
#endif
#ifndef AFMT_S32_BE
#define AFMT_S32_BE 0x00002000
#endif

#include <sys/ioctl.h>

typedef sox_fileinfo_t priv_t;
/* common r/w initialization code */
static int ossinit(sox_format_t * ft)
{
    int sampletype, samplesize, dsp_stereo;
    int tmp, rc;
    priv_t *file = (priv_t *)ft->priv;
    int fn = fileno((FILE *)(ft->fp));

    if (ft->encoding.bits_per_sample == 8) {
        sampletype = AFMT_U8;
        samplesize = 8;
        if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN)
            ft->encoding.encoding = SOX_ENCODING_UNSIGNED;
        if (ft->encoding.encoding != SOX_ENCODING_UNSIGNED) {
            lsx_report("OSS driver only supports unsigned with bytes");
            lsx_report("Forcing to unsigned");
            ft->encoding.encoding = SOX_ENCODING_UNSIGNED;
        }
    }
    else if (ft->encoding.bits_per_sample == 16) {
        /* Attempt to use endian that user specified */
        if (ft->encoding.reverse_bytes)
            sampletype = (MACHINE_IS_BIGENDIAN) ? AFMT_S16_LE : AFMT_S16_BE;
        else
            sampletype = (MACHINE_IS_BIGENDIAN) ? AFMT_S16_BE : AFMT_S16_LE;
        samplesize = 16;
        if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN)
            ft->encoding.encoding = SOX_ENCODING_SIGN2;
        if (ft->encoding.encoding != SOX_ENCODING_SIGN2) {
            lsx_report("OSS driver only supports signed with words");
            lsx_report("Forcing to signed linear");
            ft->encoding.encoding = SOX_ENCODING_SIGN2;
        }
    }
    else if (ft->encoding.bits_per_sample == 32) {
	/* Attempt to use endian that user specified */
	if (ft->encoding.reverse_bytes)
	    sampletype = (MACHINE_IS_BIGENDIAN) ? AFMT_S32_LE : AFMT_S32_BE;
	else
	    sampletype = (MACHINE_IS_BIGENDIAN) ? AFMT_S32_BE : AFMT_S32_LE;
	samplesize = 32;
	if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN)
	    ft->encoding.encoding = SOX_ENCODING_SIGN2;
	if (ft->encoding.encoding != SOX_ENCODING_SIGN2) {
	    lsx_report("OSS driver only supports signed with words");
	    lsx_report("Forcing to signed linear");
	    ft->encoding.encoding = SOX_ENCODING_SIGN2;
	}
    }
    else {
        /* Attempt to use endian that user specified */
        if (ft->encoding.reverse_bytes)
            sampletype = (MACHINE_IS_BIGENDIAN) ? AFMT_S16_LE : AFMT_S16_BE;
        else
            sampletype = (MACHINE_IS_BIGENDIAN) ? AFMT_S16_BE : AFMT_S16_LE;
        samplesize = 16;
        ft->encoding.bits_per_sample = 16;
        ft->encoding.encoding = SOX_ENCODING_SIGN2;
        lsx_report("OSS driver only supports bytes and words");
        lsx_report("Forcing to signed linear word");
    }

    if (ft->signal.channels > 2) ft->signal.channels = 2;

    if (ioctl(fn, (size_t) SNDCTL_DSP_RESET, 0) < 0)
    {
        lsx_fail_errno(ft,SOX_EOF,"Unable to reset OSS driver.  Possibly accessing an invalid file/device");
        return(SOX_EOF);
    }

    /* Query the supported formats and find the best match
     */
    rc = ioctl(fn, SNDCTL_DSP_GETFMTS, &tmp);
    if (rc == 0) {
        if ((tmp & sampletype) == 0)
        {
            /* is 16-bit supported? */
            if (samplesize == 16 && (tmp & (AFMT_S16_LE|AFMT_S16_BE)) == 0)
            {
                /* Must not like 16-bits, try 8-bits */
                ft->encoding.bits_per_sample = 8;
                ft->encoding.encoding = SOX_ENCODING_UNSIGNED;
                lsx_report("OSS driver doesn't like signed words");
                lsx_report("Forcing to unsigned bytes");
                tmp = sampletype = AFMT_U8;
                samplesize = 8;
            }
            /* is 8-bit supported */
            else if (samplesize == 8 && (tmp & AFMT_U8) == 0)
            {
                ft->encoding.bits_per_sample = 16;
                ft->encoding.encoding = SOX_ENCODING_SIGN2;
                lsx_report("OSS driver doesn't like unsigned bytes");
                lsx_report("Forcing to signed words");
                sampletype = (MACHINE_IS_BIGENDIAN) ? AFMT_S16_BE : AFMT_S16_LE;
                samplesize = 16;
            }
            /* determine which 16-bit format to use */
            if (samplesize == 16 && (tmp & sampletype) == 0)
            {
                /* Either user requested something not supported
                 * or hardware doesn't support machine endian.
                 * Force to opposite as the above test showed
                 * it supports at least one of the two endians.
                 */
                sampletype = (sampletype == AFMT_S16_BE) ? AFMT_S16_LE : AFMT_S16_BE;
                ft->encoding.reverse_bytes = !ft->encoding.reverse_bytes;
            }

        }
        tmp = sampletype;
        rc = ioctl(fn, SNDCTL_DSP_SETFMT, &tmp);
    }
    /* Give up and exit */
    if (rc < 0 || tmp != sampletype)
    {
        lsx_fail_errno(ft,SOX_EOF,"Unable to set the sample size to %d", samplesize);
        return (SOX_EOF);
    }

    if (ft->signal.channels == 2) dsp_stereo = 1;
    else dsp_stereo = 0;

    tmp = dsp_stereo;
    if (ioctl(fn, SNDCTL_DSP_STEREO, &tmp) < 0)
    {
        lsx_warn("Couldn't set to %s", dsp_stereo?  "stereo":"mono");
        dsp_stereo = 0;
    }

    if (tmp != dsp_stereo)
        ft->signal.channels = tmp + 1;

    tmp = ft->signal.rate;
    if (ioctl(fn, SNDCTL_DSP_SPEED, &tmp) < 0 ||
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
            tmp - (int)ft->signal.rate > (tmp * .01))
            ft->signal.rate = tmp;
    }

    /* Find out block size to use last because the driver could compute
     * its size based on specific rates/formats.
     */
    file->size = 0;
    ioctl(fn, SNDCTL_DSP_GETBLKSIZE, &file->size);
    if (file->size < 4 || file->size > 65536) {
            lsx_fail_errno(ft,SOX_EOF,"Invalid audio buffer size %" PRIuPTR, file->size);
            return (SOX_EOF);
    }
    file->count = 0;
    file->pos = 0;
    file->buf = lsx_malloc(file->size);

    if (ioctl(fn, (size_t) SNDCTL_DSP_SYNC, NULL) < 0) {
        lsx_fail_errno(ft,SOX_EOF,"Unable to sync dsp");
        return (SOX_EOF);
    }

    /* Change to non-buffered I/O */
    setvbuf(ft->fp, NULL, _IONBF, sizeof(char) * file->size);
    return(SOX_SUCCESS);
}

LSX_FORMAT_HANDLER(oss)
{
  static char const * const names[] = {"ossdsp", "oss", NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_SIGN2, 32, 16, 0,
    SOX_ENCODING_UNSIGNED, 8, 0,
    0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "Open Sound Sytem device driver for unix-like systems",
    names, SOX_FILE_DEVICE,
    ossinit, lsx_rawread, lsx_rawstopread,
    ossinit, lsx_rawwrite, lsx_rawstopwrite,
    NULL, write_encodings, NULL, sizeof(priv_t)
  };
  return &handler;
}
