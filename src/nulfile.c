/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools nul file format driver.
 * Written by Carsten Borchardt 
 * The author is not responsible for the consequences 
 * of using this software
 *
 */

#include <math.h>
#include "st_i.h"

/* Private data for nul file */
typedef struct nulstuff {
        int     rest;                   /* bytes remaining in current block */
    unsigned long readsamples;
    unsigned long writesamples;
} *nul_t;

/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *      Find out sampling rate, 
 *      size and encoding of samples, 
 *      mono/stereo/quad.
 */
int st_nulstartread(ft_t ft) 
{
        nul_t sk = (nul_t) ft->priv;
        /* no samples read yet */
        sk->readsamples=0;

        /* if no input rate is given as parameter, switch to 
         * default parameter
         */
        if(ft->info.rate == 0){
            /* input rate not set, switch to default */
            ft->info.rate = 44100;
            ft->info.size = ST_SIZE_WORD;
            ft->info.encoding = ST_ENCODING_SIGN2;
            ft->info.channels = 2;
        }
        ft->comment = "nul file";
        
        return (ST_SUCCESS);
}

/*
 * Read up to len samples, we read always '0'
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

st_ssize_t st_nulread(ft_t ft, st_sample_t *buf, st_ssize_t len) 
{
        nul_t sk = (nul_t) ft->priv;
        int done = 0;
        st_sample_t l;
        for(; done < len; done++) {
            if (ft->file.eof)
                break;
            l = 0; /* nul samples are always 0 */
            sk->readsamples++;
            *buf++ = l;
        }
        return done;
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 * .. nothing to be done
 */
int st_nulstopread(ft_t ft) 
{ 
    return (ST_SUCCESS);
}

int st_nulstartwrite(ft_t ft) 
{
        nul_t sk = (nul_t) ft->priv;
        sk->writesamples=0;
        return(ST_SUCCESS);
        
}

st_ssize_t st_nulwrite(ft_t ft, st_sample_t *buf, st_ssize_t len) 
{
        nul_t sk = (nul_t) ft->priv;
        while(len--)
            sk->writesamples++;
        st_writeb(ft, (*buf++ >> 24) ^ 0x80);
        return (ST_SUCCESS);
        
}

int st_nulstopwrite(ft_t ft) 
{
    /* nothing to do */
    return (ST_SUCCESS);
}













