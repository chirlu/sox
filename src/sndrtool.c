/*
 * Sounder/Sndtool format handler: W V Neisius, February 1992
 *
 * June 28, 93: force output to mono.
 * 
 * March 3, 1999 - cbagwell@sprynet.com
 *   Forced extra comment fields to zero.
 */

#include <math.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>	/* For SEEK_* defines if not found in stdio */
#endif

#include "st.h"

/* Private data used by writer */
struct sndpriv {
        ULONG nsamples;
};

#ifndef	SEEK_CUR
#define	SEEK_CUR	1
#endif

static void  sndtwriteheader(P2(ft_t ft,LONG nsamples));

/*======================================================================*/
/*                         SNDSTARTREAD                                */
/*======================================================================*/

int st_sndtstartread(ft)
ft_t ft;
{
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

	if (fread(buf, 1, 2, ft->fp) != 2)
	{
		st_fail("SND: unexpected EOF");
		return(ST_EOF);
	}
	if (strncmp(buf,"\0\0",2) == 0)
	{
	/* sounder */
	st_readw(ft, &rate);
	if (rate < 4000 || rate > 25000 )
	{
		st_fail("SND: sample rate out of range");
		return(ST_EOF);
	}
	fseek(ft->fp,4,SEEK_CUR);
	}
	else
	{
	/* sndtool ? */
	fread(&buf[2], 1, 6, ft->fp);
	if (strncmp(buf,"SOUND",5))
	{
		st_fail("SND: unrecognized SND format");
		return(ST_EOF);
	}
	fseek(ft->fp,12,SEEK_CUR);
	st_readw(ft, &rate);
	fseek(ft->fp,6,SEEK_CUR);
	if (st_reads(ft, buf, 96) == ST_EOF)
	{
		st_fail("SND: unexpected EOF in SND header");
		return(ST_EOF);
	}
	st_report("%s",buf);
	}

ft->info.channels = 1;
ft->info.rate = rate;
ft->info.encoding = ST_ENCODING_UNSIGNED;
ft->info.size = ST_SIZE_BYTE;

return (ST_SUCCESS);
}

/*======================================================================*/
/*                         SNDTSTARTWRITE                               */
/*======================================================================*/
int st_sndtstartwrite(ft)
ft_t ft;
{
	struct sndpriv *p = (struct sndpriv *) ft->priv;
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
int st_sndrstartwrite(ft)
ft_t ft;
{
	int littlendian = 1;
	char *endptr;
	int rc;

	/* Needed for rawread() */
	rc = st_rawstartread(ft);
	if (rc)
	    return rc;

	endptr = (char *) &littlendian;
	/* sndr is in little endian format so
	 * swap bytes on big endian machines
	 */
	if (!*endptr)
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

LONG st_sndtwrite(ft, buf, len)
ft_t ft;
LONG *buf, len;
{
	struct sndpriv *p = (struct sndpriv *) ft->priv;
	p->nsamples += len;
	return st_rawwrite(ft, buf, len);
}

/*======================================================================*/
/*                         SNDTSTOPWRITE                                */
/*======================================================================*/

int st_sndtstopwrite(ft)
ft_t ft;
{
	struct sndpriv *p = (struct sndpriv *) ft->priv;
	int rc;

	/* Flush remaining buffer out */
	rc = st_rawstopwrite(ft);
	if (rc)
	    return rc;

	/* fixup file sizes in header */
	if (fseek(ft->fp, 0L, 0) != 0)
		st_fail("can't rewind output file to rewrite SND header");
	sndtwriteheader(ft, p->nsamples);

	return(ST_SUCCESS);
}

/*======================================================================*/
/*                         SNDTWRITEHEADER                              */
/*======================================================================*/
static void sndtwriteheader(ft,nsamples)
ft_t ft;
LONG nsamples;
{
char name_buf[97];

/* sndtool header */
st_writes(ft, "SOUND"); /* magic */
st_writeb(ft, 0x1a);
st_writew (ft,(LONG)0);  /* hGSound */
st_writedw (ft,nsamples);
st_writedw (ft,(LONG)0);
st_writedw (ft,nsamples);
st_writew (ft,(int) ft->info.rate);
st_writew (ft,0);
st_writew (ft,10);
st_writew (ft,4);
memset (name_buf, 0, 96);
sprintf (name_buf,"%s - File created by Sound Exchange",ft->filename);
fwrite (name_buf, 1, 96, ft->fp);
}

