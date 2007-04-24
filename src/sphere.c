/*
 * NIST Sphere file format handler.
 *
 * August 7, 2000
 *
 * Copyright (C) 2000 Chris Bagwell (cbagwell@sprynet.com)
 *
 */

#include "sox_i.h"

#include <math.h>
#include <string.h>
#include <errno.h>

/* Private data for sphere file */
typedef struct spherestuff {
        char      shorten_check[4];
        sox_size_t numSamples;
} *sphere_t;

/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *      Find out sampling rate, 
 *      size and encoding of samples, 
 *      mono/stereo/quad.
 */
static int sox_spherestartread(ft_t ft) 
{
        sphere_t sphere = (sphere_t) ft->priv;
        int rc;
        char *buf;
        char fldname[64], fldtype[16], fldsval[128];
        int i;
        sox_size_t header_size, bytes_read;
        long rate;

        /* Needed for rawread() */
        rc = sox_rawstartread(ft);
        if (rc)
            return rc;

        /* Magic header */
        if (sox_reads(ft, fldname, 8) == SOX_EOF || strncmp(fldname, "NIST_1A", 7) != 0)
        {
            sox_fail_errno(ft,SOX_EHDR,"Sphere header does not begin with magic mord 'NIST_1A'");
            return(SOX_EOF);
        }

        if (sox_reads(ft, fldsval, 8) == SOX_EOF)
        {
            sox_fail_errno(ft,SOX_EHDR,"Error reading Sphere header");
            return(SOX_EOF);
        }

        /* Determine header size, and allocate a buffer large enough to hold it. */
        sscanf(fldsval, "%u", &header_size);
        buf = xmalloc(header_size);

        /* Skip what we have read so far */
        header_size -= 16;

        if (sox_reads(ft, buf, header_size) == SOX_EOF)
        {
            sox_fail_errno(ft,SOX_EHDR,"Error reading Sphere header");
            free(buf);
            return(SOX_EOF);
        }

        header_size -= (strlen(buf) + 1);

        while (strncmp(buf, "end_head", 8) != 0)
        {
            if (strncmp(buf, "sample_n_bytes", 14) == 0 && ft->signal.size == -1)
            {
                sscanf(buf, "%63s %15s %d", fldname, fldtype, &i);
                ft->signal.size = i;
            }
            if (strncmp(buf, "channel_count", 13) == 0 && 
                ft->signal.channels == 0)
            {
                sscanf(buf, "%63s %15s %d", fldname, fldtype, &i);
                ft->signal.channels = i;
            }
            if (strncmp(buf, "sample_coding", 13) == 0)
            {
                sscanf(buf, "%63s %15s %127s", fldname, fldtype, fldsval);
                /* Only bother looking for ulaw flag.  All others
                 * should be caught below by default PCM check
                 */
                if (ft->signal.encoding == SOX_ENCODING_UNKNOWN && 
                    strncmp(fldsval,"ulaw",4) == 0)
                {
                    ft->signal.encoding = SOX_ENCODING_ULAW;
                }
            }
            if (strncmp(buf, "sample_rate ", 12) == 0 &&
                ft->signal.rate == 0)
            {
                sscanf(buf, "%53s %15s %ld", fldname, fldtype, &rate);
                ft->signal.rate = rate;
            }
            if (strncmp(buf, "sample_byte_format", 18) == 0)
            {
                sscanf(buf, "%53s %15s %127s", fldname, fldtype, fldsval);
                if (strncmp(fldsval,"01",2) == 0)
                  ft->signal.reverse_bytes = SOX_IS_BIGENDIAN; /* Data is little endian. */
                else if (strncmp(fldsval,"10",2) == 0)
                  ft->signal.reverse_bytes = SOX_IS_LITTLEENDIAN; /* Data is big endian. */
            }

            if (sox_reads(ft, buf, header_size) == SOX_EOF)
            {
                sox_fail_errno(ft,SOX_EHDR,"Error reading Sphere header");
                free(buf);
                return(SOX_EOF);
            }

            header_size -= (strlen(buf) + 1);
        }

        if (ft->signal.size == -1)
            ft->signal.size = SOX_SIZE_BYTE;

        /* sample_coding is optional and is PCM if missing.
         * This means encoding is signed if size = word or
         * unsigned if size = byte.
         */
        if (ft->signal.encoding == SOX_ENCODING_UNKNOWN)
        {
            if (ft->signal.size == 1)
                ft->signal.encoding = SOX_ENCODING_UNSIGNED;
            else
                ft->signal.encoding = SOX_ENCODING_SIGN2;
        }

        while (header_size)
        {
            bytes_read = sox_readbuf(ft, buf, header_size);
            if (bytes_read == 0)
            {
                free(buf);
                return(SOX_EOF);
            }
            header_size -= bytes_read;
        }

        sphere->shorten_check[0] = 0;

        if (ft->seekable) {
          /* Check first four bytes of data to see if it's shorten compressed. */
          sox_ssize_t pos = sox_tell(ft);
          sox_reads(ft, sphere->shorten_check, 4);

          if (!strcmp(sphere->shorten_check,"ajkg")) {
            sox_fail_errno(ft, SOX_EFMT, "File uses shorten compression, cannot handle this.");
            free(buf);
            return(SOX_EOF);
          }

          /* Can't just seek -4, as sox_reads has read 1-4 bytes */
          sox_seeki(ft, pos, SEEK_SET); 
        }

        free(buf);
        return (SOX_SUCCESS);
}

static int sox_spherestartwrite(ft_t ft) 
{
    int rc;
    int x;
    sphere_t sphere = (sphere_t) ft->priv;

    if (!ft->seekable)
    {
        sox_fail_errno(ft,SOX_EOF,"File must be seekable for sphere file output");
        return (SOX_EOF);
    }

    switch (ft->signal.encoding)
    {
        case SOX_ENCODING_ULAW:
        case SOX_ENCODING_SIGN2:
        case SOX_ENCODING_UNSIGNED:
            break;
        default:
            sox_fail_errno(ft,SOX_EFMT,"SPHERE format only supports ulaw and PCM data.");
            return(SOX_EOF);
    }

    sphere->numSamples = 0;

    /* Needed for rawwrite */
    rc = sox_rawstartwrite(ft);
    if (rc)
        return rc;

    for (x = 0; x < 1024; x++)
    {
        sox_writeb(ft, ' ');
    }

    return(SOX_SUCCESS);
        
}

static sox_size_t sox_spherewrite(ft_t ft, const sox_ssample_t *buf, sox_size_t len) 
{
    sphere_t sphere = (sphere_t) ft->priv;

    sphere->numSamples += len; /* must later be divided by channels */
    return sox_rawwrite(ft, buf, len);
}

static int sox_spherestopwrite(ft_t ft) 
{
    int rc;
    char buf[128];
    sphere_t sphere = (sphere_t) ft->priv;
    long samples, rate;

    rc = sox_rawstopwrite(ft);
    if (rc)
        return rc;

    if (sox_seeki(ft, 0, 0) != 0)
    {
        sox_fail_errno(ft,errno,"Could not rewird output file to rewrite sphere header.");
        return (SOX_EOF);
    }

    sox_writes(ft, "NIST_1A\n");
    sox_writes(ft, "   1024\n");

    samples = sphere->numSamples/ft->signal.channels;
    sprintf(buf, "sample_count -i %ld\n", samples);
    sox_writes(ft, buf);

    sprintf(buf, "sample_n_bytes -i %d\n", ft->signal.size);
    sox_writes(ft, buf);

    sprintf(buf, "channel_count -i %d\n", ft->signal.channels);
    sox_writes(ft, buf);

    sprintf(buf, "sample_byte_format -s2 %s\n",
        ft->signal.reverse_bytes != SOX_IS_BIGENDIAN ? "10" : "01");
    sox_writes(ft, buf);

    rate = ft->signal.rate;
    sprintf(buf, "sample_rate -i %ld\n", rate);
    sox_writes(ft, buf);

    if (ft->signal.encoding == SOX_ENCODING_ULAW)
        sox_writes(ft, "sample_coding -s4 ulaw\n");
    else
        sox_writes(ft, "sample_coding -s3 pcm\n");

    sox_writes(ft, "end_head\n");

    return (SOX_SUCCESS);
}

/* NIST Sphere File */
static const char *spherenames[] = {
  "sph",
  "nist",
  NULL
};

static sox_format_t sox_sphere_format = {
  spherenames,
  0,
  sox_spherestartread,
  sox_rawread,
  sox_rawstopread,
  sox_spherestartwrite,
  sox_spherewrite,
  sox_spherestopwrite,
  sox_format_nothing_seek
};

const sox_format_t *sox_sphere_format_fn(void);

const sox_format_t *sox_sphere_format_fn(void)
{
    return &sox_sphere_format;
}
