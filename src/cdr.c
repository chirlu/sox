/*
 * CD-R format handler
 *
 * David Elliott, Sony Microsystems -  July 5, 1991
 *
 * Copyright 1991 David Elliott And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 *
 * This code automatically handles endianness differences
 *
 * cbagwell (cbagwell@sprynet.com) - 20 April 1998
 *
 *   Changed endianness handling.  Seemed to be reversed (since format
 *   is in big endian) and made it so that user could always override
 *   swapping no matter what endian machine they are one.
 *
 *   Fixed bug were trash could be appended to end of file for certain
 *   endian machines.
 *
 */

#include "sox_i.h"

#define SECTORSIZE      (2352 / 2)

/* Private data for SKEL file */
typedef struct cdrstuff {
        sox_size_t samples;      /* number of samples written */
} *cdr_t;

/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *      Find out sampling rate, 
 *      size and encoding of samples, 
 *      mono/stereo/quad.
 */

static int sox_cdrstartread(ft_t ft) 
{
        int rc;

        /* Needed because of rawread() */
        rc = sox_rawstartread(ft);
        if (rc)
            return rc;

        ft->signal.rate = 44100;
        ft->signal.size = SOX_SIZE_16BIT;
        ft->signal.encoding = SOX_ENCODING_SIGN2;
        ft->signal.channels = 2;
        ft->comment = NULL;

/* Need length for seeking */
        if(ft->seekable){
                ft->length = sox_filelength(ft)/SOX_SIZE_16BIT;
        } else {
                ft->length = 0;
        }
        
        return(SOX_SUCCESS);
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

static sox_size_t sox_cdrread(ft_t ft, sox_sample_t *buf, sox_size_t len) 
{

        return sox_rawread(ft, buf, len);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
static int sox_cdrstopread(ft_t ft) 
{
        /* Needed because of rawread() */
        return sox_rawstopread(ft);
}

static int sox_cdrstartwrite(ft_t ft) 
{
        cdr_t cdr = (cdr_t) ft->priv;
        int rc;

        /* Needed because of rawwrite() */
        rc = sox_rawstartwrite(ft);
        if (rc)
            return rc;

        cdr->samples = 0;

        ft->signal.rate = 44100;
        ft->signal.size = SOX_SIZE_16BIT;
        ft->signal.encoding = SOX_ENCODING_SIGN2;
        ft->signal.channels = 2;

        return(SOX_SUCCESS);
}

static sox_size_t sox_cdrwrite(ft_t ft, const sox_sample_t *buf, sox_size_t len) 
{
        cdr_t cdr = (cdr_t) ft->priv;

        cdr->samples += len;

        return sox_rawwrite(ft, buf, len);
}

/*
 * A CD-R file needs to be padded to SECTORSIZE, which is in terms of
 * samples.  We write -32768 for each sample to pad it out.
 */

static int sox_cdrstopwrite(ft_t ft) 
{
        cdr_t cdr = (cdr_t) ft->priv;
        int padsamps = SECTORSIZE - (cdr->samples % SECTORSIZE);
        short zero;
        int rc;

        /* Flush buffer before writing anything else */
        rc = sox_rawstopwrite(ft);

        if (rc)
            return rc;

        zero = 0;

        if (padsamps != SECTORSIZE) 
        {
                while (padsamps > 0) {
                        sox_writew(ft, zero);
                        padsamps--;
                }
        }
        return(SOX_SUCCESS);
}

static const char *cdrnames[] = {
  "cdda",
  "cdr",
  NULL
};

static sox_format_t sox_cdr_format = {
  cdrnames,
  NULL,
  SOX_FILE_SEEK | SOX_FILE_BIG_END,
  sox_cdrstartread,
  sox_cdrread,
  sox_cdrstopread,
  sox_cdrstartwrite,
  sox_cdrwrite,
  sox_cdrstopwrite,
  sox_rawseek
};

const sox_format_t *sox_cdr_format_fn(void)
{
    return &sox_cdr_format;
}
