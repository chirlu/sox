/*
 * August 7, 2000
 *
 * Copyright (C) 2000 Chris Bagwell (cbagwell@sprynet.com)
 *
 */

/*
 * NIST Sphere file format handler.
 */

#include "st_i.h"

#include <math.h>
#include <string.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
/* Private data for SKEL file */
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
int st_spherestartread(ft_t ft) 
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
        buf = (char *)malloc(header_size);
        if (buf == NULL)
        {
            st_fail_errno(ft,ST_ENOMEM,"Unable to allocate memory");
            return(ST_ENOMEM);
        }

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
            if (strncmp(buf, "sample_n_bytes", 14) == 0 && ft->info.size == -1)
            {
                sscanf(buf, "%s %s %d", fldname, fldtype, &i);
                ft->info.size = i;
            }
            if (strncmp(buf, "channel_count", 13) == 0 && 
                ft->info.channels == -1)
            {
                sscanf(buf, "%s %s %d", fldname, fldtype, &i);
                ft->info.channels = i;
            }
            if (strncmp(buf, "sample_coding", 13) == 0)
            {
                sscanf(buf, "%s %s %s", fldname, fldtype, fldsval);
                /* Only bother looking for ulaw flag.  All others
                 * should be caught below by default PCM check
                 */
                if (ft->info.encoding == -1 && 
                    strncmp(fldsval,"ulaw",4) == 0)
                {
                    ft->info.encoding = ST_ENCODING_ULAW;
                }
            }
            if (strncmp(buf, "sample_rate ", 12) == 0 &&
                ft->info.rate == 0)
            {
                sscanf(buf, "%s %s %ld", fldname, fldtype, &rate);
                ft->info.rate = rate;
            }
            if (strncmp(buf, "sample_byte_format", 18) == 0)
            {
                sscanf(buf, "%s %s %s", fldname, fldtype, fldsval);
                if (strncmp(fldsval,"01",2) == 0)
                {
                    /* Data is in little endian. */
                    if (ST_IS_BIGENDIAN)
                    {
                        ft->swap = ft->swap ? 0 : 1;
                    }
                }
                else if (strncmp(fldsval,"10",2) == 0)
                {
                    /* Data is in big endian. */
                    if (ST_IS_LITTLEENDIAN)
                    {
                        ft->swap = ft->swap ? 0 : 1;
                    }
                }
            }

            if (st_reads(ft, buf, header_size) == ST_EOF)
            {
                st_fail_errno(ft,ST_EHDR,"Error reading Sphere header");
                free(buf);
                return(ST_EOF);
            }

            header_size -= (strlen(buf) + 1);
        }

        if (ft->info.size == -1)
            ft->info.size = ST_SIZE_BYTE;

        /* sample_coding is optional and is PCM if missing.
         * This means encoding is signed if size = word or
         * unsigned if size = byte.
         */
        if (ft->info.encoding == -1)
        {
            if (ft->info.size == 1)
                ft->info.encoding = ST_ENCODING_UNSIGNED;
            else
                ft->info.encoding = ST_ENCODING_SIGN2;
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

        /* TODO: Check first four bytes of data to see if its shorten
         * compressed or not.  This data will need to be written to
         * buffer during first st_sphereread().
         */
#if 0
        st_reads(ft, sphere->shorten_check, 4);

        if (!strcmp(sphere->shorten_check,"ajkg"))
        {
            st_fail_errno(ft,ST_EFMT,"File uses shorten compression, can not handle this.\n");
            free(buf);
            return(ST_EOF);
        }
#endif

        free(buf);
        return (ST_SUCCESS);
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

st_ssize_t st_sphereread(ft_t ft, st_sample_t *buf, st_ssize_t len) 
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

int st_spherestartwrite(ft_t ft) 
{
    int rc;
    int x;
    sphere_t sphere = (sphere_t) ft->priv;

    if (!ft->seekable)
    {
        st_fail_errno(ft,ST_EOF,"File must be seekable for sphere file output");
        return (ST_EOF);
    }

    switch (ft->info.encoding)
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

st_ssize_t st_spherewrite(ft_t ft, st_sample_t *buf, st_ssize_t len) 
{
    sphere_t sphere = (sphere_t) ft->priv;

    sphere->numSamples += len; /* must later be divided by channels */
    return st_rawwrite(ft, buf, len);
}

int st_spherestopwrite(ft_t ft) 
{
    int rc;
    char buf[128];
    sphere_t sphere = (sphere_t) ft->priv;
    long samples, rate;

    rc = st_rawstopwrite(ft);
    if (rc)
        return rc;

    if (st_seeki(ft, 0L, 0) != 0)
    {
        st_fail_errno(ft,errno,"Could not rewird output file to rewrite sphere header.\n");
        return (ST_EOF);
    }

    st_writes(ft, "NIST_1A\n");
    st_writes(ft, "   1024\n");

    samples = sphere->numSamples/ft->info.channels;
    sprintf(buf, "sample_count -i %ld\n", samples);
    st_writes(ft, buf);

    sprintf(buf, "sample_n_bytes -i %d\n", ft->info.size);
    st_writes(ft, buf);

    sprintf(buf, "channel_count -i %d\n", ft->info.channels);
    st_writes(ft, buf);

    if (ft->swap)
    {
        sprintf(buf, "sample_byte_format -s2 %s\n", ST_IS_BIGENDIAN ? "01" : "10");
    }
    else
    {
        sprintf(buf, "sample_byte_format -s2 %s\n", ST_IS_BIGENDIAN ? "10" : "01");
    }
    st_writes(ft, buf);

    rate = ft->info.rate;
    sprintf(buf, "sample_rate -i %ld\n", rate);
    st_writes(ft, buf);

    if (ft->info.encoding == ST_ENCODING_ULAW)
        st_writes(ft, "sample_coding -s4 ulaw\n");
    else
        st_writes(ft, "sample_coding -s3 pcm\n");

    st_writes(ft, "end_head\n");

    return (ST_SUCCESS);
}
