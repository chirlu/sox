/************************************************************************/
/*      Copyright 1989 by Rich Gopstein and Harris Corporation          */
/*                                                                      */
/*      Permission to use, copy, modify, and distribute this software   */
/*      and its documentation for any purpose and without fee is        */
/*      hereby granted, provided that the above copyright notice        */
/*      appears in all copies and that both that copyright notice and   */
/*      this permission notice appear in supporting documentation, and  */
/*      that the name of Rich Gopstein and Harris Corporation not be    */
/*      used in advertising or publicity pertaining to distribution     */
/*      of the software without specific, written prior permission.     */
/*      Rich Gopstein and Harris Corporation make no representations    */
/*      about the suitability of this software for any purpose.  It     */
/*      provided "as is" without express or implied warranty.           */
/************************************************************************/

/************************************************************************/
/* sound2sun.c - Convert sampled audio files into uLAW format for the   */
/*               Sparcstation 1.                                        */
/*               Send comments to ..!rutgers!soleil!gopstein            */
/************************************************************************/
/*									*/
/*  Modified November 27, 1989 to convert to 8000 samples/sec           */
/*   (contrary to man page)                                             */
/*  Modified December 13, 1992 to write standard Sun .au header with	*/
/*   unspecified length.  Also made miscellaneous changes for 		*/
/*   VMS port.  (K. S. Kubo, ken@hmcvax.claremont.edu)			*/
/*  Fixed Bug with converting slow sample speeds			*/
/*									*/
/************************************************************************/


#include <stdio.h>

#define DEFAULT_FREQUENCY 11000

#ifdef VAXC
#define	READ_OPEN	"r", "mbf=16", "shr=get"
#define WR_OPEN		"w", "mbf=16"
#else
#define READ_OPEN	"r"
#define WR_OPEN		"w"
#endif

FILE *infile, *outfile;

/* convert two's complement ch into uLAW format */

unsigned int cvt(ch)
int ch;
{

  int mask;

  if (ch < 0) {
    ch = -ch;
    mask = 0x7f;
  } else {
    mask = 0xff;
  }

  if (ch < 32) {
    ch = 0xF0 | 15 - (ch / 2);
  } else if (ch < 96) {
    ch = 0xE0 | 15 - (ch - 32) / 4;
  } else if (ch < 224) {
    ch = 0xD0 | 15 - (ch - 96) / 8;
  } else if (ch < 480) {
    ch = 0xC0 | 15 - (ch - 224) / 16;
  } else if (ch < 992) {
    ch = 0xB0 | 15 - (ch - 480) / 32;
  } else if (ch < 2016) {
    ch = 0xA0 | 15 - (ch - 992) / 64;
  } else if (ch < 4064) {
    ch = 0x90 | 15 - (ch - 2016) / 128;
  } else if (ch < 8160) {
    ch = 0x80 | 15 - (ch - 4064) /  256;
  } else {
    ch = 0x80;
  }
return (mask & ch);
}

/* write a "standard" sun header with an unspecified length */
#define wrulong(fp, ul) putc((ul >> 24) & 0xff, fp); \
    putc((ul >> 16) & 0xff, fp); putc((ul >> 8) & 0xff, fp); \
    putc(ul & 0xff, fp);

static void
wr_header(optr)
FILE *optr;
{
    wrulong(optr, 0x2e736e64);	/* Sun magic */
    wrulong(optr, 24);		/* header size in bytes */
    wrulong(optr, ((unsigned)(~0)));	/* unspecified data size */
    wrulong(optr, 1);		/* Sun uLaw format */
    wrulong(optr, 8000);	/* sample rate by definition :-) */
    wrulong(optr, 1);		/* single channel */
}

/*******************************************************
/*                                                     */
/* Usage is "sound2sun [-f frequency] infile outfile"  */
/*                                                     */
/* "frequency" is the samples per second of the infile */
/* the outfile is always 8000 samples per second.      */
/*                                                     */
/*******************************************************/

/***********************************************************************/
/*                                                                     */
/* The input file is expected to be a stream of one-byte excess-128    */
/* samples.  Each sample is converted to 2's complement by subtracting */
/* 128, then converted to uLAW and output.  We calculate the proper    */
/* number of input bytes to skip in order to make the sample frequency */
/* convert to 8000/sec properly.  Interpolation could be added, but it */
/* doesn't appear to be necessary.                                     */
/*                                                                     */
/***********************************************************************/


main(argc, argv)
int argc;
char *argv[];
{

  float sum = 0;
  float frequency, increment;

  unsigned char ch;
  unsigned char ulaw;

  int chr;

  if ((argc != 3) && (argc != 5)) {
    fprintf(stderr,"Usage: sound2sun [-f frequency] infile outfile\n");
    exit(1);
  }

  if (argc == 5) {
    if (strcmp(argv[1], "-f") != 0) {
      fprintf(stderr, "Usage: sound2sun [-f frequency] infile outfile\n");
      exit(1);
    } else {
      frequency = atoi(argv[2]);
    }
  } else {
    frequency = DEFAULT_FREQUENCY;
  }

  if ((infile = fopen(argv[argc-2], READ_OPEN)) == NULL) {
    perror("Error opening infile");
    exit(0);
  }

  if ((outfile = fopen(argv[argc-1], WR_OPEN)) == NULL) {
    perror("Error opening outfile");
    exit(0);
  }

  wr_header(outfile);

  /* increment is the number of bytes to read each time */

  increment = frequency / 8000;

  ch = fgetc(infile);

  while (!feof(infile)) {

    /* convert the excess 128 to two's complement */

    chr = 0x80 - ch;

    /* increase the volume */
    /* convert to uLAW */

    ulaw = cvt(chr * 16);

    /* output it */

    fputc((char) ulaw, outfile);

    /* skip enough input bytes to compensate for sampling frequency diff */

    sum += increment;

    while(sum > 0) {
      if (!feof(infile)) ch = fgetc(infile);
      sum--;
    }

  }

  fclose(infile);
  fclose(outfile);
}

/*  DEC/CMS REPLACEMENT HISTORY, Element SOUND2SUN.C */
/*  *1    14-DEC-1992 17:46:37 CENYDD "main program" */
/*  DEC/CMS REPLACEMENT HISTORY, Element SOUND2SUN.C */
