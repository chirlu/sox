/*
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 *
 * September 8, 1993
 * Copyright 1993 T. Allen Grider - for changes to support block type 9
 * and word sized samples.  Same caveats and disclaimer as above.
 *
 * February 22, 1996
 * by Chris Bagwell (cbagwell@sprynet.com)
 * Added support for block type 8 (extended) which allows for 8-bit stereo
 * files.  Added support for saving stereo files and 16-bit files.
 * Added VOC format info from audio format FAQ so I don't have to keep
 * looking around for it.
 *
 * February 5, 2001
 * For sox-12-17 by Annonymous (see notes ANN)
 * Added comments and notes for each procedure.
 * Fixed so this now works with pipes, input does not have to
 * be seekable anymore (in st_vocstartread() )
 * Added support for uLAW and aLaw (aLaw not tested).
 * Fixed support of multi-part VOC files, and files with
 * block 9 but no audio in the block....
 * The following need to be tested:  16-bit, 2 channel, and aLaw.
 *
 * December 10, 2001
 * For sox-12-17-3 by Annonymous (see notes ANN)
 * Patch for sox-12-17 merged with sox-12-17-3-pre3 code.
 *
 */

/*
 * Sound Tools Sound Blaster VOC handler sources.
 */

/*------------------------------------------------------------------------
The following is taken from the Audio File Formats FAQ dated 2-Jan-1995
and submitted by Guido van Rossum <guido@cwi.nl>.
--------------------------------------------------------------------------
Creative Voice (VOC) file format
--------------------------------

From: galt@dsd.es.com

(byte numbers are hex!)

    HEADER (bytes 00-19)
    Series of DATA BLOCKS (bytes 1A+) [Must end w/ Terminator Block]

- ---------------------------------------------------------------

HEADER:
-------
     byte #     Description
     ------     ------------------------------------------
     00-12      "Creative Voice File"
     13         1A (eof to abort printing of file)
     14-15      Offset of first datablock in .voc file (std 1A 00
                in Intel Notation)
     16-17      Version number (minor,major) (VOC-HDR puts 0A 01)
     18-19      2's Comp of Ver. # + 1234h (VOC-HDR puts 29 11)

- ---------------------------------------------------------------

DATA BLOCK:
-----------

   Data Block:  TYPE(1-byte), SIZE(3-bytes), INFO(0+ bytes)
   NOTE: Terminator Block is an exception -- it has only the TYPE byte.

      TYPE   Description     Size (3-byte int)   Info
      ----   -----------     -----------------   -----------------------
      00     Terminator      (NONE)              (NONE)
      01     Sound data      2+length of data    *
      02     Sound continue  length of data      Voice Data
      03     Silence         3                   **
      04     Marker          2                   Marker# (2 bytes)
      05     ASCII           length of string    null terminated string
      06     Repeat          2                   Count# (2 bytes)
      07     End repeat      0                   (NONE)
      08     Extended        4                   ***
      09     New Header      16                  see below


      *Sound Info Format:       **Silence Info Format:
       ---------------------      ----------------------------
       00   Sample Rate           00-01  Length of silence - 1
       01   Compression Type      02     Sample Rate
       02+  Voice Data

    ***Extended Info Format:
       ---------------------
       00-01  Time Constant: Mono: 65536 - (256000000/sample_rate)
                             Stereo: 65536 - (25600000/(2*sample_rate))
       02     Pack
       03     Mode: 0 = mono
                    1 = stereo


  Marker#           -- Driver keeps the most recent marker in a status byte
  Count#            -- Number of repetitions + 1
                         Count# may be 1 to FFFE for 0 - FFFD repetitions
                         or FFFF for endless repetitions
  Sample Rate       -- SR byte = 256-(1000000/sample_rate)
  Length of silence -- in units of sampling cycle
  Compression Type  -- of voice data
                         8-bits    = 0
                         4-bits    = 1
                         2.6-bits  = 2
                         2-bits    = 3
                         Multi DAC = 3+(# of channels) [interesting--
                                       this isn't in the developer's manual]

Detailed description of new data blocks (VOC files version 1.20 and above):

        (Source is fax from Barry Boone at Creative Labs, 405/742-6622)

BLOCK 8 - digitized sound attribute extension, must preceed block 1.
          Used to define stereo, 8 bit audio
        BYTE bBlockID;       // = 8
        BYTE nBlockLen[3];   // 3 byte length
        WORD wTimeConstant;  // time constant = same as block 1
        BYTE bPackMethod;    // same as in block 1
        BYTE bVoiceMode;     // 0-mono, 1-stereo

        Data is stored left, right

BLOCK 9 - data block that supersedes blocks 1 and 8.
          Used for stereo, 16 bit (and uLaw, aLaw).

        BYTE bBlockID;          // = 9
        BYTE nBlockLen[3];      // length 12 plus length of sound
        DWORD dwSamplesPerSec;  // samples per second, not time const.
        BYTE bBitsPerSample;    // e.g., 8 or 16
        BYTE bChannels;         // 1 for mono, 2 for stereo
        WORD wFormat;           // see below
        BYTE reserved[4];       // pad to make block w/o data
                                // have a size of 16 bytes

        Valid values of wFormat are:

                0x0000  8-bit unsigned PCM
                0x0001  Creative 8-bit to 4-bit ADPCM
                0x0002  Creative 8-bit to 3-bit ADPCM
                0x0003  Creative 8-bit to 2-bit ADPCM
                0x0004  16-bit signed PCM
                0x0006  CCITT a-Law
                0x0007  CCITT u-Law
                0x02000 Creative 16-bit to 4-bit ADPCM

        Data is stored left, right

        ANN:  Multi-byte quantities are in Intel byte order (Little Endian).

------------------------------------------------------------------------*/

#include "st_i.h"
#include "g711.h"
#include <string.h>

/* Private data for VOC file */
typedef struct vocstuff {
    long           rest;        /* bytes remaining in current block */
    long           rate;        /* rate code (byte) of this chunk */
    int            silent;      /* sound or silence? */
    long           srate;       /* rate code (byte) of silence */
    long           blockseek;   /* start of current output block */
    long           samples;     /* number of samples output */
    uint16_t       format;      /* VOC audio format */
    int            size;        /* word length of data */
    unsigned char  channels;    /* number of sound channels */
    long           total_size;  /* total size of all audio in file */
    int            extended;    /* Has an extended block been read? */
} *vs_t;

#define VOC_TERM        0
#define VOC_DATA        1
#define VOC_CONT        2
#define VOC_SILENCE     3
#define VOC_MARKER      4
#define VOC_TEXT        5
#define VOC_LOOP        6
#define VOC_LOOPEND     7
#define VOC_EXTENDED    8
#define VOC_DATA_16     9

/* ANN:  Format encoding types */
#define VOC_FMT_LIN8U          0   /* 8 bit unsigned linear PCM */
#define VOC_FMT_CRLADPCM4      1   /* Creative 8-bit to 4-bit ADPCM */
#define VOC_FMT_CRLADPCM3      2   /* Creative 8-bit to 3-bit ADPCM */
#define VOC_FMT_CRLADPCM2      3   /* Creative 8-bit to 2-bit ADPCM */
#define VOC_FMT_LIN16          4   /* 16-bit signed PCM */
#define VOC_FMT_ALAW           6   /* CCITT a-Law 8-bit PCM */
#define VOC_FMT_MU255          7   /* CCITT u-Law 8-bit PCM */
#define VOC_FMT_CRLADPCM4A 0x200   /* Creative 16-bit to 4-bit ADPCM */

#define min(a, b)       (((a) < (b)) ? (a) : (b))

/* Prototypes for internal functions */
static int getblock(ft_t);
static void blockstart(ft_t);
static void blockstop(ft_t);

/* Conversion macros (from raw.c) */
#define ST_ALAW_BYTE_TO_SAMPLE(d) ((st_sample_t)(st_alaw2linear16(d)) << 16)
#define ST_ULAW_BYTE_TO_SAMPLE(d) ((st_sample_t)(st_ulaw2linear16(d)) << 16)

/* public VOC functions for SOX */
/*-----------------------------------------------------------------
 * st_vocstartread() -- start reading a VOC file
 *-----------------------------------------------------------------*/
int st_vocstartread(ft_t ft)
{
        int rtn = ST_SUCCESS;
        char header[20];
        vs_t v = (vs_t) ft->priv;
        unsigned short sbseek;
        int rc;
        int ii;  /* for getting rid of lseek */
        unsigned char uc;

        /* VOC is in Little Endian format.  Swap bytes read in on */
        /* Big Endian mahcines.                                   */
        if (ST_IS_BIGENDIAN)
        {
                ft->swap = ft->swap ? 0 : 1;
        }


        if (st_readbuf(ft, header, 1, 20) != 20)
        {
                st_fail_errno(ft,ST_EHDR,"unexpected EOF in VOC header");
                return(ST_EOF);
        }
        if (strncmp(header, "Creative Voice File\032", 19))
        {
                st_fail_errno(ft,ST_EHDR,"VOC file header incorrect");
                return(ST_EOF);
        }

        /* read the offset to data, from start of file */
        /* after this read we have read 20 bytes of header + 2 */
        st_readw(ft, &sbseek);

        /* ANN:  read to skip the header, instead of lseek */
        /* this should allow use with pipes.... */
        for (ii=22; ii<sbseek; ii++)
            st_readb(ft, &uc);

        v->rate = -1;
        v->rest = 0;
        v->total_size = 0;  /* ANN added */
        v->extended = 0;
        v->format = VOC_FMT_LIN8U;

        /* read until we get the format information.... */
        rc = getblock(ft);
        if (rc)
            return rc;

        /* get rate of data */
        if (v->rate == -1)
        {
                st_fail_errno(ft,ST_EOF,"Input .voc file had no sound!");
                return(ST_EOF);
        }

        /* setup word length of data */
        ft->info.size = v->size;

        /* ANN:  Check VOC format and map to the proper ST format value */
        switch (v->format) {
        case VOC_FMT_LIN8U:      /*     0    8 bit unsigned linear PCM */
            ft->info.encoding = ST_ENCODING_UNSIGNED;
            break;
        case VOC_FMT_CRLADPCM4:  /*     1    Creative 8-bit to 4-bit ADPCM */
            st_warn ("Unsupported VOC format CRLADPCM4 %d", v->format);
            rtn=ST_EOF;
            break;
        case VOC_FMT_CRLADPCM3:  /*     2    Creative 8-bit to 3-bit ADPCM */
            st_warn ("Unsupported VOC format CRLADPCM3 %d", v->format);
            rtn=ST_EOF;
            break;
        case VOC_FMT_CRLADPCM2:  /*     3    Creative 8-bit to 2-bit ADPCM */
            st_warn ("Unsupported VOC format CRLADPCM2 %d", v->format);
            rtn=ST_EOF;
            break;
        case VOC_FMT_LIN16:      /*     4    16-bit signed PCM */
            ft->info.encoding = ST_ENCODING_SIGN2;
            break;
        case VOC_FMT_ALAW:       /*     6    CCITT a-Law 8-bit PCM */
            ft->info.encoding = ST_ENCODING_ALAW;
            break;
        case VOC_FMT_MU255:      /*     7    CCITT u-Law 8-bit PCM */
            ft->info.encoding = ST_ENCODING_ULAW;
            break;
        case VOC_FMT_CRLADPCM4A: /*0x200    Creative 16-bit to 4-bit ADPCM */
            printf ("Unsupported VOC format CRLADPCM4A %d", v->format);
            rtn=ST_EOF;
            break;
        default:
            printf ("Unknown VOC format %d", v->format);
            rtn=ST_EOF;
            break;
        }

        /* setup number of channels */
        if (ft->info.channels == -1)
                ft->info.channels = v->channels;

        return(ST_SUCCESS);
}

/*-----------------------------------------------------------------
 * st_vocread() -- read data from a VOC file
 * ANN:  Major changes here to support multi-part files and files
 *       that do not have audio in block 9's.
 *-----------------------------------------------------------------*/
st_ssize_t st_vocread(ft_t ft, st_sample_t *buf, st_ssize_t len)
{
        vs_t v = (vs_t) ft->priv;
        int done = 0;
        int rc = 0;
        int16_t sw;
        unsigned char  uc;

        /* handle getting another cont. buffer */
        if (v->rest == 0)
        {
                rc = getblock(ft);
                if (rc)
                    return 0;
        }

        /* if no more data, return 0, i.e., done */
        if (v->rest == 0)
                return 0;

        /* if silence, fill it in with 0's */
        if (v->silent) {
                /* Fill in silence */
                for(;v->rest && (done < len); v->rest--, done++)
                        *buf++ = 0x80000000L;
        }
        /* else, not silence, read the block */
        else {
            /* read len samples of audio from the file */

            /* for(;v->rest && (done < len); v->rest--, done++) { */
            for(; (done < len); done++) {

                /* IF no more in this block, get another */
                if (v->rest == 0) {

                    /* DO until we have either EOF or a block with data */
                    while (v->rest == 0) {
                        rc = getblock(ft);
                        if (rc)
                            break;
                    }
                    /* ENDDO ... */

                    /* IF EOF, break out, no more data, next will return 0 */
                    if (rc)
                        break;
                }
                /* ENDIF no more data in block */

                /* Read the data in the file */
                switch(v->size) {
                case ST_SIZE_BYTE:
                    if (st_readb(ft, &uc) == ST_EOF) {
                        st_warn("VOC input: short file");
                        v->rest = 0;
                        return done;
                    }
                    /* IF uLaw,alaw, expand to linear, else convert??? */
                    /* ANN:  added uLaw and aLaw support */
                    if (v->format == VOC_FMT_MU255) {
                        *buf++ =  ST_ULAW_BYTE_TO_SAMPLE(uc);
                    } else if (v->format == VOC_FMT_ALAW) {
                        *buf++ =  ST_ALAW_BYTE_TO_SAMPLE(uc);
                    } else {
                        *buf++ = ST_UNSIGNED_BYTE_TO_SAMPLE(uc);
                    }
                    break;
                case ST_SIZE_WORD:
                    st_readw(ft, (unsigned short *)&sw);
                    if (st_eof(ft))
                        {
                            st_warn("VOC input: short file");
                            v->rest = 0;
                            return done;
                        }
                    *buf++ = ST_SIGNED_WORD_TO_SAMPLE(sw);
                    v->rest--; /* Processed 2 bytes so update */
                    break;
                }
                /* decrement count of processed bytes */
                v->rest--; /* Processed 2 bytes so update */
            }
        }
        v->total_size+=done;
        return done;
}

/*-----------------------------------------------------------------
 * st_vocstartread() -- start reading a VOC file
 * nothing to do
 *-----------------------------------------------------------------*/
int st_vocstopread(ft_t ft)
{
    return(ST_SUCCESS);
}

/* When saving samples in VOC format the following outline is followed:
 * If an 8-bit mono sample then use a VOC_DATA header.
 * If an 8-bit stereo sample then use a VOC_EXTENDED header followed
 * by a VOC_DATA header.
 * If a 16-bit sample (either stereo or mono) then save with a
 * VOC_DATA_16 header.
 *
 * ANN:  Not supported:  uLaw and aLaw output VOC files....
 *
 * This approach will cause the output to be an its most basic format
 * which will work with the oldest software (eg. an 8-bit mono sample
 * will be able to be played with a really old SB VOC player.)
 */
int st_vocstartwrite(ft_t ft)
{
        vs_t v = (vs_t) ft->priv;

        /* VOC is in Little Endian format.  Swap whats read */
        /* in on Big Endian machines.                       */
        if (ST_IS_BIGENDIAN)
        {
                ft->swap = ft->swap ? 0 : 1;
        }

        if (! ft->seekable)
        {
                st_fail_errno(ft,ST_EOF,
                              "Output .voc file must be a file, not a pipe");
                return(ST_EOF);
        }

        v->samples = 0;

        /* File format name and a ^Z (aborts printing under DOS) */
        st_writes(ft, "Creative Voice File\032");
        st_writew(ft, 26);                      /* size of header */
        st_writew(ft, 0x10a);              /* major/minor version number */
        st_writew(ft, 0x1129);          /* checksum of version number */

        if (ft->info.size == ST_SIZE_BYTE)
          ft->info.encoding = ST_ENCODING_UNSIGNED;
        else
          ft->info.encoding = ST_ENCODING_SIGN2;
        if (ft->info.channels == -1)
                ft->info.channels = 1;

        return(ST_SUCCESS);
}

/*-----------------------------------------------------------------
 * st_vocstartread() -- start reading a VOC file
 *-----------------------------------------------------------------*/
st_ssize_t st_vocwrite(ft_t ft, st_sample_t *buf, st_ssize_t len)
{
        vs_t v = (vs_t) ft->priv;
        unsigned char uc;
        int16_t sw;
        st_ssize_t done = 0;

        if (v->samples == 0) {
          /* No silence packing yet. */
          v->silent = 0;
          blockstart(ft);
        }
        v->samples += len;
        while(done < len) {
          if (ft->info.size == ST_SIZE_BYTE) {
            uc = ST_SAMPLE_TO_UNSIGNED_BYTE(*buf++);
            st_writeb(ft, uc);
          } else {
            sw = (int) ST_SAMPLE_TO_SIGNED_WORD(*buf++);
            st_writew(ft,sw);
          }
          done++;
        }
        return done;
}

/*-----------------------------------------------------------------
 * st_vocstopwrite() -- stop writing a VOC file
 *-----------------------------------------------------------------*/
int st_vocstopwrite(ft_t ft)
{
        blockstop(ft);
        return(ST_SUCCESS);
}

/*-----------------------------------------------------------------
 * Voc-file handlers (static, private to this module)
 *-----------------------------------------------------------------*/

/*-----------------------------------------------------------------
 * getblock() -- Read next block header, save info,
 *               leave position at start of dat
 *-----------------------------------------------------------------*/
static int getblock(ft_t ft)
{
        vs_t v = (vs_t) ft->priv;
        unsigned char uc, block;
        uint32_t sblen;
        uint16_t new_rate_16;
        uint32_t new_rate_32;
        uint32_t i;
        uint32_t trash;

        v->silent = 0;
        /* DO while we have no audio to read */
        while (v->rest == 0) {
                /* IF EOF, return EOF
                 * ANN:  was returning SUCCESS */
                if (st_eof(ft))
                        return ST_EOF;

                if (st_readb(ft, &block) == ST_EOF)
                        return ST_EOF;

                /* IF TERM block (end of file), return EOF */
                if (block == VOC_TERM)
                        return ST_EOF;

                /* IF EOF after reading block type, return EOF
                 * ANN:  was returning SUCCESS */
                if (st_eof(ft))
                        return ST_EOF;
                /*
                 * Size is an 24-bit value.  Currently there is no util
                 * func to read this so do it this cross-platform way
                 *
                 */
                st_readb(ft, &uc);
                sblen = uc;
                st_readb(ft, &uc);
                sblen |= ((uint32_t) uc) << 8;
                st_readb(ft, &uc);
                sblen |= ((uint32_t) uc) << 16;

                /* Based on VOC block type, process the block */
                /* audio may be in one or multiple blocks */
                switch(block) {
                case VOC_DATA:
                        st_readb(ft, &uc);
                        /* When DATA block preceeded by an EXTENDED     */
                        /* block, the DATA blocks rate value is invalid */
                        if (!v->extended) {
                          if (uc == 0)
                          {
                            st_fail_errno(ft,ST_EFMT,
                              "File %s: Sample rate is zero?");
                            return(ST_EOF);
                          }
                          if ((v->rate != -1) && (uc != v->rate))
                          {
                            st_fail_errno(ft,ST_EFMT,
                              "File %s: sample rate codes differ: %d != %d",
                                 ft->filename,v->rate, uc);
                            return(ST_EOF);
                          }
                          v->rate = uc;
                          ft->info.rate = 1000000.0/(256 - v->rate);
                          v->channels = 1;
                        }
                        st_readb(ft, &uc);
                        if (uc != 0)
                        {
                          st_fail_errno(ft,ST_EFMT,
                            "File %s: only interpret 8-bit data!",
                               ft->filename);
                          return(ST_EOF);
                        }
                        v->extended = 0;
                        v->rest = sblen - 2;
                        v->size = ST_SIZE_BYTE;
                        return (ST_SUCCESS);
                case VOC_DATA_16:
                        st_readdw(ft, &new_rate_32);
                        if (new_rate_32 == 0)
                        {
                            st_fail_errno(ft,ST_EFMT,
                              "File %s: Sample rate is zero?",ft->filename);
                            return(ST_EOF);
                        }
                        if ((v->rate != -1) && ((long)new_rate_32 != v->rate))
                        {
                            st_fail_errno(ft,ST_EFMT,
                              "File %s: sample rate codes differ: %d != %d",
                                ft->filename, v->rate, new_rate_32);
                            return(ST_EOF);
                        }
                        v->rate = new_rate_32;
                        ft->info.rate = new_rate_32;
                        st_readb(ft, &uc);
                        switch (uc)
                        {
                            case 8:     v->size = ST_SIZE_BYTE; break;
                            case 16:    v->size = ST_SIZE_WORD; break;
                            default:
                                st_fail_errno(ft,ST_EFMT,
                                              "Don't understand size %d", uc);
                                return(ST_EOF);
                        }
                        st_readb(ft, &(v->channels));
                        st_readw(ft, &(v->format));  /* ANN: added format */
                        st_readb(ft, (unsigned char *)&trash); /* notused */
                        st_readb(ft, (unsigned char *)&trash); /* notused */
                        st_readb(ft, (unsigned char *)&trash); /* notused */
                        st_readb(ft, (unsigned char *)&trash); /* notused */
                        v->rest = sblen - 12;
                        return (ST_SUCCESS);
                case VOC_CONT:
                        v->rest = sblen;
                        return (ST_SUCCESS);
                case VOC_SILENCE:
                        {
                        unsigned short period;

                        st_readw(ft, &period);
                        st_readb(ft, &uc);
                        if (uc == 0)
                        {
                                st_fail_errno(ft,ST_EFMT,
                                  "File %s: Silence sample rate is zero");
                                return(ST_EOF);
                        }
                        /*
                         * Some silence-packed files have gratuitously
                         * different sample rate codes in silence.
                         * Adjust period.
                         */
                        if ((v->rate != -1) && (uc != v->rate))
                                period = (period * (256 - uc))/(256 - v->rate);
                        else
                                v->rate = uc;
                        v->rest = period;
                        v->silent = 1;
                        return (ST_SUCCESS);
                        }
                case VOC_MARKER:
                        st_readb(ft, &uc);
                        st_readb(ft, &uc);
                        /* Falling! Falling! */
                case VOC_TEXT:
                        {
                            uint32_t i;
                            /* Could add to comment in SF? */
                            for(i = 0; i < sblen; i++) {
                                st_readb(ft, (unsigned char *)&trash);
                                /* uncomment lines below to display text */
                                /* Note, if this is uncommented, studio */
                                /* will not be able to read the VOC file */
                                /* ANN:  added verbose dump of text info */
                                /* */
                                if (verbose) {
                                    if ((trash != '\0') && (trash != '\r'))
                                        putc (trash, stderr);
                                }
                                /* */
                            }
                        }
                        continue;       /* get next block */
                case VOC_LOOP:
                case VOC_LOOPEND:
                        st_report("File %s: skipping repeat loop");
                        for(i = 0; i < sblen; i++)
                            st_readb(ft, (unsigned char *)&trash);
                        break;
                case VOC_EXTENDED:
                        /* An Extended block is followed by a data block */
                        /* Set this byte so we know to use the rate      */
                        /* value from the extended block and not the     */
                        /* data block.                                   */
                        v->extended = 1;
                        st_readw(ft, &new_rate_16);
                        if (new_rate_16 == 0)
                        {
                           st_fail_errno(ft,ST_EFMT,
                             "File %s: Sample rate is zero?");
                           return(ST_EOF);
                        }
                        if ((v->rate != -1) && (new_rate_16 != v->rate))
                        {
                           st_fail_errno(ft,ST_EFMT,
                             "File %s: sample rate codes differ: %d != %d",
                                        ft->filename, v->rate, new_rate_16);
                           return(ST_EOF);
                        }
                        v->rate = new_rate_16;
                        st_readb(ft, &uc);
                        if (uc != 0)
                        {
                                st_fail_errno(ft,ST_EFMT,
                                  "File %s: only interpret 8-bit data!",
                                        ft->filename);
                                return(ST_EOF);
                        }
                        st_readb(ft, &uc);
                        if (uc)
                                ft->info.channels = 2;  /* Stereo */
                        /* Needed number of channels before finishing
                           compute for rate */
                        ft->info.rate = (256000000L/(65536L - v->rate))/
                            ft->info.channels;
                        /* An extended block must be followed by a data */
                        /* block to be valid so loop back to top so it  */
                        /* can be grabed.                               */
                        continue;
                default:
                        st_report("File %s: skipping unknown block code %d",
                                ft->filename, block);
                        for(i = 0; i < sblen; i++)
                            st_readb(ft, (unsigned char *)&trash);
                }
        }
        return ST_SUCCESS;
}

/*-----------------------------------------------------------------
 * vlockstart() -- start an output block
 *-----------------------------------------------------------------*/
static void blockstart(ft_t ft)
{
        vs_t v = (vs_t) ft->priv;

        v->blockseek = st_tell(ft);
        if (v->silent) {
                st_writeb(ft, VOC_SILENCE);     /* Silence block code */
                st_writeb(ft, 0);               /* Period length */
                st_writeb(ft, 0);               /* Period length */
                st_writeb(ft, v->rate);         /* Rate code */
        } else {
          if (ft->info.size == ST_SIZE_BYTE) {
            /* 8-bit sample section.  By always setting the correct     */
            /* rate value in the DATA block (even when its preceeded    */
            /* by an EXTENDED block) old software can still play stereo */
            /* files in mono by just skipping over the EXTENDED block.  */
            /* Prehaps the rate should be doubled though to make up for */
            /* double amount of samples for a given time????            */
            if (ft->info.channels > 1) {
              st_writeb(ft, VOC_EXTENDED);      /* Voice Extended block code */
              st_writeb(ft, 4);                /* block length = 4 */
              st_writeb(ft, 0);                /* block length = 4 */
              st_writeb(ft, 0);                /* block length = 4 */
                  v->rate = 65536L - (256000000.0/(2*(float)ft->info.rate));
              st_writew(ft,v->rate);    /* Rate code */
              st_writeb(ft, 0);         /* File is not packed */
              st_writeb(ft, 1);         /* samples are in stereo */
            }
            st_writeb(ft, VOC_DATA);    /* Voice Data block code */
            st_writeb(ft, 0);           /* block length (for now) */
            st_writeb(ft, 0);           /* block length (for now) */
            st_writeb(ft, 0);           /* block length (for now) */
            v->rate = 256 - (1000000.0/(float)ft->info.rate);
            st_writeb(ft, (int) v->rate);/* Rate code */
            st_writeb(ft, 0);           /* 8-bit raw data */
        } else {
            st_writeb(ft, VOC_DATA_16); /* Voice Data block code */
            st_writeb(ft, 0);           /* block length (for now) */
            st_writeb(ft, 0);           /* block length (for now) */
            st_writeb(ft, 0);           /* block length (for now) */
            v->rate = ft->info.rate;
            st_writedw(ft, v->rate);    /* Rate code */
            st_writeb(ft, 16);          /* Sample Size */
            st_writeb(ft, ft->info.channels);   /* Sample Size */
            st_writew(ft, 0x0004);      /* Encoding */
            st_writeb(ft, 0);           /* Unused */
            st_writeb(ft, 0);           /* Unused */
            st_writeb(ft, 0);           /* Unused */
            st_writeb(ft, 0);           /* Unused */
          }
        }
}

/*-----------------------------------------------------------------
 * blockstop() -- stop an output block
 * End the current data or silence block.
 *-----------------------------------------------------------------*/
static void blockstop(ft_t ft)
{
        vs_t v = (vs_t) ft->priv;
        st_sample_t datum;

        st_writeb(ft, 0);                     /* End of file block code */
        st_seeki(ft, v->blockseek, 0);         /* seek back to block length */
        st_seeki(ft, 1, 1);                    /* seek forward one */
        if (v->silent) {
                st_writew(ft, v->samples);
        } else {
          if (ft->info.size == ST_SIZE_BYTE) {
            if (ft->info.channels > 1) {
              st_seeki(ft, 8, 1); /* forward 7 + 1 for new block header */
            }
          }
                v->samples += 2;                /* adjustment: SBDK pp. 3-5 */
                datum = (v->samples * ft->info.size) & 0xff;
                st_writeb(ft, (int)datum);       /* low byte of length */
                datum = ((v->samples * ft->info.size) >> 8) & 0xff;
                st_writeb(ft, (int)datum);  /* middle byte of length */
                datum = ((v->samples  * ft->info.size)>> 16) & 0xff;
                st_writeb(ft, (int)datum); /* high byte of length */
        }
}
