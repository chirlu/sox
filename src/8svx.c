/*
 * Amiga 8SVX format handler: W V Neisius, February 1992
 */

#include <math.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>	/* For SEEK_* defines if not found in stdio */
#endif

#include "st.h"

/* Private data used by writer */
struct svxpriv {
	ULONG nsamples;
	FILE *ch[4];
};

static void svxwriteheader(P2(ft_t, LONG));

/*======================================================================*/
/*                         8SVXSTARTREAD                                */
/*======================================================================*/

int st_svxstartread(ft)
ft_t ft;
{
	struct svxpriv *p = (struct svxpriv *) ft->priv;

	char buf[12];
	char *chunk_buf;
 
	ULONG totalsize;
	ULONG chunksize;

	int channels;
	LONG rate;
	int i;
	int littlendian = 1;
	char *endptr;

	ULONG chan1_pos;

	endptr = (char*) &littlendian;
	/* 8svx is in big endian format. Swap whats
	 * read in on little endian machines.
	 */
	if (*endptr)
	{
		ft->swap = ft->swap ? 0 : 1;
	}

	rate = 0;
	channels = 1;

	/* read FORM chunk */
	if (fread(buf, 1, 4, ft->fp) != 4 || strncmp(buf, "FORM", 4) != 0)
	{
		fail("8SVX: header does not begin with magic word 'FORM'");
		return(ST_EOF);
	}
	totalsize = rlong(ft);
	if (fread(buf, 1, 4, ft->fp) != 4 || strncmp(buf, "8SVX", 4) != 0)
	{
		fail("8SVX: 'FORM' chunk does not specify '8SVX' as type");
		return(ST_EOF);
	}

	/* read chunks until 'BODY' (or end) */
	while (fread(buf,1,4,ft->fp) == 4 && strncmp(buf,"BODY",4) != 0) {
		if (strncmp(buf,"VHDR",4) == 0) {
			chunksize = rlong(ft);
			if (chunksize != 20)
			{
				fail ("8SVX: VHDR chunk has bad size");
				return(ST_EOF);
			}
			fseek(ft->fp,12,SEEK_CUR);
			rate = rshort(ft);
			fseek(ft->fp,1,SEEK_CUR);
			fread(buf,1,1,ft->fp);
			if (buf[0] != 0)
			{
				fail ("8SVX: unsupported data compression");
				return(ST_EOF);
			}
			fseek(ft->fp,4,SEEK_CUR);
			continue;
		}

		if (strncmp(buf,"ANNO",4) == 0) {
			chunksize = rlong(ft);
			if (chunksize & 1)
				chunksize++;
			chunk_buf = (char *) malloc(chunksize + 2);
			if (chunk_buf == 0)
			{
			    fail("Couldn't alloc resources");
			    return(ST_EOF);
			}
			if (fread(chunk_buf,1,(size_t)chunksize,ft->fp) 
					!= chunksize)
			{
				fail("8SVX: Unexpected EOF in ANNO header");
				return(ST_EOF);
			}
			chunk_buf[chunksize] = '\0';
			report ("%s",chunk_buf);
			free(chunk_buf);

			continue;
		}

		if (strncmp(buf,"NAME",4) == 0) {
			chunksize = rlong(ft);
			if (chunksize & 1)
				chunksize++;
			chunk_buf = (char *) malloc(chunksize + 1);
			if (chunk_buf == 0)
			{
			    fail("Couldn't alloc resources");
			    return(ST_EOF);
			}
			if (fread (chunk_buf,1,(size_t)chunksize,ft->fp) 
					!= chunksize)
			{
				fail("8SVX: Unexpected EOF in NAME header");
				return(ST_EOF);
			}
			chunk_buf[chunksize] = '\0';
			report ("%s",chunk_buf);
			free(chunk_buf);

			continue;
		}

		if (strncmp(buf,"CHAN",4) == 0) {
			chunksize = rlong(ft);
			if (chunksize != 4) 
			{
				fail("8SVX: Short channel chunk");
				return(ST_EOF);
			}
			channels = rlong(ft);
			channels = (channels & 0x01) + 
					((channels & 0x02) >> 1) +
				   	((channels & 0x04) >> 2) + 
					((channels & 0x08) >> 3);

			continue;
		}

		/* some other kind of chunk */
		chunksize = rlong(ft);
		if (chunksize & 1)
			chunksize++;
		fseek(ft->fp,chunksize,SEEK_CUR);
		continue;

	}

	if (rate == 0)
	{
		fail ("8SVX: invalid rate");
		return(ST_EOF);
	}
	if (strncmp(buf,"BODY",4) != 0)
	{
		fail ("8SVX: BODY chunk not found");
		return(ST_EOF);
	}
	p->nsamples = rlong(ft);

	ft->info.channels = channels;
	ft->info.rate = rate;
	ft->info.style = ST_ENCODING_SIGN2;
	ft->info.size = ST_SIZE_BYTE;

	/* open files to channels */
	p->ch[0] = ft->fp;
	chan1_pos = ftell(p->ch[0]);

	for (i = 1; i < channels; i++) {
		if ((p->ch[i] = fopen(ft->filename, READBINARY)) == NULL)
		{
			fail("Can't open channel file '%s': %s",
				ft->filename, strerror(errno));
			return(ST_EOF);
		}

		/* position channel files */
		if (fseek(p->ch[i],chan1_pos,SEEK_SET))
		{
		    fail ("Can't position channel %d: %s",i,strerror(errno));
		    return(ST_EOF);
		}
		if (fseek(p->ch[i],p->nsamples/channels*i,SEEK_CUR))
		{
		    fail ("Can't seek channel %d: %s",i,strerror(errno));
		    return(ST_EOF);
		}
	}
	return(ST_SUCCESS);
}

/*======================================================================*/
/*                         8SVXREAD                                     */
/*======================================================================*/
LONG st_svxread(ft, buf, nsamp) 
ft_t ft;
LONG *buf, nsamp;
{
	ULONG datum;
	int done = 0;
	int i;

	struct svxpriv *p = (struct svxpriv *) ft->priv;

	while (done < nsamp) {
		for (i = 0; i < ft->info.channels; i++) {
			datum = getc(p->ch[i]);
			if (feof(p->ch[i]))
				return done;
			/* scale signed up to long's range */
			*buf++ = LEFT(datum, 24);
		}
		done += ft->info.channels;
	}
	return done;
}

/*======================================================================*/
/*                         8SVXSTOPREAD                                 */
/*======================================================================*/
int st_svxstopread(ft)
ft_t ft;
{
	int i;

	struct svxpriv *p = (struct svxpriv *) ft->priv;

	/* close channel files */
	for (i = 1; i < ft->info.channels; i++) {
		fclose (p->ch[i]);
	}
	return(ST_SUCCESS);
}

/*======================================================================*/
/*                         8SVXSTARTWRITE                               */
/*======================================================================*/
int st_svxstartwrite(ft)
ft_t ft;
{
	struct svxpriv *p = (struct svxpriv *) ft->priv;
	int i;

	int littlendian = 1;
	char *endptr;

	endptr = (char *) &littlendian;
	/* 8svx is in big endian format.  Swaps wahst
	 * read in on little endian machines.
	 */
	if (*endptr)
	{
		ft->swap = ft->swap ? 0 : 1;
	}

	/* open channel output files */
	p->ch[0] = ft->fp;
	for (i = 1; i < ft->info.channels; i++) {
		if ((p->ch[i] = tmpfile()) == NULL)
		{
			fail("Can't open channel output file: %s",
				strerror(errno));
			return(ST_EOF);
		}
	}

	/* write header (channel 0) */
	ft->info.style = ST_ENCODING_SIGN2;
	ft->info.size = ST_SIZE_BYTE;

	p->nsamples = 0;
	svxwriteheader(ft, p->nsamples);
	return(ST_SUCCESS);
}

/*======================================================================*/
/*                         8SVXWRITE                                    */
/*======================================================================*/

LONG st_svxwrite(ft, buf, len)
ft_t ft;
LONG *buf, len;
{
	struct svxpriv *p = (struct svxpriv *) ft->priv;

	LONG datum;
	int done = 0;
	int i;

	p->nsamples += len;

	while(done < len) {
		for (i = 0; i < ft->info.channels; i++) {
			datum = RIGHT(*buf++, 24);
			putc((int)datum, p->ch[i]);
		}
		done += ft->info.channels;
	}
	return (done);
}

/*======================================================================*/
/*                         8SVXSTOPWRITE                                */
/*======================================================================*/

int st_svxstopwrite(ft)
ft_t ft;
{
	struct svxpriv *p = (struct svxpriv *) ft->priv;

	int i;
	int len;
	char svxbuf[512];

	/* append all channel pieces to channel 0 */
	/* close temp files */
	for (i = 1; i < ft->info.channels; i++) {
		if (fseek (p->ch[i], 0L, 0))
		{
			fail ("Can't rewind channel output file %d",i);
			return(ST_EOF);
		}
		while (!feof(p->ch[i])) {
			len = fread (svxbuf, 1, 512, p->ch[i]);
			fwrite (svxbuf, 1, len, p->ch[0]);
		}
		fclose (p->ch[i]);
	}

	/* add a pad byte if BODY size is odd */
	if(p->nsamples % 2 != 0)
		fputc('\0', ft->fp);

	/* fixup file sizes in header */
	if (fseek(ft->fp, 0L, 0) != 0)
	{
		fail("can't rewind output file to rewrite 8SVX header");
		return(ST_EOF);
	}
	svxwriteheader(ft, p->nsamples);
	return(ST_SUCCESS);
}

/*======================================================================*/
/*                         8SVXWRITEHEADER                              */
/*======================================================================*/
#define SVXHEADERSIZE 100
static void svxwriteheader(ft,nsamples)
ft_t ft;
LONG nsamples;
{
	LONG formsize =  nsamples + SVXHEADERSIZE - 8;

	/* FORM size must be even */
	if(formsize % 2 != 0) formsize++;

	fputs ("FORM", ft->fp);
	wlong(ft, formsize);  /* size of file */
	fputs("8SVX", ft->fp); /* File type */

	fputs ("VHDR", ft->fp);
	wlong(ft, (LONG) 20); /* number of bytes to follow */
	wlong(ft, nsamples);  /* samples, 1-shot */
	wlong(ft, (LONG) 0);  /* samples, repeat */
	wlong(ft, (LONG) 0);  /* samples per repeat cycle */
	wshort(ft, (int) ft->info.rate); /* samples per second */
	fputc(1,ft->fp); /* number of octaves */
	fputc(0,ft->fp); /* data compression (none) */
	wshort(ft,1); wshort(ft,0); /* volume */

	fputs ("ANNO", ft->fp);
	wlong(ft, (LONG) 32); /* length of block */
	fputs ("File created by Sound Exchange  ", ft->fp);

	fputs ("CHAN", ft->fp);
	wlong(ft, (LONG) 4);
	wlong(ft, (ft->info.channels == 2) ? (LONG) 6 :
		   (ft->info.channels == 4) ? (LONG) 15 : (LONG) 2);

	fputs ("BODY", ft->fp);
	wlong(ft, nsamples); /* samples in file */
}
