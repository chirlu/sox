
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

#include "st.h"

/* Private data for dat file */
typedef struct dat {
	double timevalue, deltat;
} *dat_t;

/* FIXME: Move this to misc.c */
static LONG roundoff(x)
double x;
{
    if (x < 0.0) return(x - 0.5);
    else return(x + 0.5);
}

int st_datstartread(ft)
ft_t ft;
{
   char inpstr[82];
   char sc;

   while (ft->info.rate == 0) {
      fgets(inpstr, 82, ft->fp);
      inpstr[81] = 0;
      sscanf(inpstr," %c",&sc);
      if (sc != ';') 
      {
	  st_fail_errno(ft,ST_EHDR,"Cannot determine sample rate.");
	  return (ST_EOF);
      }
#ifdef __alpha__
      sscanf(inpstr," ; Sample Rate %d", &ft->info.rate);
#else
      sscanf(inpstr," ; Sample Rate %ld",&ft->info.rate);
#endif
      }

   if (ft->info.channels == -1)
       ft->info.channels = 1;

   ft->info.size = ST_SIZE_DOUBLE;
   ft->info.encoding = ST_ENCODING_SIGN2;

   return (ST_SUCCESS);
}

int st_datstartwrite(ft)
ft_t ft;
{
   dat_t dat = (dat_t) ft->priv;
   double srate;
   char s[80];

   if (ft->info.channels > 1)
   {
        st_report("Can only create .dat files with one channel.");
	st_report("Forcing output to 1 channel.");
	ft->info.channels = 1;
   }
   
   ft->info.size = ST_SIZE_DOUBLE;
   ft->info.encoding = ST_ENCODING_SIGN2;
   dat->timevalue = 0.0;
   srate = ft->info.rate;
   dat->deltat = 1.0 / srate;
#ifdef __alpha__
   sprintf(s,"; Sample Rate %d\015\n", ft->info.rate);
#else
   sprintf(s,"; Sample Rate %ld\015\n",ft->info.rate);
#endif
   st_writes(ft, s);

   return (ST_SUCCESS);
}

LONG st_datread(ft, buf, nsamp)
ft_t ft;
LONG *buf, nsamp;
{
    char inpstr[82];
    double sampval;
    int retc;
    int done = 0;
    char sc;

    while (done < nsamp) {
        do {
          fgets(inpstr,82,ft->fp);
          if (feof(ft->fp)) {
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

LONG st_datwrite(ft, buf, nsamp)
ft_t ft;
LONG *buf, nsamp;
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

