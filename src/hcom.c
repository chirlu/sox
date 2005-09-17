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

#include "st_i.h"
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

/* Dictionary entry for Huffman (de)compression */
typedef struct {
        long frequ;
        short dict_leftson;
        short dict_rightson;
} dictent;

/* Private data used by reader */
struct readpriv {
        /* Static data from the header */
        dictent *dictionary;
        int32_t checksum;
        int deltacompression;
        /* Engine state */
        long huffcount;
        long cksum;
        int dictentry;
        int nrbits;
        uint32_t current;
        short sample;
};

static int skipbytes(ft_t, int);

int st_hcomstartread(ft_t ft)
{
        struct readpriv *p = (struct readpriv *) ft->priv;
        int i;
        char buf[5];
        uint32_t datasize, rsrcsize;
        uint32_t huffcount, checksum, compresstype, divisor;
        unsigned short dictsize;
        int rc;


        /* hcom is in big endian format.  Swap whats
         * read in on little machine
         */
        if (ST_IS_LITTLEENDIAN)
        {
                ft->swap = ft->swap ? 0 : 1;
        }

        /* Skip first 65 bytes of header */
        rc = skipbytes(ft, 65);
        if (rc)
            return rc;

        /* Check the file type (bytes 65-68) */
        if (st_reads(ft, buf, 4) == ST_EOF || strncmp(buf, "FSSD", 4) != 0)
        {
                st_fail_errno(ft,ST_EHDR,"Mac header type is not FSSD");
                return (ST_EOF);
        }

        /* Skip to byte 83 */
        rc = skipbytes(ft, 83-69);
        if (rc)
            return rc;

        /* Get essential numbers from the header */
        st_readdw(ft, &datasize); /* bytes 83-86 */
        st_readdw(ft, &rsrcsize); /* bytes 87-90 */

        /* Skip the rest of the header (total 128 bytes) */
        rc = skipbytes(ft, 128-91);
        if (rc != 0)
            return rc;

        /* The data fork must contain a "HCOM" header */
        if (st_reads(ft, buf, 4) == ST_EOF || strncmp(buf, "HCOM", 4) != 0)
        {
                st_fail_errno(ft,ST_EHDR,"Mac data fork is not HCOM");
                return (ST_EOF);
        }

        /* Then follow various parameters */
        st_readdw(ft, &huffcount);
        st_readdw(ft, &checksum);
        st_readdw(ft, &compresstype);
        if (compresstype > 1)
        {
                st_fail_errno(ft,ST_EHDR,"Bad compression type in HCOM header");
                return (ST_EOF);
        }
        st_readdw(ft, &divisor);
        if (divisor == 0 || divisor > 4)
        {
                st_fail_errno(ft,ST_EHDR,"Bad sampling rate divisor in HCOM header");
                return (ST_EOF);
        }
        st_readw(ft, &dictsize);

        /* Translate to sox parameters */
        ft->info.encoding = ST_ENCODING_UNSIGNED;
        ft->info.size = ST_SIZE_BYTE;
        ft->info.rate = 22050 / divisor;
        ft->info.channels = 1;

        /* Allocate memory for the dictionary */
        p->dictionary = (dictent *) malloc(511 * sizeof(dictent));
        if (p->dictionary == NULL)
        {
                st_fail_errno(ft,ST_ENOMEM,"can't malloc memory for Huffman dictionary");
                return (ST_EOF);
        }

        /* Read dictionary */
        for(i = 0; i < dictsize; i++) {
                st_readw(ft, (unsigned short *)&(p->dictionary[i].dict_leftson));
                st_readw(ft, (unsigned short *)&(p->dictionary[i].dict_rightson));
                /*
                st_report("%d %d",
                       p->dictionary[i].dict_leftson,
                       p->dictionary[i].dict_rightson);
                       */
        }
        rc = skipbytes(ft, 1); /* skip pad byte */
        if (rc)
            return rc;

        /* Initialized the decompression engine */
        p->checksum = checksum;
        p->deltacompression = compresstype;
        if (!p->deltacompression)
                st_report("HCOM data using value compression");
        p->huffcount = huffcount;
        p->cksum = 0;
        p->dictentry = 0;
        p->nrbits = -1; /* Special case to get first byte */

        return (ST_SUCCESS);
}

/* FIXME: Move to misc.c */
static int skipbytes(ft_t ft, int n)
{
    unsigned char trash;
        while (--n >= 0) {
                if (st_readb(ft, &trash) == ST_EOF)
                {
                        st_fail_errno(ft,ST_EOF,"unexpected EOF in Mac header");
                        return(ST_EOF);
                }
        }
        return(ST_SUCCESS);
}

st_ssize_t st_hcomread(ft_t ft, st_sample_t *buf, st_ssize_t len)
{
        register struct readpriv *p = (struct readpriv *) ft->priv;
        int done = 0;
        unsigned char sample_rate;

        if (p->nrbits < 0) {
                /* The first byte is special */
                if (p->huffcount == 0)
                        return 0; /* Don't know if this can happen... */
                if (st_readb(ft, &sample_rate) == ST_EOF)
                {
                        st_fail_errno(ft,ST_EOF,"unexpected EOF at start of HCOM data");
                        return (0);
                }
                p->sample = sample_rate;
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
                        st_readdw(ft, &(p->current));
                        if (st_eof(ft))
                        {
                                st_fail_errno(ft,ST_EOF,"unexpected EOF in HCOM data");
                                return (0);
                        }
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

int st_hcomstopread(ft_t ft)
{
        register struct readpriv *p = (struct readpriv *) ft->priv;

        if (p->huffcount != 0)
        {
                st_fail_errno(ft,ST_EFMT,"not all HCOM data read");
                return (ST_EOF);
        }
        if(p->cksum != p->checksum)
        {
                st_fail_errno(ft,ST_EFMT,"checksum error in HCOM data");
                return (ST_EOF);
        }
        free((char *)p->dictionary);
        p->dictionary = NULL;
        return (ST_SUCCESS);
}

struct writepriv {
        unsigned char *data;    /* Buffer allocated with malloc */
        unsigned int size;      /* Size of allocated buffer */
        unsigned int pos;       /* Where next byte goes */
};

#define BUFINCR (10*BUFSIZ)

int st_hcomstartwrite(ft_t ft)
{
        register struct writepriv *p = (struct writepriv *) ft->priv;

        /* hcom is inbigendian format.  Swap whats
         * read in on little endian machines.
         */
        if (ST_IS_LITTLEENDIAN)
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
                st_fail_errno(ft,ST_EFMT,"unacceptable output rate for HCOM: try 5512, 7350, 11025 or 22050 hertz");
                return (ST_EOF);
        }
        ft->info.size = ST_SIZE_BYTE;
        ft->info.encoding = ST_ENCODING_UNSIGNED;
        ft->info.channels = 1;

        p->size = BUFINCR;
        p->pos = 0;
        p->data = (unsigned char *) malloc(p->size);
        if (p->data == NULL)
        {
                st_fail_errno(ft,ST_ENOMEM,"can't malloc buffer for uncompressed HCOM data");
                return (ST_EOF);
        }
        return (ST_SUCCESS);
}

st_ssize_t st_hcomwrite(ft_t ft, st_sample_t *buf, st_ssize_t len)
{
        register struct writepriv *p = (struct writepriv *) ft->priv;
        st_sample_t datum;
        st_ssize_t save_len = len;

        if (len == 0)
            return (0);

        if (p->pos + len > p->size) {
                p->size = ((p->pos + len) / BUFINCR + 1) * BUFINCR;
                p->data = (unsigned char *) realloc(p->data, p->size);
                if (p->data == NULL)
                {
                    st_fail_errno(ft,ST_ENOMEM,"can't realloc buffer for uncompressed HCOM data");
                    return (0);
                }
        }

        while (--len >= 0) {
                datum = *buf++;
                datum >>= 24;
                datum ^= 128;
                p->data[p->pos++] = datum;
        }

        return (save_len - len);
}

/* Some global compression stuff hcom uses.  hcom currently has problems */
/* compiling here.  It could really use some cleaning up by someone that */
/* understands this format. */

/* XXX This uses global variables -- one day these should all be
   passed around in a structure instead. Use static so we don't
   polute global name space. */

/* SJB: FIXME: dangerous static variables, need to analyse code */

static dictent dictionary[511];
static dictent *de;
static long codes[256];
static long codesize[256];
static int32_t checksum;

static void makecodes(int e, int c, int s, int b)
{
  if(dictionary[e].dict_leftson < 0) {
    codes[dictionary[e].dict_rightson] = c;
    codesize[dictionary[e].dict_rightson] = s;
  } else {
    makecodes(dictionary[e].dict_leftson, c, s + 1, b << 1);
    makecodes(dictionary[e].dict_rightson, c + b, s + 1, b << 1);
  }
}

static int nbits;
static int32_t curword;

/* FIXME: Place in misc.c */
static void putlong(unsigned char *c, int32_t v)
{
  *c++ = (v >> 24) & 0xff;
  *c++ = (v >> 16) & 0xff;
  *c++ = (v >> 8) & 0xff;
  *c++ = v & 0xff;
}

static void putshort(unsigned char *c, short v)
{
  *c++ = (v >> 8) & 0xff;
  *c++ = v & 0xff;
}


static void putcode(unsigned char c, unsigned char **df)
{
long code, size;
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

static int compress(unsigned char **df, int32_t *dl, float fr)
{
  int32_t samplerate;
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
#if 0                           /* checking our table is accessed correctly */
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
  st_report("  Original size: %6d bytes", *dl);
  st_report("Compressed size: %6d bytes", l);
  if((datafork = (unsigned char *)malloc((unsigned)l)) == NULL)
  {
    return (ST_ENOMEM);
  }
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
  samplerate = 22050 / (int32_t)fr;
  putlong(datafork + 16, samplerate);
  putshort(datafork + 20, dictsize);
  *df = datafork;               /* reassign passed pointer to new datafork */
  *dl = l;                      /* and its compressed length */

  return (ST_SUCCESS);
}

/* FIXME: Place in misc.c */
static void padbytes(ft_t ft, int n)
{
        while (--n >= 0)
            st_writeb(ft, '\0');
}


/* End of hcom utility routines */

int st_hcomstopwrite(ft_t ft)
{
        register struct writepriv *p = (struct writepriv *) ft->priv;
        unsigned char *compressed_data = p->data;
        uint32_t compressed_len = p->pos;
        int rc;

        /* Compress it all at once */
        rc = compress(&compressed_data, (int32_t *)&compressed_len, (double) ft->info.rate);
        free((char *) p->data);

        if (rc){
        st_fail_errno(ft, rc,"can't malloc buffer for compressed HCOM data");
            return 0;
        }

        /* Write the header */
        st_writebuf(ft, (void *)"\000\001A", 1, 3); /* Dummy file name "A" */
        padbytes(ft, 65-3);
        st_writes(ft, "FSSD");
        padbytes(ft, 83-69);
        st_writedw(ft, (uint32_t) compressed_len); /* compressed_data size */
        st_writedw(ft, (uint32_t) 0); /* rsrc size */
        padbytes(ft, 128 - 91);
        if (st_error(ft))
        {
                st_fail_errno(ft,errno,"write error in HCOM header");
                return (ST_EOF);
        }

        /* Write the compressed_data fork */
        if (st_writebuf(ft, compressed_data, 1, (int)compressed_len) != compressed_len)
        {
                st_fail_errno(ft,errno,"can't write compressed HCOM data");
                rc = ST_EOF;
        }
        else
            rc = ST_SUCCESS;
        free((char *) compressed_data);

        if (rc)
            return rc;

        /* Pad the compressed_data fork to a multiple of 128 bytes */
        padbytes(ft, 128 - (int) (compressed_len%128));

        return (ST_SUCCESS);
}
