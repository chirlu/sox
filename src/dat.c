
/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools text format file.  Tom Littlejohn, March 93.
 *
 * Reads/writes sound files as text for use with fft and graph.
 *
 * June 28, 93: force output to mono.
 *
 * September 24, 1998: cbagwell - Set up extra output format info so that 
 * reports are accurate.  Also warn user when forcing to mono.
 *
 */

#include "st_i.h"

/* Private data for dat file */
typedef struct dat {
        double timevalue, deltat;
} *dat_t;

/* FIXME: Move this to misc.c */
static st_sample_t roundoff(double x)
{
    if (x < 0.0) return(x - 0.5);
    else return(x + 0.5);
}

int st_datstartread(ft_t ft)
{
   char inpstr[82];
   char sc;
   long rate;

   while (ft->info.rate == 0) {
      st_reads(ft, inpstr, 82);
      inpstr[81] = 0;
      sscanf(inpstr," %c",&sc);
      if (sc != ';') 
      {
          st_fail_errno(ft,ST_EHDR,"Cannot determine sample rate.");
          return (ST_EOF);
      }
      /* Store in system dependent long to get around cross platform
       * problems.
       */
      sscanf(inpstr," ; Sample Rate %ld", &rate);
      ft->info.rate = rate;
   }

   if (ft->info.channels == -1)
       ft->info.channels = 1;

   ft->info.size = ST_SIZE_64BIT;
   ft->info.encoding = ST_ENCODING_FLOAT;

   return (ST_SUCCESS);
}

int st_datstartwrite(ft_t ft)
{
   dat_t dat = (dat_t) ft->priv;
   double srate;
   char s[80];
   long rate;

   if (ft->info.channels > 1)
   {
        st_report("Can only create .dat files with one channel.");
        st_report("Forcing output to 1 channel.");
        ft->info.channels = 1;
   }
   
   ft->info.size = ST_SIZE_64BIT;
   ft->info.encoding = ST_ENCODING_FLOAT;
   dat->timevalue = 0.0;
   srate = ft->info.rate;
   dat->deltat = 1.0 / srate;
   rate = ft->info.rate;
   sprintf(s,"; Sample Rate %ld\015\n",rate);
   st_writes(ft, s);

   return (ST_SUCCESS);
}

st_ssize_t st_datread(ft_t ft, st_sample_t *buf, st_ssize_t nsamp)
{
    char inpstr[82];
    double sampval;
    int retc;
    int done = 0;
    char sc;

    while (done < nsamp) {
        do {
          st_reads(ft, inpstr, 82);
          if (st_eof(ft)) {
                return (done);
          }
          sscanf(inpstr," %c",&sc);
          }
          while(sc == ';');  /* eliminate comments */
        retc = sscanf(inpstr,"%*s %lg",&sampval);
        if (retc != 1) 
        {
            st_fail_errno(ft,ST_EOF,"Unable to read sample.");
            return (0);
        }
        *buf++ = roundoff(sampval * 2.147483648e9);
        ++done;
    }
        return (done);
}

st_ssize_t st_datwrite(ft_t ft, st_sample_t *buf, st_ssize_t nsamp)
{
    dat_t dat = (dat_t) ft->priv;
    int done = 0;
    double sampval;
    char s[80];

    while(done < nsamp) {
       sampval = *buf++ ;
       sampval = sampval / 2.147483648e9;  /* normalize to approx 1.0 */
       sprintf(s," %15.8g  %15.8g \015\n",dat->timevalue,sampval);
       st_writes(ft, s);
       dat->timevalue += dat->deltat;
       done++;
       }
    return done;
}

