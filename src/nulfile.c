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
 */

#include "st_i.h"

/* No private data needed for nul file */
 
/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *      Find out sampling rate, 
 *      size and encoding of samples, 
 *      mono/stereo/quad.
 */
int st_nulstartread(ft_t ft) 
{
    /* if no input rate is given as parameter, switch to 
     * default parameter
     */
    if(ft->info.rate == 0)
    {
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
    st_ssize_t done = 0;
    for(; done < len; done++)
    {
        buf[done] = 0;
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
    return(ST_SUCCESS);
}

st_ssize_t st_nulwrite(ft_t ft, st_sample_t *buf, st_ssize_t len) 
{
    return len;    
}

int st_nulstopwrite(ft_t ft) 
{
    /* nothing to do */
    return (ST_SUCCESS);
}

static char *nulnames[] = {
  "nul",
  NULL,
};

st_format_t st_nul_format = {
  nulnames,
  NULL,
  ST_FILE_STEREO | ST_FILE_NOSTDIO,
  st_nulstartread,
  st_nulread,
  st_nulstopread,
  st_nulstartwrite,
  st_nulwrite,
  st_nulstopwrite,
  st_format_nothing_seek
};
