#if defined(HAS_GSM)
/*
 * Copyright 1991, 1992, 1993 Guido van Rossum And Sundry Contributors.
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Guido van Rossum And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * GSM 06.10 courtesy Communications and Operating Systems Research Group,
 * Technische Universitaet Berlin
 *
 * More information on this format can be obtained from
 * http://www.cs.tu-berlin.de/~jutta/toast.html
 *
 * Source is available from ftp://ftp.cs.tu-berlin.de/pub/local/kbs/tubmik/gsm
 *
 * Written 26 Jan 1995 by Andrew Pam
 * Portions Copyright (c) 1995 Serious Cybernetics
 *
 * July 19, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *   Added GSM support to SOX from patches floating around with the help
 *   of Dima Barsky (ess2db@ee.surrey.ac.uk).
 */

#include "st.h"
#include "gsm.h"

/* Private data */
struct gsmpriv {
	gsm		handle;
	gsm_signal	sample[160];
	int		index;
};

void
gsmstartread(ft) 
ft_t ft;
{
	struct gsmpriv *p = (struct gsmpriv *) ft->priv;

	/* Sanity check */
	if (sizeof(struct gsmpriv) > PRIVSIZE)
		fail(
"struct gsmpriv is too big (%d); change PRIVSIZE in st.h and recompile sox",
		     sizeof(struct gsmpriv));

	ft->info.style = GSM;
	ft->info.size = BYTE;
	if (!ft->info.rate)
		ft->info.rate = 8000;
	p->handle = gsm_create();
	if (!p->handle)
		fail("unable to create GSM stream");
	p->index = 0;
}

void
gsmstartwrite(ft)
ft_t ft;
{
	struct gsmpriv *p = (struct gsmpriv *) ft->priv;

	ft->info.style = GSM;
	ft->info.size = BYTE;
	if (!ft->info.rate)
		ft->info.rate = 8000;
	p->handle = gsm_create();
	if (!p->handle)
		fail("unable to create GSM stream");
	p->index = 0;
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

LONG
gsmread(ft, buf, samp)
ft_t ft;
long *buf, samp;
{
	int bytes;
	int done = 0;
	gsm_frame	frame;
	struct gsmpriv *p = (struct gsmpriv *) ft->priv;

	while (p->index && (p->index < 160) && (done < samp))
		buf[done++] = LEFT(p->sample[p->index++], 16);

	while (done < samp)
	{
		p->index = 0;
		bytes = fread( frame, 1, sizeof(frame), ft->fp );
		if (bytes <= 0)
			return done;
		if (bytes < sizeof(frame))
			fail("invalid frame size: %d bytes", bytes);
		if (gsm_decode(p->handle, frame, p->sample) < 0)
			fail("error during GSM decode");
		while ((p->index < 160) && (done < samp))
			buf[done++] = LEFT(p->sample[p->index++], 16);
	}

	return done;
}

int
gsmwrite(ft, buf, samp)
ft_t ft;
long *buf, samp;
{
	int done = 0;
	gsm_frame	frame;
	struct gsmpriv *p = (struct gsmpriv *) ft->priv;

	while (done < samp)
	{
		while ((p->index < 160) && (done < samp))
			p->sample[p->index++] = RIGHT(buf[done++], 16);
		if (p->index < 160)
			return done;
		gsm_encode(p->handle, p->sample, frame);
		if (fwrite(frame, 1, sizeof(frame), ft->fp) != sizeof(frame))
			fail("write error");
		p->index = 0;
	}

	return done;
}

void
gsmstopread(ft)
ft_t ft;
{
	struct gsmpriv *p = (struct gsmpriv *) ft->priv;

	gsm_destroy(p->handle);
}

void
gsmstopwrite(ft)
ft_t ft;
{
	gsm_frame	frame;
	struct gsmpriv *p = (struct gsmpriv *) ft->priv;

	if (p->index)
	{
		while (p->index < 160)
			p->sample[p->index++] = 0;
		gsm_encode(p->handle, p->sample, frame);
		if (fwrite(frame, 1, sizeof(frame), ft->fp) != sizeof(frame))
			fail("write error");
	}
	gsm_destroy(p->handle);
}
#endif /* HAS_GSM */
