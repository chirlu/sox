/*
 * libSoX text format file.  Tom Littlejohn, March 93.
 *
 * Reads/writes sound files as text.
 *
 * Copyright 1998-2006 Chris Bagwell and SoX Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

#include "sox_i.h"
#include <string.h>

#define LINEWIDTH 256

/* Private data for dat file */
typedef struct dat {
    double timevalue, deltat;
    int buffered;
    char prevline[LINEWIDTH]; 
} *dat_t;

static int sox_datstartread(sox_format_t * ft)
{
    char inpstr[LINEWIDTH];
    long rate;
    int chan;
    int status;
    char sc;

    /* Read lines until EOF or first non-comment line */
    while ((status = sox_reads(ft, inpstr, LINEWIDTH-1)) != SOX_EOF) {
      inpstr[LINEWIDTH-1] = 0;
      if ((sscanf(inpstr," %c", &sc) != 0) && (sc != ';')) break;
      if (sscanf(inpstr," ; Sample Rate %ld", &rate)) {
        ft->signal.rate=rate;
      } else if (sscanf(inpstr," ; Channels %d", &chan)) {
        ft->signal.channels=chan;
      }
    }
    /* Hold a copy of the last line we read (first non-comment) */
    if (status != SOX_EOF) {
      strncpy(((dat_t)ft->priv)->prevline, inpstr, LINEWIDTH);
      ((dat_t)ft->priv)->buffered = 1;
    } else {
      ((dat_t)ft->priv)->buffered = 0;
    }

    /* Default channels to 1 if not found */
    if (ft->signal.channels == 0)
       ft->signal.channels = 1;

    ft->signal.size = SOX_SIZE_64BIT;
    ft->signal.encoding = SOX_ENCODING_FLOAT;

    return (SOX_SUCCESS);
}

static int sox_datstartwrite(sox_format_t * ft)
{
    dat_t dat = (dat_t) ft->priv;
    char s[LINEWIDTH];

    ft->signal.size = SOX_SIZE_64BIT;
    ft->signal.encoding = SOX_ENCODING_FLOAT;
    dat->timevalue = 0.0;
    dat->deltat = 1.0 / (double)ft->signal.rate;
    /* Write format comments to start of file */
    sprintf(s,"; Sample Rate %ld\015\n", (long)ft->signal.rate);
    sox_writes(ft, s);
    sprintf(s,"; Channels %d\015\n", (int)ft->signal.channels);
    sox_writes(ft, s);

    return (SOX_SUCCESS);
}

static sox_size_t sox_datread(sox_format_t * ft, sox_ssample_t *buf, sox_size_t nsamp)
{
    char inpstr[LINEWIDTH];
    int  inpPtr = 0;
    int  inpPtrInc = 0;
    double sampval = 0.0;
    int retc = 0;
    char sc = 0;
    sox_size_t done = 0;
    sox_size_t i=0;

    /* Always read a complete set of channels */
    nsamp -= (nsamp % ft->signal.channels);

    while (done < nsamp) {

      /* Read a line or grab the buffered first line */
      if (((dat_t)ft->priv)->buffered) {
        strncpy(inpstr, ((dat_t)ft->priv)->prevline, LINEWIDTH);
        ((dat_t)ft->priv)->buffered=0;
      } else {
        sox_reads(ft, inpstr, LINEWIDTH-1);
        inpstr[LINEWIDTH-1] = 0;
        if (sox_eof(ft)) return (done);
      }

      /* Skip over comments - ie. 0 or more whitespace, then ';' */
      if ((sscanf(inpstr," %c", &sc) != 0) && (sc==';')) continue;

      /* Read a complete set of channels */
      sscanf(inpstr," %*s%n", &inpPtr);
      for (i=0; i<ft->signal.channels; i++) {
        retc = sscanf(&inpstr[inpPtr]," %lg%n", &sampval, &inpPtrInc);
        inpPtr += inpPtrInc;
        if (retc != 1) {
          sox_fail_errno(ft,SOX_EOF,"Unable to read sample.");
          return 0;
        }
        sampval *= SOX_SAMPLE_MAX;
        *buf++ = SOX_ROUND_CLIP_COUNT(sampval, ft->clips);
        done++;
      }
    }

    return (done);
}

static sox_size_t sox_datwrite(sox_format_t * ft, const sox_ssample_t *buf, sox_size_t nsamp)
{
    dat_t dat = (dat_t) ft->priv;
    sox_size_t done = 0;
    double sampval=0.0;
    char s[LINEWIDTH];
    sox_size_t i=0;

    /* Always write a complete set of channels */
    nsamp -= (nsamp % ft->signal.channels);

    /* Write time, then sample values, then CRLF newline */
    while(done < nsamp) {
      sprintf(s," %15.8g ",dat->timevalue);
      sox_writes(ft, s);
      for (i=0; i<ft->signal.channels; i++) {
        sampval = SOX_SAMPLE_TO_FLOAT_64BIT(*buf++, ft->clips);
        sprintf(s," %15.8g", sampval);
        sox_writes(ft, s);
        done++;
      }
      sprintf(s," \015\n");
      sox_writes(ft, s);
      dat->timevalue += dat->deltat;
    }
    return done;
}

/* Text data samples */
static const char *datnames[] = {
  "dat",
  NULL
};

static sox_format_handler_t sox_dat_format = {
  datnames,
  0,
  sox_datstartread,
  sox_datread,
  sox_format_nothing,
  sox_datstartwrite,
  sox_datwrite,
  sox_format_nothing,
  sox_format_nothing_seek
};

const sox_format_handler_t *sox_dat_format_fn(void);

const sox_format_handler_t *sox_dat_format_fn(void)
{
    return &sox_dat_format;
}
