/*
 * Microsoft's WAVE sound format driver
 *
 * Copyright 1998-2006 Chris Bagwell and SoX Contributors
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * Copyright 1992 Rick Richardson
 * Copyright 1997 Graeme W. Gill, 93/5/17
 *
 * Info for format tags can be found at:
 *   http://www.microsoft.com/asf/resources/draft-ietf-fleischman-codec-subtree-01.txt
 *
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>             /* For SEEK_* defines if not found in stdio */
#endif

#include "st_i.h"
#include "wav.h"
#include "ima_rw.h"
#include "adpcm.h"
#ifdef EXTERNAL_GSM
#include <gsm/gsm.h>
#else
#include "libgsm/gsm.h"
#endif

/* To allow padding to samplesPerBlock. Works, but currently never true. */
static st_size_t pad_nsamps = st_false;

/* Private data for .wav file */
typedef struct wavstuff {
    st_size_t      numSamples;     /* samples/channel reading: starts at total count and decremented  */
                                   /* writing: starts at 0 and counts samples written */
    st_size_t      dataLength;     /* needed for ADPCM writing */
    unsigned short formatTag;      /* What type of encoding file is using */
    unsigned short samplesPerBlock;
    unsigned short blockAlign;
    st_size_t dataStart;  /* need to for seeking */
    int found_cooledit;   

    /* following used by *ADPCM wav files */
    unsigned short nCoefs;          /* ADPCM: number of coef sets */
    short         *iCoefs;          /* ADPCM: coef sets           */
    unsigned char *packet;          /* Temporary buffer for packets */
    short         *samples;         /* interleaved samples buffer */
    short         *samplePtr;       /* Pointer to current sample  */
    short         *sampleTop;       /* End of samples-buffer      */
    unsigned short blockSamplesRemaining;/* Samples remaining per channel */    
    int            state[16];       /* step-size info for *ADPCM writes */

    /* following used by GSM 6.10 wav */
    gsm            gsmhandle;
    gsm_signal     *gsmsample;
    int            gsmindex;
    st_size_t      gsmbytecount;    /* counts bytes written to data block */
} *wav_t;

static char *wav_format_str(unsigned wFormatTag);

static int wavwritehdr(ft_t, int);


/****************************************************************************/
/* IMA ADPCM Support Functions Section                                      */
/****************************************************************************/

/*
 *
 * ImaAdpcmReadBlock - Grab and decode complete block of samples
 *
 */
static unsigned short  ImaAdpcmReadBlock(ft_t ft)
{
    wav_t       wav = (wav_t) ft->priv;
    int bytesRead;
    int samplesThisBlock;

    /* Pull in the packet and check the header */
    bytesRead = st_readbuf(ft, wav->packet, 1, wav->blockAlign);
    samplesThisBlock = wav->samplesPerBlock;
    if (bytesRead < wav->blockAlign) 
    { 
        /* If it looks like a valid header is around then try and */
        /* work with partial blocks.  Specs say it should be null */
        /* padded but I guess this is better than trailing quiet. */
        samplesThisBlock = ImaSamplesIn(0, ft->signal.channels, bytesRead, 0);
        if (samplesThisBlock == 0) 
        {
            st_warn("Premature EOF on .wav input file");
            return 0;
        }
    }
    
    wav->samplePtr = wav->samples;
    
    /* For a full block, the following should be true: */
    /* wav->samplesPerBlock = blockAlign - 8byte header + 1 sample in header */
    ImaBlockExpandI(ft->signal.channels, wav->packet, wav->samples, samplesThisBlock);
    return samplesThisBlock;

}

/****************************************************************************/
/* MS ADPCM Support Functions Section                                       */
/****************************************************************************/

/*
 *
 * AdpcmReadBlock - Grab and decode complete block of samples
 *
 */
static unsigned short  AdpcmReadBlock(ft_t ft)
{
    wav_t       wav = (wav_t) ft->priv;
    int bytesRead;
    int samplesThisBlock;
    const char *errmsg;

    /* Pull in the packet and check the header */
    bytesRead = st_readbuf(ft, wav->packet, 1, wav->blockAlign);
    samplesThisBlock = wav->samplesPerBlock;
    if (bytesRead < wav->blockAlign) 
    {
        /* If it looks like a valid header is around then try and */
        /* work with partial blocks.  Specs say it should be null */
        /* padded but I guess this is better than trailing quiet. */
        samplesThisBlock = AdpcmSamplesIn(0, ft->signal.channels, bytesRead, 0);
        if (samplesThisBlock == 0) 
        {
            st_warn("Premature EOF on .wav input file");
            return 0;
        }
    }
    
    errmsg = AdpcmBlockExpandI(ft->signal.channels, wav->nCoefs, wav->iCoefs, wav->packet, wav->samples, samplesThisBlock);

    if (errmsg)
        st_warn((char*)errmsg);

    return samplesThisBlock;
}

/****************************************************************************/
/* Common ADPCM Write Function                                              */
/****************************************************************************/

static int xxxAdpcmWriteBlock(ft_t ft)
{
    wav_t wav = (wav_t) ft->priv;
    int chans, ct;
    short *p;

    chans = ft->signal.channels;
    p = wav->samplePtr;
    ct = p - wav->samples;
    if (ct>=chans) { 
        /* zero-fill samples if needed to complete block */
        for (p = wav->samplePtr; p < wav->sampleTop; p++) *p=0;
        /* compress the samples to wav->packet */
        if (wav->formatTag == WAVE_FORMAT_ADPCM) {
            AdpcmBlockMashI(chans, wav->samples, wav->samplesPerBlock, wav->state, wav->packet, wav->blockAlign);
        }else{ /* WAVE_FORMAT_IMA_ADPCM */
            ImaBlockMashI(chans, wav->samples, wav->samplesPerBlock, wav->state, wav->packet, 9);
        }
        /* write the compressed packet */
        if (st_writebuf(ft, wav->packet, wav->blockAlign, 1) != 1)
        {
            st_fail_errno(ft,ST_EOF,"write error");
            return (ST_EOF);
        }
        /* update lengths and samplePtr */
        wav->dataLength += wav->blockAlign;
        if (pad_nsamps)
          wav->numSamples += wav->samplesPerBlock;
        else
          wav->numSamples += ct/chans;
        wav->samplePtr = wav->samples;
    }
    return (ST_SUCCESS);
}

/****************************************************************************/
/* WAV GSM6.10 support functions                                            */
/****************************************************************************/
/* create the gsm object, malloc buffer for 160*2 samples */
static int wavgsminit(ft_t ft)
{       
    int valueP=1;
    wav_t       wav = (wav_t) ft->priv;
    wav->gsmbytecount=0;
    wav->gsmhandle=gsm_create();
    if (!wav->gsmhandle)
    {
        st_fail_errno(ft,ST_EOF,"cannot create GSM object");
        return (ST_EOF);
    }
        
    if(gsm_option(wav->gsmhandle,GSM_OPT_WAV49,&valueP) == -1){
        st_fail_errno(ft,ST_EOF,"error setting gsm_option for WAV49 format. Recompile gsm library with -DWAV49 option and relink sox");
        return (ST_EOF);
    }

    wav->gsmsample=(gsm_signal*)xmalloc(sizeof(gsm_signal)*160*2);
    wav->gsmindex=0;
    return (ST_SUCCESS);
}

/*destroy the gsm object and free the buffer */
static void wavgsmdestroy(ft_t ft)
{       
    wav_t       wav = (wav_t) ft->priv;
    gsm_destroy(wav->gsmhandle);
    free(wav->gsmsample);
}

static st_size_t wavgsmread(ft_t ft, st_sample_t *buf, st_size_t len)
{
    wav_t       wav = (wav_t) ft->priv;
    size_t done=0;
    int bytes;
    gsm_byte    frame[65];

    ft->st_errno = ST_SUCCESS;

  /* copy out any samples left from the last call */
    while(wav->gsmindex && (wav->gsmindex<160*2) && (done < len))
        buf[done++]=ST_SIGNED_WORD_TO_SAMPLE(wav->gsmsample[wav->gsmindex++],);

  /* read and decode loop, possibly leaving some samples in wav->gsmsample */
    while (done < len) {
        wav->gsmindex=0;
        bytes = st_readbuf(ft, frame, 1, 65);   
        if (bytes <=0)
            return done;
        if (bytes<65) {
            st_warn("invalid wav gsm frame size: %d bytes",bytes);
            return done;
        }
        /* decode the long 33 byte half */
        if(gsm_decode(wav->gsmhandle,frame, wav->gsmsample)<0)
        {
            st_fail_errno(ft,ST_EOF,"error during gsm decode");
            return 0;
        }
        /* decode the short 32 byte half */
        if(gsm_decode(wav->gsmhandle,frame+33, wav->gsmsample+160)<0)
        {
            st_fail_errno(ft,ST_EOF,"error during gsm decode");
            return 0;
        }

        while ((wav->gsmindex <160*2) && (done < len)){
            buf[done++]=ST_SIGNED_WORD_TO_SAMPLE(wav->gsmsample[(wav->gsmindex)++],);
        }
    }

    return done;
}

static int wavgsmflush(ft_t ft)
{
    gsm_byte    frame[65];
    wav_t       wav = (wav_t) ft->priv;

    /* zero fill as needed */
    while(wav->gsmindex<160*2)
        wav->gsmsample[wav->gsmindex++]=0;

    /*encode the even half short (32 byte) frame */
    gsm_encode(wav->gsmhandle, wav->gsmsample, frame);
    /*encode the odd half long (33 byte) frame */
    gsm_encode(wav->gsmhandle, wav->gsmsample+160, frame+32);
    if (st_writebuf(ft, frame, 1, 65) != 65)
    {
        st_fail_errno(ft,ST_EOF,"write error");
        return (ST_EOF);
    }
    wav->gsmbytecount += 65;

    wav->gsmindex = 0;
    return (ST_SUCCESS);
}

static st_size_t wavgsmwrite(ft_t ft, const st_sample_t *buf, st_size_t len)
{
    wav_t wav = (wav_t) ft->priv;
    size_t done = 0;
    int rc;

    ft->st_errno = ST_SUCCESS;

    while (done < len) {
        while ((wav->gsmindex < 160*2) && (done < len))
            wav->gsmsample[(wav->gsmindex)++] = 
                ST_SAMPLE_TO_SIGNED_WORD(buf[done++], ft->clips);

        if (wav->gsmindex < 160*2)
            break;

        rc = wavgsmflush(ft);
        if (rc)
            return 0;
    }     
    return done;

}

static void wavgsmstopwrite(ft_t ft)
{
    wav_t       wav = (wav_t) ft->priv;

    ft->st_errno = ST_SUCCESS;

    if (wav->gsmindex)
        wavgsmflush(ft);

    /* Add a pad byte if amount of written bytes is not even. */
    if (wav->gsmbytecount && wav->gsmbytecount % 2){
        if(st_writeb(ft, 0))
            st_fail_errno(ft,ST_EOF,"write error");
        else
            wav->gsmbytecount += 1;
    }

    wavgsmdestroy(ft);
}

/****************************************************************************/
/* General Sox WAV file code                                                */
/****************************************************************************/
static int findChunk(ft_t ft, const char *Label, st_size_t *len)
{
    char magic[5];
    for (;;)
    {
        if (st_reads(ft, magic, 4) == ST_EOF)
        {
            st_fail_errno(ft, ST_EHDR, "WAVE file has missing %s chunk", 
                          Label);
            return ST_EOF;
        }
        st_debug("WAV Chunk %s", magic);
        if (st_readdw(ft, len) == ST_EOF)
        {
            st_fail_errno(ft, ST_EHDR, "WAVE file %s chunk is too short", 
                          magic);
            return ST_EOF;
        }

        if (strncmp(Label, magic, 4) == 0)
            break; /* Found the given chunk */

        /* skip to next chunk */
        if (*len == 0 || st_seeki(ft, *len, SEEK_CUR) != ST_SUCCESS)
        {
            st_fail_errno(ft,ST_EHDR, 
                          "WAV chunk appears to have invalid size %d.", *len);
            return ST_EOF;
        }
    }
    return ST_SUCCESS;
}

/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *      Find out sampling rate, 
 *      size and encoding of samples, 
 *      mono/stereo/quad.
 */
static int st_wavstartread(ft_t ft) 
{
    wav_t       wav = (wav_t) ft->priv;
    char        magic[5];
    uint32_t    len;
    int         rc;

    /* wave file characteristics */
    uint32_t      dwRiffLength;
    unsigned short wChannels;       /* number of channels */
    uint32_t      dwSamplesPerSecond; /* samples per second per channel */
    uint32_t      dwAvgBytesPerSec;/* estimate of bytes per second needed */
    uint16_t wBitsPerSample;  /* bits per sample */
    uint32_t wFmtSize;
    uint16_t wExtSize = 0;    /* extended field for non-PCM */

    uint32_t      dwDataLength;    /* length of sound data in bytes */
    st_size_t    bytesPerBlock = 0;
    int    bytespersample;          /* bytes per sample (per channel */
    char text[256];
    uint32_t      dwLoopPos;

        ft->st_errno = ST_SUCCESS;

    if (st_reads(ft, magic, 4) == ST_EOF || (strncmp("RIFF", magic, 4) != 0 &&
                                             strncmp("RIFX", magic, 4) != 0))
    {
        st_fail_errno(ft,ST_EHDR,"WAVE: RIFF header not found");
        return ST_EOF;
    }

    /* RIFX is a Big-endian RIFF */
    if (strncmp("RIFX", magic, 4) == 0) 
    {
        st_debug("Found RIFX header, swapping bytes");
        ft->signal.reverse_bytes = ST_IS_LITTLEENDIAN;
    }

    st_readdw(ft, &dwRiffLength);

    if (st_reads(ft, magic, 4) == ST_EOF || strncmp("WAVE", magic, 4))
    {
        st_fail_errno(ft,ST_EHDR,"WAVE header not found");
        return ST_EOF;
    }

    /* Now look for the format chunk */
    if (findChunk(ft, "fmt ", &len) == ST_EOF)
    {
        st_fail_errno(ft,ST_EHDR,"WAVE chunk fmt not found");
        return ST_EOF;
    }
    wFmtSize = len;
    
    if (wFmtSize < 16)
    {
        st_fail_errno(ft,ST_EHDR,"WAVE file fmt chunk is too short");
        return ST_EOF;
    }

    st_readw(ft, &(wav->formatTag));
    st_readw(ft, &wChannels);
    st_readdw(ft, &dwSamplesPerSecond);
    st_readdw(ft, &dwAvgBytesPerSec);   /* Average bytes/second */
    st_readw(ft, &(wav->blockAlign));   /* Block align */
    st_readw(ft, &wBitsPerSample);      /* bits per sample per channel */
    len -= 16;

    if (wav->formatTag == WAVE_FORMAT_EXTENSIBLE)
    {
      uint16_t extensionSize;
      uint16_t numberOfValidBits;
      uint32_t speakerPositionMask;
      uint16_t subFormatTag;
      uint8_t dummyByte;
      int i;

      if (wFmtSize < 18)
      {
        st_fail_errno(ft,ST_EHDR,"WAVE file fmt chunk is too short");
        return ST_EOF;
      }
      st_readw(ft, &extensionSize);
      len -= 2;
      if (extensionSize < 22)
      {
        st_fail_errno(ft,ST_EHDR,"WAVE file fmt chunk is too short");
        return ST_EOF;
      }
      st_readw(ft, &numberOfValidBits);
      st_readdw(ft, &speakerPositionMask);
      st_readw(ft, &subFormatTag);
      for (i = 0; i < 14; ++i) st_readb(ft, &dummyByte);
      len -= 22;
      if (numberOfValidBits != wBitsPerSample)
      {
        st_fail_errno(ft,ST_EHDR,"WAVE file fmt with padded samples is not supported yet");
        return ST_EOF;
      }
      wav->formatTag = subFormatTag;
    }

    switch (wav->formatTag)
    {
    case WAVE_FORMAT_UNKNOWN:
        st_fail_errno(ft,ST_EHDR,"WAVE file is in unsupported Microsoft Official Unknown format.");
        return ST_EOF;
        
    case WAVE_FORMAT_PCM:
        /* Default (-1) depends on sample size.  Set that later on. */
        if (ft->signal.encoding != ST_ENCODING_UNKNOWN && ft->signal.encoding != ST_ENCODING_UNSIGNED &&
            ft->signal.encoding != ST_ENCODING_SIGN2)
            st_report("User options overriding encoding read in .wav header");

        /* Needed by rawread() functions */
        rc = st_rawstartread(ft);
        if (rc)
            return rc;

        break;
        
    case WAVE_FORMAT_IMA_ADPCM:
        if (ft->signal.encoding == ST_ENCODING_UNKNOWN || ft->signal.encoding == ST_ENCODING_IMA_ADPCM)
            ft->signal.encoding = ST_ENCODING_IMA_ADPCM;
        else
            st_report("User options overriding encoding read in .wav header");
        break;

    case WAVE_FORMAT_ADPCM:
        if (ft->signal.encoding == ST_ENCODING_UNKNOWN || ft->signal.encoding == ST_ENCODING_ADPCM)
            ft->signal.encoding = ST_ENCODING_ADPCM;
        else
            st_report("User options overriding encoding read in .wav header");
        break;

    case WAVE_FORMAT_IEEE_FLOAT:
        if (ft->signal.encoding == ST_ENCODING_UNKNOWN || ft->signal.encoding == ST_ENCODING_FLOAT)
            ft->signal.encoding = ST_ENCODING_FLOAT;
        else
            st_report("User options overriding encoding read in .wav header");

        /* Needed by rawread() functions */
        rc = st_rawstartread(ft);
        if (rc)
            return rc;

        break;
        
    case WAVE_FORMAT_ALAW:
        if (ft->signal.encoding == ST_ENCODING_UNKNOWN || ft->signal.encoding == ST_ENCODING_ALAW)
            ft->signal.encoding = ST_ENCODING_ALAW;
        else
            st_report("User options overriding encoding read in .wav header");

        /* Needed by rawread() functions */
        rc = st_rawstartread(ft);
        if (rc)
            return rc;

        break;
        
    case WAVE_FORMAT_MULAW:
        if (ft->signal.encoding == ST_ENCODING_UNKNOWN || ft->signal.encoding == ST_ENCODING_ULAW)
            ft->signal.encoding = ST_ENCODING_ULAW;
        else
            st_report("User options overriding encoding read in .wav header");

        /* Needed by rawread() functions */
        rc = st_rawstartread(ft);
        if (rc)
            return rc;

        break;
        
    case WAVE_FORMAT_OKI_ADPCM:
        st_fail_errno(ft,ST_EHDR,"Sorry, this WAV file is in OKI ADPCM format.");
        return ST_EOF;
    case WAVE_FORMAT_DIGISTD:
        st_fail_errno(ft,ST_EHDR,"Sorry, this WAV file is in Digistd format.");
        return ST_EOF;
    case WAVE_FORMAT_DIGIFIX:
        st_fail_errno(ft,ST_EHDR,"Sorry, this WAV file is in Digifix format.");
        return ST_EOF;
    case WAVE_FORMAT_DOLBY_AC2:
        st_fail_errno(ft,ST_EHDR,"Sorry, this WAV file is in Dolby AC2 format.");
        return ST_EOF;
    case WAVE_FORMAT_GSM610:
        if (ft->signal.encoding == ST_ENCODING_UNKNOWN || ft->signal.encoding == ST_ENCODING_GSM )
            ft->signal.encoding = ST_ENCODING_GSM;
        else
            st_report("User options overriding encoding read in .wav header");
        break;
    case WAVE_FORMAT_ROCKWELL_ADPCM:
        st_fail_errno(ft,ST_EOF,"Sorry, this WAV file is in Rockwell ADPCM format.");
        return ST_EOF;
    case WAVE_FORMAT_ROCKWELL_DIGITALK:
        st_fail_errno(ft,ST_EOF,"Sorry, this WAV file is in Rockwell DIGITALK format.");
        return ST_EOF;
    case WAVE_FORMAT_G721_ADPCM:
        st_fail_errno(ft,ST_EOF,"Sorry, this WAV file is in G.721 ADPCM format.");
        return ST_EOF;
    case WAVE_FORMAT_G728_CELP:
        st_fail_errno(ft,ST_EOF,"Sorry, this WAV file is in G.728 CELP format.");
        return ST_EOF;
    case WAVE_FORMAT_MPEG:
        st_fail_errno(ft,ST_EOF,"Sorry, this WAV file is in MPEG format.");
        return ST_EOF;
    case WAVE_FORMAT_MPEGLAYER3:
        st_fail_errno(ft,ST_EOF,"Sorry, this WAV file is in MPEG Layer 3 format.");
        return ST_EOF;
    case WAVE_FORMAT_G726_ADPCM:
        st_fail_errno(ft,ST_EOF,"Sorry, this WAV file is in G.726 ADPCM format.");
        return ST_EOF;
    case WAVE_FORMAT_G722_ADPCM:
        st_fail_errno(ft,ST_EOF,"Sorry, this WAV file is in G.722 ADPCM format.");
        return ST_EOF;
    default:    st_fail_errno(ft,ST_EOF,"WAV file has unknown format type of %x",wav->formatTag);
                return ST_EOF;
    }

    /* User options take precedence */
    if (ft->signal.channels == 0 || ft->signal.channels == wChannels)
        ft->signal.channels = wChannels;
    else
        st_report("User options overriding channels read in .wav header");

    if (ft->signal.rate == 0 || ft->signal.rate == dwSamplesPerSecond)
        ft->signal.rate = dwSamplesPerSecond;
    else
        st_report("User options overriding rate read in .wav header");
    

    wav->iCoefs = NULL;
    wav->packet = NULL;
    wav->samples = NULL;

    /* non-PCM formats expect alaw and mulaw formats have extended fmt chunk.  
     * Check for those cases. 
     */
    if (wav->formatTag != WAVE_FORMAT_PCM &&
        wav->formatTag != WAVE_FORMAT_ALAW &&
        wav->formatTag != WAVE_FORMAT_MULAW) {
        if (len >= 2) {
            st_readw(ft, &wExtSize);
            len -= 2;
        } else {
            st_warn("wave header missing FmtExt chunk");
        }
    }

    if (wExtSize > len)
    {
        st_fail_errno(ft,ST_EOF,"wave header error: wExtSize inconsistent with wFmtLen");
        return ST_EOF;
    }

    switch (wav->formatTag)
    {
    case WAVE_FORMAT_ADPCM:
        if (wExtSize < 4)
        {
            st_fail_errno(ft,ST_EOF,"format[%s]: expects wExtSize >= %d",
                        wav_format_str(wav->formatTag), 4);
            return ST_EOF;
        }

        if (wBitsPerSample != 4)
        {
            st_fail_errno(ft,ST_EOF,"Can only handle 4-bit MS ADPCM in wav files");
            return ST_EOF;
        }

        st_readw(ft, &(wav->samplesPerBlock));
        bytesPerBlock = AdpcmBytesPerBlock(ft->signal.channels, wav->samplesPerBlock);
        if (bytesPerBlock > wav->blockAlign)
        {
            st_fail_errno(ft,ST_EOF,"format[%s]: samplesPerBlock(%d) incompatible with blockAlign(%d)",
                wav_format_str(wav->formatTag), wav->samplesPerBlock, wav->blockAlign);
            return ST_EOF;
        }

        st_readw(ft, &(wav->nCoefs));
        if (wav->nCoefs < 7 || wav->nCoefs > 0x100) {
            st_fail_errno(ft,ST_EOF,"ADPCM file nCoefs (%.4hx) makes no sense", wav->nCoefs);
            return ST_EOF;
        }
        wav->packet = (unsigned char *)xmalloc(wav->blockAlign);

        len -= 4;

        if (wExtSize < 4 + 4*wav->nCoefs)
        {
            st_fail_errno(ft,ST_EOF,"wave header error: wExtSize(%d) too small for nCoefs(%d)", wExtSize, wav->nCoefs);
            return ST_EOF;
        }

        wav->samples = (short *)xmalloc(wChannels*wav->samplesPerBlock*sizeof(short));

        /* nCoefs, iCoefs used by adpcm.c */
        wav->iCoefs = (short *)xmalloc(wav->nCoefs * 2 * sizeof(short));
        {
            int i, errct=0;
            for (i=0; len>=2 && i < 2*wav->nCoefs; i++) {
                st_readw(ft, (unsigned short *)&(wav->iCoefs[i]));
                len -= 2;
                if (i<14) errct += (wav->iCoefs[i] != iCoef[i/2][i%2]);
                /* st_debug("iCoefs[%2d] %4d",i,wav->iCoefs[i]); */
            }
            if (errct) st_warn("base iCoefs differ in %d/14 positions",errct);
        }

        bytespersample = ST_SIZE_16BIT;  /* AFTER de-compression */
        break;

    case WAVE_FORMAT_IMA_ADPCM:
        if (wExtSize < 2)
        {
            st_fail_errno(ft,ST_EOF,"format[%s]: expects wExtSize >= %d",
                    wav_format_str(wav->formatTag), 2);
            return ST_EOF;
        }

        if (wBitsPerSample != 4)
        {
            st_fail_errno(ft,ST_EOF,"Can only handle 4-bit IMA ADPCM in wav files");
            return ST_EOF;
        }

        st_readw(ft, &(wav->samplesPerBlock));
        bytesPerBlock = ImaBytesPerBlock(ft->signal.channels, wav->samplesPerBlock);
        if (bytesPerBlock > wav->blockAlign || wav->samplesPerBlock%8 != 1)
        {
            st_fail_errno(ft,ST_EOF,"format[%s]: samplesPerBlock(%d) incompatible with blockAlign(%d)",
                wav_format_str(wav->formatTag), wav->samplesPerBlock, wav->blockAlign);
            return ST_EOF;
        }

        wav->packet = (unsigned char *)xmalloc(wav->blockAlign);
        len -= 2;

        wav->samples = (short *)xmalloc(wChannels*wav->samplesPerBlock*sizeof(short));

        bytespersample = ST_SIZE_16BIT;  /* AFTER de-compression */
        break;

    /* GSM formats have extended fmt chunk.  Check for those cases. */
    case WAVE_FORMAT_GSM610:
        if (wExtSize < 2)
        {
            st_fail_errno(ft,ST_EOF,"format[%s]: expects wExtSize >= %d",
                    wav_format_str(wav->formatTag), 2);
            return ST_EOF;
        }
        st_readw(ft, &wav->samplesPerBlock);
        bytesPerBlock = 65;
        if (wav->blockAlign != 65)
        {
            st_fail_errno(ft,ST_EOF,"format[%s]: expects blockAlign(%d) = %d",
                    wav_format_str(wav->formatTag), wav->blockAlign, 65);
            return ST_EOF;
        }
        if (wav->samplesPerBlock != 320)
        {
            st_fail_errno(ft,ST_EOF,"format[%s]: expects samplesPerBlock(%d) = %d",
                    wav_format_str(wav->formatTag), wav->samplesPerBlock, 320);
            return ST_EOF;
        }
        bytespersample = ST_SIZE_16BIT;  /* AFTER de-compression */
        len -= 2;
        break;

    default:
      bytespersample = (wBitsPerSample + 7)/8;

    }

    switch (bytespersample)
    {
        
    case ST_SIZE_BYTE:
        /* User options take precedence */
        if (ft->signal.size == -1 || ft->signal.size == ST_SIZE_BYTE)
            ft->signal.size = ST_SIZE_BYTE;
        else
            st_warn("User options overriding size read in .wav header");

        /* Now we have enough information to set default encodings. */
        if (ft->signal.encoding == ST_ENCODING_UNKNOWN)
            ft->signal.encoding = ST_ENCODING_UNSIGNED;
        break;
        
    case ST_SIZE_16BIT:
        if (ft->signal.size == -1 || ft->signal.size == ST_SIZE_16BIT)
            ft->signal.size = ST_SIZE_16BIT;
        else
            st_warn("User options overriding size read in .wav header");

        /* Now we have enough information to set default encodings. */
        if (ft->signal.encoding == ST_ENCODING_UNKNOWN)
            ft->signal.encoding = ST_ENCODING_SIGN2;
        break;
        
    case ST_SIZE_24BIT:
        if (ft->signal.size == -1 || ft->signal.size == ST_SIZE_24BIT)
            ft->signal.size = ST_SIZE_24BIT;
        else
            st_warn("User options overriding size read in .wav header");

        /* Now we have enough information to set default encodings. */
        if (ft->signal.encoding == ST_ENCODING_UNKNOWN)
            ft->signal.encoding = ST_ENCODING_SIGN2;
        break;
        
    case ST_SIZE_32BIT:
        if (ft->signal.size == -1 || ft->signal.size == ST_SIZE_32BIT)
            ft->signal.size = ST_SIZE_32BIT;
        else
            st_warn("User options overriding size read in .wav header");

        /* Now we have enough information to set default encodings. */
        if (ft->signal.encoding == ST_ENCODING_UNKNOWN)
            ft->signal.encoding = ST_ENCODING_SIGN2;
        break;
        
    default:
        st_fail_errno(ft,ST_EOF,"Sorry, don't understand .wav size");
        return ST_EOF;
    }

    /* Skip anything left over from fmt chunk */
    st_seeki(ft, len, SEEK_CUR);

    /* for non-PCM formats, there's a 'fact' chunk before
     * the upcoming 'data' chunk */

    /* Now look for the wave data chunk */
    if (findChunk(ft, "data", &len) == ST_EOF)
    {
        st_fail_errno(ft, ST_EOF, "Could not find data chunk.");
        return ST_EOF;
    }
    dwDataLength = len;

    /* Data starts here */
    wav->dataStart = st_tell(ft);

    switch (wav->formatTag)
    {

    case WAVE_FORMAT_ADPCM:
        wav->numSamples = 
            AdpcmSamplesIn(dwDataLength, ft->signal.channels, 
                           wav->blockAlign, wav->samplesPerBlock);
        /*st_debug("datalen %d, numSamples %d",dwDataLength, wav->numSamples);*/
        wav->blockSamplesRemaining = 0;        /* Samples left in buffer */
        ft->length = wav->numSamples*ft->signal.channels;
        break;

    case WAVE_FORMAT_IMA_ADPCM:
        /* Compute easiest part of number of samples.  For every block, there
           are samplesPerBlock samples to read. */
        wav->numSamples = 
            ImaSamplesIn(dwDataLength, ft->signal.channels, 
                         wav->blockAlign, wav->samplesPerBlock);
        /*st_debug("datalen %d, numSamples %d",dwDataLength, wav->numSamples);*/
        wav->blockSamplesRemaining = 0;        /* Samples left in buffer */
        initImaTable();
        ft->length = wav->numSamples*ft->signal.channels;
        break;

    case WAVE_FORMAT_GSM610:
        wav->numSamples = ((dwDataLength / wav->blockAlign) * wav->samplesPerBlock);
        wavgsminit(ft);
        ft->length = wav->numSamples*ft->signal.channels;
        break;

    default:
        wav->numSamples = dwDataLength/ft->signal.size/ft->signal.channels;
        ft->length = wav->numSamples*ft->signal.channels;
    }

    st_debug("Reading Wave file: %s format, %d channel%s, %d samp/sec",
           wav_format_str(wav->formatTag), ft->signal.channels,
           wChannels == 1 ? "" : "s", dwSamplesPerSecond);
    st_debug("        %d byte/sec, %d block align, %d bits/samp, %u data bytes",
           dwAvgBytesPerSec, wav->blockAlign, wBitsPerSample, dwDataLength);

    /* Can also report extended fmt information */
    switch (wav->formatTag)
    {
        case WAVE_FORMAT_ADPCM:
            st_debug("        %d Extsize, %d Samps/block, %d bytes/block %d Num Coefs, %d Samps/chan",
                      wExtSize,wav->samplesPerBlock,bytesPerBlock,wav->nCoefs,
                      wav->numSamples);
            break;

        case WAVE_FORMAT_IMA_ADPCM:
            st_debug("        %d Extsize, %d Samps/block, %d bytes/block %d Samps/chan",
                      wExtSize, wav->samplesPerBlock, bytesPerBlock, 
                      wav->numSamples);
            break;

        case WAVE_FORMAT_GSM610:
            st_debug("GSM .wav: %d Extsize, %d Samps/block, %d Samples/chan",
                      wExtSize, wav->samplesPerBlock, wav->numSamples);
            break;

        default:
            st_debug("        %d Samps/chans", wav->numSamples);
    }

    /* Horrible way to find Cool Edit marker points. Taken from Quake source*/
    ft->loops[0].start = -1;
    wav->found_cooledit = 0;
    if(ft->seekable){
        /*Got this from the quake source.  I think it 32bit aligns the chunks 
         * doubt any machine writing Cool Edit Chunks writes them at an odd 
         * offset */
        len = (len + 1) & ~1;
        if (st_seeki(ft, len, SEEK_CUR) == ST_SUCCESS &&
            findChunk(ft, "LIST", &len) != ST_EOF)
        {
            wav->found_cooledit = 1;
            ft->comment = (char*)xmalloc(256);
            /* Initialize comment to a NULL string */
            ft->comment[0] = 0;
            while(!st_eof(ft))
            {
                if (st_reads(ft,magic,4) == ST_EOF)
                    break;

                /* First look for type fields for LIST Chunk and
                 * skip those if found.  Since a LIST is a list
                 * of Chunks, treat the remaining data as Chunks
                 * again.
                 */
                if (strncmp(magic, "INFO", 4) == 0)
                {
                    /*Skip*/
                    st_debug("Type INFO");
                }
                else if (strncmp(magic, "adtl", 4) == 0)
                {
                    /* Skip */
                    st_debug("Type adtl");
                }
                else
                {
                    if (st_readdw(ft,&len) == ST_EOF)
                        break;
                    if (strncmp(magic,"ICRD",4) == 0)
                    {
                        st_debug("Chunk ICRD");
                        if (len > 254)
                        {
                            st_warn("Possible buffer overflow hack attack (ICRD)!");
                            break;
                        }
                        st_reads(ft,text,len);
                        if (strlen(ft->comment) + strlen(text) < 254)
                        {
                            if (ft->comment[0] != 0)
                                strcat(ft->comment,"\n");

                            strcat(ft->comment,text);
                        }
                        if (strlen(text) < len)
                           st_seeki(ft, len - strlen(text), SEEK_CUR); 
                    } 
                    else if (strncmp(magic,"ISFT",4) == 0)
                    {
                        st_debug("Chunk ISFT");
                        if (len > 254)
                        {
                            st_warn("Possible buffer overflow hack attack (ISFT)!");
                            break;
                        }
                        st_reads(ft,text,len);
                        if (strlen(ft->comment) + strlen(text) < 254)
                        {
                            if (ft->comment[0] != 0)
                                strcat(ft->comment,"\n");

                            strcat(ft->comment,text);
                        }
                        if (strlen(text) < len)
                           st_seeki(ft, len - strlen(text), SEEK_CUR); 
                    } 
                    else if (strncmp(magic,"cue ",4) == 0)
                    {
                        st_debug("Chunk cue ");
                        st_seeki(ft,len-4,SEEK_CUR);
                        st_readdw(ft,&dwLoopPos);
                        ft->loops[0].start = dwLoopPos;
                    } 
                    else if (strncmp(magic,"ltxt",4) == 0)
                    {
                        st_debug("Chunk ltxt");
                        st_readdw(ft,&dwLoopPos);
                        ft->loops[0].length = dwLoopPos - ft->loops[0].start;
                        if (len > 4)
                           st_seeki(ft, len - 4, SEEK_CUR); 
                    } 
                    else 
                    {
                        st_debug("Attempting to seek beyond unsupported chunk '%c%c%c%c' of length %d bytes", magic[0], magic[1], magic[2], magic[3], len);
                        len = (len + 1) & ~1;
                        st_seeki(ft, len, SEEK_CUR);
                    }
                }
            }
        }
        st_clearerr(ft);
        st_seeki(ft,wav->dataStart,SEEK_SET);
    }   
    return ST_SUCCESS;
}


/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

static st_size_t st_wavread(ft_t ft, st_sample_t *buf, st_size_t len) 
{
        wav_t   wav = (wav_t) ft->priv;
        st_size_t done;

        ft->st_errno = ST_SUCCESS;
        
        /* If file is in ADPCM encoding then read in multiple blocks else */
        /* read as much as possible and return quickly. */
        switch (ft->signal.encoding)
        {
        case ST_ENCODING_IMA_ADPCM:
        case ST_ENCODING_ADPCM:

            /* See reason for cooledit check in comments below */
            if (wav->found_cooledit && len > (wav->numSamples*ft->signal.channels)) 
                len = (wav->numSamples*ft->signal.channels);

            done = 0;
            while (done < len) { /* Still want data? */
                /* See if need to read more from disk */
                if (wav->blockSamplesRemaining == 0) { 
                    if (wav->formatTag == WAVE_FORMAT_IMA_ADPCM)
                        wav->blockSamplesRemaining = ImaAdpcmReadBlock(ft);
                    else
                        wav->blockSamplesRemaining = AdpcmReadBlock(ft);
                    if (wav->blockSamplesRemaining == 0)
                    {
                        /* Don't try to read any more samples */
                        wav->numSamples = 0;
                        return done;
                    }
                    wav->samplePtr = wav->samples;
                }

                /* Copy interleaved data into buf, converting to st_sample_t */
                {
                    short *p, *top;
                    size_t ct;
                    ct = len-done;
                    if (ct > (wav->blockSamplesRemaining*ft->signal.channels))
                        ct = (wav->blockSamplesRemaining*ft->signal.channels);

                    done += ct;
                    wav->blockSamplesRemaining -= (ct/ft->signal.channels);
                    p = wav->samplePtr;
                    top = p+ct;
                    /* Output is already signed */
                    while (p<top)
                        *buf++ = ST_SIGNED_WORD_TO_SAMPLE((*p++),);

                    wav->samplePtr = p;
                }
            }
            /* "done" for ADPCM equals total data processed and not
             * total samples procesed.  The only way to take care of that
             * is to return here and not fall thru.
             */
            wav->numSamples -= (done / ft->signal.channels);
            return done;
            break;

        case ST_ENCODING_GSM:
            /* See reason for cooledit check in comments below */
            if (wav->found_cooledit && len > wav->numSamples*ft->signal.channels) 
                len = (wav->numSamples*ft->signal.channels);

            done = wavgsmread(ft, buf, len);
            if (done == 0 && wav->numSamples != 0)
                st_warn("Premature EOF on .wav input file");
        break;

        default: /* assume PCM or float encoding */
            /* Cooledit seems to put a non-standard IFF LIST at
             * the end of the file.  When this is detected,
             * go ahead and only read in the reported size
             * of data chunk so the LIST data is not treated
             * as audio.  
             * In other cases, go ahead and read unit EOF
             * This allows us to process WAV files that are
             * greater then 2Gig and can't be represented
             * by the 32-bit size field.
             */
            if (wav->found_cooledit && len > wav->numSamples*ft->signal.channels) 
                len = (wav->numSamples*ft->signal.channels);

            done = st_rawread(ft, buf, len);
            /* If software thinks there are more samples but I/O */
            /* says otherwise, let the user know about this.     */
            if (done == 0 && wav->numSamples != 0)
                st_warn("Premature EOF on .wav input file");
        }

        /* Only return buffers that contain a totally playable
         * amount of audio.
         */
        done -= done % ft->signal.channels;
        if (done/ft->signal.channels > wav->numSamples)
            wav->numSamples = 0;
        else
            wav->numSamples -= (done/ft->signal.channels);
        return done;
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
static int st_wavstopread(ft_t ft) 
{
    wav_t       wav = (wav_t) ft->priv;
    int         rc = ST_SUCCESS;

        ft->st_errno = ST_SUCCESS;

    free(wav->packet);
    free(wav->samples);
    free(wav->iCoefs);
    free(ft->comment);
    ft->comment = NULL;

    switch (ft->signal.encoding)
    {
    case ST_ENCODING_GSM:
        wavgsmdestroy(ft);
        break;
    case ST_ENCODING_IMA_ADPCM:
    case ST_ENCODING_ADPCM:
        break;
    default:
        /* Needed for rawread() */
        rc = st_rawstopread(ft);
    }
    return rc;
}

static int st_wavstartwrite(ft_t ft) 
{
    wav_t wav = (wav_t) ft->priv;
    int rc;

    ft->st_errno = ST_SUCCESS;

    if (ft->signal.encoding != ST_ENCODING_ADPCM &&
        ft->signal.encoding != ST_ENCODING_IMA_ADPCM &&
        ft->signal.encoding != ST_ENCODING_GSM)
    {
        rc = st_rawstartwrite(ft);
        if (rc)
            return rc;
    }

    wav->numSamples = 0;
    wav->dataLength = 0;
    if (!ft->seekable)
        st_warn("Length in output .wav header will be wrong since can't seek to fix it");
    rc = wavwritehdr(ft, 0);  /* also calculates various wav->* info */
    if (rc != 0)
        return rc;

    wav->packet = NULL;
    wav->samples = NULL;
    wav->iCoefs = NULL;
    switch (wav->formatTag)
    {
        size_t ch, sbsize;

        case WAVE_FORMAT_IMA_ADPCM:
            initImaTable();
        /* intentional case fallthru! */
        case WAVE_FORMAT_ADPCM:
            /* #channels already range-checked for overflow in wavwritehdr() */
            for (ch=0; ch<ft->signal.channels; ch++)
                wav->state[ch] = 0;
            sbsize = ft->signal.channels * wav->samplesPerBlock;
            wav->packet = (unsigned char *)xmalloc(wav->blockAlign);
            wav->samples = (short *)xmalloc(sbsize*sizeof(short));
            wav->sampleTop = wav->samples + sbsize;
            wav->samplePtr = wav->samples;
            break;

        case WAVE_FORMAT_GSM610:
            wavgsminit(ft);
            break;

        default:
            break;
    }
    return ST_SUCCESS;
}

/* wavwritehdr:  write .wav headers as follows:
 
bytes      variable      description
0  - 3     'RIFF'/'RIFX' Little/Big-endian
4  - 7     wRiffLength   length of file minus the 8 byte riff header
8  - 11    'WAVE'
12 - 15    'fmt '
16 - 19    wFmtSize       length of format chunk minus 8 byte header 
20 - 21    wFormatTag     identifies PCM, ULAW etc
22 - 23    wChannels      
24 - 27    dwSamplesPerSecond  samples per second per channel
28 - 31    dwAvgBytesPerSec    non-trivial for compressed formats
32 - 33    wBlockAlign         basic block size
34 - 35    wBitsPerSample      non-trivial for compressed formats

PCM formats then go straight to the data chunk:
36 - 39    'data'
40 - 43     dwDataLength   length of data chunk minus 8 byte header
44 - (dwDataLength + 43)   the data

non-PCM formats must write an extended format chunk and a fact chunk:

ULAW, ALAW formats:
36 - 37    wExtSize = 0  the length of the format extension
38 - 41    'fact'
42 - 45    dwFactSize = 4  length of the fact chunk minus 8 byte header
46 - 49    dwSamplesWritten   actual number of samples written out
50 - 53    'data'
54 - 57     dwDataLength  length of data chunk minus 8 byte header
58 - (dwDataLength + 57)  the data


GSM6.10  format:
36 - 37    wExtSize = 2 the length in bytes of the format-dependent extension
38 - 39    320           number of samples per  block 
40 - 43    'fact'
44 - 47    dwFactSize = 4  length of the fact chunk minus 8 byte header
48 - 51    dwSamplesWritten   actual number of samples written out
52 - 55    'data'
56 - 59     dwDataLength  length of data chunk minus 8 byte header
60 - (dwDataLength + 59)  the data
(+ a padding byte if dwDataLength is odd) 


note that header contains (up to) 3 separate ways of describing the
length of the file, all derived here from the number of (input)
samples wav->numSamples in a way that is non-trivial for the blocked 
and padded compressed formats:

wRiffLength -      (riff header) the length of the file, minus 8 
dwSamplesWritten - (fact header) the number of samples written (after padding
                   to a complete block eg for GSM)
dwDataLength     - (data chunk header) the number of (valid) data bytes written

*/

static int wavwritehdr(ft_t ft, int second_header) 
{
    wav_t       wav = (wav_t) ft->priv;

    /* variables written to wav file header */
    /* RIFF header */    
    uint32_t wRiffLength ;  /* length of file after 8 byte riff header */
    /* fmt chunk */
    uint16_t wFmtSize = 16;       /* size field of the fmt chunk */
    uint16_t wFormatTag = 0;      /* data format */
    uint16_t wChannels;           /* number of channels */
    uint32_t dwSamplesPerSecond;  /* samples per second per channel*/
    uint32_t dwAvgBytesPerSec=0;  /* estimate of bytes per second needed */
    uint16_t wBlockAlign=0;       /* byte alignment of a basic sample block */
    uint16_t wBitsPerSample=0;    /* bits per sample */
    /* fmt chunk extension (not PCM) */
    uint16_t wExtSize=0;          /* extra bytes in the format extension */
    uint16_t wSamplesPerBlock;    /* samples per channel per block */
    /* wSamplesPerBlock and other things may go into format extension */

    /* fact chunk (not PCM) */
    uint32_t dwFactSize=4;        /* length of the fact chunk */
    uint32_t dwSamplesWritten=0;  /* windows doesnt seem to use this*/

    /* data chunk */
    uint32_t  dwDataLength=0x7ffff000; /* length of sound data in bytes */
    /* end of variables written to header */

    /* internal variables, intermediate values etc */
    int bytespersample; /* (uncompressed) bytes per sample (per channel) */
    long blocksWritten = 0;
    st_bool isExtensible = st_false;    /* WAVE_FORMAT_EXTENSIBLE? */

    dwSamplesPerSecond = ft->signal.rate;
    wChannels = ft->signal.channels;

    /* Check to see if encoding is ADPCM or not.  If ADPCM
     * possibly override the size to be bytes.  It isn't needed
     * by this routine will look nicer (and more correct)
     * on verbose output.
     */
    if ((ft->signal.encoding == ST_ENCODING_ADPCM ||
         ft->signal.encoding == ST_ENCODING_IMA_ADPCM ||
         ft->signal.encoding == ST_ENCODING_GSM) &&
         ft->signal.size != ST_SIZE_BYTE)
    {
        st_report("Overriding output size to bytes for compressed data.");
        ft->signal.size = ST_SIZE_BYTE;
    }

    switch (ft->signal.size)
    {
        case ST_SIZE_BYTE:
            wBitsPerSample = 8;
            if (ft->signal.encoding != ST_ENCODING_UNSIGNED &&
                    ft->signal.encoding != ST_ENCODING_ULAW &&
                    ft->signal.encoding != ST_ENCODING_ALAW &&
                    ft->signal.encoding != ST_ENCODING_GSM &&
                    ft->signal.encoding != ST_ENCODING_ADPCM &&
                    ft->signal.encoding != ST_ENCODING_IMA_ADPCM)
            {
                st_report("Do not support %s with 8-bit data.  Forcing to unsigned",st_encodings_str[(unsigned char)ft->signal.encoding]);
                ft->signal.encoding = ST_ENCODING_UNSIGNED;
            }
            break;
        case ST_SIZE_16BIT:
            wBitsPerSample = 16;
            if (ft->signal.encoding != ST_ENCODING_SIGN2)
            {
                st_report("Do not support %s with 16-bit data.  Forcing to Signed.",st_encodings_str[(unsigned char)ft->signal.encoding]);
                ft->signal.encoding = ST_ENCODING_SIGN2;
            }
            break;
        case ST_SIZE_24BIT:
            wBitsPerSample = 24;
            if (ft->signal.encoding != ST_ENCODING_SIGN2)
            {
                st_report("Do not support %s with 24-bit data.  Forcing to Signed.",st_encodings_str[(unsigned char)ft->signal.encoding]);
                ft->signal.encoding = ST_ENCODING_SIGN2;
            }
            break;

        case ST_SIZE_32BIT:
            wBitsPerSample = 32;
            if (ft->signal.encoding != ST_ENCODING_SIGN2 &&
                ft->signal.encoding != ST_ENCODING_FLOAT)
            {
                st_report("Do not support %s with 32-bit data.  Forcing to Signed.",st_encodings_str[(unsigned char)ft->signal.encoding]);
                ft->signal.encoding = ST_ENCODING_SIGN2;
            }

            break;
        default:
            st_report("Do not support %s in WAV files.  Forcing to Signed Words.",st_sizes_str[(unsigned char)ft->signal.size]);
            ft->signal.encoding = ST_ENCODING_SIGN2;
            ft->signal.size = ST_SIZE_16BIT;
            wBitsPerSample = 16;
            break;
    }

    wSamplesPerBlock = 1;       /* common default for PCM data */

    switch (ft->signal.encoding)
    {
        case ST_ENCODING_UNSIGNED:
        case ST_ENCODING_SIGN2:
            wFormatTag = WAVE_FORMAT_PCM;
            bytespersample = (wBitsPerSample + 7)/8;
            wBlockAlign = wChannels * bytespersample;
            break;
        case ST_ENCODING_FLOAT:
            wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
            bytespersample = (wBitsPerSample + 7)/8;
            wBlockAlign = wChannels * bytespersample;
            break;
        case ST_ENCODING_ALAW:
            wFormatTag = WAVE_FORMAT_ALAW;
            wBlockAlign = wChannels;
            break;
        case ST_ENCODING_ULAW:
            wFormatTag = WAVE_FORMAT_MULAW;
            wBlockAlign = wChannels;
            break;
        case ST_ENCODING_IMA_ADPCM:
            if (wChannels>16)
            {
                st_fail_errno(ft,ST_EOF,"Channels(%d) must be <= 16",wChannels);
                return ST_EOF;
            }
            wFormatTag = WAVE_FORMAT_IMA_ADPCM;
            wBlockAlign = wChannels * 256; /* reasonable default */
            wBitsPerSample = 4;
            wExtSize = 2;
            wSamplesPerBlock = ImaSamplesIn(0, wChannels, wBlockAlign, 0);
            break;
        case ST_ENCODING_ADPCM:
            if (wChannels>16)
            {
                st_fail_errno(ft,ST_EOF,"Channels(%d) must be <= 16",wChannels);
                return ST_EOF;
            }
            wFormatTag = WAVE_FORMAT_ADPCM;
            wBlockAlign = wChannels * 128; /* reasonable default */
            wBitsPerSample = 4;
            wExtSize = 4+4*7;      /* Ext fmt data length */
            wSamplesPerBlock = AdpcmSamplesIn(0, wChannels, wBlockAlign, 0);
            break;
        case ST_ENCODING_GSM:
            if (wChannels!=1)
            {
                st_report("Overriding GSM audio from %d channel to 1",wChannels);
                wChannels = ft->signal.channels = 1;
            }
            wFormatTag = WAVE_FORMAT_GSM610;
            /* dwAvgBytesPerSec = 1625*(dwSamplesPerSecond/8000.)+0.5; */
            wBlockAlign=65;
            wBitsPerSample=0;  /* not representable as int   */
            wExtSize=2;        /* length of format extension */
            wSamplesPerBlock = 320;
            break;
        default:
                break;
    }
    wav->formatTag = wFormatTag;
    wav->blockAlign = wBlockAlign;
    wav->samplesPerBlock = wSamplesPerBlock;

    if (!second_header) {       /* adjust for blockAlign */
        blocksWritten = dwDataLength/wBlockAlign;
        dwDataLength = blocksWritten * wBlockAlign;
        dwSamplesWritten = blocksWritten * wSamplesPerBlock;
    } else {    /* fixup with real length */
        dwSamplesWritten = wav->numSamples;
        switch(wFormatTag)
        {
            case WAVE_FORMAT_ADPCM:
            case WAVE_FORMAT_IMA_ADPCM:
                dwDataLength = wav->dataLength;
                break;
            case WAVE_FORMAT_GSM610:
                /* intentional case fallthrough! */
            default:
                blocksWritten = (dwSamplesWritten+wSamplesPerBlock-1)/wSamplesPerBlock;
                dwDataLength = blocksWritten * wBlockAlign;
        }
    }

    if (wFormatTag == WAVE_FORMAT_GSM610)
        dwDataLength = (dwDataLength+1) & ~1; /*round up to even */

    if ((wFormatTag == WAVE_FORMAT_PCM && wBitsPerSample > 16) || wChannels > 2)
    {
      isExtensible = st_true;
      wFmtSize += 2 + 22;
    }
    else if (wFormatTag != WAVE_FORMAT_PCM)
        wFmtSize += 2+wExtSize; /* plus ExtData */

    wRiffLength = 4 + (8+wFmtSize) + (8+dwDataLength); 
    if (wFormatTag != WAVE_FORMAT_PCM) /* PCM omits the "fact" chunk */
        wRiffLength += (8+dwFactSize);

    /* dwAvgBytesPerSec <-- this is BEFORE compression, isn't it? guess not. */
    dwAvgBytesPerSec = (double)wBlockAlign*ft->signal.rate / (double)wSamplesPerBlock + 0.5;

    /* figured out header info, so write it */


    /* If user specified opposite swap than we think, assume they are
     * asking to write a RIFX file.
     */
    if (ft->signal.reverse_bytes != ST_IS_BIGENDIAN)
    {
        if (!second_header)
            st_report("Requested to swap bytes so writing RIFX header");
        st_writes(ft, "RIFX");
    }
    else
        st_writes(ft, "RIFF");
    st_writedw(ft, wRiffLength);
    st_writes(ft, "WAVE");
    st_writes(ft, "fmt ");
    st_writedw(ft, wFmtSize);
    st_writew(ft, isExtensible? WAVE_FORMAT_EXTENSIBLE : wFormatTag);
    st_writew(ft, wChannels);
    st_writedw(ft, dwSamplesPerSecond);
    st_writedw(ft, dwAvgBytesPerSec);
    st_writew(ft, wBlockAlign);
    st_writew(ft, wBitsPerSample); /* end info common to all fmts */

    if (isExtensible)
    {
      size_t i;
      static const char guid[14] = "\x00\x00\x00\x00\x10\x00\x80\x00\x00\xAA\x00\x38\x9B\x71";
      st_writew(ft, 22);
      st_writew(ft, wBitsPerSample); /* No padding in container */
      st_writedw(ft, 0);             /* Speaker mapping not specified */
      st_writew(ft, wFormatTag);
      for (i = 0; i < array_length(guid); ++i)
      {
        st_writeb(ft, guid[i]);
      }
    }
    else 
    /* if not PCM, we need to write out wExtSize even if wExtSize=0 */
    if (wFormatTag != WAVE_FORMAT_PCM)
        st_writew(ft,wExtSize);

    switch (wFormatTag)
    {
        int i;
        case WAVE_FORMAT_IMA_ADPCM:
        st_writew(ft, wSamplesPerBlock);
        break;
        case WAVE_FORMAT_ADPCM:
        st_writew(ft, wSamplesPerBlock);
        st_writew(ft, 7); /* nCoefs */
        for (i=0; i<7; i++) {
            st_writew(ft, iCoef[i][0]);
            st_writew(ft, iCoef[i][1]);
        }
        break;
        case WAVE_FORMAT_GSM610:
        st_writew(ft, wSamplesPerBlock);
        break;
        default:
        break;
    }

    /* if not PCM, write the 'fact' chunk */
    if (isExtensible || wFormatTag != WAVE_FORMAT_PCM){
        st_writes(ft, "fact");
        st_writedw(ft,dwFactSize); 
        st_writedw(ft,dwSamplesWritten);
    }

    st_writes(ft, "data");
    st_writedw(ft, dwDataLength);               /* data chunk size */

    if (!second_header) {
        st_debug("Writing Wave file: %s format, %d channel%s, %d samp/sec",
                wav_format_str(wFormatTag), wChannels,
                wChannels == 1 ? "" : "s", dwSamplesPerSecond);
        st_debug("        %d byte/sec, %d block align, %d bits/samp",
                dwAvgBytesPerSec, wBlockAlign, wBitsPerSample);
    } else {
        st_debug("Finished writing Wave file, %u data bytes %u samples",
                dwDataLength,wav->numSamples);
        if (wFormatTag == WAVE_FORMAT_GSM610){
            st_debug("GSM6.10 format: %u blocks %u padded samples %u padded data bytes",
                    blocksWritten, dwSamplesWritten, dwDataLength);
            if (wav->gsmbytecount != dwDataLength)
                st_warn("help ! internal inconsistency - data_written %u gsmbytecount %u",
                        dwDataLength, wav->gsmbytecount);

        }
    }
    return ST_SUCCESS;
}

static st_size_t st_wavwrite(ft_t ft, const st_sample_t *buf, st_size_t len) 
{
        wav_t   wav = (wav_t) ft->priv;
        st_ssize_t total_len = len;

        ft->st_errno = ST_SUCCESS;

        switch (wav->formatTag)
        {
        case WAVE_FORMAT_IMA_ADPCM:
        case WAVE_FORMAT_ADPCM:
            while (len>0) {
                short *p = wav->samplePtr;
                short *top = wav->sampleTop;

                if (top>p+len) top = p+len;
                len -= top-p; /* update residual len */
                while (p < top)
                   *p++ = (*buf++) >> 16;

                wav->samplePtr = p;
                if (p == wav->sampleTop)
                    xxxAdpcmWriteBlock(ft);

            }
            return total_len - len;
            break;

        case WAVE_FORMAT_GSM610:
            len = wavgsmwrite(ft, buf, len);
            wav->numSamples += (len/ft->signal.channels);
            return len;
            break;

        default:
            len = st_rawwrite(ft, buf, len);
            wav->numSamples += (len/ft->signal.channels);
            return len;
        }
}

static int st_wavstopwrite(ft_t ft) 
{
        wav_t   wav = (wav_t) ft->priv;

        ft->st_errno = ST_SUCCESS;


        /* Call this to flush out any remaining data. */
        switch (wav->formatTag)
        {
        case WAVE_FORMAT_IMA_ADPCM:
        case WAVE_FORMAT_ADPCM:
            xxxAdpcmWriteBlock(ft);
            break;
        case WAVE_FORMAT_GSM610:
            wavgsmstopwrite(ft);
            break;
        }
        free(wav->packet);
        free(wav->samples);
        free(wav->iCoefs);

        /* Flush any remaining data */
        if (wav->formatTag != WAVE_FORMAT_IMA_ADPCM &&
            wav->formatTag != WAVE_FORMAT_ADPCM &&
            wav->formatTag != WAVE_FORMAT_GSM610)
            st_rawstopwrite(ft);

        /* All samples are already written out. */
        /* If file header needs fixing up, for example it needs the */
        /* the number of samples in a field, seek back and write them here. */
        if (!ft->seekable)
                return ST_EOF;

        if (st_seeki(ft, 0, SEEK_SET) != 0)
        {
                st_fail_errno(ft,ST_EOF,"Can't rewind output file to rewrite .wav header.");
                return ST_EOF;
        }

        return (wavwritehdr(ft, 1));
}

/*
 * Return a string corresponding to the wave format type.
 */
static char *wav_format_str(unsigned wFormatTag) 
{
        switch (wFormatTag)
        {
                case WAVE_FORMAT_UNKNOWN:
                        return "Microsoft Official Unknown";
                case WAVE_FORMAT_PCM:
                        return "Microsoft PCM";
                case WAVE_FORMAT_ADPCM:
                        return "Microsoft ADPCM";
                case WAVE_FORMAT_IEEE_FLOAT:
                       return "IEEE Float";
                case WAVE_FORMAT_ALAW:
                        return "Microsoft A-law";
                case WAVE_FORMAT_MULAW:
                        return "Microsoft U-law";
                case WAVE_FORMAT_OKI_ADPCM:
                        return "OKI ADPCM format.";
                case WAVE_FORMAT_IMA_ADPCM:
                        return "IMA ADPCM";
                case WAVE_FORMAT_DIGISTD:
                        return "Digistd format.";
                case WAVE_FORMAT_DIGIFIX:
                        return "Digifix format.";
                case WAVE_FORMAT_DOLBY_AC2:
                        return "Dolby AC2";
                case WAVE_FORMAT_GSM610:
                        return "GSM 6.10";
                case WAVE_FORMAT_ROCKWELL_ADPCM:
                        return "Rockwell ADPCM";
                case WAVE_FORMAT_ROCKWELL_DIGITALK:
                        return "Rockwell DIGITALK";
                case WAVE_FORMAT_G721_ADPCM:
                        return "G.721 ADPCM";
                case WAVE_FORMAT_G728_CELP:
                        return "G.728 CELP";
                case WAVE_FORMAT_MPEG:
                        return "MPEG";
                case WAVE_FORMAT_MPEGLAYER3:
                        return "MPEG Layer 3";
                case WAVE_FORMAT_G726_ADPCM:
                        return "G.726 ADPCM";
                case WAVE_FORMAT_G722_ADPCM:
                        return "G.722 ADPCM";
                default:
                        return "Unknown";
        }
}

static int st_wavseek(ft_t ft, st_size_t offset) 
{
    wav_t   wav = (wav_t) ft->priv;
    int new_offset, channel_block, alignment;

    switch (wav->formatTag)
    {
        case WAVE_FORMAT_IMA_ADPCM:
        case WAVE_FORMAT_ADPCM:
            st_fail_errno(ft,ST_ENOTSUP,"ADPCM not supported");
            break;

        case WAVE_FORMAT_GSM610:
            {   
                st_size_t gsmoff;

                /* rounding bytes to blockAlign so that we
                 * don't have to decode partial block. */
                gsmoff = offset * wav->blockAlign / wav->samplesPerBlock + 
                         wav->blockAlign * ft->signal.channels / 2;
                gsmoff -= gsmoff % (wav->blockAlign * ft->signal.channels);

                ft->st_errno = st_seeki(ft, gsmoff + wav->dataStart, SEEK_SET);
                if (ft->st_errno != ST_SUCCESS)
                    return ST_EOF;

                /* offset is in samples */
                new_offset = offset;
                alignment = offset % wav->samplesPerBlock;                
                if (alignment != 0)
                    new_offset += (wav->samplesPerBlock - alignment);
                wav->numSamples = ft->length - (new_offset / ft->signal.channels);
            }
            break;

        default:
            new_offset = offset * ft->signal.size;
            /* Make sure request aligns to a channel block (ie left+right) */
            channel_block = ft->signal.channels * ft->signal.size;
            alignment = new_offset % channel_block;
            /* Most common mistaken is to compute something like
             * "skip everthing upto and including this sample" so
             * advance to next sample block in this case.
             */
            if (alignment != 0)
                new_offset += (channel_block - alignment);
            new_offset += wav->dataStart;

            ft->st_errno = st_seeki(ft, new_offset, SEEK_SET);

            if( ft->st_errno == ST_SUCCESS )
                wav->numSamples = (ft->length / ft->signal.channels) -
                                  (new_offset / ft->signal.size / ft->signal.channels);
    }

    return(ft->st_errno);
}

/* Microsoft RIFF */
static const char *wavnames[] = {
  "wav",
  NULL
};

static st_format_t st_wav_format = {
  wavnames,
  NULL,
  ST_FILE_SEEK | ST_FILE_LIT_END,
  st_wavstartread,
  st_wavread,
  st_wavstopread,
  st_wavstartwrite,
  st_wavwrite,
  st_wavstopwrite,
  st_wavseek
};

const st_format_t *st_wav_format_fn()
{
    return &st_wav_format;
}
