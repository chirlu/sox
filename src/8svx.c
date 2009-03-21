/* Amiga 8SVX format handler: W V Neisius, February 1992 */

#include "sox_i.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Private data used by writer */
typedef struct{
  uint32_t nsamples;
  FILE * ch[4];
} priv_t;

static void svxwriteheader(sox_format_t *, size_t);

/*======================================================================*/
/*                         8SVXSTARTREAD                                */
/*======================================================================*/

static int startread(sox_format_t * ft)
{
        priv_t * p = (priv_t * ) ft->priv;

        char buf[12];
        char *chunk_buf;

        uint32_t totalsize;
        uint32_t chunksize;

        uint32_t channels, i;
        unsigned short rate;

        off_t chan1_pos;

        if (! ft->seekable)
        {
                lsx_fail_errno(ft,SOX_EINVAL,"8svx input file must be a file, not a pipe");
                return (SOX_EOF);
        }
        rate = 0;
        channels = 1;

        /* read FORM chunk */
        if (lsx_reads(ft, buf, (size_t)4) == SOX_EOF || strncmp(buf, "FORM", (size_t)4) != 0)
        {
                lsx_fail_errno(ft, SOX_EHDR, "Header did not begin with magic word `FORM'");
                return(SOX_EOF);
        }
        lsx_readdw(ft, &totalsize);
        if (lsx_reads(ft, buf, (size_t)4) == SOX_EOF || strncmp(buf, "8SVX", (size_t)4) != 0)
        {
                lsx_fail_errno(ft, SOX_EHDR, "'FORM' chunk does not specify `8SVX' as type");
                return(SOX_EOF);
        }

        /* read chunks until 'BODY' (or end) */
        while (lsx_reads(ft, buf, (size_t)4) == SOX_SUCCESS && strncmp(buf,"BODY",(size_t)4) != 0) {
                if (strncmp(buf,"VHDR",(size_t)4) == 0) {
                        lsx_readdw(ft, &chunksize);
                        if (chunksize != 20)
                        {
                                lsx_fail_errno(ft, SOX_EHDR, "VHDR chunk has bad size");
                                return(SOX_EOF);
                        }
                        lsx_seeki(ft,(off_t)12,SEEK_CUR);
                        lsx_readw(ft, &rate);
                        lsx_seeki(ft,(off_t)1,SEEK_CUR);
                        lsx_readbuf(ft, buf,(size_t)1);
                        if (buf[0] != 0)
                        {
                                lsx_fail_errno(ft, SOX_EFMT, "Unsupported data compression");
                                return(SOX_EOF);
                        }
                        lsx_seeki(ft,(off_t)4,SEEK_CUR);
                        continue;
                }

                if (strncmp(buf,"ANNO",(size_t)4) == 0) {
                        lsx_readdw(ft, &chunksize);
                        if (chunksize & 1)
                                chunksize++;
                        chunk_buf = lsx_malloc(chunksize + (size_t)2);
                        if (lsx_readbuf(ft, chunk_buf,(size_t)chunksize)
                                        != chunksize)
                        {
                                lsx_fail_errno(ft, SOX_EHDR, "Couldn't read all of header");
                                return(SOX_EOF);
                        }
                        chunk_buf[chunksize] = '\0';
                        lsx_debug("%s",chunk_buf);
                        free(chunk_buf);

                        continue;
                }

                if (strncmp(buf,"NAME",(size_t)4) == 0) {
                        lsx_readdw(ft, &chunksize);
                        if (chunksize & 1)
                                chunksize++;
                        chunk_buf = lsx_malloc(chunksize + (size_t)1);
                        if (lsx_readbuf(ft, chunk_buf,(size_t)chunksize)
                                        != chunksize)
                        {
                                lsx_fail_errno(ft, SOX_EHDR, "Couldn't read all of header");
                                return(SOX_EOF);
                        }
                        chunk_buf[chunksize] = '\0';
                        lsx_debug("%s",chunk_buf);
                        free(chunk_buf);

                        continue;
                }

                if (strncmp(buf,"CHAN",(size_t)4) == 0) {
                        lsx_readdw(ft, &chunksize);
                        if (chunksize != 4)
                        {
                                lsx_fail_errno(ft, SOX_EHDR, "Couldn't read all of header");
                                return(SOX_EOF);
                        }
                        lsx_readdw(ft, &channels);
                        channels = (channels & 0x01) +
                                        ((channels & 0x02) >> 1) +
                                        ((channels & 0x04) >> 2) +
                                        ((channels & 0x08) >> 3);

                        continue;
                }

                /* some other kind of chunk */
                lsx_readdw(ft, &chunksize);
                if (chunksize & 1)
                        chunksize++;
                lsx_seeki(ft,(off_t)chunksize,SEEK_CUR);
                continue;

        }

        if (rate == 0)
        {
                lsx_fail_errno(ft, SOX_EHDR, "Invalid sample rate");
                return(SOX_EOF);
        }
        if (strncmp(buf,"BODY",(size_t)4) != 0)
        {
                lsx_fail_errno(ft, SOX_EHDR, "BODY chunk not found");
                return(SOX_EOF);
        }
        lsx_readdw(ft, &(p->nsamples));

        ft->signal.length = p->nsamples;
        ft->signal.channels = channels;
        ft->signal.rate = rate;
        ft->encoding.encoding = SOX_ENCODING_SIGN2;
        ft->encoding.bits_per_sample = 8;

        /* open files to channels */
        p->ch[0] = ft->fp;
        chan1_pos = lsx_tell(ft);

        for (i = 1; i < channels; i++) {
                if ((p->ch[i] = fopen(ft->filename, "rb")) == NULL)
                {
                        lsx_fail_errno(ft,errno,"Can't open channel file '%s'",
                                ft->filename);
                        return(SOX_EOF);
                }

                /* position channel files */
                if (fseeko(p->ch[i],chan1_pos,SEEK_SET))
                {
                    lsx_fail_errno (ft,errno,"Can't position channel %d",i);
                    return(SOX_EOF);
                }
                if (fseeko(p->ch[i],(off_t)(p->nsamples/channels*i),SEEK_CUR))
                {
                    lsx_fail_errno (ft,errno,"Can't seek channel %d",i);
                    return(SOX_EOF);
                }
        }
        return(SOX_SUCCESS);
}

/*======================================================================*/
/*                         8SVXREAD                                     */
/*======================================================================*/
static size_t read_samples(sox_format_t * ft, sox_sample_t *buf, size_t nsamp)
{
        unsigned char datum;
        size_t done = 0, i;

        priv_t * p = (priv_t * ) ft->priv;

        while (done < nsamp) {
                for (i = 0; i < ft->signal.channels; i++) {
                        /* FIXME: don't pass FILE pointers! */
                        datum = getc(p->ch[i]);
                        if (feof(p->ch[i]))
                                return done;
                        /* scale signed up to long's range */
                        *buf++ = SOX_SIGNED_8BIT_TO_SAMPLE(datum,);
                }
                done += ft->signal.channels;
        }
        return done;
}

/*======================================================================*/
/*                         8SVXSTOPREAD                                 */
/*======================================================================*/
static int stopread(sox_format_t * ft)
{
        size_t i;

        priv_t * p = (priv_t * ) ft->priv;

        /* close channel files */
        for (i = 1; i < ft->signal.channels; i++) {
                fclose (p->ch[i]);
        }
        return(SOX_SUCCESS);
}

/*======================================================================*/
/*                         8SVXSTARTWRITE                               */
/*======================================================================*/
static int startwrite(sox_format_t * ft)
{
        priv_t * p = (priv_t * ) ft->priv;
        size_t i;

        /* open channel output files */
        p->ch[0] = ft->fp;
        for (i = 1; i < ft->signal.channels; i++) {
                if ((p->ch[i] = lsx_tmpfile()) == NULL)
                {
                        lsx_fail_errno(ft,errno,"Can't open channel output file");
                        return(SOX_EOF);
                }
        }

        /* write header (channel 0) */
        p->nsamples = 0;
        svxwriteheader(ft, (size_t) p->nsamples);
        return(SOX_SUCCESS);
}

/*======================================================================*/
/*                         8SVXWRITE                                    */
/*======================================================================*/

static size_t write_samples(sox_format_t * ft, const sox_sample_t *buf, size_t len)
{
        priv_t * p = (priv_t * ) ft->priv;
        SOX_SAMPLE_LOCALS;

        unsigned char datum;
        size_t done = 0, i;

        p->nsamples += len;

        while(done < len) {
                for (i = 0; i < ft->signal.channels; i++) {
                        datum = SOX_SAMPLE_TO_SIGNED_8BIT(*buf++, ft->clips);
                        /* FIXME: Needs to pass ft struct and not FILE */
                        putc(datum, p->ch[i]);
                }
                done += ft->signal.channels;
        }
        return (done);
}

/*======================================================================*/
/*                         8SVXSTOPWRITE                                */
/*======================================================================*/

static int stopwrite(sox_format_t * ft)
{
        priv_t * p = (priv_t * ) ft->priv;

        size_t i, len;
        char svxbuf[512];

        /* append all channel pieces to channel 0 */
        /* close temp files */
        for (i = 1; i < ft->signal.channels; i++) {
                if (fseeko(p->ch[i], (off_t)0, 0))
                {
                        lsx_fail_errno (ft,errno,"Can't rewind channel output file %lu",(unsigned long)i);
                        return(SOX_EOF);
                }
                while (!feof(p->ch[i])) {
                        len = fread(svxbuf, (size_t) 1, (size_t) 512, p->ch[i]);
                        if (fwrite (svxbuf, (size_t) 1, len, p->ch[0]) != len) {
                          lsx_fail_errno (ft,errno,"Can't write channel output file %lu",(unsigned long)i);
                          return SOX_EOF;
                        }
                }
                fclose (p->ch[i]);
        }

        /* add a pad byte if BODY size is odd */
        if(p->nsamples % 2 != 0)
            lsx_writeb(ft, '\0');

        /* fixup file sizes in header */
        if (lsx_seeki(ft, (off_t)0, 0) != 0)
        {
                lsx_fail_errno(ft,errno,"can't rewind output file to rewrite 8SVX header");
                return(SOX_EOF);
        }
        svxwriteheader(ft, (size_t) p->nsamples);
        return(SOX_SUCCESS);
}

/*======================================================================*/
/*                         8SVXWRITEHEADER                              */
/*======================================================================*/
#define SVXHEADERSIZE 100
static void svxwriteheader(sox_format_t * ft, size_t nsamples)
{
        size_t formsize =  nsamples + SVXHEADERSIZE - 8;

        /* FORM size must be even */
        if(formsize % 2 != 0) formsize++;

        lsx_writes(ft, "FORM");
        lsx_writedw(ft, (unsigned) formsize);  /* size of file */
        lsx_writes(ft, "8SVX"); /* File type */

        lsx_writes(ft, "VHDR");
        lsx_writedw(ft, 20); /* number of bytes to follow */
        lsx_writedw(ft, (unsigned) nsamples/ft->signal.channels);  /* samples, 1-shot */
        lsx_writedw(ft, 0);  /* samples, repeat */
        lsx_writedw(ft, 0);  /* samples per repeat cycle */
        lsx_writew(ft, min(65535, (unsigned)(ft->signal.rate + .5)));
        lsx_writeb(ft,1); /* number of octabes */
        lsx_writeb(ft,0); /* data compression (none) */
        lsx_writew(ft,1); lsx_writew(ft,0); /* volume */

        lsx_writes(ft, "ANNO");
        lsx_writedw(ft, 32); /* length of block */
        lsx_writes(ft, "File created by Sound Exchange  ");

        lsx_writes(ft, "CHAN");
        lsx_writedw(ft, 4);
        lsx_writedw(ft, (ft->signal.channels == 2) ? 6u :
                   (ft->signal.channels == 4) ? 15u : 2u);

        lsx_writes(ft, "BODY");
        lsx_writedw(ft, (unsigned) nsamples); /* samples in file */
}

LSX_FORMAT_HANDLER(svx)
{
  static char const * const names[] = {"8svx", NULL};
  static unsigned const write_encodings[] = {SOX_ENCODING_SIGN2, 8, 0, 0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "Amiga audio format (a subformat of the Interchange File Format)",
    names, SOX_FILE_BIG_END|SOX_FILE_MONO|SOX_FILE_STEREO|SOX_FILE_QUAD,
    startread, read_samples, stopread,
    startwrite, write_samples, stopwrite,
    NULL, write_encodings, NULL, sizeof(priv_t)
  };
  return &handler;
}
