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

#include "st.h"
#include "g72x.h"
#include <stdlib.h>

/* Magic numbers used in Sun and NeXT audio files */
#define SUN_MAGIC 	0x2e736e64		/* Really '.snd' */
#define SUN_INV_MAGIC	0x646e732e		/* '.snd' upside-down */
#define DEC_MAGIC	0x2e736400		/* Really '\0ds.' (for DEC) */
#define DEC_INV_MAGIC	0x0064732e		/* '\0ds.' upside-down */
#define SUN_HDRSIZE	24			/* Size of minimal header */
#define SUN_UNSPEC	((unsigned)(~0))	/* Unspecified data size */
#define SUN_ULAW	1			/* u-law encoding */
#define SUN_LIN_8	2			/* Linear 8 bits */
#define SUN_LIN_16	3			/* Linear 16 bits */
#define SUN_LIN_24	4			/* Linear 24 bits */
#define SUN_LIN_32	5			/* Linear 32 bits */
#define SUN_FLOAT	6			/* IEEE FP 32 bits */
#define SUN_DOUBLE	7			/* IEEE FP 64 bits */
#define SUN_G721	23			/* CCITT G.721 4-bits ADPCM */
#define SUN_G723_3	25			/* CCITT G.723 3-bits ADPCM */
#define SUN_G723_5	26			/* CCITT G.723 5-bits ADPCM */
/* The other formats are not supported by sox at the moment */

/* Private data */
struct aupriv {
	/* For writer: */
	ULONG data_size;
	/* For G72x decoding: */
	struct g72x_state state;
	int (*dec_routine)();
	int dec_bits;
	unsigned int in_buffer;
	int in_bits;
};

void auwriteheader(P2(ft_t ft, ULONG data_size));
LONG rawread(P3(ft_t, LONG *, LONG));
void rawwrite(P3(ft_t,LONG *, LONG));

void austartread(ft) 
ft_t ft;
{
	/* The following 6 variables represent a Sun sound header on disk.
	   The numbers are written as big-endians.
	   Any extra bytes (totalling hdr_size - 24) are an
	   "info" field of unspecified nature, usually a string.
	   By convention the header size is a multiple of 4. */
	ULONG magic;
	ULONG hdr_size;
	ULONG data_size;
	ULONG encoding;
	ULONG sample_rate;
	ULONG channels;

	register int i;
	char *buf;
	struct aupriv *p = (struct aupriv *) ft->priv;

	/* Sanity check */
	if (sizeof(struct aupriv) > PRIVSIZE)
		fail(
"struct aupriv is too big (%d); change PRIVSIZE in st.h and recompile sox",
		     sizeof(struct aupriv));

	/* Check the magic word */
	magic = rlong(ft);
	if (magic == DEC_INV_MAGIC) {
		ft->swap = 1;
		report("Found inverted DEC magic word");
	}
	else if (magic == SUN_INV_MAGIC) {
		ft->swap = 1;
		report("Found inverted Sun/NeXT magic word");
	}
	else if (magic == SUN_MAGIC) {
		ft->swap = 0;
		report("Found Sun/NeXT magic word");
	}
	else if (magic == DEC_MAGIC) {
		ft->swap = 0;
		report("Found DEC magic word");
	}
	else
		fail("Sun/NeXT/DEC header doesn't start with magic word\nTry the '.ul' file type with '-t ul -r 8000 filename'");

	/* Read the header size */
	hdr_size = rlong(ft);
	if (hdr_size < SUN_HDRSIZE)
		fail("Sun/NeXT header size too small.");

	/* Read the data size; may be ~0 meaning unspecified */
	data_size = rlong(ft);

	/* Read the encoding; there are some more possibilities */
	encoding = rlong(ft);


	/* Translate the encoding into style and size parameters */
	/* (Or, for G.72x, set the decoding routine and parameters) */
	p->dec_routine = NULL;
	p->in_buffer = 0;
	p->in_bits = 0;
	switch (encoding) {
	case SUN_ULAW:
		ft->info.style = ULAW;
		ft->info.size = BYTE;
		break;
	case SUN_LIN_8:
		ft->info.style = SIGN2;
		ft->info.size = BYTE;
		break;
	case SUN_LIN_16:
		ft->info.style = SIGN2;
		ft->info.size = WORD;
		break;
	case SUN_G721:
		ft->info.style = SIGN2;
		ft->info.size = WORD;
		g72x_init_state(&p->state);
		p->dec_routine = g721_decoder;
		p->dec_bits = 4;
		break;
	case SUN_G723_3:
		ft->info.style = SIGN2;
		ft->info.size = WORD;
		g72x_init_state(&p->state);
		p->dec_routine = g723_24_decoder;
		p->dec_bits = 3;
		break;
	case SUN_G723_5:
		ft->info.style = SIGN2;
		ft->info.size = WORD;
		g72x_init_state(&p->state);
		p->dec_routine = g723_40_decoder;
		p->dec_bits = 5;
		break;
	default:
		report("encoding: 0x%lx", encoding);
		fail("Unsupported encoding in Sun/NeXT header.\nOnly U-law, signed bytes, signed words, and ADPCM are supported.");
		/*NOTREACHED*/
	}

	/* Read the sampling rate */
	sample_rate = rlong(ft);
	ft->info.rate = sample_rate;

	/* Read the number of channels */
	channels = rlong(ft);
	ft->info.channels = (int) channels;

	/* Skip the info string in header; print it if verbose */
	hdr_size -= SUN_HDRSIZE; /* #bytes already read */
	if (hdr_size > 0) {
		buf = (char *) malloc(hdr_size + 1);
		for(i = 0; i < hdr_size; i++) {
			buf[i] = (char) getc(ft->fp);
			if (feof(ft->fp))
				fail("Unexpected EOF in Sun/NeXT header info.");
		}
		buf[i] = '\0';
		ft->comment = buf;
		report("Input file %s: Sun header info: %s", ft->filename, buf);
	}
}

/* When writing, the header is supposed to contain the number of
   data bytes written, unless it is written to a pipe.
   Since we don't know how many bytes will follow until we're done,
   we first write the header with an unspecified number of bytes,
   and at the end we rewind the file and write the header again
   with the right size.  This only works if the file is seekable;
   if it is not, the unspecified size remains in the header
   (this is legal). */

void austartwrite(ft) 
ft_t ft;
{
	struct aupriv *p = (struct aupriv *) ft->priv;
	int littlendian = 0;
	char *endptr;

	p->data_size = 0;
	auwriteheader(ft, SUN_UNSPEC);
	endptr = (char *) &littlendian;
	*endptr = 1;
	if (littlendian == 1)
		ft->swap = 1;
}

/*
 * Unpack input codes and pass them back as bytes.
 * Returns 1 if there is residual input, returns -1 if eof, else returns 0.
 * (Adapted from Sun's decode.c.)
 */
int
unpack_input(ft, code)
ft_t			ft;
unsigned char		*code;
{
	struct aupriv		*p = (struct aupriv *) ft->priv;
	unsigned char		in_byte;

	if (p->in_bits < p->dec_bits) {
		if (fread(&in_byte, sizeof (char), 1, ft->fp) != 1) {
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

LONG auread(ft, buf, samp)
ft_t ft;
LONG *buf, samp;
{
	struct aupriv *p = (struct aupriv *) ft->priv;
	unsigned char code;
	int done;
	if (p->dec_routine == NULL)
		return rawread(ft, buf, samp);
	done = 0;
	while (samp > 0 && unpack_input(ft, &code) >= 0) {
		*buf++ = LEFT((*p->dec_routine)(code, AUDIO_ENCODING_LINEAR,
						&p->state),
			      16);
		samp--;
		done++;
	}
	return done;
}

void auwrite(ft, buf, samp)
ft_t ft;
LONG *buf, samp;
{
	struct aupriv *p = (struct aupriv *) ft->priv;
	p->data_size += samp * ft->info.size;
	rawwrite(ft, buf, samp);
}

void austopwrite(ft)
ft_t ft;
{
	struct aupriv *p = (struct aupriv *) ft->priv;
	if (!ft->seekable)
		return;
	if (fseek(ft->fp, 0L, 0) != 0)
		fail("Can't rewind output file to rewrite Sun header.");
	auwriteheader(ft, p->data_size);
}

void auwriteheader(ft, data_size)
ft_t ft;
ULONG data_size;
{
	ULONG magic;
	ULONG hdr_size;
	ULONG encoding;
	ULONG sample_rate;
	ULONG channels;

	if (ft->info.style == ULAW && ft->info.size == BYTE)
		encoding = SUN_ULAW;
	else if (ft->info.style == SIGN2 && ft->info.size == BYTE)
		encoding = SUN_LIN_8;
	else if (ft->info.style == SIGN2 && ft->info.size == WORD)
		encoding = SUN_LIN_16;
	else {
		report("Unsupported output style/size for Sun/NeXT header or .AU format not specified.");
		report("Only U-law, signed bytes, and signed words are supported.");
		report("Defaulting to 8khz u-law\n");
		encoding = SUN_ULAW;
		ft->info.style = ULAW;
		ft->info.size = BYTE;
		ft->info.rate = 8000;  /* strange but true */
	}

	magic = SUN_MAGIC;
	wblong(ft, magic);

	if (ft->comment == NULL)
		ft->comment = "";
	hdr_size = SUN_HDRSIZE + strlen(ft->comment);
	wblong(ft, hdr_size);

	wblong(ft, data_size);

	wblong(ft, encoding);

	sample_rate = ft->info.rate;
	wblong(ft, sample_rate);

	channels = ft->info.channels;
	wblong(ft, channels);

	fputs(ft->comment, ft->fp);
}

