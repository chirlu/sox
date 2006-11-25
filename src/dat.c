
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
 * November 25, 2005: tomchristie - Work with multiple channels
 *
 */

#include "st_i.h"
#include <string.h>

/* float output normalized to approx 1.0 */
#define FLOATTOLONG (2.147483648e9)
#define LONGTOFLOAT (1 / FLOATTOLONG)
#define LINEWIDTH 256

/* Private data for dat file */
typedef struct dat {
    double timevalue, deltat;
    int buffered;
    char prevline[LINEWIDTH]; 
} *dat_t;

int st_datstartread(ft_t ft)
{
    char inpstr[LINEWIDTH];
    long rate;
    int chan;
    int status;
    char sc;

    /* Read lines until EOF or first non-comment line */
    while ((status = st_reads(ft, inpstr, LINEWIDTH-1)) != ST_EOF) {
      inpstr[LINEWIDTH-1] = 0;
      if ((sscanf(inpstr," %c", &sc) != 0) && (sc != ';')) break;
      if (sscanf(inpstr," ; Sample Rate %ld", &rate)) {
        ft->info.rate=rate;
      } else if (sscanf(inpstr," ; Channels %d", &chan)) {
        ft->info.channels=chan;
      }
    }
    /* Hold a copy of the last line we read (first non-comment) */
    if (status != ST_EOF) {
      strncpy(((dat_t)ft->priv)->prevline, inpstr, LINEWIDTH);
      ((dat_t)ft->priv)->buffered = 1;
    } else {
      ((dat_t)ft->priv)->buffered = 0;
    }

    /* Default channels to 1 if not found */
    if (ft->info.channels == -1)
       ft->info.channels = 1;

    ft->info.size = ST_SIZE_64BIT;
    ft->info.encoding = ST_ENCODING_FLOAT;

    return (ST_SUCCESS);
}

int st_datstartwrite(ft_t ft)
{
    dat_t dat = (dat_t) ft->priv;
    char s[LINEWIDTH];

    ft->info.size = ST_SIZE_64BIT;
    ft->info.encoding = ST_ENCODING_FLOAT;
    dat->timevalue = 0.0;
    dat->deltat = 1.0 / (double)ft->info.rate;
    /* Write format comments to start of file */
    sprintf(s,"; Sample Rate %ld\015\n", (long)ft->info.rate);
    st_writes(ft, s);
    sprintf(s,"; Channels %d\015\n", (int)ft->info.channels);
    st_writes(ft, s);

    return (ST_SUCCESS);
}

st_ssize_t st_datread(ft_t ft, st_sample_t *buf, st_ssize_t nsamp)
{
    char inpstr[LINEWIDTH];
    int  inpPtr = 0;
    int  inpPtrInc = 0;
    double sampval = 0.0;
    int retc = 0;
    char sc = 0;
    int done = 0;
    int i=0;

    /* Always read a complete set of channels */
    nsamp -= (nsamp % ft->info.channels);

    while (done < nsamp) {

      /* Read a line or grab the buffered first line */
      if (((dat_t)ft->priv)->buffered) {
        strncpy(inpstr, ((dat_t)ft->priv)->prevline, LINEWIDTH);
        ((dat_t)ft->priv)->buffered=0;
      } else {
        st_reads(ft, inpstr, LINEWIDTH-1);
        inpstr[LINEWIDTH-1] = 0;
        if (st_eof(ft)) return (done);
      }

      /* Skip over comments - ie. 0 or more whitespace, then ';' */
      if ((sscanf(inpstr," %c", &sc) != 0) && (sc==';')) continue;

      /* Read a complete set of channels */
      sscanf(inpstr," %*s%n", &inpPtr);
      for (i=0; i<ft->info.channels; i++) {
        retc = sscanf(&inpstr[inpPtr]," %lg%n", &sampval, &inpPtrInc);
        inpPtr += inpPtrInc;
        if (retc != 1) {
          st_fail_errno(ft,ST_EOF,"Unable to read sample.");
          return (ST_EOF);
        }
        sampval *= FLOATTOLONG;
        *buf++ = ST_ROUND_CLIP_COUNT(sampval, ft->clippedCount);
        done++;
      }
    }

    return (done);
}

st_ssize_t st_datwrite(ft_t ft, st_sample_t *buf, st_ssize_t nsamp)
{
    dat_t dat = (dat_t) ft->priv;
    int done = 0;
    double sampval=0.0;
    char s[LINEWIDTH];
    int i=0;

    /* Always write a complete set of channels */
    nsamp -= (nsamp % ft->info.channels);

    /* Write time, then sample values, then CRLF newline */
    while(done < nsamp) {
      sprintf(s," %15.8g ",dat->timevalue);
      st_writes(ft, s);
      for (i=0; i<ft->info.channels; i++) {
        sampval = *buf++ ;
        sampval = sampval * LONGTOFLOAT;
        sprintf(s," %15.8g", sampval);
        st_writes(ft, s);
        done++;
      }
      sprintf(s," \015\n");
      st_writes(ft, s);
      dat->timevalue += dat->deltat;
    }
    return done;
}

/* Text data samples */
static char *datnames[] = {
  "dat",
  NULL
};

static st_format_t st_dat_format = {
  datnames,
  NULL,
  0,
  st_datstartread,
  st_datread,
  st_format_nothing,
  st_datstartwrite,
  st_datwrite,
  st_format_nothing,
  st_format_nothing_seek
};

const st_format_t *st_dat_format_fn(void)
{
    return &st_dat_format;
}
