/*
 * Sound Tools Macintosh HCOM format.
 * These are really FSSD type files with Huffman compression,
 * in MacBinary format.
 * To do: make the MacBinary format optional (so that .data files
 * are also acceptable).  (How to do this on output?)
 *
 * September 25, 1991
 * Copyright 1991 Guido van Rossum And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Guido van Rossum And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 *
 * April 28, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *
 *  Rearranged some functions so that they are declared before they are
 *  used.  Clears up some compiler warnings.  Because this functions passed
 *  foats, it helped out some dump compilers pass stuff on the stack
 *  correctly.
 *
 */

#include "st.h"
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

/* Dictionary entry for Huffman (de)compression */
typedef struct {
	LONG frequ;
	short dict_leftson;
	short dict_rightson;
} dictent;

/* Private data used by reader */
struct readpriv {
	/* Static data from the header */
	dictent *dictionary;
	LONG checksum;
	int deltacompression;
	/* Engine state */
	LONG huffcount;
	LONG cksum;
	int dictentry;
	int nrbits;
	ULONG current;
	short sample;
};

void skipbytes(P2(ft_t, int));

void hcomstartread(ft)
ft_t ft;
{
	struct readpriv *p = (struct readpriv *) ft->priv;
	int i;
	char buf[4];
	ULONG datasize, rsrcsize;
	ULONG huffcount, checksum, compresstype, divisor;
	unsigned short dictsize;

	int littlendian = 1;
	char *endptr;


	endptr = (char *) &littlendian;
	/* hcom is in big endian format.  Swap whats
	 * read in on little machine
	 */
	if (*endptr)
	{
		ft->swap = ft->swap ? 0 : 1;
	}

	/* Skip first 65 bytes of header */
	skipbytes(ft, 65);

	/* Check the file type (bytes 65-68) */
	if (fread(buf, 1, 4, ft->fp) != 4 || strncmp(buf, "FSSD", 4) != 0)
		fail("Mac header type is not FSSD");

	/* Skip to byte 83 */
	skipbytes(ft, 83-69);

	/* Get essential numbers from the header */
	datasize = rlong(ft); /* bytes 83-86 */
	rsrcsize = rlong(ft); /* bytes 87-90 */

	/* Skip the rest of the header (total 128 bytes) */
	skipbytes(ft, 128-91);

	/* The data fork must contain a "HCOM" header */
	if (fread(buf, 1, 4, ft->fp) != 4 || strncmp(buf, "HCOM", 4) != 0)
		fail("Mac data fork is not HCOM");

	/* Then follow various parameters */
	huffcount = rlong(ft);
	checksum = rlong(ft);
	compresstype = rlong(ft);
	if (compresstype > 1)
		fail("Bad compression type in HCOM header");
	divisor = rlong(ft);
	if (divisor == 0 || divisor > 4)
		fail("Bad sampling rate divisor in HCOM header");
	dictsize = rshort(ft);

	/* Translate to sox parameters */
	ft->info.style = UNSIGNED;
	ft->info.size = BYTE;
	ft->info.rate = 22050 / divisor;
	ft->info.channels = 1;

	/* Allocate memory for the dictionary */
	p->dictionary = (dictent *) malloc(511 * sizeof(dictent));
	if (p->dictionary == NULL)
		fail("can't malloc memory for Huffman dictionary");

	/* Read dictionary */
	for(i = 0; i < dictsize; i++) {
		p->dictionary[i].dict_leftson = rshort(ft);
		p->dictionary[i].dict_rightson = rshort(ft);
		/*
		report("%d %d",
		       p->dictionary[i].dict_leftson,
		       p->dictionary[i].dict_rightson);
		       */
	}
	skipbytes(ft, 1); /* skip pad byte */

	/* Initialized the decompression engine */
	p->checksum = checksum;
	p->deltacompression = compresstype;
	if (!p->deltacompression)
		report("HCOM data using value compression");
	p->huffcount = huffcount;
	p->cksum = 0;
	p->dictentry = 0;
	p->nrbits = -1; /* Special case to get first byte */
}

void skipbytes(ft, n)
ft_t ft;
int n;
{
	while (--n >= 0) {
		if (getc(ft->fp) == EOF)
			fail("unexpected EOF in Mac header");
	}
}

int hcomread(ft, buf, len)
ft_t ft;
LONG *buf, len;
{
	register struct readpriv *p = (struct readpriv *) ft->priv;
	int done = 0;

	if (p->nrbits < 0) {
		/* The first byte is special */
		if (p->huffcount == 0)
			return 0; /* Don't know if this can happen... */
		p->sample = getc(ft->fp);
		if (p->sample == EOF)
			fail("unexpected EOF at start of HCOM data");
		*buf++ = (p->sample - 128) * 0x1000000L;
		p->huffcount--;
		p->nrbits = 0;
		done++;
		len--;
		if (len == 0)
			return done;
	}

	while (p->huffcount > 0) {
		if(p->nrbits == 0) {
			p->current = rlong(ft);
			if (feof(ft->fp))
				fail("unexpected EOF in HCOM data");
			p->cksum += p->current;
			p->nrbits = 32;
		}
		if(p->current & 0x80000000L) {
			p->dictentry =
				p->dictionary[p->dictentry].dict_rightson;
		} else {
			p->dictentry =
				p->dictionary[p->dictentry].dict_leftson;
		}
		p->current = p->current << 1;
		p->nrbits--;
		if(p->dictionary[p->dictentry].dict_leftson < 0) {
			short datum;
			datum = p->dictionary[p->dictentry].dict_rightson;
			if (!p->deltacompression)
				p->sample = 0;
			p->sample = (p->sample + datum) & 0xff;
			p->huffcount--;
			if (p->sample == 0)
				*buf++ = -127 * 0x1000000L;
			else
				*buf++ = (p->sample - 128) * 0x1000000L;
			p->dictentry = 0;
			done++;
			len--;
			if (len == 0)
				break;
		}
	}

	return done;
}

void hcomstopread(ft) 
ft_t ft;
{
	register struct readpriv *p = (struct readpriv *) ft->priv;

	if (p->huffcount != 0)
		fail("not all HCOM data read");
	if(p->cksum != p->checksum)
		fail("checksum error in HCOM data");
	free((char *)p->dictionary);
	p->dictionary = NULL;
}

struct writepriv {
	unsigned char *data;	/* Buffer allocated with malloc */
	unsigned int size;	/* Size of allocated buffer */
	unsigned int pos;	/* Where next byte goes */
};

#define BUFINCR (10*BUFSIZ)

void hcomstartwrite(ft) 
ft_t ft;
{
	register struct writepriv *p = (struct writepriv *) ft->priv;

	int littlendian = 1;
	char *endptr;

	endptr = (char *) &littlendian;
	/* hcom is inbigendian format.  Swap whats
	 * read in on little endian machines.
	 */
	if (*endptr)
	{
		ft->swap = ft->swap ? 0 : 1;
	}

	switch (ft->info.rate) {
	case 22050:
	case 22050/2:
	case 22050/3:
	case 22050/4:
		break;
	default:
		fail("unacceptable output rate for HCOM: try 5512, 7350, 11025 or 22050 hertz");
	}
	ft->info.size = BYTE;
	ft->info.style = UNSIGNED;
	ft->info.channels = 1;

	p->size = BUFINCR;
	p->pos = 0;
	p->data = (unsigned char *) malloc(p->size);
	if (p->data == NULL)
		fail("can't malloc buffer for uncompressed HCOM data");
}

void hcomwrite(ft, buf, len)
ft_t ft;
LONG *buf, len;
{
	register struct writepriv *p = (struct writepriv *) ft->priv;
	LONG datum;

	if (p->pos + len > p->size) {
		p->size = ((p->pos + len) / BUFINCR + 1) * BUFINCR;
		p->data = (unsigned char *) realloc(p->data, p->size);
		if (p->data == NULL)
		    fail("can't realloc buffer for uncompressed HCOM data");
	}

	while (--len >= 0) {
		datum = *buf++;
		datum >>= 24;
		datum ^= 128;
		p->data[p->pos++] = datum;
	}
}

/* Some global compression stuff hcom uses.  hcom currently has problems */
/* compiling here.  It could really use some cleaning up by someone that */
/* understands this format. */

/* XXX This uses global variables -- one day these should all be
   passed around in a structure instead. */

dictent dictionary[511];
dictent *de;
LONG codes[256];
LONG codesize[256];
LONG checksum;

void makecodes(e, c, s, b)
int e, c, s, b;
{
  if(dictionary[e].dict_leftson < 0) {
    codes[dictionary[e].dict_rightson] = c;
    codesize[dictionary[e].dict_rightson] = s;
  } else {
    makecodes(dictionary[e].dict_leftson, c, s + 1, b << 1);
    makecodes(dictionary[e].dict_rightson, c + b, s + 1, b << 1);
  }
}

LONG curword;
int nbits;

void putlong(c, v)
unsigned char *c;
LONG v;
{
  *c++ = (v >> 24) & 0xff;
  *c++ = (v >> 16) & 0xff;
  *c++ = (v >> 8) & 0xff;
  *c++ = v & 0xff;
}

void putshort(c, v)
unsigned char *c;
short v;
{
  *c++ = (v >> 8) & 0xff;
  *c++ = v & 0xff;
}


void putcode(c, df)
unsigned char c;
unsigned char ** df;
{
LONG code, size;
int i;
  code = codes[c];
  size = codesize[c];
  for(i = 0; i < size; i++) {
    curword = (curword << 1);
    if(code & 1) curword += 1;
    nbits++;
    if(nbits == 32) {
      putlong(*df, curword);
      checksum += curword;
      (*df) += 4;
      nbits = 0;
      curword = 0;
    }
    code = code >> 1;
  }
}

void compress(df, dl, fr)
unsigned char **df;
LONG *dl;
float fr;
{
  LONG samplerate;
  unsigned char *datafork = *df;
  unsigned char *ddf;
  short dictsize;
  int frequtable[256];
  int i, sample, j, k, d, l, frequcount;

  sample = *datafork;
  for(i = 0; i < 256; i++) frequtable[i] = 0;
  for(i = 1; i < *dl; i++) {
    d = (datafork[i] - (sample & 0xff)) & 0xff; /* creates absolute entries LMS */
    sample = datafork[i];
    datafork[i] = d;
#if 0				/* checking our table is accessed correctly */
    if(d < 0 || d > 255)
      printf("d is outside array bounds %d\n", d);
#endif
    frequtable[d]++;
  }
  de = dictionary;
  for(i = 0; i < 256; i++) if(frequtable[i] != 0) {
    de->frequ = -frequtable[i];
    de->dict_leftson = -1;
    de->dict_rightson = i;
    de++;
  }
  frequcount = de - dictionary;
  for(i = 0; i < frequcount; i++) {
    for(j = i + 1; j < frequcount; j++) {
      if(dictionary[i].frequ > dictionary[j].frequ) {
        k = dictionary[i].frequ;
        dictionary[i].frequ = dictionary[j].frequ;
        dictionary[j].frequ = k;
        k = dictionary[i].dict_leftson;
        dictionary[i].dict_leftson = dictionary[j].dict_leftson;
        dictionary[j].dict_leftson = k;
        k = dictionary[i].dict_rightson;
        dictionary[i].dict_rightson = dictionary[j].dict_rightson;
        dictionary[j].dict_rightson = k;
      }
    }
  }
  while(frequcount > 1) {
    j = frequcount - 1;
    de->frequ = dictionary[j - 1].frequ;
    de->dict_leftson = dictionary[j - 1].dict_leftson;
    de->dict_rightson = dictionary[j - 1].dict_rightson;
    l = dictionary[j - 1].frequ + dictionary[j].frequ;
    for(i = j - 2; i >= 0; i--) {
      if(l >= dictionary[i].frequ) break;
      dictionary[i + 1] = dictionary[i];
    }
    i = i + 1;
    dictionary[i].frequ = l;
    dictionary[i].dict_leftson = j;
    dictionary[i].dict_rightson = de - dictionary;
    de++;
    frequcount--;
  }
  dictsize = de - dictionary;
  for(i = 0; i < 256; i++) {
    codes[i] = 0;
    codesize[i] = 0;
  }
  makecodes(0, 0, 0, 1);
  l = 0;
  for(i = 0; i < 256; i++) {
	  l += frequtable[i] * codesize[i];
  }
  l = (((l + 31) >> 5) << 2) + 24 + dictsize * 4;
  report("  Original size: %6d bytes", *dl);
  report("Compressed size: %6d bytes", l);
  if((datafork = (unsigned char *)malloc((unsigned)l)) == NULL)
    fail("can't malloc buffer for compressed HCOM data");
  ddf = datafork + 22;
  for(i = 0; i < dictsize; i++) {
    putshort(ddf, dictionary[i].dict_leftson);
    ddf += 2;
    putshort(ddf, dictionary[i].dict_rightson);
    ddf += 2;
  }
  *ddf++ = 0;
  *ddf++ = *(*df)++;
  checksum = 0;
  nbits = 0;
  curword = 0;
  for(i = 1; i < *dl; i++) putcode(*(*df)++, &ddf);
  if(nbits != 0) {
    codes[0] = 0;
    codesize[0] = 32 - nbits;
    putcode(0, &ddf);
  }
  strncpy((char *) datafork, "HCOM", 4);
  putlong(datafork + 4, *dl);
  putlong(datafork + 8, checksum);
  putlong(datafork + 12, 1L);
  samplerate = 22050 / (LONG)fr;
  putlong(datafork + 16, samplerate);
  putshort(datafork + 20, dictsize);
  *df = datafork;		/* reassign passed pointer to new datafork */
  *dl = l;			/* and its compressed length */
}

void padbytes(ft, n)
ft_t ft;
int n;
{
	while (--n >= 0)
		putc('\0', ft->fp);
}


/* End of hcom utility routines */

void hcomstopwrite(ft) 
ft_t ft;
{
	register struct writepriv *p = (struct writepriv *) ft->priv;
	unsigned char *compressed_data = p->data;
	LONG compressed_len = p->pos;

	/* Compress it all at once */
	compress(&compressed_data, &compressed_len, (double) ft->info.rate);
	free((char *) p->data);

	/* Write the header */
	(void) fwrite("\000\001A", 1, 3, ft->fp); /* Dummy file name "A" */
	padbytes(ft, 65-3);
	(void) fwrite("FSSD", 1, 4, ft->fp);
	padbytes(ft, 83-69);
	wlong(ft, (ULONG) compressed_len); /* compressed_data size */
	wlong(ft, (ULONG) 0); /* rsrc size */
	padbytes(ft, 128 - 91);
	if (ferror(ft->fp))
		fail("write error in HCOM header");

	/* Write the compressed_data fork */
	if (fwrite((char *) compressed_data, 1, (int)compressed_len, ft->fp) != compressed_len)
		fail("can't write compressed HCOM data");
	free((char *) compressed_data);

	/* Pad the compressed_data fork to a multiple of 128 bytes */
	padbytes(ft, 128 - (int) (compressed_len%128));
}

