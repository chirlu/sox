/*
 * Amiga 8SVX format handler: W V Neisius, February 1992
 */

#include "st_i.h"

#include <math.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>     /* For SEEK_* defines if not found in stdio */
#endif

/* Private data used by writer */
typedef struct svxpriv {
        uint32_t nsamples;
        FILE *ch[4];
}*svx_t;

static void svxwriteheader(ft_t, st_size_t);

/*======================================================================*/
/*                         8SVXSTARTREAD                                */
/*======================================================================*/

static int st_svxstartread(ft_t ft)
{
        svx_t p = (svx_t ) ft->priv;

        char buf[12];
        char *chunk_buf;

        uint32_t totalsize;
        uint32_t chunksize;

        uint32_t channels, i;
        unsigned short rate;

        long chan1_pos;

        if (! ft->seekable)
        {
                st_fail_errno(ft,ST_EINVAL,"8svx input file must be a file, not a pipe");
                return (ST_EOF);
        }
        rate = 0;
        channels = 1;

        /* read FORM chunk */
        if (st_reads(ft, buf, 4) == ST_EOF || strncmp(buf, "FORM", 4) != 0)
        {
                st_fail_errno(ft, ST_EHDR, "Header did not begin with magic word 'FORM'");
                return(ST_EOF);
        }
        st_readdw(ft, &totalsize);
        if (st_reads(ft, buf, 4) == ST_EOF || strncmp(buf, "8SVX", 4) != 0)
        {
                st_fail_errno(ft, ST_EHDR, "'FORM' chunk does not specify '8SVX' as type");
                return(ST_EOF);
        }

        /* read chunks until 'BODY' (or end) */
        while (st_reads(ft, buf, 4) == ST_SUCCESS && strncmp(buf,"BODY",4) != 0) {
                if (strncmp(buf,"VHDR",4) == 0) {
                        st_readdw(ft, &chunksize);
                        if (chunksize != 20)
                        {
                                st_fail_errno(ft, ST_EHDR, "VHDR chunk has bad size");
                                return(ST_EOF);
                        }
                        st_seeki(ft,12,SEEK_CUR);
                        st_readw(ft, &rate);
                        st_seeki(ft,1,SEEK_CUR);
                        st_readbuf(ft, buf,1,1);
                        if (buf[0] != 0)
                        {
                                st_fail_errno(ft, ST_EFMT, "Unsupported data compression");
                                return(ST_EOF);
                        }
                        st_seeki(ft,4,SEEK_CUR);
                        continue;
                }

                if (strncmp(buf,"ANNO",4) == 0) {
                        st_readdw(ft, &chunksize);
                        if (chunksize & 1)
                                chunksize++;
                        chunk_buf = (char *) xmalloc(chunksize + 2);
                        if (st_readbuf(ft, chunk_buf,1,(size_t)chunksize)
                                        != chunksize)
                        {
                                st_fail_errno(ft, ST_EHDR, "Couldn't read all of header");
                                return(ST_EOF);
                        }
                        chunk_buf[chunksize] = '\0';
                        st_debug("%s",chunk_buf);
                        free(chunk_buf);

                        continue;
                }

                if (strncmp(buf,"NAME",4) == 0) {
                        st_readdw(ft, &chunksize);
                        if (chunksize & 1)
                                chunksize++;
                        chunk_buf = (char *) xmalloc(chunksize + 1);
                        if (st_readbuf(ft, chunk_buf,1,(size_t)chunksize)
                                        != chunksize)
                        {
                                st_fail_errno(ft, ST_EHDR, "Couldn't read all of header");
                                return(ST_EOF);
                        }
                        chunk_buf[chunksize] = '\0';
                        st_debug("%s",chunk_buf);
                        free(chunk_buf);

                        continue;
                }

                if (strncmp(buf,"CHAN",4) == 0) {
                        st_readdw(ft, &chunksize);
                        if (chunksize != 4)
                        {
                                st_fail_errno(ft, ST_EHDR, "Couldn't read all of header");
                                return(ST_EOF);
                        }
                        st_readdw(ft, &channels);
                        channels = (channels & 0x01) +
                                        ((channels & 0x02) >> 1) +
                                        ((channels & 0x04) >> 2) +
                                        ((channels & 0x08) >> 3);

                        continue;
                }

                /* some other kind of chunk */
                st_readdw(ft, &chunksize);
                if (chunksize & 1)
                        chunksize++;
                st_seeki(ft,chunksize,SEEK_CUR);
                continue;

        }

        if (rate == 0)
        {
                st_fail_errno(ft, ST_ERATE, "Invalid sample rate");
                return(ST_EOF);
        }
        if (strncmp(buf,"BODY",4) != 0)
        {
                st_fail_errno(ft, ST_EHDR, "BODY chunk not found");
                return(ST_EOF);
        }
        st_readdw(ft, &(p->nsamples));

        ft->length = p->nsamples;
        ft->signal.channels = channels;
        ft->signal.rate = rate;
        ft->signal.encoding = ST_ENCODING_SIGN2;
        ft->signal.size = ST_SIZE_BYTE;

        /* open files to channels */
        p->ch[0] = ft->fp;
        chan1_pos = st_tell(ft);

        for (i = 1; i < channels; i++) {
                if ((p->ch[i] = fopen(ft->filename, "rb")) == NULL)
                {
                        st_fail_errno(ft,errno,"Can't open channel file '%s'",
                                ft->filename);
                        return(ST_EOF);
                }

                /* position channel files */
                if (fseeko(p->ch[i],chan1_pos,SEEK_SET))
                {
                    st_fail_errno (ft,errno,"Can't position channel %d",i);
                    return(ST_EOF);
                }
                if (fseeko(p->ch[i],p->nsamples/channels*i,SEEK_CUR))
                {
                    st_fail_errno (ft,errno,"Can't seek channel %d",i);
                    return(ST_EOF);
                }
        }
        return(ST_SUCCESS);
}

/*======================================================================*/
/*                         8SVXREAD                                     */
/*======================================================================*/
static st_size_t st_svxread(ft_t ft, st_sample_t *buf, st_size_t nsamp)
{
        unsigned char datum;
        size_t done = 0, i;

        svx_t p = (svx_t ) ft->priv;

        while (done < nsamp) {
                for (i = 0; i < ft->signal.channels; i++) {
                        /* FIXME: don't pass FILE pointers! */
                        datum = getc(p->ch[i]);
                        if (feof(p->ch[i]))
                                return done;
                        /* scale signed up to long's range */
                        *buf++ = ST_SIGNED_BYTE_TO_SAMPLE(datum,);
                }
                done += ft->signal.channels;
        }
        return done;
}

/*======================================================================*/
/*                         8SVXSTOPREAD                                 */
/*======================================================================*/
static int st_svxstopread(ft_t ft)
{
        size_t i;

        svx_t p = (svx_t ) ft->priv;

        /* close channel files */
        for (i = 1; i < ft->signal.channels; i++) {
                fclose (p->ch[i]);
        }
        return(ST_SUCCESS);
}

/*======================================================================*/
/*                         8SVXSTARTWRITE                               */
/*======================================================================*/
static int st_svxstartwrite(ft_t ft)
{
        svx_t p = (svx_t ) ft->priv;
        size_t i;

        /* open channel output files */
        p->ch[0] = ft->fp;
        for (i = 1; i < ft->signal.channels; i++) {
                if ((p->ch[i] = tmpfile()) == NULL)
                {
                        st_fail_errno(ft,errno,"Can't open channel output file");
                        return(ST_EOF);
                }
        }

        /* write header (channel 0) */
        ft->signal.encoding = ST_ENCODING_SIGN2;
        ft->signal.size = ST_SIZE_BYTE;

        p->nsamples = 0;
        svxwriteheader(ft, p->nsamples);
        return(ST_SUCCESS);
}

/*======================================================================*/
/*                         8SVXWRITE                                    */
/*======================================================================*/

static st_size_t st_svxwrite(ft_t ft, const st_sample_t *buf, st_size_t len)
{
        svx_t p = (svx_t ) ft->priv;

        unsigned char datum;
        size_t done = 0, i;

        p->nsamples += len;

        while(done < len) {
                for (i = 0; i < ft->signal.channels; i++) {
                        datum = ST_SAMPLE_TO_SIGNED_BYTE(*buf++, ft->clips);
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

static int st_svxstopwrite(ft_t ft)
{
        svx_t p = (svx_t ) ft->priv;

        size_t i, len;
        char svxbuf[512];

        /* append all channel pieces to channel 0 */
        /* close temp files */
        for (i = 1; i < ft->signal.channels; i++) {
                if (fseeko(p->ch[i], 0, 0))
                {
                        st_fail_errno (ft,errno,"Can't rewind channel output file %d",i);
                        return(ST_EOF);
                }
                while (!feof(p->ch[i])) {
                        len = fread(svxbuf, 1, 512, p->ch[i]);
                        fwrite (svxbuf, 1, len, p->ch[0]);
                }
                fclose (p->ch[i]);
        }

        /* add a pad byte if BODY size is odd */
        if(p->nsamples % 2 != 0)
            st_writeb(ft, '\0');

        /* fixup file sizes in header */
        if (st_seeki(ft, 0, 0) != 0)
        {
                st_fail_errno(ft,errno,"can't rewind output file to rewrite 8SVX header");
                return(ST_EOF);
        }
        svxwriteheader(ft, p->nsamples);
        return(ST_SUCCESS);
}

/*======================================================================*/
/*                         8SVXWRITEHEADER                              */
/*======================================================================*/
#define SVXHEADERSIZE 100
static void svxwriteheader(ft_t ft, st_size_t nsamples)
{
        int32_t formsize =  nsamples + SVXHEADERSIZE - 8;

        /* FORM size must be even */
        if(formsize % 2 != 0) formsize++;

        st_writes(ft, "FORM");
        st_writedw(ft, formsize);  /* size of file */
        st_writes(ft, "8SVX"); /* File type */

        st_writes(ft, "VHDR");
        st_writedw(ft, 20); /* number of bytes to follow */
        st_writedw(ft, nsamples/ft->signal.channels);  /* samples, 1-shot */
        st_writedw(ft, 0);  /* samples, repeat */
        st_writedw(ft, 0);  /* samples per repeat cycle */
        st_writew(ft, (int) ft->signal.rate); /* samples per second */
        st_writeb(ft,1); /* number of octabes */
        st_writeb(ft,0); /* data compression (none) */
        st_writew(ft,1); st_writew(ft,0); /* volume */

        st_writes(ft, "ANNO");
        st_writedw(ft, 32); /* length of block */
        st_writes(ft, "File created by Sound Exchange  ");

        st_writes(ft, "CHAN");
        st_writedw(ft, 4);
        st_writedw(ft, (ft->signal.channels == 2) ? 6 :
                   (ft->signal.channels == 4) ? 15 : 2);

        st_writes(ft, "BODY");
        st_writedw(ft, nsamples); /* samples in file */
}

/* Amiga 8SVX */
static const char *svxnames[] = {
  "8svx",
  NULL
};

static st_format_t st_svx_format = {
  svxnames,
  NULL,
  ST_FILE_BIG_END,
  st_svxstartread,
  st_svxread,
  st_svxstopread,
  st_svxstartwrite,
  st_svxwrite,
  st_svxstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_svx_format_fn(void)
{
    return &st_svx_format;
}
