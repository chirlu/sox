/*
 * NIST Sphere file format handler.
 *
 * August 7, 2000
 *
 * Copyright (C) 2000 Chris Bagwell (cbagwell@sprynet.com)
 *
 */

#include "st_i.h"

#include <math.h>
#include <string.h>
#include <errno.h>

/* Private data for sphere file */
typedef struct spherestuff {
        char      shorten_check[4];
        st_size_t numSamples;
} *sphere_t;

/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *      Find out sampling rate, 
 *      size and encoding of samples, 
 *      mono/stereo/quad.
 */
static int st_spherestartread(ft_t ft) 
{
        sphere_t sphere = (sphere_t) ft->priv;
        int rc;
        char *buf;
        char fldname[64], fldtype[16], fldsval[128];
        int i;
        int header_size, bytes_read;
        long rate;

        /* Needed for rawread() */
        rc = st_rawstartread(ft);
        if (rc)
            return rc;

        /* Magic header */
        if (st_reads(ft, fldname, 8) == ST_EOF || strncmp(fldname, "NIST_1A", 7) != 0)
        {
            st_fail_errno(ft,ST_EHDR,"Sphere header does not begin with magic mord 'NIST_1A'");
            return(ST_EOF);
        }

        if (st_reads(ft, fldsval, 8) == ST_EOF)
        {
            st_fail_errno(ft,ST_EHDR,"Error reading Sphere header");
            return(ST_EOF);
        }

        /* Determine header size, and allocate a buffer large enough to hold it. */
        sscanf(fldsval, "%d", &header_size);
        buf = (char *)xmalloc(header_size);

        /* Skip what we have read so far */
        header_size -= 16;

        if (st_reads(ft, buf, header_size) == ST_EOF)
        {
            st_fail_errno(ft,ST_EHDR,"Error reading Sphere header");
            free(buf);
            return(ST_EOF);
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
                if (ft->signal.encoding == ST_ENCODING_UNKNOWN && 
                    strncmp(fldsval,"ulaw",4) == 0)
                {
                    ft->signal.encoding = ST_ENCODING_ULAW;
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
                  ft->signal.reverse_bytes = ST_IS_BIGENDIAN; /* Data is little endian. */
                else if (strncmp(fldsval,"10",2) == 0)
                  ft->signal.reverse_bytes = ST_IS_LITTLEENDIAN; /* Data is big endian. */
            }

            if (st_reads(ft, buf, header_size) == ST_EOF)
            {
                st_fail_errno(ft,ST_EHDR,"Error reading Sphere header");
                free(buf);
                return(ST_EOF);
            }

            header_size -= (strlen(buf) + 1);
        }

        if (ft->signal.size == -1)
            ft->signal.size = ST_SIZE_BYTE;

        /* sample_coding is optional and is PCM if missing.
         * This means encoding is signed if size = word or
         * unsigned if size = byte.
         */
        if (ft->signal.encoding == ST_ENCODING_UNKNOWN)
        {
            if (ft->signal.size == 1)
                ft->signal.encoding = ST_ENCODING_UNSIGNED;
            else
                ft->signal.encoding = ST_ENCODING_SIGN2;
        }

        while (header_size)
        {
            bytes_read = st_readbuf(ft, buf, ST_SIZE_BYTE, header_size);
            if (bytes_read == 0)
            {
                free(buf);
                return(ST_EOF);
            }
            header_size -= bytes_read;
        }

        sphere->shorten_check[0] = 0;

        /* Check first four bytes of data to see if it's shorten
         * compressed or not.
         */
        st_reads(ft, sphere->shorten_check, 4);

        if (!strcmp(sphere->shorten_check,"ajkg"))
        {
            st_fail_errno(ft,ST_EFMT,"File uses shorten compression, cannot handle this.");
            free(buf);
            return(ST_EOF);
        }

        free(buf);
        return (ST_SUCCESS);
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

static st_size_t st_sphereread(ft_t ft, st_sample_t *buf, st_size_t len) 
{
    sphere_t sphere = (sphere_t) ft->priv;

    if (sphere->shorten_check[0])
    {
        /* TODO: put these 4 bytes into the buffer.  Requires
         * knowing how to process ulaw and all version of PCM data size.
         */
        sphere->shorten_check[0] = 0;
    }
    return st_rawread(ft, buf, len);
}

static int st_spherestartwrite(ft_t ft) 
{
    int rc;
    int x;
    sphere_t sphere = (sphere_t) ft->priv;

    if (!ft->seekable)
    {
        st_fail_errno(ft,ST_EOF,"File must be seekable for sphere file output");
        return (ST_EOF);
    }

    switch (ft->signal.encoding)
    {
        case ST_ENCODING_ULAW:
        case ST_ENCODING_SIGN2:
        case ST_ENCODING_UNSIGNED:
            break;
        default:
            st_fail_errno(ft,ST_EFMT,"SPHERE format only supports ulaw and PCM data.");
            return(ST_EOF);
    }

    sphere->numSamples = 0;

    /* Needed for rawwrite */
    rc = st_rawstartwrite(ft);
    if (rc)
        return rc;

    for (x = 0; x < 1024; x++)
    {
        st_writeb(ft, ' ');
    }

    return(ST_SUCCESS);
        
}

static st_size_t st_spherewrite(ft_t ft, const st_sample_t *buf, st_size_t len) 
{
    sphere_t sphere = (sphere_t) ft->priv;

    sphere->numSamples += len; /* must later be divided by channels */
    return st_rawwrite(ft, buf, len);
}

static int st_spherestopwrite(ft_t ft) 
{
    int rc;
    char buf[128];
    sphere_t sphere = (sphere_t) ft->priv;
    long samples, rate;

    rc = st_rawstopwrite(ft);
    if (rc)
        return rc;

    if (st_seeki(ft, 0, 0) != 0)
    {
        st_fail_errno(ft,errno,"Could not rewird output file to rewrite sphere header.");
        return (ST_EOF);
    }

    st_writes(ft, "NIST_1A\n");
    st_writes(ft, "   1024\n");

    samples = sphere->numSamples/ft->signal.channels;
    sprintf(buf, "sample_count -i %ld\n", samples);
    st_writes(ft, buf);

    sprintf(buf, "sample_n_bytes -i %d\n", ft->signal.size);
    st_writes(ft, buf);

    sprintf(buf, "channel_count -i %d\n", ft->signal.channels);
    st_writes(ft, buf);

    sprintf(buf, "sample_byte_format -s2 %s\n",
        ft->signal.reverse_bytes != ST_IS_BIGENDIAN ? "10" : "01");
    st_writes(ft, buf);

    rate = ft->signal.rate;
    sprintf(buf, "sample_rate -i %ld\n", rate);
    st_writes(ft, buf);

    if (ft->signal.encoding == ST_ENCODING_ULAW)
        st_writes(ft, "sample_coding -s4 ulaw\n");
    else
        st_writes(ft, "sample_coding -s3 pcm\n");

    st_writes(ft, "end_head\n");

    return (ST_SUCCESS);
}

/* NIST Sphere File */
static const char *spherenames[] = {
  "sph",
  "nist",
  NULL
};

static st_format_t st_sphere_format = {
  spherenames,
  NULL,
  0,
  st_spherestartread,
  st_sphereread,
  st_rawstopread,
  st_spherestartwrite,
  st_spherewrite,
  st_spherestopwrite,
  st_format_nothing_seek
};

const st_format_t *st_sphere_format_fn(void)
{
    return &st_sphere_format;
}
