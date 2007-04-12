/*
 * Psion wve format, based on the au format file. Hacked by
 * Richard Caley (R.Caley@ed.ac.uk)
 */

#include "sox_i.h"
#include <string.h>
#include <errno.h>

/* Magic numbers used in Psion audio files */
#define PSION_MAGIC     "ALawSoundFile**"
#define PSION_VERSION   ((short)3856)
#define PSION_HDRSIZE   32

typedef struct wvepriv
    {
    uint32_t length;
    short padding;
    short repeats;
/* For seeking */
        sox_size_t dataStart;
    } *wve_t;

static void wvewriteheader(ft_t ft);

static int sox_wveseek(ft_t ft, sox_size_t offset)
{
    int new_offset, channel_block, alignment;
    wve_t wve = (wve_t)ft->priv;

    new_offset = offset * ft->signal.size;
    /* Make sure request aligns to a channel block (i.e. left+right) */
    channel_block = ft->signal.channels * ft->signal.size;
    alignment = new_offset % channel_block;
    /* Most common mistake is to compute something like
     * "skip everthing up to and including this sample" so
     * advance to next sample block in this case.
     */
    if (alignment != 0)
        new_offset += (channel_block - alignment);
    new_offset += wve->dataStart;

    return sox_seeki(ft, offset, SEEK_SET);
}

static int sox_wvestartread(ft_t ft)
{
        wve_t p = (wve_t)ft->priv;
        char magic[16];
        short version;
        int rc;

        uint16_t trash16;

        /* Needed for rawread() */
        rc = sox_rawstartread(ft);
        if (rc)
            return rc;

        /* Check the magic word (null-terminated) */
        sox_reads(ft, magic, 16);
        if (strncmp(magic, PSION_MAGIC, 15)==0) {
                sox_debug("Found Psion magic word");
        }
        else
        {
                sox_fail_errno(ft,SOX_EHDR,"Psion header doesn't start with magic word\nTry the '.al' file type with '-t al -r 8000 filename'");
                return (SOX_EOF);
        }

        sox_readw(ft, (unsigned short *)&version);

        /* Check magic version */
        if (version == PSION_VERSION)
            sox_debug("Found Psion magic word");
        else
        {
            sox_fail_errno(ft,SOX_EHDR,"Wrong version in Psion header");
            return(SOX_EOF);
        }

        sox_readdw(ft, &(p->length));

        sox_readw(ft, (unsigned short *)&(p->padding));

        sox_readw(ft, (unsigned short *)&(p->repeats));

        sox_readw(ft, (unsigned short *)&trash16);
        sox_readw(ft, (unsigned short *)&trash16);
        sox_readw(ft, (unsigned short *)&trash16);

        ft->signal.encoding = SOX_ENCODING_ALAW;
        ft->signal.size = SOX_SIZE_BYTE;

        if (ft->signal.rate != 0)
            sox_report("WVE must use 8000 sample rate.  Overriding");
        ft->signal.rate = 8000;

        if (ft->signal.channels != SOX_ENCODING_UNKNOWN && ft->signal.channels != 1)
            sox_report("WVE must only supports 1 channel.  Overriding");
        ft->signal.channels = 1;

        p->dataStart = sox_tell(ft);
        ft->length = p->length/ft->signal.size;

        return (SOX_SUCCESS);
}

/* When writing, the header is supposed to contain the number of
   data bytes written, unless it is written to a pipe.
   Since we don't know how many bytes will follow until we're done,
   we first write the header with an unspecified number of bytes,
   and at the end we rewind the file and write the header again
   with the right size.  This only works if the file is seekable;
   if it is not, the unspecified size remains in the header
   (this is illegal). */

static int sox_wvestartwrite(ft_t ft)
{
        wve_t p = (wve_t)ft->priv;
        int rc;

        /* Needed for rawwrite() */
        rc = sox_rawstartwrite(ft);
        if (rc)
            return SOX_EOF;

        p->length = 0;
        if (p->repeats == 0)
            p->repeats = 1;

        if (ft->signal.rate != 0)
            sox_report("WVE must use 8000 sample rate.  Overriding");

        if (ft->signal.channels != 0 && ft->signal.channels != 1)
            sox_report("WVE must only supports 1 channel.  Overriding");

        ft->signal.encoding = SOX_ENCODING_ALAW;
        ft->signal.size = SOX_SIZE_BYTE;
        ft->signal.rate = 8000;

        wvewriteheader(ft);
        return SOX_SUCCESS;
}

static sox_size_t sox_wvewrite(ft_t ft, const sox_ssample_t *buf, sox_size_t samp)
{
        wve_t p = (wve_t)ft->priv;
        p->length += samp * ft->signal.size;
        return sox_rawwrite(ft, buf, samp);
}

static int sox_wvestopwrite(ft_t ft)
{

        /* Call before seeking to flush buffer */
        sox_rawstopwrite(ft);

        if (!ft->seekable)
        {
            sox_warn("Header will be have invalid file length since file is not seekable");
            return SOX_SUCCESS;
        }

        if (sox_seeki(ft, 0, 0) != 0)
        {
                sox_fail_errno(ft,errno,"Can't rewind output file to rewrite Psion header.");
                return(SOX_EOF);
        }
        wvewriteheader(ft);
        return SOX_SUCCESS;
}

static void wvewriteheader(ft_t ft)
{

    char magic[16];
    short version;
    short zero;
    wve_t p = (wve_t)ft->priv;

    strcpy(magic,PSION_MAGIC);
    version=PSION_VERSION;
    zero=0;

    sox_writes(ft, magic);
    /* Null terminate string */
    sox_writeb(ft, 0);

    sox_writew(ft, version);

    sox_writedw(ft, p->length);
    sox_writew(ft, p->padding);
    sox_writew(ft, p->repeats);

    sox_writew(ft, zero);
    sox_writew(ft, zero);
    sox_writew(ft, zero);
}

/* Psion .wve */
static const char *wvenames[] = {
  "wve",
  NULL
};

static sox_format_t sox_wve_format = {
  wvenames,
  SOX_FILE_SEEK | SOX_FILE_BIG_END,
  sox_wvestartread,
  sox_rawread,
  sox_rawstopread,
  sox_wvestartwrite,
  sox_wvewrite,
  sox_wvestopwrite,
  sox_wveseek
};

const sox_format_t *sox_wve_format_fn(void)
{
    return &sox_wve_format;
}
