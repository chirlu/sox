/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools MAUD file format driver, by Lutz Vieweg 1993
 *
 * supports: mono and stereo, linear, a-lawa and u-law reading and writing
 *
 */

#include "st.h"
#include "libst.h"
#include <string.h>
#include <stdlib.h>

#define SEEK_CUR 1		/* nasty nasty */

/* Private data for MAUD file */
struct maudstuff { /* max. 100 bytes!!!! */
	ULONG nsamples;
};

void maudwriteheader(P1(ft_t));
void rawwrite(P3(ft_t, LONG *, LONG));

/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *	Find out sampling rate, 
 *	size and style of samples, 
 *	mono/stereo/quad.
 */
void maudstartread(ft) 
ft_t ft;
{
	struct maudstuff * p = (struct maudstuff *) ft->priv;
	
	char buf[12];
	char *endptr;
	char *chunk_buf;
	
	unsigned short bitpersam;
	ULONG nom;
	unsigned short denom;
	unsigned short chaninf;
	
	ULONG chunksize;
	
	int littlendian = 0;
	
	/* read FORM chunk */
	if (fread(buf, 1, 4, ft->fp) != 4 || strncmp(buf, "FORM", 4) != 0)
		fail("MAUD: header does not begin with magic word 'FORM'");
	
	rblong(ft); /* totalsize */
	
	if (fread(buf, 1, 4, ft->fp) != 4 || strncmp(buf, "MAUD", 4) != 0)
		fail("MAUD: 'FORM' chunk does not specify 'MAUD' as type");
	
	/* read chunks until 'BODY' (or end) */
	
	while (fread(buf,1,4,ft->fp) == 4 && strncmp(buf,"MDAT",4) != 0) {
		
		/*
		buf[4] = 0;
		report("chunk %s",buf);
		*/
		
		if (strncmp(buf,"MHDR",4) == 0) {
			
			chunksize = rblong(ft);
			if (chunksize != 8*4) fail ("MAUD: MHDR chunk has bad size");
			
			/* fseek(ft->fp,12,SEEK_CUR); */
			
			p->nsamples = rblong(ft); /* number of samples stored in MDAT */
			bitpersam = rbshort(ft);  /* number of bits per sample as stored in MDAT */
			rbshort(ft);              /* number of bits per sample after decompression */
			nom = rblong(ft);         /* clock source frequency */
			denom = rbshort(ft);       /* clock devide           */
			if (denom == 0) fail("MAUD: frequency denominator == 0, failed");
			
			ft->info.rate = nom / denom;
			
			chaninf = rbshort(ft); /* channel information */
			switch (chaninf) {
			case 0:
				ft->info.channels = 1;
				break;
			case 1:
				ft->info.channels = 2;
				break;
			default:
				fail("MAUD: unsupported number of channels in file");
				break;
			}
			
			chaninf = rbshort(ft); /* number of channels (mono: 1, stereo: 2, ...) */
			if (chaninf != ft->info.channels) fail("MAUD: unsupported number of channels in file");
			
			chaninf = rbshort(ft); /* compression type */
			
			rblong(ft); /* rest of chunk, unused yet */
			rblong(ft);
			rblong(ft);
			
			if (bitpersam == 8 && chaninf == 0) {
				ft->info.size = BYTE;
				ft->info.style = UNSIGNED;
			}
			else if (bitpersam == 8 && chaninf == 2) {
				ft->info.size = BYTE;
				ft->info.style = ALAW;
			}
			else if (bitpersam == 8 && chaninf == 3) {
				ft->info.size = BYTE;
				ft->info.style = ULAW;
			}
			else if (bitpersam == 16 && chaninf == 0) {
				ft->info.size = WORD;
				ft->info.style = SIGN2;
			}
			else fail("MAUD: unsupported compression type detected");
			
			ft->comment = 0;
			
			continue;
		}
		
		if (strncmp(buf,"ANNO",4) == 0) {
			chunksize = rblong(ft);
			if (chunksize & 1)
				chunksize++;
			chunk_buf = (char *) malloc(chunksize + 1);
			if (fread(chunk_buf,1,(int)chunksize,ft->fp) 
					!= chunksize)
				fail("MAUD: Unexpected EOF in ANNO header");
			chunk_buf[chunksize] = '\0';
			report ("%s",chunk_buf);
			free(chunk_buf);
			
			continue;
		}
		
		/* some other kind of chunk */
		chunksize = rblong(ft);
		if (chunksize & 1)
			chunksize++;
		fseek(ft->fp,chunksize,SEEK_CUR);
		continue;
		
	}
	
	if (strncmp(buf,"MDAT",4) != 0) fail("MAUD: MDAT chunk not found");
	p->nsamples = rblong(ft);
	
	endptr = (char *) &littlendian;
	*endptr = 1;
	if (littlendian == 1) ft->swap = 1;
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

LONG maudread(ft, buf, len) 
ft_t ft;
LONG *buf, len;
{
	register int datum;
	int done = 0;
	
	if (ft->info.channels == 1) {
		if (ft->info.size == BYTE) {
			switch(ft->info.style) {
			case UNSIGNED:
				while(done < len) {
					datum = getc(ft->fp);
					if (feof(ft->fp)) return done;
					/* Convert to signed */
					datum ^= 128;
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 24);
					done++;
				}
				break;
			case ULAW:
				/* grab table from Posk stuff */
				while(done < len) {
					datum = getc(ft->fp);
					if (feof(ft->fp)) return done;
					datum = st_ulaw_to_linear(datum);
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 16);
					done++;
				}
				break;
			case ALAW:
				while(done < len) {
					datum = st_Alaw_to_linear((unsigned char) getc(ft->fp));
					if (feof(ft->fp)) return done;
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 16);
					done++;
				}
				break;
			}
		}
		else {
			while(done < len) {
				datum = rbshort(ft);
				if (feof(ft->fp)) return done;
				/* scale signed up to long's range */
				*buf++ = LEFT(datum, 16);
				done++;
			}
		}
	}
	else { /* stereo */
		if (ft->info.size == BYTE) {
			switch(ft->info.style) {
			case UNSIGNED:
				while(done < len) {
					datum = getc(ft->fp);
					if (feof(ft->fp)) return done;
					/* Convert to signed */
					datum ^= 128;
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 24);
					
					datum = getc(ft->fp);
					if (feof(ft->fp)) return done;
					/* Convert to signed */
					datum ^= 128;
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 24);
					done += 2;
				}
				break;
			case ULAW:
				/* grab table from Posk stuff */
				while(done < len) {
					datum = getc(ft->fp);
					if (feof(ft->fp)) return done;
					datum = st_ulaw_to_linear(datum);
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 16);
					
					datum = getc(ft->fp);
					if (feof(ft->fp)) return done;
					datum = st_ulaw_to_linear(datum);
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 16);
					done += 2;
				}
				break;
			case ALAW:
				while(done < len) {
					datum = st_Alaw_to_linear((unsigned char) getc(ft->fp));
					if (feof(ft->fp)) return done;
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 16);
					
					datum = st_Alaw_to_linear((unsigned char) getc(ft->fp));
					if (feof(ft->fp)) return done;
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 16);
					done += 2;
				}
				break;
			}
		}
		else {
			while(done < len) {
				datum = rbshort(ft);
				if (feof(ft->fp)) return done;
				/* scale signed up to long's range */
				*buf++ = LEFT(datum, 16);
				
				datum = rbshort(ft);
				if (feof(ft->fp)) return done;
				/* scale signed up to long's range */
				*buf++ = LEFT(datum, 16);
				done += 2;
			}
		}
	}
	return done;
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
void maudstopread(ft) 
ft_t ft;
{
}

void maudstartwrite(ft) 
ft_t ft;
{
	struct maudstuff * p = (struct maudstuff *) ft->priv;
	int littlendian = 0;
	char *endptr;
	
	/* If you have to seek around the output file */
	if (! ft->seekable) fail("Output .maud file must be a file, not a pipe");
	
	if (ft->info.channels != 1 && ft->info.channels != 2) {
		fail("MAUD: unsupported number of channels, unable to store");
	}
	if (ft->info.size == WORD) ft->info.style = SIGN2;
	if (ft->info.style == ULAW || ft->info.style == ALAW) ft->info.size = BYTE;
	if (ft->info.size == BYTE && ft->info.style == SIGN2) ft->info.style = UNSIGNED;
	
	p->nsamples = 0x7f000000L;
	maudwriteheader(ft);
	p->nsamples = 0;
	
	endptr = (char *) &littlendian;
	*endptr = 1;
	if (littlendian == 1) ft->swap = 1;
}

void maudwrite(ft, buf, len) 
ft_t ft;
LONG *buf, len;
{
	struct maudstuff * p = (struct maudstuff *) ft->priv;
	
	p->nsamples += len;
	
	rawwrite(ft, buf, len);
}

void maudstopwrite(ft) 
ft_t ft;
{
	/* All samples are already written out. */
	
	if (fseek(ft->fp, 0L, 0) != 0) fail("can't rewind output file to rewrite MAUD header");
	
	maudwriteheader(ft);
}

#define MAUDHEADERSIZE (4+(4+4+32)+(4+4+32)+(4+4))
void maudwriteheader(ft)
ft_t ft;
{
	struct maudstuff * p = (struct maudstuff *) ft->priv;
	
	fputs ("FORM", ft->fp);
	wblong(ft, (p->nsamples*ft->info.size) + MAUDHEADERSIZE);  /* size of file */
	fputs("MAUD", ft->fp); /* File type */
	
	fputs ("MHDR", ft->fp);
	wblong(ft, (LONG) 8*4); /* number of bytes to follow */
	wblong(ft, (LONG) (p->nsamples ));  /* number of samples stored in MDAT */
	
	switch (ft->info.style) {
		
	case UNSIGNED:
		wbshort(ft, (int) 8); /* number of bits per sample as stored in MDAT */
		wbshort(ft, (int) 8); /* number of bits per sample after decompression */
		break;
		
	case SIGN2:
		wbshort(ft, (int) 16); /* number of bits per sample as stored in MDAT */
		wbshort(ft, (int) 16); /* number of bits per sample after decompression */
		break;
		
	case ALAW:
	case ULAW:
		wbshort(ft, (int) 8); /* number of bits per sample as stored in MDAT */
		wbshort(ft, (int) 16); /* number of bits per sample after decompression */
		break;
		
	}
	
	wblong(ft, (LONG) ft->info.rate); /* clock source frequency */
	wbshort(ft, (int) 1); /* clock devide */
	
	if (ft->info.channels == 1) {
		wbshort(ft, (int) 0); /* channel information */
		wbshort(ft, (int) 1); /* number of channels (mono: 1, stereo: 2, ...) */
	}
	else {
		wbshort(ft, (int) 1);
		wbshort(ft, (int) 2);
	}
	
	switch (ft->info.style) {
		
	case UNSIGNED:
	case SIGN2:
		wbshort(ft, (int) 0); /* no compression */
		break;
		
	case ULAW:
		wbshort(ft, (int) 3);
		break;
		
	case ALAW:
		wbshort(ft, (int) 2);
		break;
		
	}
	
	wblong(ft, (LONG) 0); /* reserved */
	wblong(ft, (LONG) 0); /* reserved */
	wblong(ft, (LONG) 0); /* reserved */
	
	fputs ("ANNO", ft->fp);
	wblong(ft, (LONG) 32); /* length of block */
	fputs ("file written by SOX MAUD-export ", ft->fp);
	
	fputs ("MDAT", ft->fp);
	wblong(ft, p->nsamples * ft->info.size ); /* samples in file */
}

