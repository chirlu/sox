/*
 * Sounder/Sndtool format handler: W V Neisius, February 1992
 *
 * June 28, 93: force output to mono.
 * 
 * March 3, 1999 - cbagwell@sprynet.com
 *   Forced extra comment fields to zero.
 */

#include "st_i.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>     /* For SEEK_* defines if not found in stdio */
#endif

/* Private data used by writer */
typedef struct sndpriv {
        st_size_t nsamples;
        st_size_t dataStart;
} *snd_t;

#ifndef SEEK_CUR
#define SEEK_CUR        1
#endif

static void  sndtwriteheader(ft_t ft, st_size_t nsamples);

int st_sndseek(ft_t ft, st_size_t offset) 
{
    st_size_t new_offset, channel_block, alignment;
    snd_t snd = (snd_t ) ft->priv;

    new_offset = offset * ft->info.size;
    /* Make sure request aligns to a channel block (ie left+right) */
    channel_block = ft->info.channels * ft->info.size;
    alignment = new_offset % channel_block;
    /* Most common mistaken is to compute something like
     * "skip everthing upto and including this sample" so
     * advance to next sample block in this case.
     */
    if (alignment != 0)
        new_offset += (channel_block - alignment);
    new_offset += snd->dataStart;

    return st_seeki(ft, new_offset, SEEK_SET);
}
/*======================================================================*/
/*                         SNDSTARTREAD                                */
/*======================================================================*/

int st_sndtstartread(ft_t ft)
{
        snd_t snd = (snd_t ) ft->priv;

        char buf[97];

        unsigned short rate;
        int rc;

        /* Needed for rawread() */
        rc = st_rawstartread(ft);
        if (rc)
            return rc;

        /* sndt is in little endian format so 
         * swap bytes on big endian machines.
         */
        if (ST_IS_BIGENDIAN)
        {
                ft->swap = ft->swap ? 0 : 1;
        }

        rate = 0;

        /* determine file type */
        /* if first 5 bytes == SOUND then this is probably a sndtool sound */
        /* if first word (16 bits) == 0 
         and second word is between 4000 & 25000 then this is sounder sound */
        /* otherwise, its probably raw, not handled here */

        if (st_readbuf(ft, buf, 1, 2) != 2)
        {
                st_fail_errno(ft,errno,"SND: unexpected EOF");
                return(ST_EOF);
        }
        if (strncmp(buf,"\0\0",2) == 0)
        {
        /* sounder */
        st_readw(ft, &rate);
        if (rate < 4000 || rate > 25000 )
        {
                st_fail_errno(ft,ST_EFMT,"SND: sample rate out of range");
                return(ST_EOF);
        }
        st_seeki(ft, 4, SEEK_CUR);
        }
        else
        {
        /* sndtool ? */
        st_readbuf(ft, &buf[2], 1, 6);
        if (strncmp(buf,"SOUND",5))
        {
                st_fail_errno(ft,ST_EFMT,"SND: unrecognized SND format");
                return(ST_EOF);
        }
        st_seeki(ft, 12, SEEK_CUR);
        st_readw(ft, &rate);
        st_seeki(ft, 6, SEEK_CUR);
        if (st_reads(ft, buf, 96) == ST_EOF)
        {
                st_fail_errno(ft,ST_EHDR,"SND: unexpected EOF in SND header");
                return(ST_EOF);
        }
        st_report("%s",buf);
        }

        ft->info.channels = 1;
        ft->info.rate = rate;
        ft->info.encoding = ST_ENCODING_UNSIGNED;
        ft->info.size = ST_SIZE_BYTE;

        snd->dataStart = st_tell(ft);
        ft->length = st_filelength(ft) - snd->dataStart;

        return (ST_SUCCESS);
}

/*======================================================================*/
/*                         SNDTSTARTWRITE                               */
/*======================================================================*/
int st_sndtstartwrite(ft_t ft)
{
        snd_t p = (snd_t ) ft->priv;
        int rc;

        /* Needed for rawwrite() */
        rc = st_rawstartwrite(ft);
        if (rc)
            return rc;

        /* sndt is in little endian format so
         * swap bytes on big endian machines
         */
        if (ST_IS_BIGENDIAN)
        {
                ft->swap = ft->swap ? 0 : 1;
        }

/* write header */
ft->info.channels = 1;
ft->info.encoding = ST_ENCODING_UNSIGNED;
ft->info.size = ST_SIZE_BYTE;
p->nsamples = 0;
sndtwriteheader(ft, 0);

return(ST_SUCCESS);
}

/*======================================================================*/
/*                         SNDRSTARTWRITE                               */
/*======================================================================*/
int st_sndrstartwrite(ft_t ft)
{
        int rc;

        /* Needed for rawread() */
        rc = st_rawstartread(ft);
        if (rc)
            return rc;

        /* sndr is in little endian format so
         * swap bytes on big endian machines
         */
        if (ST_IS_BIGENDIAN)
        {
                ft->swap = ft->swap ? 0 : 1;
        }

/* write header */
ft->info.channels = 1;
ft->info.encoding = ST_ENCODING_UNSIGNED;
ft->info.size = ST_SIZE_BYTE;

/* sounder header */
st_writew (ft,0); /* sample size code */
st_writew (ft,(int) ft->info.rate);     /* sample rate */
st_writew (ft,10);        /* volume */
st_writew (ft,4); /* shift */

return(ST_SUCCESS);
}

/*======================================================================*/
/*                         SNDTWRITE                                     */
/*======================================================================*/

st_ssize_t st_sndtwrite(ft_t ft, st_sample_t *buf, st_ssize_t len)
{
        snd_t p = (snd_t ) ft->priv;
        p->nsamples += len;
        return st_rawwrite(ft, buf, len);
}

/*======================================================================*/
/*                         SNDTSTOPWRITE                                */
/*======================================================================*/

int st_sndtstopwrite(ft_t ft)
{
        snd_t p = (snd_t ) ft->priv;
        int rc;

        /* Flush remaining buffer out */
        rc = st_rawstopwrite(ft);
        if (rc)
            return rc;

        /* fixup file sizes in header */
        if (st_seeki(ft, 0L, 0) != 0){
                st_fail_errno(ft,errno,"can't rewind output file to rewrite SND header");
                return ST_EOF;
        }
                
        sndtwriteheader(ft, p->nsamples);
                

        return(ST_SUCCESS);
}

/*======================================================================*/
/*                         SNDTWRITEHEADER                              */
/*======================================================================*/
static void sndtwriteheader(ft_t ft, st_size_t nsamples)
{
    char name_buf[97];

    /* sndtool header */
    st_writes(ft, "SOUND"); /* magic */
    st_writeb(ft, 0x1a);
    st_writew (ft,0);  /* hGSound */
    st_writedw (ft,nsamples);
    st_writedw (ft,0);
    st_writedw (ft,nsamples);
    st_writew (ft,(int) ft->info.rate);
    st_writew (ft,0);
    st_writew (ft,10);
    st_writew (ft,4);
    memset (name_buf, 0, 96);
    sprintf (name_buf,"%.62s - File created by Sound Exchange",ft->filename);
    st_writebuf(ft, name_buf, 1, 96);
}

