
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
#include "libst.h"

/* Private data for dat file */
typedef struct dat {
	double timevalue, deltat;
} *dat_t;

LONG roundoff(x)
double x;
{
    if (x < 0.0) return(x - 0.5);
    else return(x + 0.5);
    }

void
datstartread(ft)
ft_t ft;
{
   char inpstr[82];
   char sc;

   while (ft->info.rate == 0) {
      fgets(inpstr,82,ft->fp);
      sscanf(inpstr," %c",&sc);
      if (sc != ';') fail("Cannot determine sample rate.");
#ifdef __alpha__
      sscanf(inpstr," ; Sample Rate %d", &ft->info.rate);
#else
      sscanf(inpstr," ; Sample Rate %ld",&ft->info.rate);
#endif
      }

   /* size and style are really not necessary except to satisfy caller. */

   ft->info.size = DOUBLE;
   ft->info.style = SIGN2;
}

void
datstartwrite(ft)
ft_t ft;
{
   dat_t dat = (dat_t) ft->priv;
   double srate;

   if (ft->info.channels > 1)
   {
        report("Can only create .dat files with one channel.");
	report("Forcing output to 1 channel.");
	ft->info.channels = 1;
   }
   
   ft->info.size = DOUBLE;
   ft->info.style = SIGN2;
   dat->timevalue = 0.0;
   srate = ft->info.rate;
   dat->deltat = 1.0 / srate;
#ifdef __alpha__
   fprintf(ft->fp,"; Sample Rate %d\015\n", ft->info.rate);
#else
   fprintf(ft->fp,"; Sample Rate %ld\015\n",ft->info.rate);
#endif
}

LONG datread(ft, buf, nsamp)
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
        if (retc != 1) fail("Unable to read sample.");
        *buf++ = roundoff(sampval * 2.147483648e9);
        ++done;
    }
	return (done);
}

void
datwrite(ft, buf, nsamp)
ft_t ft;
LONG *buf, nsamp;
{
    dat_t dat = (dat_t) ft->priv;
    int done = 0;
    double sampval;

    while(done < nsamp) {
       sampval = *buf++ ;
       sampval = sampval / 2.147483648e9;  /* normalize to approx 1.0 */
       fprintf(ft->fp," %15.8g  %15.8g \015\n",dat->timevalue,sampval);
       dat->timevalue += dat->deltat;
       done++;
       }
    return;
}

