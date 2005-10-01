/*
 * Psion wve format, based on the au format file. Hacked by
 * Richard Caley (R.Caley@ed.ac.uk)
 */

#include "st_i.h"
#include "g72x.h"
#include <string.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

/* Magic numbers used in Psion audio files */
#define PSION_MAGIC     "ALawSoundFile**"
#define PSION_VERSION   ((short)3856)
#define PSION_INV_VERSION   ((short)4111)
#define PSION_HDRSIZE   32

typedef struct wvepriv
    {
    uint32_t length;
    short padding;
    short repeats;
/* For seeking */
        st_size_t dataStart;
    } *wve_t;

static void wvewriteheader(ft_t ft);

int st_wveseek(ft_t ft, st_size_t offset)
{
    int new_offset, channel_block, alignment;
    wve_t wve = (wve_t ) ft->priv;

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
    new_offset += wve->dataStart;

    return st_seeki(ft, offset, SEEK_SET);
}

int st_wvestartread(ft_t ft)
{
        wve_t p = (wve_t ) ft->priv;
        char magic[16];
        short version;
        int rc;

        uint16_t trash16;

        /* Needed for rawread() */
        rc = st_rawstartread(ft);
        if (rc)
            return rc;

        /* WVE is in big endian format.  Swap whats read in
         * on little endian machines.
         */
        if (ST_IS_LITTLEENDIAN)
        {
                ft->swap = ft->swap ? 0 : 1;
        }

        /* Check the magic word (null-terminated) */
        st_reads(ft, magic, 16);
        if (strncmp(magic, PSION_MAGIC, 15)==0) {
                st_report("Found Psion magic word");
        }
        else
        {
                st_fail_errno(ft,ST_EHDR,"Psion header doesn't start with magic word\nTry the '.al' file type with '-t al -r 8000 filename'");
                return (ST_EOF);
        }

        st_readw(ft, (unsigned short *)&version);

        /* Check for what type endian machine its read on */
        if (version == PSION_INV_VERSION)
        {
                /* This is probably left over from a time before
                 * testing for endianess was standardized.  Leaving since
                 * it doesn't hurt.
                 */
                ft->swap = ft->swap ? 0 : 1;
                st_report("Found inverted PSION magic word.  Swapping bytes.");
        }
        else if (version == PSION_VERSION)
        {
            st_report("Found PSION magic word");
        }
        else
        {
            st_fail_errno(ft,ST_EHDR,"Wrong version in Psion header");
            return(ST_EOF);
        }

        st_readdw(ft, &(p->length));

        st_readw(ft, (unsigned short *)&(p->padding));

        st_readw(ft, (unsigned short *)&(p->repeats));

        (void)st_readw(ft, (unsigned short *)&trash16);
        (void)st_readw(ft, (unsigned short *)&trash16);
        (void)st_readw(ft, (unsigned short *)&trash16);

        ft->info.encoding = ST_ENCODING_ALAW;
        ft->info.size = ST_SIZE_BYTE;

        if (ft->info.rate != 0)
            st_report("WVE must use 8000 sample rate.  Overriding");
        ft->info.rate = 8000;

        if (ft->info.channels != -1 && ft->info.channels != 1)
            st_report("WVE must only supports 1 channel.  Overriding");
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

int st_wvestartwrite(ft_t ft)
{
        wve_t p = (wve_t ) ft->priv;
        int rc;

        /* Needed for rawwrite() */
        rc = st_rawstartwrite(ft);
        if (rc)
            return ST_EOF;

        /* wve is in big endian format.  Swap whats read in
         * on little endian machines.
         */
        if (ST_IS_LITTLEENDIAN)
        {
                ft->swap = ft->swap ? 0 : 1;
        }

        p->length = 0;
        if (p->repeats == 0)
            p->repeats = 1;

        if (ft->info.rate != 0)
            st_report("WVE must use 8000 sample rate.  Overriding");

        if (ft->info.channels != -1 && ft->info.channels != 1)
            st_report("WVE must only supports 1 channel.  Overriding");

        ft->info.encoding = ST_ENCODING_ALAW;
        ft->info.size = ST_SIZE_BYTE;
        ft->info.rate = 8000;

        wvewriteheader(ft);
        return ST_SUCCESS;
}

st_ssize_t st_wveread(ft_t ft, st_sample_t *buf, st_ssize_t samp)
{
        return st_rawread(ft, buf, samp);
}

st_ssize_t st_wvewrite(ft_t ft, st_sample_t *buf, st_ssize_t samp)
{
        wve_t p = (wve_t ) ft->priv;
        p->length += samp * ft->info.size;
        return st_rawwrite(ft, buf, samp);
}

int st_wvestopwrite(ft_t ft)
{

        /* Call before seeking to flush buffer */
        return st_rawstopwrite(ft);

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
        wvewriteheader(ft);
}

static void wvewriteheader(ft_t ft)
{

    char magic[16];
    short version;
    short zero;
    wve_t p = (wve_t ) ft->priv;

    strcpy(magic,PSION_MAGIC);
    version=PSION_VERSION;
    zero=0;

    st_writes(ft, magic);
    /* Null terminate string */
    st_writeb(ft, 0);

    st_writew(ft, version);
    st_writedw(ft, p->length);
    st_writew(ft, p->padding);
    st_writew(ft, p->repeats);

    st_writew(ft, zero);
    st_writew(ft, zero);
    st_writew(ft, zero);
}
