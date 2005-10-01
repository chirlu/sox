/*
 * Psion record.app format (format of files needed for Revo,Revo+,Mako in 
 * System/Alarms to provide new alarm sounds. Note that the file normally
 * has no extension, so I've made it .prc for now (Psion ReCord), until
 * somebody can come up with a better one. Also, I have absolutely no idea
 * what the header format is, I just looked at a few files, saw they all
 * had identical headers except for two places where the length words go,
 * so its very likely that most of this is wrong. It has worked for me,
 * however.
 * Based (heavily) on the wve.c format file. 
 * Hacked by Bert van Leeuwen (bert@e.co.za)
 */

#include "st_i.h"
#include "g72x.h"
#include <string.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

/* Magic numbers used in Psion audio files */
#define PSION_PRC_HDRSIZE   72

typedef struct prcpriv
    {
    uint32_t length;
    short padding;
    short repeats;
      /*For seeking */
        st_size_t dataStart;
    } *prc_t;

char header[]={
  0x37,0x00,0x00,0x10,0x6d,0x00,0x00,0x10,
  0x7e,0x00,0x00,0x10,0xcf,0xac,0x08,0x55,
  0x14,0x00,0x00,0x00,0x04,0x52,0x00,0x00,
  0x10,0x34,0x00,0x00,0x00,0x89,0x00,0x00,
  0x10,0x25,0x00,0x00,0x00,0x7e,0x00,0x00,
  0x10,0x2a,0x52,0x65,0x63,0x6f,0x72,0x64,
  0x2e,0x61,0x70,0x70
};

static void prcwriteheader(ft_t ft);

int st_prcseek(ft_t ft, st_size_t offset)
{
    prc_t prc = (prc_t ) ft->priv;
    st_size_t new_offset, channel_block, alignment;

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
    new_offset += prc->dataStart;

    return st_seeki(ft, new_offset, SEEK_SET);
}

int st_prcstartread(ft_t ft)
{
        prc_t p = (prc_t ) ft->priv;
        char head[sizeof(header)];
        int rc;

        uint16_t len;

        /* Needed for rawread() */
        rc = st_rawstartread(ft);
        if (rc)
            return rc;

        /* PRC is in little endian format.  Swap whats read in
         * on big endian machines.
         */
        if (ST_IS_LITTLEENDIAN)
        {
                ft->swap = ft->swap ? 1 : 0;
        }

        /* Check the header */
        st_readbuf(ft, head,1, sizeof(header));
        if (memcmp(head, header, sizeof(header))==0) {
                st_report("Found Psion record.app header");
        }
        else
        {
                st_fail_errno(ft,ST_EHDR,"Psion header doesn't start with the correct bytes\nTry the '.al' (A-law raw) file type with '-t al -r 8000 filename'");
                return (ST_EOF);
        }

        st_readw(ft, &(len));
        p->length=len;
        st_report("Found length=%d",len);

        /* dummy read rest */
        st_readbuf(ft, head,1,14+2+2);

        ft->info.encoding = ST_ENCODING_ALAW;
        ft->info.size = ST_SIZE_BYTE;

        if (ft->info.rate != 0)
            st_report("PRC must use 8000 sample rate.  Overriding");
        ft->info.rate = 8000;

        if (ft->info.channels != -1 && ft->info.channels != 1)
            st_report("PRC must only supports 1 channel.  Overriding");
        ft->info.channels = 1;

        p->dataStart = st_tell(ft);
        ft->length = p->length/ft->info.size;

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

int st_prcstartwrite(ft_t ft)
{
        prc_t p = (prc_t ) ft->priv;
        int rc;

        /* Needed for rawwrite() */
        rc = st_rawstartwrite(ft);
        if (rc)
            return ST_EOF;

        /* prc is in little endian format.  Swap whats read in
         * on big endian machines.
         */
        if (ST_IS_LITTLEENDIAN)
        {
                ft->swap = ft->swap ? 1 : 0;
        }

        p->length = 0;
        if (p->repeats == 0)
            p->repeats = 1;

        if (ft->info.rate != 0)
            st_report("PRC must use 8000 sample rate.  Overriding");

        if (ft->info.channels != -1 && ft->info.channels != 1)
            st_report("PRC must only supports 1 channel.  Overriding");

        ft->info.encoding = ST_ENCODING_ALAW;
        ft->info.size = ST_SIZE_BYTE;
        ft->info.rate = 8000;

        prcwriteheader(ft);
        return ST_SUCCESS;
}

st_ssize_t st_prcread(ft_t ft, st_sample_t *buf, st_ssize_t samp)
{
        return st_rawread(ft, buf, samp);
}

st_ssize_t st_prcwrite(ft_t ft, st_sample_t *buf, st_ssize_t samp)
{
        prc_t p = (prc_t ) ft->priv;
        p->length += samp * ft->info.size;
        st_report("length now = %d", p->length);
        return st_rawwrite(ft, buf, samp);
}

int st_prcstopwrite(ft_t ft)
{
        /* Call before seeking to flush buffer */
        st_rawstopwrite(ft);

        if (!ft->seekable)
        {
            st_warn("Header will be have invalid file length since file is not seekable");
            return ST_SUCCESS;
        }

        if (st_seeki(ft, 0L, 0) != 0)
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

  st_report("Final length=%d",p->length);
  memset(nullbuf,0,14);
  st_writebuf(ft, header, 1, sizeof(header));
  st_writew(ft, p->length);
  st_writebuf(ft, nullbuf,1,14);
  st_writew(ft, p->length);
  st_writebuf(ft, nullbuf,1,2);
}
