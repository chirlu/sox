/*
 * September 25, 1991
 * Copyright 1991 Guido van Rossum And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Guido van Rossum And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools SGI/Amiga AIFF format.
 * Used by SGI on 4D/35 and Indigo.
 * This is a subformat of the EA-IFF-85 format.
 * This is related to the IFF format used by the Amiga.
 * But, apparently, not the same.
 *
 * Jan 93: new version from Guido Van Rossum that 
 * correctly skips unwanted sections.
 *
 * Jan 94: add loop & marker support
 * Jul 97: added comments I/O by Leigh Smith
 * Nov 97: added verbose chunk comments
 *
 * June 1, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *   Fixed compile warnings reported by Kjetil Torgrim Homme
 *   <kjetilho@ifi.uio.no>
 *
 * Sept 9, 1998 - fixed loop markers.
 *
 * Feb. 9, 1999 - Small fix to work with invalid headers that include
 *   a INST block with markers that equal 0.  It should ingore those.
 *   Also fix endian problems when ran on Intel machines.  The check
 *   for endianness was being performed AFTER reading the header instead
 *   of before reading it.
 *
 * Nov 25, 1999 - internal functions made static
 *
 * Jul 12, 2000 - Leigh Smith <leigh@tomandandy.com>
 *   Replaced ANNO with COMT chunk writing headers and added COMT
 *   chunk reading
 */

#include "st_i.h"

#include <math.h>
#include <time.h>      /* for time stamping comments */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>     /* For SEEK_* defines if not found in stdio */
#endif

/* Private data used by writer */
typedef struct aiffpriv {
    st_size_t nsamples;  /* number of 1-channel samples read or written */
                         /* Decrements for read increments for write */
    st_size_t dataStart; /* need to for seeking */
} *aiff_t;

/* forward declarations */
static double read_ieee_extended(ft_t);
static int aiffwriteheader(ft_t, st_size_t);
static void write_ieee_extended(ft_t, double);
static double ConvertFromIeeeExtended(unsigned char*);
static void ConvertToIeeeExtended(double, char *);
static int textChunk(char **text, char *chunkDescription, ft_t ft);
static int commentChunk(char **text, char *chunkDescription, ft_t ft);
static void reportInstrument(ft_t ft);

int st_aiffseek(ft_t ft, st_size_t offset) 
{
    aiff_t aiff = (aiff_t ) ft->priv;
    st_size_t new_offset, channel_block, alignment;

    new_offset = offset * ft->info.size;
    /* Make sure request aligns to a channel block (ie left+right) */
    channel_block = ft->info.channels * ft->info.size;
    alignment = new_offset % channel_block;
    /* Most common mistaken is to compute something like
     * "skip everthing upto and including this sample" so
     * advance to next sample block in this case.
     */
    if (alignment != 0)
        new_offset += (channel_block - alignment);
    new_offset += aiff->dataStart;

    ft->st_errno = st_seeki(ft, new_offset, SEEK_SET);

    if (ft->st_errno == ST_SUCCESS)
        aiff->nsamples = ft->length - (new_offset / ft->info.size);

    return(ft->st_errno);
}

int st_aiffstartread(ft_t ft) 
{
        aiff_t aiff = (aiff_t ) ft->priv;
        char buf[5];
        uint32_t totalsize;
        uint32_t chunksize;
        unsigned short channels = 0;
        uint32_t frames;
        unsigned short bits = 0;
        double rate = 0.0;
        uint32_t offset = 0;
        uint32_t blocksize = 0;
        int foundcomm = 0, foundmark = 0, foundinstr = 0, is_sowt = 0;
        struct mark {
                unsigned short id;
                uint32_t position;
                char name[40]; 
        } marks[32];
        unsigned short looptype;
        int i, j;
        unsigned short nmarks = 0;
        unsigned short sustainLoopBegin = 0, sustainLoopEnd = 0,
                       releaseLoopBegin = 0, releaseLoopEnd = 0;
        st_size_t seekto = 0L, ssndsize = 0L;
        char *author;
        char *copyright;
        char *nametext;

        uint8_t trash8;
        uint16_t trash16;
        uint32_t trash32;

        int rc;

        /* AIFF is in Big Endian format.  Swap whats read in on Little */
        /* Endian machines.                                            */
        if (ST_IS_LITTLEENDIAN)
        {
            ft->swap = ft->swap ? 0 : 1;
        }

        /* FORM chunk */
        if (st_reads(ft, buf, 4) == ST_EOF || strncmp(buf, "FORM", 4) != 0)
        {
                st_fail_errno(ft,ST_EHDR,"AIFF header does not begin with magic word 'FORM'");
                return(ST_EOF);
        }
        st_readdw(ft, &totalsize);
        if (st_reads(ft, buf, 4) == ST_EOF || (strncmp(buf, "AIFF", 4) != 0 && 
            strncmp(buf, "AIFC", 4) != 0))
        {
                st_fail_errno(ft,ST_EHDR,"AIFF 'FORM' chunk does not specify 'AIFF' or 'AIFC' as type");
                return(ST_EOF);
        }

        
        /* Skip everything but the COMM chunk and the SSND chunk */
        /* The SSND chunk must be the last in the file */
        while (1) {
                if (st_reads(ft, buf, 4) == ST_EOF)
                {
                        if (ssndsize > 0)
                        {
                                break;
                        }
                        else
                        {
                                st_fail_errno(ft,ST_EHDR,"Missing SSND chunk in AIFF file");
                                return(ST_EOF);
                        }
                }
                if (strncmp(buf, "COMM", 4) == 0) {
                        /* COMM chunk */
                        st_readdw(ft, &chunksize);
                        st_readw(ft, &channels);
                        st_readdw(ft, &frames);
                        st_readw(ft, &bits);
                        rate = read_ieee_extended(ft);
                        chunksize -= 18;
                        if (chunksize > 0)
                        {
                            st_reads(ft, buf, 4);
                            chunksize -= 4;
                            if (strncmp(buf, "sowt", 4) == 0)
                            {
                                /* CD audio as read on Mac OS machines */
                                /* Need to endian swap all the data */
                                is_sowt = 1;
                            }
                            else if (strncmp(buf, "NONE", 4) != 0)
                            {
                                buf[4] = 0;
                                st_fail_errno(ft,ST_EHDR,"Can not support AIFC files that contain compressed data: %s",buf);
                                return(ST_EOF);
                            }
                        }
                        while(chunksize-- > 0)
                            st_readb(ft, (unsigned char *)&trash8);
                        foundcomm = 1;
                }
                else if (strncmp(buf, "SSND", 4) == 0) {
                        /* SSND chunk */
                        st_readdw(ft, &chunksize);
                        st_readdw(ft, &offset);
                        st_readdw(ft, &blocksize);
                        chunksize -= 8;
                        ssndsize = chunksize;
                        /* word-align chunksize in case it wasn't
                         * done by writing application already.
                         */
                        chunksize += (chunksize % 2);
                        /* if can't seek, just do sound now */
                        if (!ft->seekable)
                                break;
                        /* else, seek to end of sound and hunt for more */
                        seekto = st_tell(ft);
                        st_seeki(ft, chunksize, SEEK_CUR); 
                }
                else if (strncmp(buf, "MARK", 4) == 0) {
                        /* MARK chunk */
                        st_readdw(ft, &chunksize);
                        st_readw(ft, &nmarks);

                        /* Some programs like to always have a MARK chunk
                         * but will set number of marks to 0 and force
                         * software to detect and ignore it.
                         */
                        if (nmarks == 0)
                            foundmark = 0;
                        else
                            foundmark = 1;

                        chunksize -= 2;
                        for(i = 0; i < nmarks; i++) {
                                unsigned char len;

                                st_readw(ft, &(marks[i].id));
                                st_readdw(ft, &(marks[i].position));
                                chunksize -= 6;
                                st_readb(ft, &len);
                                chunksize -= len + 1;
                                for(j = 0; j < len ; j++) 
                                    st_readb(ft, (unsigned char *)&(marks[i].name[j]));
                                marks[i].name[j] = 0;
                                if ((len & 1) == 0) {
                                        chunksize--;
                                        st_readb(ft, (unsigned char *)&trash8);
                                }
                        }
                        /* HA HA!  Sound Designer (and others) makes */
                        /* bogus files. It spits out bogus chunksize */
                        /* for MARK field */
                        while(chunksize-- > 0)
                            st_readb(ft, (unsigned char *)&trash8);
                }
                else if (strncmp(buf, "INST", 4) == 0) {
                        /* INST chunk */
                        st_readdw(ft, &chunksize);
                        st_readb(ft, (unsigned char *)&(ft->instr.MIDInote));
                        st_readb(ft, (unsigned char *)&trash8);
                        st_readb(ft, (unsigned char *)&(ft->instr.MIDIlow));
                        st_readb(ft, (unsigned char *)&(ft->instr.MIDIhi));
                        /* Low  velocity */
                        st_readb(ft, (unsigned char *)&trash8);
                        /* Hi  velocity */
                        st_readb(ft, (unsigned char *)&trash8);
                        st_readw(ft, (unsigned short *)&trash16);/* gain */
                        st_readw(ft, &looptype); /* sustain loop */
                        ft->loops[0].type = looptype;
                        st_readw(ft, &sustainLoopBegin); /* begin marker */
                        st_readw(ft, &sustainLoopEnd);    /* end marker */
                        st_readw(ft, &looptype); /* release loop */
                        ft->loops[1].type = looptype;
                        st_readw(ft, &releaseLoopBegin);  /* begin marker */
                        st_readw(ft, &releaseLoopEnd);    /* end marker */

                        foundinstr = 1;
                }
                else if (strncmp(buf, "APPL", 4) == 0) {
                        st_readdw(ft, &chunksize);
                        /* word-align chunksize in case it wasn't
                         * done by writing application already.
                         */
                        chunksize += (chunksize % 2);
                        while(chunksize-- > 0)
                            st_readb(ft, (unsigned char *)&trash8);
                }
                else if (strncmp(buf, "ALCH", 4) == 0) {
                        /* I think this is bogus and gets grabbed by APPL */
                        /* INST chunk */
                        st_readdw(ft, &trash32);                /* ENVS - jeez! */
                        st_readdw(ft, &chunksize);
                        while(chunksize-- > 0)
                            st_readb(ft, (unsigned char *)&trash8);
                }
                else if (strncmp(buf, "ANNO", 4) == 0) {
                        rc = textChunk(&(ft->comment), "Annotation:", ft);
                        if (rc)
                        {
                          /* Fail already called in function */
                          return(ST_EOF);
                        }
                }
                else if (strncmp(buf, "COMT", 4) == 0) {
                  rc = commentChunk(&(ft->comment), "Comment:", ft);
                  if (rc) {
                    /* Fail already called in function */
                    return(ST_EOF);
                  }
                }
                else if (strncmp(buf, "AUTH", 4) == 0) {
                  /* Author chunk */
                  rc = textChunk(&author, "Author:", ft);
                  if (rc)
                  {
                      /* Fail already called in function */
                      return(ST_EOF);
                  }
                  free(author);
                }
                else if (strncmp(buf, "NAME", 4) == 0) {
                  /* Name chunk */
                  rc = textChunk(&nametext, "Name:", ft);
                  if (rc)
                  {
                      /* Fail already called in function */
                      return(ST_EOF);
                  }
                  free(nametext);
                }
                else if (strncmp(buf, "(c) ", 4) == 0) {
                  /* Copyright chunk */
                  rc = textChunk(&copyright, "Copyright:", ft);
                  if (rc)
                  {
                      /* Fail already called in function */
                      return(ST_EOF);
                  }
                  free(copyright);
                }
                else {
                        if (st_eof(ft))
                                break;
                        buf[4] = 0;
                        st_report("AIFFstartread: ignoring '%s' chunk\n", buf);
                        st_readdw(ft, &chunksize);
                        if (st_eof(ft))
                                break;
                        /* Skip the chunk using st_readb() so we may read
                           from a pipe */
                        while (chunksize-- > 0) {
                            if (st_readb(ft, (unsigned char *)&trash8) == ST_EOF)
                                        break;
                        }
                }
                if (st_eof(ft))
                        break;
        }

        /* 
         * if a pipe, we lose all chunks after sound.  
         * Like, say, instrument loops. 
         */
        if (ft->seekable)
        {
                if (seekto > 0)
                        st_seeki(ft, seekto, SEEK_SET);
                else
                {
                        st_fail_errno(ft,ST_EOF,"AIFF: no sound data on input file");
                        return(ST_EOF);
                }
        }
        /* SSND chunk just read */
        if (blocksize != 0)
            st_warn("AIFF header has invalid blocksize.  Ignoring but expect a premature EOF");

        while (offset-- > 0) {
                if (st_readb(ft, (unsigned char *)&trash8) == ST_EOF)
                {
                        st_fail_errno(ft,errno,"unexpected EOF while skipping AIFF offset");
                        return(ST_EOF);
                }
        }

        if (foundcomm) {
                ft->info.channels = channels;
                ft->info.rate = rate;
                if (ft->info.encoding != -1 && ft->info.encoding != ST_ENCODING_SIGN2)
                    st_report("AIFF only supports signed data.  Forcing to signed.");
                ft->info.encoding = ST_ENCODING_SIGN2;
                if (bits <= 8)
                {
                    ft->info.size = ST_SIZE_BYTE;
                    if (bits < 8)
                        st_report("Forcing data size from %d bits to 8 bits",bits);
                }
                else if (bits <= 16)
                {
                    ft->info.size = ST_SIZE_WORD;
                    if (bits < 16)
                        st_report("Forcing data size from %d bits to 16 bits",bits);
                }
                else if (bits <= 32)
                {
                    ft->info.size = ST_SIZE_DWORD;
                    if (bits < 32)
                        st_report("Forcing data size from %d bits to 32 bits",bits);
                }
                else
                {
                    st_fail_errno(ft,ST_EFMT,"unsupported sample size in AIFF header: %d", bits);
                    return(ST_EOF);
                }
        } else  {
                if ((ft->info.channels == -1)
                        || (ft->info.rate == 0)
                        || (ft->info.encoding == -1)
                        || (ft->info.size == -1)) {
                  st_report("You must specify # channels, sample rate, signed/unsigned,\n");
                  st_report("and 8/16 on the command line.");
                  st_fail_errno(ft,ST_EFMT,"Bogus AIFF file: no COMM section.");
                  return(ST_EOF);
                }

        }

        aiff->nsamples = ssndsize / ft->info.size;

        /* Cope with 'sowt' CD tracks as read on Macs */
        if (is_sowt)
        {
                aiff->nsamples -= 4;
                ft->swap = ft->swap ? 0 : 1;
        }
        
        if (foundmark && !foundinstr)
        {
            st_report("Ignoring MARK chunk since no INSTR found.");
            foundmark = 0;
        }
        if (!foundmark && foundinstr)
        {
            st_report("Ignoring INSTR chunk since no MARK found.");
            foundinstr = 0;
        }
        if (foundmark && foundinstr) {
                int i;
                int slbIndex = 0, sleIndex = 0;
                int rlbIndex = 0, rleIndex = 0;

                /* find our loop markers and save their marker indexes */
                for(i = 0; i < nmarks; i++) { 
                  if(marks[i].id == sustainLoopBegin)
                    slbIndex = i;
                  if(marks[i].id == sustainLoopEnd)
                    sleIndex = i;
                  if(marks[i].id == releaseLoopBegin)
                    rlbIndex = i;
                  if(marks[i].id == releaseLoopEnd)
                    rleIndex = i;
                }

                ft->instr.nloops = 0;
                if (ft->loops[0].type != 0) {
                        ft->loops[0].start = marks[slbIndex].position;
                        ft->loops[0].length = 
                            marks[sleIndex].position - marks[slbIndex].position;
                        /* really the loop count should be infinite */
                        ft->loops[0].count = 1; 
                        ft->instr.loopmode = ST_LOOP_SUSTAIN_DECAY | ft->loops[0].type;
                        ft->instr.nloops++;
                }
                if (ft->loops[1].type != 0) {
                        ft->loops[1].start = marks[rlbIndex].position;
                        ft->loops[1].length = 
                            marks[rleIndex].position - marks[rlbIndex].position;
                        /* really the loop count should be infinite */
                        ft->loops[1].count = 1;
                        ft->instr.loopmode = ST_LOOP_SUSTAIN_DECAY | ft->loops[1].type;
                        ft->instr.nloops++;
                } 
        }
        reportInstrument(ft);

        /* Needed because of st_rawread() */
        rc = st_rawstartread(ft);
        if (rc)
            return rc;

        ft->length = aiff->nsamples;    /* for seeking */
        aiff->dataStart = st_tell(ft);

        return(ST_SUCCESS);
}

/* print out the MIDI key allocations, loop points, directions etc */
static void reportInstrument(ft_t ft)
{
  int loopNum;

  if(ft->instr.nloops > 0)
    st_report("AIFF Loop markers:\n");
  for(loopNum  = 0; loopNum < ft->instr.nloops; loopNum++) {
    if (ft->loops[loopNum].count) {
      st_report("Loop %d: start: %6d", loopNum, ft->loops[loopNum].start);
      st_report(" end:   %6d", 
              ft->loops[loopNum].start + ft->loops[loopNum].length);
      st_report(" count: %6d", ft->loops[loopNum].count);
      st_report(" type:  ");
      switch(ft->loops[loopNum].type & ~ST_LOOP_SUSTAIN_DECAY) {
      case 0: st_report("off\n"); break;
      case 1: st_report("forward\n"); break;
      case 2: st_report("forward/backward\n"); break;
      }
    }
  }
  st_report("Unity MIDI Note: %d\n", ft->instr.MIDInote);
  st_report("Low   MIDI Note: %d\n", ft->instr.MIDIlow);
  st_report("High  MIDI Note: %d\n", ft->instr.MIDIhi);
}

/* Process a text chunk, allocate memory, display it if verbose and return */
static int textChunk(char **text, char *chunkDescription, ft_t ft) 
{
  uint32_t chunksize;
  st_readdw(ft, &chunksize);
  /* allocate enough memory to hold the text including a terminating \0 */
  *text = (char *) malloc((size_t) chunksize + 1);
  if (*text == NULL)
  {
    st_fail_errno(ft,ST_ENOMEM,"AIFF: Couldn't allocate %s header", chunkDescription);
    return(ST_EOF);
  }
  if (st_readbuf(ft, *text, 1, chunksize) != chunksize)
  {
    st_fail_errno(ft,ST_EOF,"AIFF: Unexpected EOF in %s header", chunkDescription);
    return(ST_EOF);
  }
  *(*text + chunksize) = '\0';
        if (chunksize % 2)
        {
                /* Read past pad byte */
                char c;
                if (st_readbuf(ft, &c, 1, 1) != 1)
                {
                st_fail_errno(ft,ST_EOF,"AIFF: Unexpected EOF in %s header", chunkDescription);
                        return(ST_EOF);
                }
        }
  st_report("%-10s   \"%s\"\n", chunkDescription, *text);
  return(ST_SUCCESS);
}

/* Comment lengths are words, not double words, and we can have several, so
   we use a special function, not textChunk().;
 */
static int commentChunk(char **text, char *chunkDescription, ft_t ft)
{
  uint32_t chunksize;
  unsigned short numComments;
  uint32_t timeStamp;
  unsigned short markerId;
  unsigned short totalCommentLength = 0;
  unsigned int totalReadLength = 0;
  unsigned int commentIndex;

  st_readdw(ft, &chunksize);
  st_readw(ft, &numComments);
  totalReadLength += 2; /* chunksize doesn't count */
  for(commentIndex = 0; commentIndex < numComments; commentIndex++) {
    unsigned short commentLength;

    st_readdw(ft, &timeStamp);
    st_readw(ft, &markerId);
    st_readw(ft, &commentLength);
    totalCommentLength += commentLength;
    /* allocate enough memory to hold the text including a terminating \0 */
    if(commentIndex == 0) {
      *text = (char *) malloc((size_t) totalCommentLength + 1);
    }
    else {
      *text = realloc(*text, (size_t) totalCommentLength + 1);
    }

    if (*text == NULL) {
        st_fail_errno(ft,ST_ENOMEM,"AIFF: Couldn't allocate %s header", chunkDescription);
        return(ST_EOF);
    }
    if (st_readbuf(ft, *text + totalCommentLength - commentLength, 1, commentLength) != commentLength) {
        st_fail_errno(ft,ST_EOF,"AIFF: Unexpected EOF in %s header", chunkDescription);
        return(ST_EOF);
    }
    *(*text + totalCommentLength) = '\0';
    totalReadLength += totalCommentLength + 4 + 2 + 2; /* include header */
    if (commentLength % 2) {
        /* Read past pad byte */
        char c;
        if (st_readbuf(ft, &c, 1, 1) != 1) {
            st_fail_errno(ft,ST_EOF,"AIFF: Unexpected EOF in %s header", chunkDescription);
            return(ST_EOF);
        }
    }
  }
  st_report("%-10s   \"%s\"\n", chunkDescription, *text);
  /* make sure we read the whole chunk */
  if (totalReadLength < chunksize) {
       int i;
       char c;
       for (i=0; i < chunksize - totalReadLength; i++ ) {
               st_readbuf(ft, &c, 1, 1);
       }
  }
  return(ST_SUCCESS);
}

st_ssize_t st_aiffread(ft_t ft, st_sample_t *buf, st_ssize_t len)
{
        aiff_t aiff = (aiff_t ) ft->priv;
        st_ssize_t done;

        if (len < 0)
            return ST_EOF;
        else if ((st_size_t)len > aiff->nsamples)
                len = aiff->nsamples;
        done = st_rawread(ft, buf, len);
        if (done == 0 && aiff->nsamples != 0)
                st_warn("Premature EOF on AIFF input file");
        aiff->nsamples -= done;
        return done;
}

int st_aiffstopread(ft_t ft) 
{
        char buf[5];
        uint32_t chunksize;
        uint32_t trash;

        if (!ft->seekable)
        {
            while (! st_eof(ft)) 
            {
                if (st_readbuf(ft, buf, 1, 4) != 4)
                        break;

                st_readdw(ft, &chunksize);
                if (st_eof(ft))
                        break;
                buf[4] = '\0';
                st_warn("Ignoring AIFF tail chunk: '%s', %d bytes long\n", 
                        buf, chunksize);
                if (! strcmp(buf, "MARK") || ! strcmp(buf, "INST"))
                        st_warn("       You're stripping MIDI/loop info!\n");
                while (chunksize-- > 0) 
                {
                        if (st_readb(ft, (unsigned char *)&trash) == ST_EOF)
                                break;
                }
            }
        }

        /* Needed because of st_rawwrite() */
        return st_rawstopread(ft);
}

/* When writing, the header is supposed to contain the number of
   samples and data bytes written.
   Since we don't know how many samples there are until we're done,
   we first write the header with an very large number,
   and at the end we rewind the file and write the header again
   with the right number.  This only works if the file is seekable;
   if it is not, the very large size remains in the header.
   Strictly spoken this is not legal, but the playaiff utility
   will still be able to play the resulting file. */

int st_aiffstartwrite(ft_t ft)
{
        aiff_t aiff = (aiff_t ) ft->priv;
        int rc;

        /* Needed because st_rawwrite() */
        rc = st_rawstartwrite(ft);
        if (rc)
            return rc;

        /* AIFF is in Big Endian format.  Swap whats read in on Little */
        /* Endian machines.                                            */
        if (ST_IS_LITTLEENDIAN)
        {
            ft->swap = ft->swap ? 0 : 1;
        }

        aiff->nsamples = 0;
        if ((ft->info.encoding == ST_ENCODING_ULAW ||
             ft->info.encoding == ST_ENCODING_ALAW) && 
            ft->info.size == ST_SIZE_BYTE) {
                st_report("expanding 8-bit u-law to signed 16 bits");
                ft->info.encoding = ST_ENCODING_SIGN2;
                ft->info.size = ST_SIZE_WORD;
        }
        if (ft->info.encoding != -1 && ft->info.encoding != ST_ENCODING_SIGN2)
            st_report("AIFF only supports signed data.  Forcing to signed.");
        ft->info.encoding = ST_ENCODING_SIGN2; /* We have a fixed encoding */

        /* Compute the "very large number" so that a maximum number
           of samples can be transmitted through a pipe without the
           risk of causing overflow when calculating the number of bytes.
           At 48 kHz, 16 bits stereo, this gives ~3 hours of music.
           Sorry, the AIFF format does not provide for an "infinite"
           number of samples. */
        return(aiffwriteheader(ft, 0x7f000000L / (ft->info.size*ft->info.channels)));
}

st_ssize_t st_aiffwrite(ft_t ft, st_sample_t *buf, st_ssize_t len)
{
        aiff_t aiff = (aiff_t ) ft->priv;
        aiff->nsamples += len;
        st_rawwrite(ft, buf, len);
        return(len);
}

int st_aiffstopwrite(ft_t ft)
{
        aiff_t aiff = (aiff_t ) ft->priv;
        int rc;

        /* Needed because of st_rawwrite().  Call now to flush
         * buffer now before seeking around below.
         */
        rc = st_rawstopwrite(ft);
        if (rc)
            return rc;

        if (!ft->seekable)
        {
            st_fail_errno(ft,ST_EOF,"Non-seekable file.");
            return(ST_EOF);
        }
        if (st_seeki(ft, 0L, SEEK_SET) != 0)
        {
                st_fail_errno(ft,errno,"can't rewind output file to rewrite AIFF header");
                return(ST_EOF);
        }
        return(aiffwriteheader(ft, aiff->nsamples / ft->info.channels));
}

static int aiffwriteheader(ft_t ft, st_size_t nframes)
{
        int hsize =
                8 /*COMM hdr*/ + 18 /*COMM chunk*/ +
                8 /*SSND hdr*/ + 12 /*SSND chunk*/;
        int bits = 0;
        int i;
        int padded_comment_size = 0;
        int comment_size = 0;
        st_size_t comment_chunk_size = 0L;

        /* MARK and INST chunks */
        if (ft->instr.nloops) {
          hsize += 8 /* MARK hdr */ + 2 + 16*ft->instr.nloops;
          hsize += 8 /* INST hdr */ + 20; /* INST chunk */
        }

        if (ft->info.encoding == ST_ENCODING_SIGN2 && 
            ft->info.size == ST_SIZE_BYTE)
                bits = 8;
        else if (ft->info.encoding == ST_ENCODING_SIGN2 && 
                 ft->info.size == ST_SIZE_WORD)
                bits = 16;
        else if (ft->info.encoding == ST_ENCODING_SIGN2 && 
                 ft->info.size == ST_SIZE_DWORD)
                bits = 32;
        else
        {
                st_fail_errno(ft,ST_EFMT,"unsupported output encoding/size for AIFF header");
                return(ST_EOF);
        }

        /* COMT comment chunk -- holds comments text with a timestamp and marker id */
        /* We calculate the comment_chunk_size if we will be writing a comment */
        if (ft->comment)
        {
          comment_size = strlen(ft->comment);
          /* Must put an even number of characters out.
           * True 68k processors OS's seem to require this.
           */
          padded_comment_size = ((comment_size % 2) == 0) ?
                                comment_size : comment_size + 1;
          /* one comment, timestamp, marker ID and text count */
          comment_chunk_size = (2L + 4 + 2 + 2 + padded_comment_size);
          hsize += 8 /* COMT hdr */ + comment_chunk_size; 
        }

        st_writes(ft, "FORM"); /* IFF header */
        /* file size */
        st_writedw(ft, hsize + nframes * ft->info.size * ft->info.channels); 
        st_writes(ft, "AIFF"); /* File type */

        /* Now we write the COMT comment chunk using the precomputed sizes */
        if (ft->comment)
        {
          st_writes(ft, "COMT");
          st_writedw(ft, comment_chunk_size);

          /* one comment */
          st_writew(ft, 1);

          /* time stamp of comment, Unix knows of time from 1/1/1970,
             Apple knows time from 1/1/1904 */
          st_writedw(ft, ((int32_t) time(NULL)) + 2082844800L);

          /* A marker ID of 0 indicates the comment is not associated
             with a marker */
          st_writew(ft, 0);

          /* now write the count and the bytes of text */
          st_writew(ft, padded_comment_size);
          st_writes(ft, ft->comment);
          if (comment_size != padded_comment_size)
                st_writes(ft, " ");
        }

        /* COMM chunk -- describes encoding (and #frames) */
        st_writes(ft, "COMM");
        st_writedw(ft, 18); /* COMM chunk size */
        st_writew(ft, ft->info.channels); /* nchannels */
        st_writedw(ft, nframes); /* number of frames */
        st_writew(ft, bits); /* sample width, in bits */
        write_ieee_extended(ft, (double)ft->info.rate);

        /* MARK chunk -- set markers */
        if (ft->instr.nloops) {
                st_writes(ft, "MARK");
                if (ft->instr.nloops > 2)
                        ft->instr.nloops = 2;
                st_writedw(ft, 2 + 16*ft->instr.nloops);
                st_writew(ft, ft->instr.nloops);

                for(i = 0; i < ft->instr.nloops; i++) {
                        st_writew(ft, i + 1);
                        st_writedw(ft, ft->loops[i].start);
                        st_writeb(ft, 0);
                        st_writeb(ft, 0);
                        st_writew(ft, i*2 + 1);
                        st_writedw(ft, ft->loops[i].start + ft->loops[i].length);
                        st_writeb(ft, 0);
                        st_writeb(ft, 0);
                }

                st_writes(ft, "INST");
                st_writedw(ft, 20);
                /* random MIDI shit that we default on */
                st_writeb(ft, ft->instr.MIDInote);
                st_writeb(ft, 0);                       /* detune */
                st_writeb(ft, ft->instr.MIDIlow);
                st_writeb(ft, ft->instr.MIDIhi);
                st_writeb(ft, 1);                       /* low velocity */
                st_writeb(ft, 127);                     /* hi  velocity */
                st_writew(ft, 0);                               /* gain */

                /* sustain loop */
                st_writew(ft, ft->loops[0].type);
                st_writew(ft, 1);                               /* marker 1 */
                st_writew(ft, 3);                               /* marker 3 */
                /* release loop, if there */
                if (ft->instr.nloops == 2) {
                        st_writew(ft, ft->loops[1].type);
                        st_writew(ft, 2);                       /* marker 2 */
                        st_writew(ft, 4);                       /* marker 4 */
                } else {
                        st_writew(ft, 0);                       /* no release loop */
                        st_writew(ft, 0);
                        st_writew(ft, 0);
                }
        }

        /* SSND chunk -- describes data */
        st_writes(ft, "SSND");
        /* chunk size */
        st_writedw(ft, 8 + nframes * ft->info.channels * ft->info.size); 
        st_writedw(ft, 0); /* offset */
        st_writedw(ft, 0); /* block size */
        return(ST_SUCCESS);
}

static double read_ieee_extended(ft_t ft)
{
        char buf[10];
        if (st_readbuf(ft, buf, 1, 10) != 10)
        {
                st_fail_errno(ft,ST_EOF,"EOF while reading IEEE extended number");
                return(ST_EOF);
        }
        return ConvertFromIeeeExtended((unsigned char *)buf);
}

static void write_ieee_extended(ft_t ft, double x)
{
        char buf[10];
        ConvertToIeeeExtended(x, buf);
        /*
        st_report("converted %g to %o %o %o %o %o %o %o %o %o %o",
                x,
                buf[0], buf[1], buf[2], buf[3], buf[4],
                buf[5], buf[6], buf[7], buf[8], buf[9]);
        */
        (void)st_writebuf(ft, buf, 1, 10);
}


/*
 * C O N V E R T   T O   I E E E   E X T E N D E D
 */

/* Copyright (C) 1988-1991 Apple Computer, Inc.
 * All rights reserved.
 *
 * Machine-independent I/O routines for IEEE floating-point numbers.
 *
 * NaN's and infinities are converted to HUGE_VAL or HUGE, which
 * happens to be infinity on IEEE machines.  Unfortunately, it is
 * impossible to preserve NaN's in a machine-independent way.
 * Infinities are, however, preserved on IEEE machines.
 *
 * These routines have been tested on the following machines:
 *    Apple Macintosh, MPW 3.1 C compiler
 *    Apple Macintosh, THINK C compiler
 *    Silicon Graphics IRIS, MIPS compiler
 *    Cray X/MP and Y/MP
 *    Digital Equipment VAX
 *
 *
 * Implemented by Malcolm Slaney and Ken Turkowski.
 *
 * Malcolm Slaney contributions during 1988-1990 include big- and little-
 * endian file I/O, conversion to and from Motorola's extended 80-bit
 * floating-point format, and conversions to and from IEEE single-
 * precision floating-point format.
 *
 * In 1991, Ken Turkowski implemented the conversions to and from
 * IEEE double-precision format, added more precision to the extended
 * conversions, and accommodated conversions involving +/- infinity,
 * NaN's, and denormalized numbers.
 */

#ifndef HUGE_VAL
# define HUGE_VAL HUGE
#endif /*HUGE_VAL*/

# define FloatToUnsigned(f) ((uint32_t)(((int32_t)(f - 2147483648.0)) + 2147483647L) + 1)

static void ConvertToIeeeExtended(double num, char *bytes)
{
    int    sign;
    int expon;
    double fMant, fsMant;
    uint32_t hiMant, loMant;

    if (num < 0) {
        sign = 0x8000;
        num *= -1;
    } else {
        sign = 0;
    }

    if (num == 0) {
        expon = 0; hiMant = 0; loMant = 0;
    }
    else {
        fMant = frexp(num, &expon);
        if ((expon > 16384) || !(fMant < 1)) {    /* Infinity or NaN */
            expon = sign|0x7FFF; hiMant = 0; loMant = 0; /* infinity */
        }
        else {    /* Finite */
            expon += 16382;
            if (expon < 0) {    /* denormalized */
                fMant = ldexp(fMant, expon);
                expon = 0;
            }
            expon |= sign;
            fMant = ldexp(fMant, 32);          
            fsMant = floor(fMant); 
            hiMant = FloatToUnsigned(fsMant);
            fMant = ldexp(fMant - fsMant, 32); 
            fsMant = floor(fMant); 
            loMant = FloatToUnsigned(fsMant);
        }
    }
    
    bytes[0] = expon >> 8;
    bytes[1] = expon;
    bytes[2] = hiMant >> 24;
    bytes[3] = hiMant >> 16;
    bytes[4] = hiMant >> 8;
    bytes[5] = hiMant;
    bytes[6] = loMant >> 24;
    bytes[7] = loMant >> 16;
    bytes[8] = loMant >> 8;
    bytes[9] = loMant;
}


/*
 * C O N V E R T   F R O M   I E E E   E X T E N D E D  
 */

/* 
 * Copyright (C) 1988-1991 Apple Computer, Inc.
 * All rights reserved.
 *
 * Machine-independent I/O routines for IEEE floating-point numbers.
 *
 * NaN's and infinities are converted to HUGE_VAL or HUGE, which
 * happens to be infinity on IEEE machines.  Unfortunately, it is
 * impossible to preserve NaN's in a machine-independent way.
 * Infinities are, however, preserved on IEEE machines.
 *
 * These routines have been tested on the following machines:
 *    Apple Macintosh, MPW 3.1 C compiler
 *    Apple Macintosh, THINK C compiler
 *    Silicon Graphics IRIS, MIPS compiler
 *    Cray X/MP and Y/MP
 *    Digital Equipment VAX
 *
 *
 * Implemented by Malcolm Slaney and Ken Turkowski.
 *
 * Malcolm Slaney contributions during 1988-1990 include big- and little-
 * endian file I/O, conversion to and from Motorola's extended 80-bit
 * floating-point format, and conversions to and from IEEE single-
 * precision floating-point format.
 *
 * In 1991, Ken Turkowski implemented the conversions to and from
 * IEEE double-precision format, added more precision to the extended
 * conversions, and accommodated conversions involving +/- infinity,
 * NaN's, and denormalized numbers.
 */

#ifndef HUGE_VAL
# define HUGE_VAL HUGE
#endif /*HUGE_VAL*/

# define UnsignedToFloat(u)         (((double)((int32_t)(u - 2147483647L - 1))) + 2147483648.0)

/****************************************************************
 * Extended precision IEEE floating-point conversion routine.
 ****************************************************************/

static double ConvertFromIeeeExtended(unsigned char *bytes)
{
    double    f;
    int    expon;
    uint32_t hiMant, loMant;
    
    expon = ((bytes[0] & 0x7F) << 8) | (bytes[1] & 0xFF);
    hiMant    =    ((uint32_t)(bytes[2] & 0xFF) << 24)
            |    ((uint32_t)(bytes[3] & 0xFF) << 16)
            |    ((uint32_t)(bytes[4] & 0xFF) << 8)
            |    ((uint32_t)(bytes[5] & 0xFF));
    loMant    =    ((uint32_t)(bytes[6] & 0xFF) << 24)
            |    ((uint32_t)(bytes[7] & 0xFF) << 16)
            |    ((uint32_t)(bytes[8] & 0xFF) << 8)
            |    ((uint32_t)(bytes[9] & 0xFF));

    if (expon == 0 && hiMant == 0 && loMant == 0) {
        f = 0;
    }
    else {
        if (expon == 0x7FFF) {    /* Infinity or NaN */
            f = HUGE_VAL;
        }
        else {
            expon -= 16383;
            f  = ldexp(UnsignedToFloat(hiMant), expon-=31);
            f += ldexp(UnsignedToFloat(loMant), expon-=32);
        }
    }

    if (bytes[0] & 0x80)
        return -f;
    else
        return f;
}
