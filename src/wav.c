/* libSoX microsoft's WAVE sound format handler
 *
 * Copyright 1998-2006 Chris Bagwell and SoX Contributors
 * Copyright 1997 Graeme W. Gill, 93/5/17
 * Copyright 1992 Rick Richardson
 * Copyright 1991 Lance Norskog And Sundry Contributors
 *
 * Info for format tags can be found at:
 *   http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
 *
 */

#include "sox_i.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "ima_rw.h"
#include "adpcm.h"
#ifdef EXTERNAL_GSM

#ifdef HAVE_GSM_GSM_H
#include <gsm/gsm.h>
#else
#include <gsm.h>
#endif

#else
#include "../libgsm/gsm.h"
#endif

/* Magic length writen when its not possible to write valid lengths.
 * This can be either because of non-seekable output or because
 * the length can not be represented by the 32-bits used in WAV files.
 * When magic length is detected on inputs, disable any length
 * logic.
 */
#define MS_UNSPEC 0x7ffff000

#define WAVE_FORMAT_UNKNOWN             (0x0000U)
#define WAVE_FORMAT_PCM                 (0x0001U)
#define WAVE_FORMAT_ADPCM               (0x0002U)
#define WAVE_FORMAT_IEEE_FLOAT          (0x0003U)
#define WAVE_FORMAT_ALAW                (0x0006U)
#define WAVE_FORMAT_MULAW               (0x0007U)
#define WAVE_FORMAT_OKI_ADPCM           (0x0010U)
#define WAVE_FORMAT_IMA_ADPCM           (0x0011U)
#define WAVE_FORMAT_DIGISTD             (0x0015U)
#define WAVE_FORMAT_DIGIFIX             (0x0016U)
#define WAVE_FORMAT_DOLBY_AC2           (0x0030U)
#define WAVE_FORMAT_GSM610              (0x0031U)
#define WAVE_FORMAT_ROCKWELL_ADPCM      (0x003bU)
#define WAVE_FORMAT_ROCKWELL_DIGITALK   (0x003cU)
#define WAVE_FORMAT_G721_ADPCM          (0x0040U)
#define WAVE_FORMAT_G728_CELP           (0x0041U)
#define WAVE_FORMAT_MPEG                (0x0050U)
#define WAVE_FORMAT_MPEGLAYER3          (0x0055U)
#define WAVE_FORMAT_G726_ADPCM          (0x0064U)
#define WAVE_FORMAT_G722_ADPCM          (0x0065U)
#define WAVE_FORMAT_EXTENSIBLE          (0xfffeU)

/* To allow padding to samplesPerBlock. Works, but currently never true. */
static const size_t pad_nsamps = sox_false;

/* Private data for .wav file */
typedef struct {
    /* samples/channel reading: starts at total count and decremented  */
    /* writing: starts at 0 and counts samples written */
    uint64_t  numSamples;    
    size_t    dataLength;     /* needed for ADPCM writing */
    unsigned short formatTag;       /* What type of encoding file is using */
    unsigned short samplesPerBlock;
    unsigned short blockAlign;
    size_t dataStart;           /* need to for seeking */
    char           * comment;
    int ignoreSize;                 /* ignoreSize allows us to process 32-bit WAV files that are
                                     * greater then 2 Gb and can't be represented by the
                                     * 32-bit size field. */
  /* FIXME: Have some front-end code which sets this flag. */

    /* following used by *ADPCM wav files */
    unsigned short nCoefs;          /* ADPCM: number of coef sets */
    short         *lsx_ms_adpcm_i_coefs;          /* ADPCM: coef sets           */
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
    size_t      gsmbytecount;    /* counts bytes written to data block */
} priv_t;

static char *wav_format_str(unsigned wFormatTag);

static int wavwritehdr(sox_format_t *, int);


/****************************************************************************/
/* IMA ADPCM Support Functions Section                                      */
/****************************************************************************/

/*
 *
 * ImaAdpcmReadBlock - Grab and decode complete block of samples
 *
 */
static unsigned short  ImaAdpcmReadBlock(sox_format_t * ft)
{
    priv_t *       wav = (priv_t *) ft->priv;
    size_t bytesRead;
    int samplesThisBlock;

    /* Pull in the packet and check the header */
    bytesRead = lsx_readbuf(ft, wav->packet, (size_t)wav->blockAlign);
    samplesThisBlock = wav->samplesPerBlock;
    if (bytesRead < wav->blockAlign)
    {
        /* If it looks like a valid header is around then try and */
        /* work with partial blocks.  Specs say it should be null */
        /* padded but I guess this is better than trailing quiet. */
        samplesThisBlock = lsx_ima_samples_in((size_t)0, (size_t)ft->signal.channels, bytesRead, (size_t) 0);
        if (samplesThisBlock == 0)
        {
            lsx_warn("Premature EOF on .wav input file");
            return 0;
        }
    }

    wav->samplePtr = wav->samples;

    /* For a full block, the following should be true: */
    /* wav->samplesPerBlock = blockAlign - 8byte header + 1 sample in header */
    lsx_ima_block_expand_i(ft->signal.channels, wav->packet, wav->samples, samplesThisBlock);
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
static unsigned short  AdpcmReadBlock(sox_format_t * ft)
{
    priv_t *       wav = (priv_t *) ft->priv;
    size_t bytesRead;
    int samplesThisBlock;
    const char *errmsg;

    /* Pull in the packet and check the header */
    bytesRead = lsx_readbuf(ft, wav->packet, (size_t) wav->blockAlign);
    samplesThisBlock = wav->samplesPerBlock;
    if (bytesRead < wav->blockAlign)
    {
        /* If it looks like a valid header is around then try and */
        /* work with partial blocks.  Specs say it should be null */
        /* padded but I guess this is better than trailing quiet. */
        samplesThisBlock = lsx_ms_adpcm_samples_in((size_t)0, (size_t)ft->signal.channels, bytesRead, (size_t)0);
        if (samplesThisBlock == 0)
        {
            lsx_warn("Premature EOF on .wav input file");
            return 0;
        }
    }

    errmsg = lsx_ms_adpcm_block_expand_i(ft->signal.channels, wav->nCoefs, wav->lsx_ms_adpcm_i_coefs, wav->packet, wav->samples, samplesThisBlock);

    if (errmsg)
        lsx_warn("%s", errmsg);

    return samplesThisBlock;
}

/****************************************************************************/
/* Common ADPCM Write Function                                              */
/****************************************************************************/

static int xxxAdpcmWriteBlock(sox_format_t * ft)
{
    priv_t * wav = (priv_t *) ft->priv;
    size_t chans, ct;
    short *p;

    chans = ft->signal.channels;
    p = wav->samplePtr;
    ct = p - wav->samples;
    if (ct>=chans) {
        /* zero-fill samples if needed to complete block */
        for (p = wav->samplePtr; p < wav->sampleTop; p++) *p=0;
        /* compress the samples to wav->packet */
        if (wav->formatTag == WAVE_FORMAT_ADPCM) {
            lsx_ms_adpcm_block_mash_i((unsigned) chans, wav->samples, wav->samplesPerBlock, wav->state, wav->packet, wav->blockAlign);
        }else{ /* WAVE_FORMAT_IMA_ADPCM */
            lsx_ima_block_mash_i((unsigned) chans, wav->samples, wav->samplesPerBlock, wav->state, wav->packet, 9);
        }
        /* write the compressed packet */
        if (lsx_writebuf(ft, wav->packet, (size_t) wav->blockAlign) != wav->blockAlign)
        {
            lsx_fail_errno(ft,SOX_EOF,"write error");
            return (SOX_EOF);
        }
        /* update lengths and samplePtr */
        wav->dataLength += wav->blockAlign;
        if (pad_nsamps)
          wav->numSamples += wav->samplesPerBlock;
        else
          wav->numSamples += ct/chans;
        wav->samplePtr = wav->samples;
    }
    return (SOX_SUCCESS);
}

/****************************************************************************/
/* WAV GSM6.10 support functions                                            */
/****************************************************************************/
/* create the gsm object, malloc buffer for 160*2 samples */
static int wavgsminit(sox_format_t * ft)
{
    int valueP=1;
    priv_t *       wav = (priv_t *) ft->priv;
    wav->gsmbytecount=0;
    wav->gsmhandle=gsm_create();
    if (!wav->gsmhandle)
    {
        lsx_fail_errno(ft,SOX_EOF,"cannot create GSM object");
        return (SOX_EOF);
    }

    if(gsm_option(wav->gsmhandle,GSM_OPT_WAV49,&valueP) == -1){
        lsx_fail_errno(ft,SOX_EOF,"error setting gsm_option for WAV49 format. Recompile gsm library with -DWAV49 option and relink sox");
        return (SOX_EOF);
    }

    wav->gsmsample=lsx_malloc(sizeof(gsm_signal)*160*2);
    wav->gsmindex=0;
    return (SOX_SUCCESS);
}

/*destroy the gsm object and free the buffer */
static void wavgsmdestroy(sox_format_t * ft)
{
    priv_t *       wav = (priv_t *) ft->priv;
    gsm_destroy(wav->gsmhandle);
    free(wav->gsmsample);
}

static size_t wavgsmread(sox_format_t * ft, sox_sample_t *buf, size_t len)
{
    priv_t *       wav = (priv_t *) ft->priv;
    size_t done=0;
    int bytes;
    gsm_byte    frame[65];

    ft->sox_errno = SOX_SUCCESS;

  /* copy out any samples left from the last call */
    while(wav->gsmindex && (wav->gsmindex<160*2) && (done < len))
        buf[done++]=SOX_SIGNED_16BIT_TO_SAMPLE(wav->gsmsample[wav->gsmindex++],);

  /* read and decode loop, possibly leaving some samples in wav->gsmsample */
    while (done < len) {
        wav->gsmindex=0;
        bytes = lsx_readbuf(ft, frame, (size_t)65);
        if (bytes <=0)
            return done;
        if (bytes<65) {
            lsx_warn("invalid wav gsm frame size: %d bytes",bytes);
            return done;
        }
        /* decode the long 33 byte half */
        if(gsm_decode(wav->gsmhandle,frame, wav->gsmsample)<0)
        {
            lsx_fail_errno(ft,SOX_EOF,"error during gsm decode");
            return 0;
        }
        /* decode the short 32 byte half */
        if(gsm_decode(wav->gsmhandle,frame+33, wav->gsmsample+160)<0)
        {
            lsx_fail_errno(ft,SOX_EOF,"error during gsm decode");
            return 0;
        }

        while ((wav->gsmindex <160*2) && (done < len)){
            buf[done++]=SOX_SIGNED_16BIT_TO_SAMPLE(wav->gsmsample[(wav->gsmindex)++],);
        }
    }

    return done;
}

static int wavgsmflush(sox_format_t * ft)
{
    gsm_byte    frame[65];
    priv_t *       wav = (priv_t *) ft->priv;

    /* zero fill as needed */
    while(wav->gsmindex<160*2)
        wav->gsmsample[wav->gsmindex++]=0;

    /*encode the even half short (32 byte) frame */
    gsm_encode(wav->gsmhandle, wav->gsmsample, frame);
    /*encode the odd half long (33 byte) frame */
    gsm_encode(wav->gsmhandle, wav->gsmsample+160, frame+32);
    if (lsx_writebuf(ft, frame, (size_t) 65) != 65)
    {
        lsx_fail_errno(ft,SOX_EOF,"write error");
        return (SOX_EOF);
    }
    wav->gsmbytecount += 65;

    wav->gsmindex = 0;
    return (SOX_SUCCESS);
}

static size_t wavgsmwrite(sox_format_t * ft, const sox_sample_t *buf, size_t len)
{
    priv_t * wav = (priv_t *) ft->priv;
    size_t done = 0;
    int rc;

    ft->sox_errno = SOX_SUCCESS;

    while (done < len) {
        SOX_SAMPLE_LOCALS;
        while ((wav->gsmindex < 160*2) && (done < len))
            wav->gsmsample[(wav->gsmindex)++] =
                SOX_SAMPLE_TO_SIGNED_16BIT(buf[done++], ft->clips);

        if (wav->gsmindex < 160*2)
            break;

        rc = wavgsmflush(ft);
        if (rc)
            return 0;
    }
    return done;

}

static void wavgsmstopwrite(sox_format_t * ft)
{
    priv_t *       wav = (priv_t *) ft->priv;

    ft->sox_errno = SOX_SUCCESS;

    if (wav->gsmindex)
        wavgsmflush(ft);

    /* Add a pad byte if amount of written bytes is not even. */
    if (wav->gsmbytecount && wav->gsmbytecount % 2){
        if(lsx_writeb(ft, 0))
            lsx_fail_errno(ft,SOX_EOF,"write error");
        else
            wav->gsmbytecount += 1;
    }

    wavgsmdestroy(ft);
}

/****************************************************************************/
/* General Sox WAV file code                                                */
/****************************************************************************/
static int findChunk(sox_format_t * ft, const char *Label, uint32_t *len)
{
    char magic[5];
    for (;;)
    {
        if (lsx_reads(ft, magic, (size_t)4) == SOX_EOF)
        {
            lsx_fail_errno(ft, SOX_EHDR, "WAVE file has missing %s chunk",
                          Label);
            return SOX_EOF;
        }
        lsx_debug("WAV Chunk %s", magic);
        if (lsx_readdw(ft, len) == SOX_EOF)
        {
            lsx_fail_errno(ft, SOX_EHDR, "WAVE file %s chunk is too short",
                          magic);
            return SOX_EOF;
        }

        if (strncmp(Label, magic, (size_t)4) == 0)
            break; /* Found the given chunk */

	/* Chunks are required to be word aligned. */
	if ((*len) % 2) (*len)++;

        /* skip to next chunk */
        if (*len > 0 && lsx_seeki(ft, (off_t)(*len), SEEK_CUR) != SOX_SUCCESS)
        {
            lsx_fail_errno(ft,SOX_EHDR,
                          "WAV chunk appears to have invalid size %d.", *len);
            return SOX_EOF;
        }
    }
    return SOX_SUCCESS;
}


static int wavfail(sox_format_t * ft, const char *format)
{
    lsx_fail_errno(ft, SOX_EHDR, "WAV file encoding `%s' is not supported", format);
    return SOX_EOF;
}

/*
 * Do anything required before you start reading samples.
 * Read file header.
 *      Find out sampling rate,
 *      size and encoding of samples,
 *      mono/stereo/quad.
 */
static int startread(sox_format_t * ft)
{
    priv_t *       wav = (priv_t *) ft->priv;
    char        magic[5];
    uint32_t    len;

    /* wave file characteristics */
    uint32_t      dwRiffLength;
    unsigned short wChannels;       /* number of channels */
    uint32_t      dwSamplesPerSecond; /* samples per second per channel */
    uint32_t      dwAvgBytesPerSec;/* estimate of bytes per second needed */
    uint16_t wBitsPerSample;  /* bits per sample */
    uint32_t wFmtSize;
    uint16_t wExtSize = 0;    /* extended field for non-PCM */

    uint32_t      dwDataLength;    /* length of sound data in bytes */
    size_t    bytesPerBlock = 0;
    int    bytespersample;          /* bytes per sample (per channel */
    char text[256];
    uint32_t      dwLoopPos;

    ft->sox_errno = SOX_SUCCESS;
    wav->ignoreSize = ft->signal.length == SOX_IGNORE_LENGTH;

    if (lsx_reads(ft, magic, (size_t)4) == SOX_EOF || (strncmp("RIFF", magic, (size_t)4) != 0 &&
                                             strncmp("RIFX", magic, (size_t)4) != 0))
    {
        lsx_fail_errno(ft,SOX_EHDR,"WAVE: RIFF header not found");
        return SOX_EOF;
    }

    /* RIFX is a Big-endian RIFF */
    if (strncmp("RIFX", magic, (size_t)4) == 0)
    {
        lsx_debug("Found RIFX header");
        ft->encoding.reverse_bytes = MACHINE_IS_LITTLEENDIAN;
    }
    else ft->encoding.reverse_bytes = MACHINE_IS_BIGENDIAN;

    lsx_readdw(ft, &dwRiffLength);

    if (lsx_reads(ft, magic, (size_t)4) == SOX_EOF || strncmp("WAVE", magic, (size_t)4))
    {
        lsx_fail_errno(ft,SOX_EHDR,"WAVE header not found");
        return SOX_EOF;
    }

    /* Now look for the format chunk */
    if (findChunk(ft, "fmt ", &len) == SOX_EOF)
    {
        lsx_fail_errno(ft,SOX_EHDR,"WAVE chunk fmt not found");
        return SOX_EOF;
    }
    wFmtSize = len;

    if (wFmtSize < 16)
    {
        lsx_fail_errno(ft,SOX_EHDR,"WAVE file fmt chunk is too short");
        return SOX_EOF;
    }

    lsx_readw(ft, &(wav->formatTag));
    lsx_readw(ft, &wChannels);
    lsx_readdw(ft, &dwSamplesPerSecond);
    lsx_readdw(ft, &dwAvgBytesPerSec);   /* Average bytes/second */
    lsx_readw(ft, &(wav->blockAlign));   /* Block align */
    lsx_readw(ft, &wBitsPerSample);      /* bits per sample per channel */
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
        lsx_fail_errno(ft,SOX_EHDR,"WAVE file fmt chunk is too short");
        return SOX_EOF;
      }
      lsx_readw(ft, &extensionSize);
      len -= 2;
      if (extensionSize < 22)
      {
        lsx_fail_errno(ft,SOX_EHDR,"WAVE file fmt chunk is too short");
        return SOX_EOF;
      }
      lsx_readw(ft, &numberOfValidBits);
      lsx_readdw(ft, &speakerPositionMask);
      lsx_readw(ft, &subFormatTag);
      for (i = 0; i < 14; ++i) lsx_readb(ft, &dummyByte);
      len -= 22;
      if (numberOfValidBits != wBitsPerSample)
      {
        lsx_fail_errno(ft,SOX_EHDR,"WAVE file fmt with padded samples is not supported yet");
        return SOX_EOF;
      }
      wav->formatTag = subFormatTag;
      lsx_report("EXTENSIBLE");
    }

    switch (wav->formatTag)
    {
    case WAVE_FORMAT_UNKNOWN:
        lsx_fail_errno(ft,SOX_EHDR,"WAVE file is in unsupported Microsoft Official Unknown format.");
        return SOX_EOF;

    case WAVE_FORMAT_PCM:
        /* Default (-1) depends on sample size.  Set that later on. */
        if (ft->encoding.encoding != SOX_ENCODING_UNKNOWN && ft->encoding.encoding != SOX_ENCODING_UNSIGNED &&
            ft->encoding.encoding != SOX_ENCODING_SIGN2)
            lsx_report("User options overriding encoding read in .wav header");
        break;

    case WAVE_FORMAT_IMA_ADPCM:
        if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN || ft->encoding.encoding == SOX_ENCODING_IMA_ADPCM)
            ft->encoding.encoding = SOX_ENCODING_IMA_ADPCM;
        else
            lsx_report("User options overriding encoding read in .wav header");
        break;

    case WAVE_FORMAT_ADPCM:
        if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN || ft->encoding.encoding == SOX_ENCODING_MS_ADPCM)
            ft->encoding.encoding = SOX_ENCODING_MS_ADPCM;
        else
            lsx_report("User options overriding encoding read in .wav header");
        break;

    case WAVE_FORMAT_IEEE_FLOAT:
        if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN || ft->encoding.encoding == SOX_ENCODING_FLOAT)
            ft->encoding.encoding = SOX_ENCODING_FLOAT;
        else
            lsx_report("User options overriding encoding read in .wav header");
        break;

    case WAVE_FORMAT_ALAW:
        if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN || ft->encoding.encoding == SOX_ENCODING_ALAW)
            ft->encoding.encoding = SOX_ENCODING_ALAW;
        else
            lsx_report("User options overriding encoding read in .wav header");
        break;

    case WAVE_FORMAT_MULAW:
        if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN || ft->encoding.encoding == SOX_ENCODING_ULAW)
            ft->encoding.encoding = SOX_ENCODING_ULAW;
        else
            lsx_report("User options overriding encoding read in .wav header");
        break;

    case WAVE_FORMAT_OKI_ADPCM:
        return wavfail(ft, "OKI ADPCM");
    case WAVE_FORMAT_DIGISTD:
        return wavfail(ft, "Digistd");
    case WAVE_FORMAT_DIGIFIX:
        return wavfail(ft, "Digifix");
    case WAVE_FORMAT_DOLBY_AC2:
        return wavfail(ft, "Dolby AC2");
    case WAVE_FORMAT_GSM610:
        if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN || ft->encoding.encoding == SOX_ENCODING_GSM )
            ft->encoding.encoding = SOX_ENCODING_GSM;
        else
            lsx_report("User options overriding encoding read in .wav header");
        break;
    case WAVE_FORMAT_ROCKWELL_ADPCM:
        return wavfail(ft, "Rockwell ADPCM");
    case WAVE_FORMAT_ROCKWELL_DIGITALK:
        return wavfail(ft, "Rockwell DIGITALK");
    case WAVE_FORMAT_G721_ADPCM:
        return wavfail(ft, "G.721 ADPCM");
    case WAVE_FORMAT_G728_CELP:
        return wavfail(ft, "G.728 CELP");
    case WAVE_FORMAT_MPEG:
        return wavfail(ft, "MPEG");
    case WAVE_FORMAT_MPEGLAYER3:
        return wavfail(ft, "MP3");
    case WAVE_FORMAT_G726_ADPCM:
        return wavfail(ft, "G.726 ADPCM");
    case WAVE_FORMAT_G722_ADPCM:
        return wavfail(ft, "G.722 ADPCM");
    default:
        lsx_fail_errno(ft, SOX_EHDR, "Unknown WAV file encoding (type %x)", wav->formatTag);
        return SOX_EOF;
    }

    /* User options take precedence */
    if (ft->signal.channels == 0 || ft->signal.channels == wChannels)
        ft->signal.channels = wChannels;
    else
        lsx_report("User options overriding channels read in .wav header");

    if (ft->signal.rate == 0 || ft->signal.rate == dwSamplesPerSecond)
        ft->signal.rate = dwSamplesPerSecond;
    else
        lsx_report("User options overriding rate read in .wav header");


    wav->lsx_ms_adpcm_i_coefs = NULL;
    wav->packet = NULL;
    wav->samples = NULL;

    /* non-PCM formats except alaw and mulaw formats have extended fmt chunk.
     * Check for those cases.
     */
    if (wav->formatTag != WAVE_FORMAT_PCM &&
        wav->formatTag != WAVE_FORMAT_ALAW &&
        wav->formatTag != WAVE_FORMAT_MULAW) {
        if (len >= 2) {
            lsx_readw(ft, &wExtSize);
            len -= 2;
        } else {
            lsx_warn("wave header missing extended part of fmt chunk");
        }
    }

    if (wExtSize > len)
    {
        lsx_fail_errno(ft,SOX_EOF,"wave header error: wExtSize inconsistent with wFmtLen");
        return SOX_EOF;
    }

    switch (wav->formatTag)
    {
    case WAVE_FORMAT_ADPCM:
        if (wExtSize < 4)
        {
            lsx_fail_errno(ft,SOX_EOF,"format[%s]: expects wExtSize >= %d",
                        wav_format_str(wav->formatTag), 4);
            return SOX_EOF;
        }

        if (wBitsPerSample != 4)
        {
            lsx_fail_errno(ft,SOX_EOF,"Can only handle 4-bit MS ADPCM in wav files");
            return SOX_EOF;
        }

        lsx_readw(ft, &(wav->samplesPerBlock));
        bytesPerBlock = lsx_ms_adpcm_bytes_per_block((size_t) ft->signal.channels, (size_t) wav->samplesPerBlock);
        if (bytesPerBlock > wav->blockAlign)
        {
            lsx_fail_errno(ft,SOX_EOF,"format[%s]: samplesPerBlock(%d) incompatible with blockAlign(%d)",
                wav_format_str(wav->formatTag), wav->samplesPerBlock, wav->blockAlign);
            return SOX_EOF;
        }

        lsx_readw(ft, &(wav->nCoefs));
        if (wav->nCoefs < 7 || wav->nCoefs > 0x100) {
            lsx_fail_errno(ft,SOX_EOF,"ADPCM file nCoefs (%.4hx) makes no sense", wav->nCoefs);
            return SOX_EOF;
        }
        wav->packet = lsx_malloc((size_t)wav->blockAlign);

        len -= 4;

        if (wExtSize < 4 + 4*wav->nCoefs)
        {
            lsx_fail_errno(ft,SOX_EOF,"wave header error: wExtSize(%d) too small for nCoefs(%d)", wExtSize, wav->nCoefs);
            return SOX_EOF;
        }

        wav->samples = lsx_malloc(wChannels*wav->samplesPerBlock*sizeof(short));

        /* nCoefs, lsx_ms_adpcm_i_coefs used by adpcm.c */
        wav->lsx_ms_adpcm_i_coefs = lsx_malloc(wav->nCoefs * 2 * sizeof(short));
        {
            int i, errct=0;
            for (i=0; len>=2 && i < 2*wav->nCoefs; i++) {
                lsx_readsw(ft, &(wav->lsx_ms_adpcm_i_coefs[i]));
                len -= 2;
                if (i<14) errct += (wav->lsx_ms_adpcm_i_coefs[i] != lsx_ms_adpcm_i_coef[i/2][i%2]);
                /* lsx_debug("lsx_ms_adpcm_i_coefs[%2d] %4d",i,wav->lsx_ms_adpcm_i_coefs[i]); */
            }
            if (errct) lsx_warn("base lsx_ms_adpcm_i_coefs differ in %d/14 positions",errct);
        }

        bytespersample = 2;  /* AFTER de-compression */
        break;

    case WAVE_FORMAT_IMA_ADPCM:
        if (wExtSize < 2)
        {
            lsx_fail_errno(ft,SOX_EOF,"format[%s]: expects wExtSize >= %d",
                    wav_format_str(wav->formatTag), 2);
            return SOX_EOF;
        }

        if (wBitsPerSample != 4)
        {
            lsx_fail_errno(ft,SOX_EOF,"Can only handle 4-bit IMA ADPCM in wav files");
            return SOX_EOF;
        }

        lsx_readw(ft, &(wav->samplesPerBlock));
        bytesPerBlock = lsx_ima_bytes_per_block((size_t) ft->signal.channels, (size_t) wav->samplesPerBlock);
        if (bytesPerBlock > wav->blockAlign || wav->samplesPerBlock%8 != 1)
        {
            lsx_fail_errno(ft,SOX_EOF,"format[%s]: samplesPerBlock(%d) incompatible with blockAlign(%d)",
                wav_format_str(wav->formatTag), wav->samplesPerBlock, wav->blockAlign);
            return SOX_EOF;
        }

        wav->packet = lsx_malloc((size_t)wav->blockAlign);
        len -= 2;

        wav->samples = lsx_malloc(wChannels*wav->samplesPerBlock*sizeof(short));

        bytespersample = 2;  /* AFTER de-compression */
        break;

    /* GSM formats have extended fmt chunk.  Check for those cases. */
    case WAVE_FORMAT_GSM610:
        if (wExtSize < 2)
        {
            lsx_fail_errno(ft,SOX_EOF,"format[%s]: expects wExtSize >= %d",
                    wav_format_str(wav->formatTag), 2);
            return SOX_EOF;
        }
        lsx_readw(ft, &wav->samplesPerBlock);
        bytesPerBlock = 65;
        if (wav->blockAlign != 65)
        {
            lsx_fail_errno(ft,SOX_EOF,"format[%s]: expects blockAlign(%d) = %d",
                    wav_format_str(wav->formatTag), wav->blockAlign, 65);
            return SOX_EOF;
        }
        if (wav->samplesPerBlock != 320)
        {
            lsx_fail_errno(ft,SOX_EOF,"format[%s]: expects samplesPerBlock(%d) = %d",
                    wav_format_str(wav->formatTag), wav->samplesPerBlock, 320);
            return SOX_EOF;
        }
        bytespersample = 2;  /* AFTER de-compression */
        len -= 2;
        break;

    default:
      bytespersample = (wBitsPerSample + 7)/8;

    }

    /* User options take precedence */
    if (!ft->encoding.bits_per_sample || ft->encoding.bits_per_sample == wBitsPerSample)
      ft->encoding.bits_per_sample = wBitsPerSample;
    else
      lsx_warn("User options overriding size read in .wav header");

    /* Now we have enough information to set default encodings. */
    switch (bytespersample)
    {
    case 1:
      if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN)
        ft->encoding.encoding = SOX_ENCODING_UNSIGNED;
      break;

    case 2: case 3: case 4:
      if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN)
        ft->encoding.encoding = SOX_ENCODING_SIGN2;
      break;

    case 8:
      if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN)
        ft->encoding.encoding = SOX_ENCODING_FLOAT;
      break;

    default:
      lsx_fail_errno(ft,SOX_EFMT,"Sorry, don't understand .wav size");
      return SOX_EOF;
    }

    /* Skip anything left over from fmt chunk */
    lsx_seeki(ft, (off_t)len, SEEK_CUR);

    /* for non-PCM formats, there's a 'fact' chunk before
     * the upcoming 'data' chunk */

    /* Now look for the wave data chunk */
    if (findChunk(ft, "data", &len) == SOX_EOF)
    {
        lsx_fail_errno(ft, SOX_EOF, "Could not find data chunk.");
        return SOX_EOF;
    }
    dwDataLength = len;
    if (dwDataLength == MS_UNSPEC) {
      wav->ignoreSize = 1;
      lsx_debug("WAV Chunk data's length is value often used in pipes or 4G files.  Ignoring length.");
    }


    /* Data starts here */
    wav->dataStart = lsx_tell(ft);

    switch (wav->formatTag)
    {

    case WAVE_FORMAT_ADPCM:
        wav->numSamples =
            lsx_ms_adpcm_samples_in((size_t)dwDataLength, (size_t)ft->signal.channels,
                           (size_t)wav->blockAlign, (size_t)wav->samplesPerBlock);
        lsx_debug_more("datalen %d, numSamples %lu",dwDataLength, (unsigned long)wav->numSamples);
        wav->blockSamplesRemaining = 0;        /* Samples left in buffer */
        ft->signal.length = wav->numSamples*ft->signal.channels;
        break;

    case WAVE_FORMAT_IMA_ADPCM:
        /* Compute easiest part of number of samples.  For every block, there
           are samplesPerBlock samples to read. */
        wav->numSamples =
            lsx_ima_samples_in((size_t)dwDataLength, (size_t)ft->signal.channels,
                         (size_t)wav->blockAlign, (size_t)wav->samplesPerBlock);
        lsx_debug_more("datalen %d, numSamples %lu",dwDataLength, (unsigned long)wav->numSamples);
        wav->blockSamplesRemaining = 0;        /* Samples left in buffer */
        lsx_ima_init_table();
        ft->signal.length = wav->numSamples*ft->signal.channels;
        break;

    case WAVE_FORMAT_GSM610:
        wav->numSamples = ((dwDataLength / wav->blockAlign) * wav->samplesPerBlock);
        wavgsminit(ft);
        ft->signal.length = wav->numSamples*ft->signal.channels;
        break;

    default:
        wav->numSamples = div_bits(dwDataLength, ft->encoding.bits_per_sample) / ft->signal.channels;
        ft->signal.length = wav->numSamples * ft->signal.channels;
    }
     
    /* When ignoring size, reset length so that output files do
     * not mistakenly depend on it.
     */
    if (wav->ignoreSize)
      ft->signal.length = SOX_UNSPEC;

    lsx_debug("Reading Wave file: %s format, %d channel%s, %d samp/sec",
           wav_format_str(wav->formatTag), ft->signal.channels,
           wChannels == 1 ? "" : "s", dwSamplesPerSecond);
    lsx_debug("        %d byte/sec, %d block align, %d bits/samp, %u data bytes",
           dwAvgBytesPerSec, wav->blockAlign, wBitsPerSample, dwDataLength);

    /* Can also report extended fmt information */
    switch (wav->formatTag)
    {
        case WAVE_FORMAT_ADPCM:
            lsx_debug("        %d Extsize, %d Samps/block, %lu bytes/block %d Num Coefs, %lu Samps/chan",
                      wExtSize,wav->samplesPerBlock,
                      (unsigned long)bytesPerBlock,wav->nCoefs,
                      (unsigned long)wav->numSamples);
            break;

        case WAVE_FORMAT_IMA_ADPCM:
            lsx_debug("        %d Extsize, %d Samps/block, %lu bytes/block %lu Samps/chan",
                      wExtSize, wav->samplesPerBlock, 
                      (unsigned long)bytesPerBlock,
                      (unsigned long)wav->numSamples);
            break;

        case WAVE_FORMAT_GSM610:
            lsx_debug("GSM .wav: %d Extsize, %d Samps/block, %lu Samples/chan",
                      wExtSize, wav->samplesPerBlock, 
                      (unsigned long)wav->numSamples);
            break;

        default:
            lsx_debug("        %lu Samps/chans", 
                      (unsigned long)wav->numSamples);
    }

    /* Horrible way to find Cool Edit marker points. Taken from Quake source*/
    ft->oob.loops[0].start = SOX_IGNORE_LENGTH;
    if(ft->seekable){
        /*Got this from the quake source.  I think it 32bit aligns the chunks
         * doubt any machine writing Cool Edit Chunks writes them at an odd
         * offset */
        len = (len + 1) & ~1u;
        if (lsx_seeki(ft, (off_t)len, SEEK_CUR) == SOX_SUCCESS &&
            findChunk(ft, "LIST", &len) != SOX_EOF)
        {
            wav->comment = lsx_malloc((size_t)256);
            /* Initialize comment to a NULL string */
            wav->comment[0] = 0;
            while(!lsx_eof(ft))
            {
                if (lsx_reads(ft,magic,(size_t)4) == SOX_EOF)
                    break;

                /* First look for type fields for LIST Chunk and
                 * skip those if found.  Since a LIST is a list
                 * of Chunks, treat the remaining data as Chunks
                 * again.
                 */
                if (strncmp(magic, "INFO", (size_t)4) == 0)
                {
                    /*Skip*/
                    lsx_debug("Type INFO");
                }
                else if (strncmp(magic, "adtl", (size_t)4) == 0)
                {
                    /* Skip */
                    lsx_debug("Type adtl");
                }
                else
                {
                    if (lsx_readdw(ft,&len) == SOX_EOF)
                        break;
                    if (strncmp(magic,"ICRD",(size_t)4) == 0)
                    {
                        lsx_debug("Chunk ICRD");
                        if (len > 254)
                        {
                            lsx_warn("Possible buffer overflow hack attack (ICRD)!");
                            break;
                        }
                        lsx_reads(ft,text, (size_t)len);
                        if (strlen(wav->comment) + strlen(text) < 254)
                        {
                            if (wav->comment[0] != 0)
                                strcat(wav->comment,"\n");

                            strcat(wav->comment,text);
                        }
                        if (strlen(text) < len)
                           lsx_seeki(ft, (off_t)(len - strlen(text)), SEEK_CUR);
                    }
                    else if (strncmp(magic,"ISFT",(size_t)4) == 0)
                    {
                        lsx_debug("Chunk ISFT");
                        if (len > 254)
                        {
                            lsx_warn("Possible buffer overflow hack attack (ISFT)!");
                            break;
                        }
                        lsx_reads(ft,text, (size_t)len);
                        if (strlen(wav->comment) + strlen(text) < 254)
                        {
                            if (wav->comment[0] != 0)
                                strcat(wav->comment,"\n");

                            strcat(wav->comment,text);
                        }
                        if (strlen(text) < len)
                           lsx_seeki(ft, (off_t)(len - strlen(text)), SEEK_CUR);
                    }
                    else if (strncmp(magic,"cue ",(size_t)4) == 0)
                    {
                        lsx_debug("Chunk cue ");
                        lsx_seeki(ft,(off_t)(len-4),SEEK_CUR);
                        lsx_readdw(ft,&dwLoopPos);
                        ft->oob.loops[0].start = dwLoopPos;
                    }
                    else if (strncmp(magic,"ltxt",(size_t)4) == 0)
                    {
                        lsx_debug("Chunk ltxt");
                        lsx_readdw(ft,&dwLoopPos);
                        ft->oob.loops[0].length = dwLoopPos - ft->oob.loops[0].start;
                        if (len > 4)
                           lsx_seeki(ft, (off_t)(len - 4), SEEK_CUR);
                    }
                    else
                    {
                        lsx_debug("Attempting to seek beyond unsupported chunk `%c%c%c%c' of length %d bytes", magic[0], magic[1], magic[2], magic[3], len);
                        len = (len + 1) & ~1u;
                        lsx_seeki(ft, (off_t)len, SEEK_CUR);
                    }
                }
            }
        }
        lsx_clearerr(ft);
        lsx_seeki(ft,(off_t)wav->dataStart,SEEK_SET);
    }
    return lsx_rawstartread(ft);
}


/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

static size_t read_samples(sox_format_t * ft, sox_sample_t *buf, size_t len)
{
        priv_t *   wav = (priv_t *) ft->priv;
        size_t done;

        ft->sox_errno = SOX_SUCCESS;

        /* If file is in ADPCM encoding then read in multiple blocks else */
        /* read as much as possible and return quickly. */
        switch (ft->encoding.encoding)
        {
        case SOX_ENCODING_IMA_ADPCM:
        case SOX_ENCODING_MS_ADPCM:

            if (!wav->ignoreSize && len > (wav->numSamples*ft->signal.channels))
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

                /* Copy interleaved data into buf, converting to sox_sample_t */
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
                        *buf++ = SOX_SIGNED_16BIT_TO_SAMPLE((*p++),);

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

        case SOX_ENCODING_GSM:
            if (!wav->ignoreSize && len > wav->numSamples*ft->signal.channels)
                len = (wav->numSamples*ft->signal.channels);

            done = wavgsmread(ft, buf, len);
            if (done == 0 && wav->numSamples != 0 && !wav->ignoreSize)
                lsx_warn("Premature EOF on .wav input file");
        break;

        default: /* assume PCM or float encoding */
            if (!wav->ignoreSize && len > wav->numSamples*ft->signal.channels)
                len = (wav->numSamples*ft->signal.channels);

            done = lsx_rawread(ft, buf, len);
            /* If software thinks there are more samples but I/O */
            /* says otherwise, let the user know about this.     */
            if (done == 0 && wav->numSamples != 0 && !wav->ignoreSize)
                lsx_warn("Premature EOF on .wav input file");
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
static int stopread(sox_format_t * ft)
{
    priv_t *       wav = (priv_t *) ft->priv;

    ft->sox_errno = SOX_SUCCESS;

    free(wav->packet);
    free(wav->samples);
    free(wav->lsx_ms_adpcm_i_coefs);
    free(wav->comment);
    wav->comment = NULL;

    switch (ft->encoding.encoding)
    {
    case SOX_ENCODING_GSM:
        wavgsmdestroy(ft);
        break;
    case SOX_ENCODING_IMA_ADPCM:
    case SOX_ENCODING_MS_ADPCM:
        break;
    default:
        break;
    }
    return SOX_SUCCESS;
}

static int startwrite(sox_format_t * ft)
{
    priv_t * wav = (priv_t *) ft->priv;
    int rc;

    ft->sox_errno = SOX_SUCCESS;

    if (ft->encoding.encoding != SOX_ENCODING_MS_ADPCM &&
        ft->encoding.encoding != SOX_ENCODING_IMA_ADPCM &&
        ft->encoding.encoding != SOX_ENCODING_GSM)
    {
        rc = lsx_rawstartwrite(ft);
        if (rc)
            return rc;
    }

    wav->numSamples = 0;
    wav->dataLength = 0;
    if (!ft->signal.length && !ft->seekable)
        lsx_warn("Length in output .wav header will be wrong since can't seek to fix it");

    rc = wavwritehdr(ft, 0);  /* also calculates various wav->* info */
    if (rc != 0)
        return rc;

    wav->packet = NULL;
    wav->samples = NULL;
    wav->lsx_ms_adpcm_i_coefs = NULL;
    switch (wav->formatTag)
    {
        size_t ch, sbsize;

        case WAVE_FORMAT_IMA_ADPCM:
            lsx_ima_init_table();
        /* intentional case fallthru! */
        case WAVE_FORMAT_ADPCM:
            /* #channels already range-checked for overflow in wavwritehdr() */
            for (ch=0; ch<ft->signal.channels; ch++)
                wav->state[ch] = 0;
            sbsize = ft->signal.channels * wav->samplesPerBlock;
            wav->packet = lsx_malloc((size_t)wav->blockAlign);
            wav->samples = lsx_malloc(sbsize*sizeof(short));
            wav->sampleTop = wav->samples + sbsize;
            wav->samplePtr = wav->samples;
            break;

        case WAVE_FORMAT_GSM610:
            return wavgsminit(ft);

        default:
            break;
    }
    return SOX_SUCCESS;
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
(+ a padding byte if dwDataLength is odd)

non-PCM formats must write an extended format chunk and a fact chunk:

ULAW, ALAW formats:
36 - 37    wExtSize = 0  the length of the format extension
38 - 41    'fact'
42 - 45    dwFactSize = 4  length of the fact chunk minus 8 byte header
46 - 49    dwSamplesWritten   actual number of samples written out
50 - 53    'data'
54 - 57     dwDataLength  length of data chunk minus 8 byte header
58 - (dwDataLength + 57)  the data
(+ a padding byte if dwDataLength is odd)


GSM6.10  format:
36 - 37    wExtSize = 2 the length in bytes of the format-dependent extension
38 - 39    320           number of samples per  block
40 - 43    'fact'
44 - 47    dwFactSize = 4  length of the fact chunk minus 8 byte header
48 - 51    dwSamplesWritten   actual number of samples written out
52 - 55    'data'
56 - 59     dwDataLength  length of data chunk minus 8 byte header
60 - (dwDataLength + 59)  the data (including a padding byte, if necessary,
                            so dwDataLength is always even)


note that header contains (up to) 3 separate ways of describing the
length of the file, all derived here from the number of (input)
samples wav->numSamples in a way that is non-trivial for the blocked
and padded compressed formats:

wRiffLength -      (riff header) the length of the file, minus 8
dwSamplesWritten - (fact header) the number of samples written (after padding
                   to a complete block eg for GSM)
dwDataLength     - (data chunk header) the number of (valid) data bytes written

*/

static int wavwritehdr(sox_format_t * ft, int second_header)
{
    priv_t *       wav = (priv_t *) ft->priv;

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
    uint32_t  dwDataLength; /* length of sound data in bytes */
    /* end of variables written to header */

    /* internal variables, intermediate values etc */
    int bytespersample; /* (uncompressed) bytes per sample (per channel) */
    long blocksWritten = 0;
    sox_bool isExtensible = sox_false;    /* WAVE_FORMAT_EXTENSIBLE? */

    dwSamplesPerSecond = ft->signal.rate;
    wChannels = ft->signal.channels;
    wBitsPerSample = ft->encoding.bits_per_sample;
    wSamplesPerBlock = 1;       /* common default for PCM data */

    switch (ft->encoding.encoding)
    {
        case SOX_ENCODING_UNSIGNED:
        case SOX_ENCODING_SIGN2:
            wFormatTag = WAVE_FORMAT_PCM;
            bytespersample = (wBitsPerSample + 7)/8;
            wBlockAlign = wChannels * bytespersample;
            break;
        case SOX_ENCODING_FLOAT:
            wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
            bytespersample = (wBitsPerSample + 7)/8;
            wBlockAlign = wChannels * bytespersample;
            break;
        case SOX_ENCODING_ALAW:
            wFormatTag = WAVE_FORMAT_ALAW;
            wBlockAlign = wChannels;
            break;
        case SOX_ENCODING_ULAW:
            wFormatTag = WAVE_FORMAT_MULAW;
            wBlockAlign = wChannels;
            break;
        case SOX_ENCODING_IMA_ADPCM:
            if (wChannels>16)
            {
                lsx_fail_errno(ft,SOX_EOF,"Channels(%d) must be <= 16",wChannels);
                return SOX_EOF;
            }
            wFormatTag = WAVE_FORMAT_IMA_ADPCM;
            wBlockAlign = wChannels * 256; /* reasonable default */
            wBitsPerSample = 4;
            wExtSize = 2;
            wSamplesPerBlock = lsx_ima_samples_in((size_t) 0, (size_t) wChannels, (size_t) wBlockAlign, (size_t) 0);
            break;
        case SOX_ENCODING_MS_ADPCM:
            if (wChannels>16)
            {
                lsx_fail_errno(ft,SOX_EOF,"Channels(%d) must be <= 16",wChannels);
                return SOX_EOF;
            }
            wFormatTag = WAVE_FORMAT_ADPCM;
            wBlockAlign = ft->signal.rate / 11008;
            wBlockAlign = max(wBlockAlign, 1) * wChannels * 256;
            wBitsPerSample = 4;
            wExtSize = 4+4*7;      /* Ext fmt data length */
            wSamplesPerBlock = lsx_ms_adpcm_samples_in((size_t) 0, (size_t) wChannels, (size_t) wBlockAlign, (size_t) 0);
            break;
        case SOX_ENCODING_GSM:
            if (wChannels!=1)
            {
                lsx_report("Overriding GSM audio from %d channel to 1",wChannels);
                if (!second_header)
                  ft->signal.length /= max(1, ft->signal.channels);
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

    /* When creating header, use length hint given by input file.  If no
     * hint then write default value.  Also, use default value even
     * on header update if more then 32-bit length needs to be written.
     */
    if ((!second_header && !ft->signal.length) || 
        wav->numSamples > 0xffffffff) { 
        /* adjust for blockAlign */
        blocksWritten = MS_UNSPEC/wBlockAlign;
        dwDataLength = blocksWritten * wBlockAlign;
        dwSamplesWritten = blocksWritten * wSamplesPerBlock;
    } else {    /* fixup with real length */
        dwSamplesWritten = 
            second_header? wav->numSamples : ft->signal.length / wChannels;
        blocksWritten = (dwSamplesWritten+wSamplesPerBlock-1)/wSamplesPerBlock;
        dwDataLength = blocksWritten * wBlockAlign;
    }

    if (wFormatTag == WAVE_FORMAT_GSM610)
        dwDataLength = (dwDataLength+1) & ~1u; /* round up to even */

    if (wFormatTag == WAVE_FORMAT_PCM && (wBitsPerSample > 16 || wChannels > 2)
        && strcmp(ft->filetype, "wavpcm")) {
      isExtensible = sox_true;
      wFmtSize += 2 + 22;
    }
    else if (wFormatTag != WAVE_FORMAT_PCM)
        wFmtSize += 2+wExtSize; /* plus ExtData */

    wRiffLength = 4 + (8+wFmtSize) + (8+dwDataLength+dwDataLength%2);
    if (isExtensible || wFormatTag != WAVE_FORMAT_PCM) /* PCM omits the "fact" chunk */
        wRiffLength += (8+dwFactSize);

    /* dwAvgBytesPerSec <-- this is BEFORE compression, isn't it? guess not. */
    dwAvgBytesPerSec = (double)wBlockAlign*ft->signal.rate / (double)wSamplesPerBlock + 0.5;

    /* figured out header info, so write it */

    /* If user specified opposite swap than we think, assume they are
     * asking to write a RIFX file.
     */
    if (ft->encoding.reverse_bytes == MACHINE_IS_LITTLEENDIAN)
    {
        if (!second_header)
            lsx_report("Requested to swap bytes so writing RIFX header");
        lsx_writes(ft, "RIFX");
    }
    else
        lsx_writes(ft, "RIFF");
    lsx_writedw(ft, wRiffLength);
    lsx_writes(ft, "WAVE");
    lsx_writes(ft, "fmt ");
    lsx_writedw(ft, wFmtSize);
    lsx_writew(ft, isExtensible ? WAVE_FORMAT_EXTENSIBLE : wFormatTag);
    lsx_writew(ft, wChannels);
    lsx_writedw(ft, dwSamplesPerSecond);
    lsx_writedw(ft, dwAvgBytesPerSec);
    lsx_writew(ft, wBlockAlign);
    lsx_writew(ft, wBitsPerSample); /* end info common to all fmts */

    if (isExtensible) {
      uint32_t dwChannelMask=0;  /* unassigned speaker mapping by default */
      static unsigned char const guids[][14] = {
        "\x00\x00\x00\x00\x10\x00\x80\x00\x00\xAA\x00\x38\x9B\x71",  /* wav */
        "\x00\x00\x21\x07\xd3\x11\x86\x44\xc8\xc1\xca\x00\x00\x00"}; /* amb */

      /* if not amb, assume most likely channel masks from number of channels; not
       * ideal solution, but will make files playable in many/most situations
       */
      if (strcmp(ft->filetype, "amb")) {
        if      (wChannels == 1) dwChannelMask = 0x4;     /* 1 channel (mono) = FC */
        else if (wChannels == 2) dwChannelMask = 0x3;     /* 2 channels (stereo) = FL, FR */
        else if (wChannels == 4) dwChannelMask = 0x33;    /* 4 channels (quad) = FL, FR, BL, BR */
        else if (wChannels == 6) dwChannelMask = 0x3F;    /* 6 channels (5.1) = FL, FR, FC, LF, BL, BR */
        else if (wChannels == 8) dwChannelMask = 0x63F;   /* 8 channels (7.1) = FL, FR, FC, LF, BL, BR, SL, SR */
      }
 
      lsx_writew(ft, 22);
      lsx_writew(ft, wBitsPerSample); /* No padding in container */
      lsx_writedw(ft, dwChannelMask); /* Speaker mapping is something reasonable */
      lsx_writew(ft, wFormatTag);
      lsx_writebuf(ft, guids[!strcmp(ft->filetype, "amb")], (size_t)14);
    }
    else
    /* if not PCM, we need to write out wExtSize even if wExtSize=0 */
    if (wFormatTag != WAVE_FORMAT_PCM)
        lsx_writew(ft,wExtSize);

    switch (wFormatTag)
    {
        int i;
        case WAVE_FORMAT_IMA_ADPCM:
        lsx_writew(ft, wSamplesPerBlock);
        break;
        case WAVE_FORMAT_ADPCM:
        lsx_writew(ft, wSamplesPerBlock);
        lsx_writew(ft, 7); /* nCoefs */
        for (i=0; i<7; i++) {
            lsx_writew(ft, (uint16_t)(lsx_ms_adpcm_i_coef[i][0]));
            lsx_writew(ft, (uint16_t)(lsx_ms_adpcm_i_coef[i][1]));
        }
        break;
        case WAVE_FORMAT_GSM610:
        lsx_writew(ft, wSamplesPerBlock);
        break;
        default:
        break;
    }

    /* if not PCM, write the 'fact' chunk */
    if (isExtensible || wFormatTag != WAVE_FORMAT_PCM){
        lsx_writes(ft, "fact");
        lsx_writedw(ft,dwFactSize);
        lsx_writedw(ft,dwSamplesWritten);
    }

    lsx_writes(ft, "data");
    lsx_writedw(ft, dwDataLength);               /* data chunk size */

    if (!second_header) {
        lsx_debug("Writing Wave file: %s format, %d channel%s, %d samp/sec",
                wav_format_str(wFormatTag), wChannels,
                wChannels == 1 ? "" : "s", dwSamplesPerSecond);
        lsx_debug("        %d byte/sec, %d block align, %d bits/samp",
                dwAvgBytesPerSec, wBlockAlign, wBitsPerSample);
    } else {
        lsx_debug("Finished writing Wave file, %u data bytes %lu samples",
                dwDataLength, (unsigned long)wav->numSamples);
        if (wFormatTag == WAVE_FORMAT_GSM610){
            lsx_debug("GSM6.10 format: %li blocks %u padded samples %u padded data bytes",
                    blocksWritten, dwSamplesWritten, dwDataLength);
            if (wav->gsmbytecount != dwDataLength)
                lsx_warn("help ! internal inconsistency - data_written %u gsmbytecount %lu",
                        dwDataLength, (unsigned long)wav->gsmbytecount);

        }
    }
    return SOX_SUCCESS;
}

static size_t write_samples(sox_format_t * ft, const sox_sample_t *buf, size_t len)
{
        priv_t *   wav = (priv_t *) ft->priv;
        ptrdiff_t total_len = len;

        ft->sox_errno = SOX_SUCCESS;

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
            len = lsx_rawwrite(ft, buf, len);
            wav->numSamples += (len/ft->signal.channels);
            return len;
        }
}

static int stopwrite(sox_format_t * ft)
{
        priv_t *   wav = (priv_t *) ft->priv;

        ft->sox_errno = SOX_SUCCESS;


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

        /* Add a pad byte if the number of data bytes is odd.
           See wavwritehdr() above for the calculation. */
        if (wav->formatTag != WAVE_FORMAT_GSM610)
          lsx_padbytes(ft, (size_t)((wav->numSamples + wav->samplesPerBlock - 1)/wav->samplesPerBlock*wav->blockAlign) % 2);

        free(wav->packet);
        free(wav->samples);
        free(wav->lsx_ms_adpcm_i_coefs);

        /* All samples are already written out. */
        /* If file header needs fixing up, for example it needs the */
        /* the number of samples in a field, seek back and write them here. */
        if (ft->signal.length && wav->numSamples <= 0xffffffff && 
            wav->numSamples == ft->signal.length)
          return SOX_SUCCESS;
        if (!ft->seekable)
          return SOX_EOF;

        if (lsx_seeki(ft, (off_t)0, SEEK_SET) != 0)
        {
                lsx_fail_errno(ft,SOX_EOF,"Can't rewind output file to rewrite .wav header.");
                return SOX_EOF;
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

static int seek(sox_format_t * ft, uint64_t offset)
{
  priv_t *   wav = (priv_t *) ft->priv;

  if (ft->encoding.bits_per_sample & 7)
    lsx_fail_errno(ft, SOX_ENOTSUP, "seeking not supported with this encoding");
  else if (wav->formatTag == WAVE_FORMAT_GSM610) {
    int alignment;
    size_t gsmoff;

    /* rounding bytes to blockAlign so that we
     * don't have to decode partial block. */
    gsmoff = offset * wav->blockAlign / wav->samplesPerBlock +
             wav->blockAlign * ft->signal.channels / 2;
    gsmoff -= gsmoff % (wav->blockAlign * ft->signal.channels);

    ft->sox_errno = lsx_seeki(ft, (off_t)(gsmoff + wav->dataStart), SEEK_SET);
    if (ft->sox_errno == SOX_SUCCESS) {
      /* offset is in samples */
      uint64_t new_offset = offset;
      alignment = offset % wav->samplesPerBlock;
      if (alignment != 0)
          new_offset += (wav->samplesPerBlock - alignment);
      wav->numSamples = ft->signal.length - (new_offset / ft->signal.channels);
    }
  } else {
    double wide_sample = offset - (offset % ft->signal.channels);
    double to_d = wide_sample * ft->encoding.bits_per_sample / 8;
    off_t to = to_d;
    ft->sox_errno = (to != to_d)? SOX_EOF : lsx_seeki(ft, (off_t)wav->dataStart + (off_t)to, SEEK_SET);
    if (ft->sox_errno == SOX_SUCCESS)
      wav->numSamples -= (size_t)wide_sample / ft->signal.channels;
  }

  return ft->sox_errno;
}

LSX_FORMAT_HANDLER(wav)
{
  static char const * const names[] = {"wav", "wavpcm", "amb", NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_SIGN2, 16, 24, 32, 0,
    SOX_ENCODING_UNSIGNED, 8, 0,
    SOX_ENCODING_ULAW, 8, 0,
    SOX_ENCODING_ALAW, 8, 0,
    SOX_ENCODING_GSM, 0,
    SOX_ENCODING_MS_ADPCM, 4, 0,
    SOX_ENCODING_IMA_ADPCM, 4, 0,
    SOX_ENCODING_FLOAT, 32, 0,
    0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "Microsoft audio format", names, SOX_FILE_LIT_END,
    startread, read_samples, stopread,
    startwrite, write_samples, stopwrite,
    seek, write_encodings, NULL, sizeof(priv_t)
  };
  return &handler;
}
