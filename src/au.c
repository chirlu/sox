/*
 * Copyright 1991, 1992, 1993 Guido van Rossum And Sundry Contributors.
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Guido van Rossum And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 *
 * October 7, 1998 - cbagwell@sprynet.com
 *   G.723 was using incorrect # of bits.  Corrected to 3 and 5 bits.
 */

/*
 * Sound Tools Sun format with header (SunOS 4.1; see /usr/demo/SOUND).
 * NeXT uses this format also, but has more format codes defined.
 * DEC uses a slight variation and swaps bytes.
 * We only support the common formats.
 * CCITT G.721 (32 kbit/s) and G.723 (24/40 kbit/s) are also supported,
 * courtesy Sun's public domain implementation.
 * Output is always in big-endian (Sun/NeXT) order.
 */

#include "st_i.h"
#include "g72x.h"
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

/* Magic numbers used in Sun and NeXT audio files */
#define SUN_MAGIC       0x2e736e64              /* Really '.snd' */
#define SUN_INV_MAGIC   0x646e732e              /* '.snd' upside-down */
#define DEC_MAGIC       0x2e736400              /* Really '\0ds.' (for DEC) */
#define DEC_INV_MAGIC   0x0064732e              /* '\0ds.' upside-down */
#define SUN_HDRSIZE     24                      /* Size of minimal header */
#define SUN_UNSPEC      ((unsigned)(~0))        /* Unspecified data size */
#define SUN_ULAW        1                       /* u-law encoding */
#define SUN_LIN_8       2                       /* Linear 8 bits */
#define SUN_LIN_16      3                       /* Linear 16 bits */
#define SUN_LIN_24      4                       /* Linear 24 bits */
#define SUN_LIN_32      5                       /* Linear 32 bits */
#define SUN_FLOAT       6                       /* IEEE FP 32 bits */
#define SUN_DOUBLE      7                       /* IEEE FP 64 bits */
#define SUN_G721        23                      /* CCITT G.721 4-bits ADPCM */
#define SUN_G723_3      25                      /* CCITT G.723 3-bits ADPCM */
#define SUN_G723_5      26                      /* CCITT G.723 5-bits ADPCM */
#define SUN_ALAW        27                      /* a-law encoding */
/* The other formats are not supported by sox at the moment */

/* Private data */
typedef struct aupriv {
        /* For writer: size in bytes */
        st_size_t data_size;
        /* For seeking */
        st_size_t dataStart;
        /* For G72x decoding: */
        struct g72x_state state;
        int (*dec_routine)(int i, int out_coding, struct g72x_state *state_ptr);
        int dec_bits;
        unsigned int in_buffer;
        int in_bits;
} *au_t;

static void auwriteheader(ft_t ft, st_size_t data_size);

static int st_auencodingandsize(int sun_encoding, signed char *encoding, signed char *size)
{
    switch (sun_encoding) {
    case SUN_ULAW:
            *encoding = ST_ENCODING_ULAW;
            *size = ST_SIZE_BYTE;
            break;
    case SUN_ALAW:
            *encoding = ST_ENCODING_ALAW;
            *size = ST_SIZE_BYTE;
            break;
    case SUN_LIN_8:
            *encoding = ST_ENCODING_SIGN2;
            *size = ST_SIZE_BYTE;
            break;
    case SUN_LIN_16:
            *encoding = ST_ENCODING_SIGN2;
            *size = ST_SIZE_WORD;
            break;
    case SUN_G721:
            *encoding = ST_ENCODING_SIGN2;
            *size = ST_SIZE_WORD;
            break;
    case SUN_G723_3:
            *encoding = ST_ENCODING_SIGN2;
            *size = ST_SIZE_WORD;
            break;
    case SUN_G723_5:
            *encoding = ST_ENCODING_SIGN2;
            *size = ST_SIZE_WORD;
            break;
    case SUN_FLOAT:
            *encoding = ST_ENCODING_FLOAT;
            *size = ST_SIZE_32BIT;
            break;
    default:
            st_report("encoding: 0x%lx", encoding);
            return(ST_EOF);
    }
    return(ST_SUCCESS);
}

int st_auseek(ft_t ft, st_size_t offset) 
{
    au_t au = (au_t ) ft->priv;

    if (au->dec_routine != NULL)
    {
        st_fail_errno(ft,ST_ENOTSUP,"Sorry, DEC unsupported");
    }
    else 
    {
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
        new_offset += au->dataStart;

        ft->st_errno = st_seeki(ft, new_offset, SEEK_SET);
    }

    return(ft->st_errno);
}

int st_austartread(ft_t ft) 
{
        /* The following 6 variables represent a Sun sound header on disk.
           The numbers are written as big-endians.
           Any extra bytes (totalling hdr_size - 24) are an
           "info" field of unspecified nature, usually a string.
           By convention the header size is a multiple of 4. */
        uint32_t magic;
        uint32_t hdr_size;
        uint32_t data_size;
        uint32_t encoding;
        uint32_t sample_rate;
        uint32_t channels;

        unsigned int i;
        char *buf;
        au_t p = (au_t ) ft->priv;

        int rc;

        /* AU is in big endian format.  Swap whats read
         * in onlittle endian machines.
         */
        if (ST_IS_LITTLEENDIAN)
        {
                ft->swap = ft->swap ? 0 : 1;
        }

        /* Check the magic word */
        st_readdw(ft, &magic);
        if (magic == DEC_INV_MAGIC) {
                /* Inverted headers are not standard.  Code was probably
                 * left over from pre-standardize period of testing for
                 * endianess.  Its not hurting though.
                 */
                ft->swap = ft->swap ? 0 : 1;
                st_report("Found inverted DEC magic word.  Swapping bytes.");
        }
        else if (magic == SUN_INV_MAGIC) {
                ft->swap = ft->swap ? 0 : 1;
                st_report("Found inverted Sun/NeXT magic word. Swapping bytes.");
        }
        else if (magic == SUN_MAGIC) {
                st_report("Found Sun/NeXT magic word");
        }
        else if (magic == DEC_MAGIC) {
                st_report("Found DEC magic word");
        }
        else
        {
                st_fail_errno(ft,ST_EHDR,"Did not detect valid Sun/NeXT/DEC magic number in header.");
                return(ST_EOF);
        }

        /* Read the header size */
        st_readdw(ft, &hdr_size);
        if (hdr_size < SUN_HDRSIZE)
        {
                st_fail_errno(ft,ST_EHDR,"Sun/NeXT header size too small.");
                return(ST_EOF);
        }

        /* Read the data size; may be ~0 meaning unspecified */
        st_readdw(ft, &data_size);

        /* Read the encoding; there are some more possibilities */
        st_readdw(ft, &encoding);


        /* Translate the encoding into encoding and size parameters */
        /* (Or, for G.72x, set the decoding routine and parameters) */
        p->dec_routine = NULL;
        p->in_buffer = 0;
        p->in_bits = 0;
        if(st_auencodingandsize(encoding, &(ft->info.encoding),
                             &(ft->info.size)) == ST_EOF)
        {
            st_fail_errno(ft,ST_EFMT,"Unsupported encoding in Sun/NeXT header.\nOnly U-law, signed bytes, signed words, ADPCM, and 32-bit floats are supported.");
            return(ST_EOF);
        }
        switch (encoding) {
        case SUN_G721:
                g72x_init_state(&p->state);
                p->dec_routine = g721_decoder;
                p->dec_bits = 4;
                break;
        case SUN_G723_3:
                g72x_init_state(&p->state);
                p->dec_routine = g723_24_decoder;
                p->dec_bits = 3;
                break;
        case SUN_G723_5:
                g72x_init_state(&p->state);
                p->dec_routine = g723_40_decoder;
                p->dec_bits = 5;
                break;
        }

        /* Read the sampling rate */
        st_readdw(ft, &sample_rate);
        ft->info.rate = sample_rate;

        /* Read the number of channels */
        st_readdw(ft, &channels);
        ft->info.channels = (int) channels;

        /* Skip the info string in header; print it if verbose */
        hdr_size -= SUN_HDRSIZE; /* #bytes already read */
        if (hdr_size > 0) {
                /* Allocate comment buffer */
                buf = (char *) malloc(hdr_size+1);              
                for(i = 0; i < hdr_size; i++) {
                        st_readb(ft, (unsigned char *)&(buf[i]));
                        if (st_eof(ft))
                        {
                                st_fail_errno(ft,ST_EOF,"Unexpected EOF in Sun/NeXT header info.");
                                return(ST_EOF);
                        }
                }
                /* Buffer should already be null terminated but
                 * just in case we malloced an extra byte and 
                 * force the last byte to be 0 anyways.
                 * This should help work with a greater array of
                 * software.
                 */
                buf[hdr_size] = '\0';

                ft->comment = buf;
                st_report("Input file %s: Sun header info: %s", ft->filename, buf);
        }
        /* Needed for seeking */
        ft->length = data_size/ft->info.size;
        if(ft->seekable)
                p->dataStart = st_tell(ft);

        /* Needed for rawread() */
        rc = st_rawstartread(ft);
        if (rc)
            return rc;

        return(ST_SUCCESS);
}

/* When writing, the header is supposed to contain the number of
   data bytes written, unless it is written to a pipe.
   Since we don't know how many bytes will follow until we're done,
   we first write the header with an unspecified number of bytes,
   and at the end we rewind the file and write the header again
   with the right size.  This only works if the file is seekable;
   if it is not, the unspecified size remains in the header
   (this is legal). */

int st_austartwrite(ft_t ft) 
{
        au_t p = (au_t ) ft->priv;
        int rc;

        /* Needed because of rawwrite(); */
        rc = st_rawstartwrite(ft);
        if (rc)
            return rc;

        /* AU is in big endian format.  Swap whats read in
         * on little endian machines.
         */
        if (ST_IS_LITTLEENDIAN)
        {
                ft->swap = ft->swap ? 0 : 1;
        }

        p->data_size = 0;
        auwriteheader(ft, SUN_UNSPEC);
        return(ST_SUCCESS);
}

/*
 * Unpack input codes and pass them back as bytes.
 * Returns 1 if there is residual input, returns -1 if eof, else returns 0.
 * (Adapted from Sun's decode.c.)
 */
static int unpack_input(ft_t ft, unsigned char *code)
{
        au_t p = (au_t ) ft->priv;
        unsigned char           in_byte;

        if (p->in_bits < p->dec_bits) {
                if (st_readb(ft, &in_byte) == ST_EOF) {
                        *code = 0;
                        return (-1);
                }
                p->in_buffer |= (in_byte << p->in_bits);
                p->in_bits += 8;
        }
        *code = p->in_buffer & ((1 << p->dec_bits) - 1);
        p->in_buffer >>= p->dec_bits;
        p->in_bits -= p->dec_bits;
        return (p->in_bits > 0);
}

st_ssize_t st_auread(ft_t ft, st_sample_t *buf, st_ssize_t samp)
{
        au_t p = (au_t ) ft->priv;
        unsigned char code;
        int done;
        if (p->dec_routine == NULL)
                return st_rawread(ft, buf, samp);
        done = 0;
        while (samp > 0 && unpack_input(ft, &code) >= 0) {
                *buf++ = ST_SIGNED_WORD_TO_SAMPLE(
                        (*p->dec_routine)(code, AUDIO_ENCODING_LINEAR,
                                          &p->state));
                samp--;
                done++;
        }
        return done;
}

st_ssize_t st_auwrite(ft_t ft, st_sample_t *buf, st_ssize_t samp)
{
        au_t p = (au_t ) ft->priv;
        p->data_size += samp * ft->info.size;
        return(st_rawwrite(ft, buf, samp));
}

int st_austopwrite(ft_t ft)
{
        au_t p = (au_t ) ft->priv;
        int rc;

        /* Needed because of rawwrite(). Do now to flush
         * data before seeking around below.
         */
        rc = st_rawstopwrite(ft);
        if (rc)
            return rc;

        /* Attempt to update header */
        if (ft->seekable)
        {
          if (st_seeki(ft, 0L, 0) != 0)
          {
                st_fail_errno(ft,errno,"Can't rewind output file to rewrite Sun header.");
                return(ST_EOF);
          }
          auwriteheader(ft, p->data_size);
        }
        return(ST_SUCCESS);
}

static int st_ausunencoding(int size, int encoding)
{
        int sun_encoding;

        if (encoding == ST_ENCODING_ULAW && size == ST_SIZE_BYTE)
                sun_encoding = SUN_ULAW;
        else if (encoding == ST_ENCODING_ALAW && size == ST_SIZE_BYTE)
                sun_encoding = SUN_ALAW;
        else if (encoding == ST_ENCODING_SIGN2 && size == ST_SIZE_BYTE)
                sun_encoding = SUN_LIN_8;
        else if (encoding == ST_ENCODING_SIGN2 && size == ST_SIZE_WORD)
                sun_encoding = SUN_LIN_16;
        else if (encoding == ST_ENCODING_FLOAT && size == ST_SIZE_32BIT)
                sun_encoding = SUN_FLOAT;
        else
                sun_encoding = -1;
        return sun_encoding;
}

static void auwriteheader(ft_t ft, st_size_t data_size)
{
        uint32_t magic;
        uint32_t hdr_size;
        int      encoding;
        uint32_t sample_rate;
        uint32_t channels;
        int   x;
        int   comment_size;

        if ((encoding = st_ausunencoding(ft->info.size, ft->info.encoding)) == -1) {
                st_report("Unsupported output encoding/size for Sun/NeXT header or .AU format not specified.");
                st_report("Only U-law, A-law signed bytes, and signed words are supported.");
                st_report("Defaulting to 8khz u-law\n");
                encoding = SUN_ULAW;
                ft->info.encoding = ST_ENCODING_ULAW;
                ft->info.size = ST_SIZE_BYTE;
                ft->info.rate = 8000;  /* strange but true */
        }

        magic = SUN_MAGIC;
        st_writedw(ft, magic);

        /* Info field is at least 4 bytes. Here I force it to something
         * useful when there is no comments.
         */
        if (ft->comment == NULL)
                ft->comment = "SOX";

        hdr_size = SUN_HDRSIZE;

        comment_size = strlen(ft->comment) + 1; /*+1 = null-term. */
        if (comment_size < 4)
            comment_size = 4; /* minimum size */

        hdr_size += comment_size;

        st_writedw(ft, hdr_size);

        st_writedw(ft, data_size);

        st_writedw(ft, encoding);

        sample_rate = ft->info.rate;
        st_writedw(ft, sample_rate);

        channels = ft->info.channels;
        st_writedw(ft, channels);

        st_writes(ft, ft->comment);

        /* Info must be 4 bytes at least and null terminated. */
        x = strlen(ft->comment);
        for (;x < 3; x++)
            st_writeb(ft, 0);

        st_writeb(ft, 0);
}

