/*
 * Sound Tools MAUD file format driver, by Lutz Vieweg 1993
 *
 * supports: mono and stereo, linear, a-law and u-law reading and writing
 *
 * Copyright 1998-2006 Chris Bagwell and SoX Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

#include "st_i.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>     /* For SEEK_* defines if not found in stdio */
#endif

/* Private data for MAUD file */
struct maudstuff { /* max. 100 bytes!!!! */
        uint32_t nsamples;
};

static void maudwriteheader(ft_t);

/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *      Find out sampling rate, 
 *      size and encoding of samples, 
 *      mono/stereo/quad.
 */
static int st_maudstartread(ft_t ft) 
{
        struct maudstuff * p = (struct maudstuff *) ft->priv;
        
        char buf[12];
        char *chunk_buf;
        
        unsigned short bitpersam;
        uint32_t nom;
        unsigned short denom;
        unsigned short chaninf;
        
        uint32_t chunksize;
        uint32_t trash32;
        uint16_t trash16;
        int rc;

        /* Needed for rawread() */
        rc = st_rawstartread(ft);
        if (rc)
            return rc;

        /* read FORM chunk */
        if (st_reads(ft, buf, 4) == ST_EOF || strncmp(buf, "FORM", 4) != 0)
        {
                st_fail_errno(ft,ST_EHDR,"MAUD: header does not begin with magic word 'FORM'");
                return (ST_EOF);
        }
        
        st_readdw(ft, &trash32); /* totalsize */
        
        if (st_reads(ft, buf, 4) == ST_EOF || strncmp(buf, "MAUD", 4) != 0)
        {
                st_fail_errno(ft,ST_EHDR,"MAUD: 'FORM' chunk does not specify 'MAUD' as type");
                return(ST_EOF);
        }
        
        /* read chunks until 'BODY' (or end) */
        
        while (st_reads(ft, buf, 4) == ST_SUCCESS && strncmp(buf,"MDAT",4) != 0) {
                
                /*
                buf[4] = 0;
                st_debug("chunk %s",buf);
                */
                
                if (strncmp(buf,"MHDR",4) == 0) {
                        
                        st_readdw(ft, &chunksize);
                        if (chunksize != 8*4) 
                        {
                            st_fail_errno(ft,ST_EHDR,"MAUD: MHDR chunk has bad size");
                            return(ST_EOF);
                        }
                        
                        /* fseeko(ft->fp,12,SEEK_CUR); */

                        /* number of samples stored in MDAT */
                        st_readdw(ft, &(p->nsamples));

                        /* number of bits per sample as stored in MDAT */
                        st_readw(ft, &bitpersam);

                        /* number of bits per sample after decompression */
                        st_readw(ft, (unsigned short *)&trash16);

                        st_readdw(ft, &nom);         /* clock source frequency */
                        st_readw(ft, &denom);       /* clock devide           */
                        if (denom == 0) 
                        {
                            st_fail_errno(ft,ST_EHDR,"MAUD: frequency denominator == 0, failed");
                            return (ST_EOF);
                        }
                        
                        ft->signal.rate = nom / denom;
                        
                        st_readw(ft, &chaninf); /* channel information */
                        switch (chaninf) {
                        case 0:
                                ft->signal.channels = 1;
                                break;
                        case 1:
                                ft->signal.channels = 2;
                                break;
                        default:
                                st_fail_errno(ft,ST_EFMT,"MAUD: unsupported number of channels in file");
                                return (ST_EOF);
                        }
                        
                        st_readw(ft, &chaninf); /* number of channels (mono: 1, stereo: 2, ...) */
                        if (chaninf != ft->signal.channels) 
                        {
                                st_fail_errno(ft,ST_EFMT,"MAUD: unsupported number of channels in file");
                            return(ST_EOF);
                        }
                        
                        st_readw(ft, &chaninf); /* compression type */
                        
                        st_readdw(ft, &trash32); /* rest of chunk, unused yet */
                        st_readdw(ft, &trash32);
                        st_readdw(ft, &trash32);
                        
                        if (bitpersam == 8 && chaninf == 0) {
                                ft->signal.size = ST_SIZE_BYTE;
                                ft->signal.encoding = ST_ENCODING_UNSIGNED;
                        }
                        else if (bitpersam == 8 && chaninf == 2) {
                                ft->signal.size = ST_SIZE_BYTE;
                                ft->signal.encoding = ST_ENCODING_ALAW;
                        }
                        else if (bitpersam == 8 && chaninf == 3) {
                                ft->signal.size = ST_SIZE_BYTE;
                                ft->signal.encoding = ST_ENCODING_ULAW;
                        }
                        else if (bitpersam == 16 && chaninf == 0) {
                                ft->signal.size = ST_SIZE_16BIT;
                                ft->signal.encoding = ST_ENCODING_SIGN2;
                        }
                        else 
                        {
                                st_fail_errno(ft,ST_EFMT,"MAUD: unsupported compression type detected");
                                return(ST_EOF);
                        }
                        
                        ft->comment = 0;
                        
                        continue;
                }
                
                if (strncmp(buf,"ANNO",4) == 0) {
                        st_readdw(ft, &chunksize);
                        if (chunksize & 1)
                                chunksize++;
                        chunk_buf = (char *) xmalloc(chunksize + 1);
                        if (st_readbuf(ft, chunk_buf, 1, (int)chunksize) 
                            != chunksize)
                        {
                                st_fail_errno(ft,ST_EOF,"MAUD: Unexpected EOF in ANNO header");
                                return(ST_EOF);
                        }
                        chunk_buf[chunksize] = '\0';
                        st_debug("%s",chunk_buf);
                        free(chunk_buf);
                        
                        continue;
                }
                
                /* some other kind of chunk */
                st_readdw(ft, &chunksize);
                if (chunksize & 1)
                        chunksize++;
                st_seeki(ft, chunksize, SEEK_CUR);
                continue;
                
        }
        
        if (strncmp(buf,"MDAT",4) != 0) 
        {
            st_fail_errno(ft,ST_EFMT,"MAUD: MDAT chunk not found");
            return(ST_EOF);
        }
        st_readdw(ft, &(p->nsamples));
        return(ST_SUCCESS);
}

static int st_maudstartwrite(ft_t ft) 
{
        struct maudstuff * p = (struct maudstuff *) ft->priv;
        int rc;

        /* Needed for rawwrite() */
        rc = st_rawstartwrite(ft);
        if (rc)
            return rc;

        /* If you have to seek around the output file */
        if (! ft->seekable) 
        {
            st_fail_errno(ft,ST_EOF,"Output .maud file must be a file, not a pipe");
            return (ST_EOF);
        }
        
        if (ft->signal.channels != 1 && ft->signal.channels != 2) {
                st_fail_errno(ft,ST_EFMT,"MAUD: unsupported number of channels, unable to store");
                return(ST_EOF);
        }
        if (ft->signal.size == ST_SIZE_16BIT) ft->signal.encoding = ST_ENCODING_SIGN2;
        if (ft->signal.encoding == ST_ENCODING_ULAW || 
            ft->signal.encoding == ST_ENCODING_ALAW) ft->signal.size = ST_SIZE_BYTE;
        if (ft->signal.size == ST_SIZE_BYTE && 
            ft->signal.encoding == ST_ENCODING_SIGN2) 
            ft->signal.encoding = ST_ENCODING_UNSIGNED;
        
        p->nsamples = 0x7f000000;
        maudwriteheader(ft);
        p->nsamples = 0;
        return (ST_SUCCESS);
}

static st_size_t st_maudwrite(ft_t ft, const st_sample_t *buf, st_size_t len) 
{
        struct maudstuff * p = (struct maudstuff *) ft->priv;
        
        p->nsamples += len;
        
        return st_rawwrite(ft, buf, len);
}

static int st_maudstopwrite(ft_t ft) 
{
        int rc;

        /* Flush out remaining samples*/
        rc = st_rawstopwrite(ft);
        if (rc)
            return rc;

        /* All samples are already written out. */
        
        if (st_seeki(ft, 0, 0) != 0) 
        {
            st_fail_errno(ft,errno,"can't rewind output file to rewrite MAUD header");
            return(ST_EOF);
        }
        
        maudwriteheader(ft);
        return(ST_SUCCESS);
}

#define MAUDHEADERSIZE (4+(4+4+32)+(4+4+32)+(4+4))
static void maudwriteheader(ft_t ft)
{
        struct maudstuff * p = (struct maudstuff *) ft->priv;
        
        st_writes(ft, "FORM");
        st_writedw(ft, (p->nsamples*ft->signal.size) + MAUDHEADERSIZE);  /* size of file */
        st_writes(ft, "MAUD"); /* File type */
        
        st_writes(ft, "MHDR");
        st_writedw(ft,  8*4); /* number of bytes to follow */
        st_writedw(ft, p->nsamples);  /* number of samples stored in MDAT */
        
        switch (ft->signal.encoding) {
                
        case ST_ENCODING_UNSIGNED:
                st_writew(ft, (int) 8); /* number of bits per sample as stored in MDAT */
                st_writew(ft, (int) 8); /* number of bits per sample after decompression */
                break;
                
        case ST_ENCODING_SIGN2:
                st_writew(ft, (int) 16); /* number of bits per sample as stored in MDAT */
                st_writew(ft, (int) 16); /* number of bits per sample after decompression */
                break;
                
        case ST_ENCODING_ALAW:
        case ST_ENCODING_ULAW:
                st_writew(ft, (int) 8); /* number of bits per sample as stored in MDAT */
                st_writew(ft, (int) 16); /* number of bits per sample after decompression */
                break;

        default:
                break;
        }
        
        st_writedw(ft, ft->signal.rate); /* clock source frequency */
        st_writew(ft, (int) 1); /* clock devide */
        
        if (ft->signal.channels == 1) {
                st_writew(ft, (int) 0); /* channel information */
                st_writew(ft, (int) 1); /* number of channels (mono: 1, stereo: 2, ...) */
        }
        else {
                st_writew(ft, (int) 1);
                st_writew(ft, (int) 2);
        }
        
        switch (ft->signal.encoding) {
                
        case ST_ENCODING_UNSIGNED:
        case ST_ENCODING_SIGN2:
                st_writew(ft, (int) 0); /* no compression */
                break;
                
        case ST_ENCODING_ULAW:
                st_writew(ft, (int) 3);
                break;
                
        case ST_ENCODING_ALAW:
                st_writew(ft, (int) 2);
                break;

        default:
                break;
        }
        
        st_writedw(ft, 0); /* reserved */
        st_writedw(ft, 0); /* reserved */
        st_writedw(ft, 0); /* reserved */
        
        st_writes(ft, "ANNO");
        st_writedw(ft, 30); /* length of block */
        st_writes(ft, "file create by Sound eXchange ");
        
        st_writes(ft, "MDAT");
        st_writedw(ft, p->nsamples * ft->signal.size ); /* samples in file */
}

/* Amiga MAUD */
static const char *maudnames[] = {
  "maud",
  NULL,
};

static st_format_t st_maud_format = {
  maudnames,
  NULL,
  ST_FILE_BIG_END,
  st_maudstartread,
  st_rawread,
  st_rawstopread,
  st_maudstartwrite,
  st_maudwrite,
  st_maudstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_maud_format_fn(void)
{
    return &st_maud_format;
}
