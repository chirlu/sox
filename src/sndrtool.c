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

void sndtstartread(ft)
ft_t ft;
{
char buf[97];

LONG rate;

	int littlendian = 1;
	char *endptr;

	/* Needed for rawread() */
	rawstartread(ft);

	endptr = (char *) &littlendian;
	/* sndt is in little endian format so 
	 * swap bytes on big endian machines.
	 */
	if (!*endptr)
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
	fail("SND: unexpected EOF");
if (strncmp(buf,"\0\0",2) == 0)
	{
	/* sounder */
	rate = rshort(ft);
	if (rate < 4000 || rate > 25000 )
		fail ("SND: sample rate out of range");
	fseek(ft->fp,4,SEEK_CUR);
	}
else
	{
	/* sndtool ? */
	fread(&buf[2],1,6,ft->fp);
	if (strncmp(buf,"SOUND",5))
		fail ("SND: unrecognized SND format");
	fseek(ft->fp,12,SEEK_CUR);
	rate = rshort(ft);
	fseek(ft->fp,6,SEEK_CUR);
	if (fread(buf,1,96,ft->fp) != 96)
		fail ("SND: unexpected EOF in SND header");
	report ("%s",buf);
	}

ft->info.channels = 1;
ft->info.rate = rate;
ft->info.style = UNSIGNED;
ft->info.size = BYTE;

}

/*======================================================================*/
/*                         SNDTSTARTWRITE                               */
/*======================================================================*/
void sndtstartwrite(ft)
ft_t ft;
{
struct sndpriv *p = (struct sndpriv *) ft->priv;

	int littlendian = 1;
	char *endptr;

	/* Needed for rawwrite() */
	rawstartwrite(ft);

	endptr = (char *) &littlendian;
	/* sndt is in little endian format so
	 * swap bytes on big endian machines
	 */
	if (!*endptr)
	{
		ft->swap = ft->swap ? 0 : 1;
	}

/* write header */
ft->info.channels = 1;
ft->info.style = UNSIGNED;
ft->info.size = BYTE;
p->nsamples = 0;
sndtwriteheader(ft, 0);

}
/*======================================================================*/
/*                         SNDRSTARTWRITE                               */
/*======================================================================*/
void sndrstartwrite(ft)
ft_t ft;
{
	int littlendian = 1;
	char *endptr;

	/* Needed for rawread() */
	rawstartread(ft);

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
ft->info.style = UNSIGNED;
ft->info.size = BYTE;

/* sounder header */
wshort (ft,0); /* sample size code */
wshort (ft,(int) ft->info.rate);     /* sample rate */
wshort (ft,10);        /* volume */
wshort (ft,4); /* shift */
}

/*======================================================================*/
/*                         SNDTWRITE                                     */
/*======================================================================*/

void sndtwrite(ft, buf, len)
ft_t ft;
LONG *buf, len;
{
	struct sndpriv *p = (struct sndpriv *) ft->priv;
	p->nsamples += len;
	rawwrite(ft, buf, len);
}

/*======================================================================*/
/*                         SNDTSTOPWRITE                                */
/*======================================================================*/

void sndtstopwrite(ft)
ft_t ft;
{
	struct sndpriv *p = (struct sndpriv *) ft->priv;

	/* Flush remaining buffer out */
	rawstopwrite(ft);

	/* fixup file sizes in header */
	if (fseek(ft->fp, 0L, 0) != 0)
		fail("can't rewind output file to rewrite SND header");
	sndtwriteheader(ft, p->nsamples);
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
fputs ("SOUND",ft->fp); /* magic */
fputc (0x1a,ft->fp);
wshort (ft,(LONG)0);  /* hGSound */
wlong (ft,nsamples);
wlong (ft,(LONG)0);
wlong (ft,nsamples);
wshort (ft,(int) ft->info.rate);
wshort (ft,0);
wshort (ft,10);
wshort (ft,4);
memset (name_buf, 0, 96);
sprintf (name_buf,"%s - File created by Sound Exchange",ft->filename);
fwrite (name_buf, 1, 96, ft->fp);
}

