/*
 * Psion Record format (format of files used for Revo,Revo+,Mako in 
 * System/Alarms to provide alarm sounds. Note that the file normally
 * has no extension, so I've made it .prc for now (Psion ReCord), until
 * somebody can come up with a better one.
 * Based (heavily) on the wve.c format file. 
 * Hacked by Bert van Leeuwen (bert@e.co.za)
 * Header check truncated to first 16 bytes (i.e. EPOC file header)
 * and other improvements by Reuben Thomas <rrt@sc3d.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, write to the Free Software
 * Foundation, Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301,
 * USA.  */

#include "st_i.h"
#include "g72x.h"
#include <string.h>
#include <errno.h>

typedef struct prcpriv
    {
    uint32_t length;
    short padding;
    short repeats;
      /*For seeking */
        st_size_t dataStart;
    } *prc_t;

/* 16 bytes header = 3 UIDs plus checksum, standard Symbian/EPOC file
   header */
static const char prc_header[]={
  '\x37','\x00','\x00','\x10','\x6d','\x00','\x00','\x10',
  '\x7e','\x00','\x00','\x10','\xcf','\xac','\x08','\x55'
};

int prc_checkheader(ft_t ft, char *head)
{
  st_readbuf(ft, head, 1, sizeof(prc_header));
  return memcmp(head, prc_header, sizeof(prc_header)) == 0;
}

static void prcwriteheader(ft_t ft);

static int st_prcseek(ft_t ft, st_size_t offset)
{
    prc_t prc = (prc_t ) ft->priv;
    st_size_t new_offset, channel_block, alignment;

    new_offset = offset * ft->signal.size;
    /* Make sure request aligns to a channel block (i.e. left+right) */
    channel_block = ft->signal.channels * ft->signal.size;
    alignment = new_offset % channel_block;
    /* Most common mistaken is to compute something like
     * "skip everthing upto and including this sample" so
     * advance to next sample block in this case.
     */
    if (alignment != 0)
        new_offset += (channel_block - alignment);
    new_offset += prc->dataStart;

    return st_seeki(ft, new_offset, SEEK_SET);
}

static int st_prcstartread(ft_t ft)
{
        prc_t p = (prc_t ) ft->priv;
        char head[sizeof(prc_header)];
        int rc;

        uint16_t len;

        /* Needed for rawread() */
        rc = st_rawstartread(ft);
        if (rc)
            return rc;

        /* Check the header */
        if (prc_checkheader(ft, head))
                st_debug("Found Psion Record header");
        else
        {
                st_fail_errno(ft,ST_EHDR,"Psion header doesn't start with the correct bytes\nTry the '.al' (A-law raw) file type with '-t al -r 8000 filename'");
                return (ST_EOF);
        }

        st_readw(ft, &(len));
        p->length=len;
        st_debug("Found length=%d",len);

        /* dummy read rest */
        st_readbuf(ft, head,1,14+2+2);

        ft->signal.encoding = ST_ENCODING_ALAW;
        ft->signal.size = ST_SIZE_BYTE;

        if (ft->signal.rate != 0)
            st_report("PRC must use 8000 sample rate.  Overriding");
        ft->signal.rate = 8000;

        if (ft->signal.channels != ST_ENCODING_UNKNOWN && ft->signal.channels != 0)
            st_report("PRC must only supports 1 channel.  Overriding");
        ft->signal.channels = 1;

        p->dataStart = st_tell(ft);
        ft->length = p->length/ft->signal.size;

        return (ST_SUCCESS);
}

/* When writing, the header is supposed to contain the number of
   data bytes written, unless it is written to a pipe.
   Since we don't know how many bytes will follow until we're done,
   we first write the header with an unspecified number of bytes,
   and at the end we rewind the file and write the header again
   with the right size.  This only works if the file is seekable;
   if it is not, the unspecified size remains in the header
   (this is illegal). */

static int st_prcstartwrite(ft_t ft)
{
        prc_t p = (prc_t ) ft->priv;
        int rc;

        /* Needed for rawwrite() */
        rc = st_rawstartwrite(ft);
        if (rc)
            return ST_EOF;

        p->length = 0;
        if (p->repeats == 0)
            p->repeats = 1;

        if (ft->signal.rate != 0)
            st_report("PRC must use 8000 sample rate.  Overriding");

        if (ft->signal.channels != ST_ENCODING_UNKNOWN && ft->signal.channels != 0)
            st_report("PRC must only supports 1 channel.  Overriding");

        ft->signal.encoding = ST_ENCODING_ALAW;
        ft->signal.size = ST_SIZE_BYTE;
        ft->signal.rate = 8000;

        prcwriteheader(ft);
        return ST_SUCCESS;
}

static st_size_t st_prcwrite(ft_t ft, const st_sample_t *buf, st_size_t samp)
{
        prc_t p = (prc_t ) ft->priv;
        p->length += samp * ft->signal.size;
        st_debug("length now = %d", p->length);
        return st_rawwrite(ft, buf, samp);
}

static int st_prcstopwrite(ft_t ft)
{
        /* Call before seeking to flush buffer */
        st_rawstopwrite(ft);

        if (!ft->seekable)
        {
            st_warn("Header will be have invalid file length since file is not seekable");
            return ST_SUCCESS;
        }

        if (st_seeki(ft, 0, 0) != 0)
        {
                st_fail_errno(ft,errno,"Can't rewind output file to rewrite Psion header.");
                return(ST_EOF);
        }
        prcwriteheader(ft);
        return ST_SUCCESS;
}

static void prcwriteheader(ft_t ft)
{
  char nullbuf[15];
  prc_t p = (prc_t ) ft->priv;

  st_debug("Final length=%d",p->length);
  memset(nullbuf,0,14);
  st_writebuf(ft, prc_header, 1, sizeof(prc_header));
  st_writew(ft, p->length);
  st_writebuf(ft, nullbuf,1,14);
  st_writew(ft, p->length);
  st_writebuf(ft, nullbuf,1,2);
}

/* Psion .prc */
static const char *prcnames[] = {
  "prc",
  NULL
};

static st_format_t st_prc_format = {
  prcnames,
  NULL,
  ST_FILE_SEEK | ST_FILE_BIG_END,
  st_prcstartread,
  st_rawread,
  st_rawstopread,
  st_prcstartwrite,
  st_prcwrite,
  st_prcstopwrite,
  st_prcseek
};

const st_format_t *st_prc_format_fn(void)
{
    return &st_prc_format;
}
