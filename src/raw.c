/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools raw format file.
 *
 * Includes .ub, .uw, .sb, .sw, and .ul formats at end
 */

/*
 * Notes: most of the headerless formats set their handlers to raw
 * in their startread/write routines.  
 *
 */

#include "st.h"
#include "libst.h"

#include <malloc.h>

void rawstartread(ft) 
ft_t ft;
{
	ft->file.buf = malloc(BUFSIZ);
	ft->file.size = BUFSIZ;
	ft->file.count = 0;
	ft->file.eof = 0;
}

void rawstartwrite(ft) 
ft_t ft;
{
	ft->file.buf = malloc(BUFSIZ);
	ft->file.size = BUFSIZ;
	ft->file.count = 0;
	ft->file.eof = 0;
}

/* Read raw file data, and convert it to */
/* the sox internal signed long format. */

unsigned char blockgetc(ft)
ft_t ft;
{
	char rval;

	if (ft->file.count < 1)
	{
		ft->file.count = read(fileno(ft->fp), (char *)ft->file.buf, 
				ft->file.size);
		if (ft->file.count == 0)
		{
			ft->file.eof = 1;
			return(0);
		}
	}
	rval = *(ft->file.buf + (ft->file.size - ft->file.count--));
	return (rval);
}

unsigned short blockrshort(ft)
ft_t ft;
{
	unsigned short rval;
	if (ft->file.count < 2)
	{
		ft->file.count = read(fileno(ft->fp), (char *)ft->file.buf,
				ft->file.size);
		if (ft->file.count == 0)
		{
			ft->file.eof = 1;
			return(0);
		}
	}
	rval = *((unsigned short *)(ft->file.buf + (ft->file.size - ft->file.count)));
	ft->file.count -= 2;
	return(rval);
}

float blockrfloat(ft)
ft_t ft;
{
	float rval;

	if (ft->file.count < sizeof(float))
	{
		ft->file.count = read(fileno(ft->fp), (char *)ft->file.buf,
				ft->file.size);
		if (ft->file.count == 0)
		{
			ft->file.eof = 1;
			return(0);
		}
	}
	rval = *((float *)(ft->file.buf + (ft->file.size - ft->file.count)));
	ft->file.count -= sizeof(float);
	return(rval);
}
	

LONG rawread(ft, buf, nsamp) 
ft_t ft;
LONG *buf, nsamp;
{
	register LONG datum;
	int done = 0;

	switch(ft->info.size) {
		case BYTE:
		    switch(ft->info.style)
		    {
			case SIGN2:
				while(done < nsamp) {
					datum = blockgetc(ft);
					if (ft->file.eof)
						return done;
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 24);
					done++;
				}
				return done;
			case UNSIGNED:
				while(done < nsamp) {
					datum = blockgetc(ft);
					if (ft->file.eof)
						return done;
					/* Convert to signed */
					datum ^= 128;
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 24);
					done++;
				}
				return done;
			case ULAW:
				while(done < nsamp) {
					datum = blockgetc(ft);
					if (ft->file.eof)
						return done;
					datum = st_ulaw_to_linear(datum);
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 16);
					done++;
				}
				return done;
			case ALAW:
				while(done < nsamp) {
				        datum = blockgetc(ft);
				        if (ft->file.eof)
				                return done;
				        datum = st_Alaw_to_linear(datum);
				        /* scale signed up to long's range */
				        *buf++ = LEFT(datum, 16);
				        done++;
				}

				return done;
		    }
		    break;
		case WORD:
		    switch(ft->info.style)
		    {
			case SIGN2:
				while(done < nsamp) {
					datum = blockrshort(ft);
					if (ft->file.eof)
						return done;
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 16);
					done++;
				}
				return done;
			case UNSIGNED:
				while(done < nsamp) {
					datum = blockrshort(ft);
					if (ft->file.eof)
						return done;
					/* Convert to signed */
					datum ^= 0x8000;
					/* scale signed up to long's range */
					*buf++ = LEFT(datum, 16);
					done++;
				}
				return done;
			case ULAW:
				fail("No U-Law support for shorts");
				return done;
			case ALAW:
				fail("No A-Law support for shorts");
				return done;
		    }
		    break;
		case FLOAT:
			while(done < nsamp) {
				datum = blockrfloat(ft);
				if (ft->file.eof)
					return done;
				*buf++ = LEFT(datum, 16);
				done++;
			}
			return done;
		default:
			fail("Drop through in rawread!");
	}
	fail("Sorry, don't have code to read %s, %s",
		styles[ft->info.style], sizes[ft->info.size]);
	return(0);
}

void rawstopread(ft)
ft_t ft;
{
	free(ft->file.buf);
}

void blockflush(ft)
ft_t ft;
{
	if (write(fileno(ft->fp), ft->file.buf, ft->file.count) != ft->file.count)
	{
		fail("Error writing data to file");
	}
	ft->file.count = 0;
}

void blockputc(ft,c)
ft_t ft;
int c;
{
	if (ft->file.count > ft->file.size-1) blockflush(ft);
	*(ft->file.buf + ft->file.count) = c;
	ft->file.count++;
}

void blockwshort(ft,ui)
ft_t ft;
unsigned short ui;
{
	if (ft->file.count > ft->file.size-2) blockflush(ft);
	*((unsigned short *)(ft->file.buf + ft->file.count)) = ui;
	ft->file.count += 2;
}

void blockwfloat(ft,f)
ft_t ft;
float f;
{
	if (ft->file.count > ft->file.size - sizeof(float)) blockflush(ft);
	*((float *)(ft->file.buf + ft->file.count)) = f;
	ft->file.count += sizeof(float);
}

/* Convert the sox internal signed long format */
/* to the raw file data, and write it. */

void
rawwrite(ft, buf, nsamp) 
ft_t ft;
LONG *buf, nsamp;
{
	register int datum;
	int done = 0;

	switch(ft->info.size) {
		case BYTE:
		    switch(ft->info.style)
		    {
			case SIGN2:
				while(done < nsamp) {
					/* scale signed up to long's range */
					datum = (int) RIGHT(*buf++, 24);
					blockputc(ft, datum);
					done++;
				}
				return;
			case UNSIGNED:
				while(done < nsamp) {
					/* scale signed up to long's range */
					datum = (int) RIGHT(*buf++, 24);
					/* Convert to unsigned */
					datum ^= 128;
					blockputc(ft, datum);
					done++;
				}
				return;
			case ULAW:
				while(done < nsamp) {
					/* scale signed up to long's range */
					datum = (int) RIGHT(*buf++, 16);
					/* round up to 12 bits of data */
					datum += 0x8;	/* + 0b1000 */
					datum = st_linear_to_ulaw(datum);
					blockputc(ft, datum);
					done++;
				}
				return;
			case ALAW:
				while(done < nsamp) {
					/* scale signed up to long's range */
					datum = (int) RIGHT(*buf++, 16);
					/* round up to 12 bits of data */
					datum += 0x8;	/* + 0b1000 */
					datum = st_linear_to_Alaw(datum);
					blockputc(ft, datum);
					done++;
				}
				return;
		    }
		    break;
		case WORD:
		    switch(ft->info.style)
		    {
			case SIGN2:
				while(done < nsamp) {
					/* scale signed up to long's range */
					datum = (int) RIGHT(*buf++, 16);
					blockwshort(ft, datum);
					done++;
				}
				return;
			case UNSIGNED:
				while(done < nsamp) {
					/* scale signed up to long's range */
					datum = (int) RIGHT(*buf++, 16);
					/* Convert to unsigned */
					datum ^= 0x8000;
					blockwshort(ft, datum);
					done++;
				}
				return;
			case ULAW:
fail("No U-Law support for shorts (try -b option ?)");
				return;
			case ALAW:
fail("No A-Law support for shorts (try -b option ?)");
				return;
		    }
		    break;
		case FLOAT:
			while(done < nsamp) {
				/* scale signed up to long's range */
				datum = (int) RIGHT(*buf++, 16);
			 	blockwfloat(ft, (double) datum);
				done++;
			}
			return;
		default:
		    fail("Drop through in rawwrite!");
	}
	fail("Sorry, don't have code to write %s, %s",
		styles[ft->info.style], sizes[ft->info.size]);
}

void rawstopwrite(ft)
ft_t ft;
{
	blockflush(ft);
	free(ft->file.buf);
}

/*
* Set parameters to the fixed parameters known for this format,
* and change format to raw format.
*/

void rawdefaults();

/* Signed byte */
void sbstartread(ft) 
ft_t ft;
{
	ft->info.size = BYTE;
	ft->info.style = SIGN2;
	rawstartread(ft);
	rawdefaults(ft);
}

void sbstartwrite(ft) 
ft_t ft;
{
	ft->info.size = BYTE;
	ft->info.style = SIGN2;
	rawstartwrite(ft);
	rawdefaults(ft);
}

void ubstartread(ft) 
ft_t ft;
{
	ft->info.size = BYTE;
	ft->info.style = UNSIGNED;
	rawstartread(ft);
	rawdefaults(ft);
}

void ubstartwrite(ft) 
ft_t ft;
{
	ft->info.size = BYTE;
	ft->info.style = UNSIGNED;
	rawstartwrite(ft);
	rawdefaults(ft);
}

void uwstartread(ft) 
ft_t ft;
{
	ft->info.size = WORD;
	ft->info.style = UNSIGNED;
	rawstartread(ft);
	rawdefaults(ft);
}

void uwstartwrite(ft) 
ft_t ft;
{
	ft->info.size = WORD;
	ft->info.style = UNSIGNED;
	rawstartwrite(ft);
	rawdefaults(ft);
}

void swstartread(ft) 
ft_t ft;
{
	ft->info.size = WORD;
	ft->info.style = SIGN2;
	rawstartread(ft);
	rawdefaults(ft);
}

void swstartwrite(ft) 
ft_t ft;
{
	ft->info.size = WORD;
	ft->info.style = SIGN2;
	rawstartwrite(ft);
	rawdefaults(ft);
}

void ulstartread(ft) 
ft_t ft;
{
	ft->info.size = BYTE;
	ft->info.style = ULAW;
	rawstartread(ft);
	rawdefaults(ft);
}

void ulstartwrite(ft) 
ft_t ft;
{
	ft->info.size = BYTE;
	ft->info.style = ULAW;
	rawstartwrite(ft);
	rawdefaults(ft);
}

void alstartread(ft) 
ft_t ft;
{
	ft->info.size = BYTE;
	ft->info.style = ALAW;
	rawstartread(ft);
	rawdefaults(ft);
}

void alstartwrite(ft) 
ft_t ft;
{
	ft->info.size = BYTE;
	ft->info.style = ALAW;
	rawstartwrite(ft);
	rawdefaults(ft);
}

void rawdefaults(ft)
ft_t ft;
{
	if (ft->info.rate == 0)
		ft->info.rate = 8000;
	if (ft->info.channels == -1)
		ft->info.channels = 1;
}


