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

#include "st_i.h"

#define SECTORSIZE      (2352 / 2)

/* Private data for SKEL file */
typedef struct cdrstuff {
        st_size_t samples;      /* number of samples written */
} *cdr_t;

/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *      Find out sampling rate, 
 *      size and encoding of samples, 
 *      mono/stereo/quad.
 */

int st_cdrstartread(ft_t ft) 
{
        int rc;

        /* Needed because of rawread() */
        rc = st_rawstartread(ft);
        if (rc)
            return rc;

        /* CDR is in Big Endian format.  Swap whats read in on */
        /* Little Endian machines.                             */
        if (ST_IS_LITTLEENDIAN)
        { 
            ft->swap = ft->swap ? 0 : 1;
        }

        ft->info.rate = 44100L;
        ft->info.size = ST_SIZE_WORD;
        ft->info.encoding = ST_ENCODING_SIGN2;
        ft->info.channels = 2;
        ft->comment = NULL;

/* Need length for seeking */
        if(ft->seekable){
                ft->length = st_filelength(ft)/ST_SIZE_WORD;
        } else {
                ft->length = 0;
        }
        
        return(ST_SUCCESS);
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

st_ssize_t st_cdrread(ft_t ft, st_sample_t *buf, st_ssize_t len) 
{

        return st_rawread(ft, buf, len);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_cdrstopread(ft_t ft) 
{
        /* Needed because of rawread() */
        return st_rawstopread(ft);
}

int st_cdrstartwrite(ft_t ft) 
{
        cdr_t cdr = (cdr_t) ft->priv;
        int rc;

        /* CDR is in Big Endian format.  Swap whats written out on */
        /* Little Endian Machines.                                 */
        if (ST_IS_LITTLEENDIAN)
        {
            ft->swap = ft->swap ? 0 : 1;
        }

        /* Needed because of rawwrite() */
        rc = st_rawstartwrite(ft);
        if (rc)
            return rc;

        cdr->samples = 0;

        ft->info.rate = 44100L;
        ft->info.size = ST_SIZE_WORD;
        ft->info.encoding = ST_ENCODING_SIGN2;
        ft->info.channels = 2;

        return(ST_SUCCESS);
}

st_ssize_t st_cdrwrite(ft_t ft, st_sample_t *buf, st_ssize_t len) 
{
        cdr_t cdr = (cdr_t) ft->priv;

        cdr->samples += len;

        return st_rawwrite(ft, buf, len);
}

/*
 * A CD-R file needs to be padded to SECTORSIZE, which is in terms of
 * samples.  We write -32768 for each sample to pad it out.
 */

int st_cdrstopwrite(ft_t ft) 
{
        cdr_t cdr = (cdr_t) ft->priv;
        int padsamps = SECTORSIZE - (cdr->samples % SECTORSIZE);
        short zero;
        int rc;

        /* Flush buffer before writing anything else */
        rc = st_rawstopwrite(ft);

        if (rc)
            return rc;

        zero = 0;

        if (padsamps != SECTORSIZE) 
        {
                while (padsamps > 0) {
                        st_writew(ft, zero);
                        padsamps--;
                }
        }
        return(ST_SUCCESS);
}
