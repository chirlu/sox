/*
 * Microsoft's WAVE sound format driver
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 *
 * Change History:
 *
 * November  23, 1999 - Stan Brooks (stabro@megsinet.com)
 *   Merged in gsm support patches from Stuart Daines...
 *   Since we had simultaneously made similar changes in
 *   wavwriteheader() and wavstartread(), this was some
 *   work.  Hopefully the result is cleaner than either
 *   version, and nothing broke.
 *
 * November  20, 1999 - Stan Brooks (stabro@megsinet.com)
 *   Mods for faster adpcm decoding and addition of IMA_ADPCM
 *   and ADPCM  writing... low-level codex functions moved to
 *   external modules ima_rw.c and adpcm.c. Some general cleanup,
 *   consistent with writing adpcm and other output formats.
 *   Headers written for adpcm include the 'fact' subchunk.
 *
 * September 11, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *   Fixed length bug for IMA and MS ADPCM files.
 *
 * June 1, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *   Fixed some compiler warnings as reported by Kjetil Torgrim Homme
 *   <kjetilho@ifi.uio.no>.
 *   Fixed bug that caused crashes when reading mono MS ADPCM files. Patch
 *   was sent from Michael Brown (mjb@pootle.demon.co.uk).
 *
 * March 15, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *   Added support for Microsoft's ADPCM and IMA (or better known as
 *   DVI) ADPCM format for wav files.  Thanks goes to Mark Podlipec's
 *   XAnim code.  It gave some real life understanding of how the ADPCM
 *   format is processed.  Actual code was implemented based off of
 *   various sources from the net.
 *
 * NOTE: Previous maintainers weren't very good at providing contact
 * information.
 *
 * Copyright 1992 Rick Richardson
 * Copyright 1991 Lance Norskog And Sundry Contributors
 *
 * Fixed by various contributors previous to 1998:
 * 1) Little-endian handling
 * 2) Skip other kinds of file data
 * 3) Handle 16-bit formats correctly
 * 4) Not go into infinite loop
 *
 * User options should override file header - we assumed user knows what
 * they are doing if they specify options.
 * Enhancements and clean up by Graeme W. Gill, 93/5/17
 *
 * Info for format tags can be found at:
 *   http://www.microsoft.com/asf/resources/draft-ietf-fleischman-codec-subtree-01.txt
 *
 */

#include <string.h>		/* Included for strncmp */
#include <stdlib.h>		/* Included for malloc and free */
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>		/* For SEEK_* defines if not found in stdio */
#endif

#include "st.h"
#include "wav.h"
#include "ima_rw.h"
#include "adpcm.h"
#ifdef HAVE_LIBGSM
#include "gsm.h"
#endif

#undef PAD_NSAMPS
/* #define PAD_NSAMPS */

/* Private data for .wav file */
typedef struct wavstuff {
    LONG	   numSamples;
    LONG	   dataLength;     /* needed for ADPCM writing */
    unsigned short formatTag;	   /* What type of encoding file is using */
    unsigned short samplesPerBlock;
    unsigned short blockAlign;
    
    /* following used by *ADPCM wav files */
    unsigned short nCoefs;	    /* ADPCM: number of coef sets */
    short	  *iCoefs;	    /* ADPCM: coef sets           */
    unsigned char *packet;	    /* Temporary buffer for packets */
    short	  *samples;	    /* interleaved samples buffer */
    short	  *samplePtr;       /* Pointer to current sample  */
    short	  *sampleTop;       /* End of samples-buffer      */
    unsigned short blockSamplesRemaining;/* Samples remaining per channel */    
    int 	   state[16];       /* step-size info for *ADPCM writes */

    /* following used by GSM 6.10 wav */
#ifdef HAVE_LIBGSM
    gsm  gsmhandle;
    gsm_signal *gsmsample;
    int  gsmindex;
    int gsmbytecount;
#endif
} *wav_t;

/*
#if sizeof(struct wavstuff) > PRIVSIZE
#	warn "Uh-Oh"
#endif
*/

static char *wav_format_str();

LONG rawread(P3(ft_t, LONG *, LONG));
void rawwrite(P3(ft_t, LONG *, LONG));
void wavwritehdr(P2(ft_t, int));


/****************************************************************************/
/* IMA ADPCM Support Functions Section                                      */
/****************************************************************************/

/*
 *
 * ImaAdpcmReadBlock - Grab and decode complete block of samples
 *
 */
unsigned short  ImaAdpcmReadBlock(ft)
ft_t ft;    
{
    wav_t	wav = (wav_t) ft->priv;
    int bytesRead;
    int samplesThisBlock;

    /* Pull in the packet and check the header */
    bytesRead = fread(wav->packet,1,wav->blockAlign,ft->fp);
    samplesThisBlock = wav->samplesPerBlock;
    if (bytesRead < wav->blockAlign) 
    { 
	/* If it looks like a valid header is around then try and */
	/* work with partial blocks.  Specs say it should be null */
	/* padded but I guess this is better than trailing quiet. */
	samplesThisBlock = ImaSamplesIn(0, ft->info.channels, bytesRead, 0);
	if (samplesThisBlock == 0) 
	{
	    warn ("Premature EOF on .wav input file");
	    return 0;
	}
    }
    
    wav->samplePtr = wav->samples;
    
    /* For a full block, the following should be true: */
    /* wav->samplesPerBlock = blockAlign - 8byte header + 1 sample in header */
    ImaBlockExpandI(ft->info.channels, wav->packet, wav->samples, samplesThisBlock);
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
unsigned short  AdpcmReadBlock(ft)
ft_t ft;    
{
    wav_t	wav = (wav_t) ft->priv;
    int bytesRead;
    int samplesThisBlock;
    const char *errmsg;

    /* Pull in the packet and check the header */
    bytesRead = fread(wav->packet,1,wav->blockAlign,ft->fp);
    samplesThisBlock = wav->samplesPerBlock;
    if (bytesRead < wav->blockAlign) 
    {
	/* If it looks like a valid header is around then try and */
	/* work with partial blocks.  Specs say it should be null */
	/* padded but I guess this is better than trailing quiet. */
	samplesThisBlock = AdpcmSamplesIn(0, ft->info.channels, bytesRead, 0);
	if (samplesThisBlock == 0) 
	{
	    warn ("Premature EOF on .wav input file");
	    return 0;
	}
    }
    
    errmsg = AdpcmBlockExpandI(ft->info.channels, wav->nCoefs, wav->iCoefs, wav->packet, wav->samples, samplesThisBlock);

    if (errmsg)
	warn((char*)errmsg);

    return samplesThisBlock;
}

/****************************************************************************/
/* Common ADPCM Write Function                                              */
/****************************************************************************/

static void xxxAdpcmWriteBlock(ft)
ft_t ft;
{
    wav_t wav = (wav_t) ft->priv;
    int chans, ct;
    short *p;

    chans = ft->info.channels;
    p = wav->samplePtr;
    ct = p - wav->samples;
    if (ct>=chans) { 
	/* zero-fill samples if needed to complete block */
	for (p = wav->samplePtr; p < wav->sampleTop; p++) *p=0;
	/* compress the samples to wav->packet */
	if (wav->formatTag == WAVE_FORMAT_ADPCM) {
	    AdpcmBlockMashI(chans, wav->samples, wav->samplesPerBlock, wav->state, wav->packet, wav->blockAlign,9);
	}else{ /* WAVE_FORMAT_IMA_ADPCM */
	    ImaBlockMashI(chans, wav->samples, wav->samplesPerBlock, wav->state, wav->packet, 9);
	}
	/* write the compressed packet */
	if (fwrite(wav->packet, wav->blockAlign, 1, ft->fp) != 1)
	    fail("write error");
	/* update lengths and samplePtr */
	wav->dataLength += wav->blockAlign;
#ifndef PAD_NSAMPS
	wav->numSamples += ct/chans;
#else
	wav->numSamples += wav->samplesPerBlock;
#endif
	wav->samplePtr = wav->samples;
    }
}

/****************************************************************************/
/* WAV GSM6.10 support functions                                            */
/****************************************************************************/
#ifdef HAVE_LIBGSM
/* create the gsm object, malloc buffer for 160*2 samples */
void wavgsminit(ft)
ft_t ft;
{	
    int valueP=1;
    wav_t	wav = (wav_t) ft->priv;
    wav->gsmbytecount=0;
    wav->gsmhandle=gsm_create();
    if (!wav->gsmhandle)
	fail("cannot create GSM object");
	
    if(gsm_option(wav->gsmhandle,GSM_OPT_WAV49,&valueP) == -1){
	fail("error setting gsm_option for WAV49 format. Recompile gsm library with -DWAV49 option and relink sox");
    }

    wav->gsmsample=malloc(sizeof(gsm_signal)*160*2);
    if (wav->gsmsample == NULL){
	fail("error allocating memory for gsm buffer");
    }
    wav->gsmindex=0;
}

/*destroy the gsm object and free the buffer */
void wavgsmdestroy(ft)
ft_t ft;
{	
    wav_t	wav = (wav_t) ft->priv;
    gsm_destroy(wav->gsmhandle);
    free(wav->gsmsample);
}

LONG wavgsmread(ft, buf, len)
ft_t ft;
LONG *buf, len;
{
    wav_t	wav = (wav_t) ft->priv;
    int done=0;
    int bytes;
    gsm_frame	frame;

  /* copy out any samples left from the last call */
    while(wav->gsmindex && (wav->gsmindex<160*2) && (done < len))
	buf[done++]=LEFT(wav->gsmsample[wav->gsmindex++],16);

  /* read and decode loop, possibly leaving some samples in wav->gsmsample */
    while (done < len) {
	wav->gsmindex=0;
	/*read the long 33 byte half */
	bytes = fread(frame,1,sizeof(frame),ft->fp);   
	if (bytes <=0)
	    return done;
	if (bytes<sizeof(frame))
	    fail("invalid wav gsm frame size: %d bytes",bytes);
	if(gsm_decode(wav->gsmhandle,frame, wav->gsmsample)<0)
	    fail("error during gsm decode");

	/*read the short 32 byte half */
	bytes = fread(frame,1,sizeof(frame)-1,ft->fp);   
	if (bytes <=0)
	    return done;
	if (bytes<sizeof(frame)-1)
	    fail("invalid wav gsm frame size: %d bytes",bytes);
	if(gsm_decode(wav->gsmhandle,frame, &(wav->gsmsample[160]))<0)
	    fail("error during gsm decode");


	while ((wav->gsmindex <160*2) && (done < len)){
	    buf[done++]=LEFT(wav->gsmsample[(wav->gsmindex)++],16);
	}
    }

    return done;
}

void wavgsmwrite(ft, buf, len)
ft_t ft;
LONG *buf, len;
{
    wav_t	wav = (wav_t) ft->priv;

    int done = 0;
    gsm_frame	frame;

    while (done < len) {
	while ((wav->gsmindex < 160*2) && (done < len)){
	    wav->gsmsample[(wav->gsmindex)++] = RIGHT(buf[done++], 16);
	}
	if (wav->gsmindex < 160*2){
	    return;
	}

	/*encode the even half and write short (32 byte) frame */
	gsm_encode(wav->gsmhandle, wav->gsmsample, frame);
	if (fwrite(frame, 1, sizeof(frame)-1, ft->fp) != sizeof(frame)-1)
	    fail("write error");
	wav->gsmbytecount += sizeof(frame)-1;

	/*encode the odd half and write long (33 byte) frame */
	gsm_encode(wav->gsmhandle, &(wav->gsmsample[160]), frame);
	if (fwrite(frame, 1, sizeof(frame), ft->fp) != sizeof(frame))
	    fail("write error");
	wav->gsmbytecount += sizeof(frame);
	wav->gsmindex = 0;
    }     

}

void wavgsmstopwrite(ft)
ft_t ft;
{
    gsm_frame frame;
    wav_t	wav = (wav_t) ft->priv;
    if (wav->gsmindex){
	while(wav->gsmindex<160*2){
	    wav->gsmsample[wav->gsmindex++]=0;
	}

	/*encode the even half and write short (32 byte) frame */
	gsm_encode(wav->gsmhandle, wav->gsmsample, frame);
	if (fwrite(frame, 1, sizeof(frame)-1, ft->fp) != sizeof(frame)-1)
	    fail("write error");
	wav->gsmbytecount += sizeof(frame)-1;
	
	/*encode the odd half and write long (33 byte) frame */
	gsm_encode(wav->gsmhandle, &(wav->gsmsample[160]), frame);
	if (fwrite(frame, 1, sizeof(frame), ft->fp) != sizeof(frame))
	    fail("write error");
	wav->gsmbytecount += sizeof(frame);

	/* pad output to an even number of bytes */
	if (wav->gsmbytecount & 0x1){
	    if(fputc(0,ft->fp))
		fail("write error");
	    wav->gsmbytecount += 1;
	}
    }      
    wavgsmdestroy(ft);
}
#endif        /*ifdef out gsm code */
/****************************************************************************/
/* General Sox WAV file code                                                */
/****************************************************************************/

static void fSkip(FILE *fp, ULONG len)
{   /* FIXME: this should also check ferror(fp) */
    while (len > 0 && !feof(fp))
    {
	getc(fp);
	len--;
    }
}

static ULONG findChunk(ft_t ft, const char *Label)
{
    char magic[4];
    ULONG len;
    for (;;)
    {
	if (fread(magic, 1, 4, ft->fp) != 4)
	    fail("WAVE file has missing %s chunk", Label);
	len = rlong(ft);
	if (strncmp(Label, magic, 4) == 0)
	    break;		/* Found the data chunk */
	
	fSkip(ft->fp, len); 	/* skip to next chunk */
    }
    return len;
}

/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *	Find out sampling rate, 
 *	size and style of samples, 
 *	mono/stereo/quad.
 */
void wavstartread(ft) 
ft_t ft;
{
    wav_t	wav = (wav_t) ft->priv;
    char	magic[4];
    ULONG	len;
    int		littlendian = 1;
    char	*endptr;

    /* wave file characteristics */
    ULONG    wRiffLength;
    unsigned short wChannels;	    /* number of channels */
    ULONG    wSamplesPerSecond;     /* samples per second per channel */
    ULONG    wAvgBytesPerSec;	    /* estimate of bytes per second needed */
    unsigned short wBitsPerSample;  /* bits per sample */
    unsigned short wFmtSize;
    unsigned short wExtSize = 0;    /* extended field for non-PCM */

    ULONG    wDataLength;	    /* length of sound data in bytes */
    ULONG    bytesPerBlock = 0;
    ULONG    bytespersample;	    /* bytes per sample (per channel */

    if (sizeof(struct wavstuff)> PRIVSIZE)
      fail("struct wav_t too big (%d); increase PRIVSIZE in st.h and recompile sox",sizeof(struct wavstuff));
    /* This is needed for rawread(), rshort, etc */
    rawstartread(ft);

    endptr = (char *) &littlendian;
    if (!*endptr) ft->swap = ft->swap ? 0 : 1;

#if 0
    /* If you need to seek around the input file. */
    if (! ft->seekable)
	fail("WAVE input file must be a file, not a pipe");
#endif

    if ( fread(magic, 1, 4, ft->fp) != 4 || strncmp("RIFF", magic, 4))
	fail("WAVE: RIFF header not found");

    wRiffLength = rlong(ft);

    if ( fread(magic, 1, 4, ft->fp) != 4 || strncmp("WAVE", magic, 4))
	fail("WAVE header not found");

    /* Now look for the format chunk */
    wFmtSize = len = findChunk(ft, "fmt ");
    /* findChunk() only returns if chunk was found */
    
    if (wFmtSize < 16)
	fail("WAVE file fmt chunk is too short");

    wav->formatTag = rshort(ft);
    wChannels = rshort(ft);
    wSamplesPerSecond = rlong(ft);
    wAvgBytesPerSec = rlong(ft);	/* Average bytes/second */
    wav->blockAlign = rshort(ft);	/* Block align */
    wBitsPerSample =  rshort(ft);	/* bits per sample per channel */
    len -= 16;

    switch (wav->formatTag)
    {
    case WAVE_FORMAT_UNKNOWN:
	fail("WAVE file is in unsupported Microsoft Official Unknown format.");
	
    case WAVE_FORMAT_PCM:
	/* Default (-1) depends on sample size.  Set that later on. */
	if (ft->info.style != -1 && ft->info.style != UNSIGNED &&
	    ft->info.style != SIGN2)
	    warn("User options overriding style read in .wav header");
	break;
	
    case WAVE_FORMAT_IMA_ADPCM:
	if (ft->info.style == -1 || ft->info.style == IMA_ADPCM)
	    ft->info.style = IMA_ADPCM;
	else
	    warn("User options overriding style read in .wav header");
	break;

    case WAVE_FORMAT_ADPCM:
	if (ft->info.style == -1 || ft->info.style == ADPCM)
	    ft->info.style = ADPCM;
	else
	    warn("User options overriding style read in .wav header");
	break;

    case WAVE_FORMAT_IEEE_FLOAT:
	fail("Sorry, this WAV file is in IEEE Float format.");
	
    case WAVE_FORMAT_ALAW:
	if (ft->info.style == -1 || ft->info.style == ALAW)
	    ft->info.style = ALAW;
	else
	    warn("User options overriding style read in .wav header");
	break;
	
    case WAVE_FORMAT_MULAW:
	if (ft->info.style == -1 || ft->info.style == ULAW)
	    ft->info.style = ULAW;
	else
	    warn("User options overriding style read in .wav header");
	break;
	
    case WAVE_FORMAT_OKI_ADPCM:
	fail("Sorry, this WAV file is in OKI ADPCM format.");
    case WAVE_FORMAT_DIGISTD:
	fail("Sorry, this WAV file is in Digistd format.");
    case WAVE_FORMAT_DIGIFIX:
	fail("Sorry, this WAV file is in Digifix format.");
    case WAVE_FORMAT_DOLBY_AC2:
	fail("Sorry, this WAV file is in Dolby AC2 format.");
    case WAVE_FORMAT_GSM610:
#ifdef HAVE_LIBGSM
	if (ft->info.style == -1 || ft->info.style == GSM )
	    ft->info.style = GSM;
	else
	    warn("User options overriding style read in .wav header");
	break;
#else
	fail("Sorry, this WAV file is in GSM6.10 format and no GSM support present, recompile sox with gsm library");
#endif
    case WAVE_FORMAT_ROCKWELL_ADPCM:
	fail("Sorry, this WAV file is in Rockwell ADPCM format.");
    case WAVE_FORMAT_ROCKWELL_DIGITALK:
	fail("Sorry, this WAV file is in Rockwell DIGITALK format.");
    case WAVE_FORMAT_G721_ADPCM:
	fail("Sorry, this WAV file is in G.721 ADPCM format.");
    case WAVE_FORMAT_G728_CELP:
	fail("Sorry, this WAV file is in G.728 CELP format.");
    case WAVE_FORMAT_MPEG:
	fail("Sorry, this WAV file is in MPEG format.");
    case WAVE_FORMAT_MPEGLAYER3:
	fail("Sorry, this WAV file is in MPEG Layer 3 format.");
    case WAVE_FORMAT_G726_ADPCM:
	fail("Sorry, this WAV file is in G.726 ADPCM format.");
    case WAVE_FORMAT_G722_ADPCM:
	fail("Sorry, this WAV file is in G.722 ADPCM format.");
    default:	fail("WAV file has unknown format type of %x",wav->formatTag);
    }

    /* User options take precedence */
    if (ft->info.channels == -1 || ft->info.channels == wChannels)
	ft->info.channels = wChannels;
    else
	warn("User options overriding channels read in .wav header");

    if (ft->info.rate == 0 || ft->info.rate == wSamplesPerSecond)
	ft->info.rate = wSamplesPerSecond;
    else
	warn("User options overriding rate read in .wav header");
    

    wav->iCoefs = NULL;
    wav->packet = NULL;
    wav->samples = NULL;

    /* non-PCM formats have extended fmt chunk.  Check for those cases. */
    if (wav->formatTag != WAVE_FORMAT_PCM) {
	if (len >= 2) {
	    wExtSize = rshort(ft);
	    len -= 2;
	} else {
	    warn("wave header missing FmtExt chunk");
	}
    }

    if (wExtSize > len)
	fail("wave header error: wExtSize inconsistent with wFmtLen");

    switch (wav->formatTag)
    {
    /* ULONG max_spb; */
    case WAVE_FORMAT_ADPCM:
	if (wExtSize < 4)
	    fail("format[%s]: expects wExtSize >= %d",
			wav_format_str(wav->formatTag), 4);

	if (wBitsPerSample != 4)
	    fail("Can only handle 4-bit MS ADPCM in wav files");

	wav->samplesPerBlock = rshort(ft);
	bytesPerBlock = AdpcmBytesPerBlock(ft->info.channels, wav->samplesPerBlock);
	if (bytesPerBlock > wav->blockAlign)
	    fail("format[%s]: samplesPerBlock(%d) incompatible with blockAlign(%d)",
		wav_format_str(wav->formatTag), wav->samplesPerBlock, wav->blockAlign);

	wav->nCoefs = rshort(ft);
	if (wav->nCoefs < 7 || wav->nCoefs > 0x100) {
	    fail("ADPCM file nCoefs (%.4hx) makes no sense\n", wav->nCoefs);
	}
	wav->packet = (unsigned char *)malloc(wav->blockAlign);
	len -= 4;

	if (wExtSize < 4 + 4*wav->nCoefs)
	    fail("wave header error: wExtSize(%d) too small for nCoefs(%d)", wExtSize, wav->nCoefs);

	wav->samples = (short *)malloc(wChannels*wav->samplesPerBlock*sizeof(short));

	/* nCoefs, iCoefs used by adpcm.c */
	wav->iCoefs = (short *)malloc(wav->nCoefs * 2 * sizeof(short));
	{
	    int i, errct=0;
	    for (i=0; len>=2 && i < 2*wav->nCoefs; i++) {
		wav->iCoefs[i] = rshort(ft);
		len -= 2;
		if (i<14) errct += (wav->iCoefs[i] != iCoef[i/2][i%2]);
		/* fprintf(stderr,"iCoefs[%2d] %4d\n",i,wav->iCoefs[i]); */
	    }
	    if (errct) warn("base iCoefs differ in %d/14 positions",errct);
	}

	bytespersample = WORD;  /* AFTER de-compression */
	break;

    case WAVE_FORMAT_IMA_ADPCM:
	if (wExtSize < 2)
	    fail("format[%s]: expects wExtSize >= %d",
		    wav_format_str(wav->formatTag), 2);

	if (wBitsPerSample != 4)
	    fail("Can only handle 4-bit IMA ADPCM in wav files");

	wav->samplesPerBlock = rshort(ft);
	bytesPerBlock = ImaBytesPerBlock(ft->info.channels, wav->samplesPerBlock);
	if (bytesPerBlock > wav->blockAlign || wav->samplesPerBlock%8 != 1)
	    fail("format[%s]: samplesPerBlock(%d) incompatible with blockAlign(%d)",
		wav_format_str(wav->formatTag), wav->samplesPerBlock, wav->blockAlign);

	wav->packet = (unsigned char *)malloc(wav->blockAlign);
	len -= 2;

	wav->samples = (short *)malloc(wChannels*wav->samplesPerBlock*sizeof(short));

	bytespersample = WORD;  /* AFTER de-compression */
	break;

#ifdef HAVE_LIBGSM
    /* GSM formats have extended fmt chunk.  Check for those cases. */
    case WAVE_FORMAT_GSM610:
	if (wExtSize < 2)
	    fail("format[%s]: expects wExtSize >= %d",
		    wav_format_str(wav->formatTag), 2);
	wav->samplesPerBlock = rshort(ft);
	bytesPerBlock = 65;
	if (wav->blockAlign != 65)
	    fail("format[%s]: expects blockAlign(%d) = %d",
		    wav_format_str(wav->formatTag), wav->blockAlign, 65);
	if (wav->samplesPerBlock != 320)
	    fail("format[%s]: expects samplesPerBlock(%d) = %d",
		    wav_format_str(wav->formatTag), wav->samplesPerBlock, 320);
	bytespersample = WORD;  /* AFTER de-compression */
	len -= 2;
	break;
#endif

    default:
      bytespersample = (wBitsPerSample + 7)/8;

    }

    switch (bytespersample)
    {
	
    case BYTE:
	/* User options take precedence */
	if (ft->info.size == -1 || ft->info.size == BYTE)
	    ft->info.size = BYTE;
	else
	    warn("User options overriding size read in .wav header");

	/* Now we have enough information to set default styles. */
	if (ft->info.style == -1)
	    ft->info.style = UNSIGNED;
	break;
	
    case WORD:
	if (ft->info.size == -1 || ft->info.size == WORD)
	    ft->info.size = WORD;
	else
	    warn("User options overriding size read in .wav header");

	/* Now we have enough information to set default styles. */
	if (ft->info.style == -1)
	    ft->info.style = SIGN2;
	break;
	
    case DWORD:
	if (ft->info.size == -1 || ft->info.size == DWORD)
	    ft->info.size = DWORD;
	else
	    warn("User options overriding size read in .wav header");

	/* Now we have enough information to set default styles. */
	if (ft->info.style == -1)
	    ft->info.style = SIGN2;
	break;
	
    default:
	fail("Sorry, don't understand .wav size");
    }

    /* Skip anything left over from fmt chunk */
    fSkip(ft->fp, len);

    /* for non-PCM formats, there's a 'fact' chunk before
     * the upcoming 'data' chunk */

    /* Now look for the wave data chunk */
    wDataLength = len = findChunk(ft, "data");
    /* findChunk() only returns if chunk was found */

    switch (wav->formatTag)
    {

    case WAVE_FORMAT_ADPCM:
	wav->numSamples = 
	    AdpcmSamplesIn(wDataLength, ft->info.channels, wav->blockAlign, wav->samplesPerBlock);
	/*report("datalen %d, numSamples %d",wDataLength, wav->numSamples);*/
	wav->blockSamplesRemaining = 0;	       /* Samples left in buffer */
	break;

    case WAVE_FORMAT_IMA_ADPCM:
	/* Compute easiest part of number of samples.  For every block, there
	   are samplesPerBlock samples to read. */
	wav->numSamples = 
	    ImaSamplesIn(wDataLength, ft->info.channels, wav->blockAlign, wav->samplesPerBlock);
	/*report("datalen %d, numSamples %d",wDataLength, wav->numSamples);*/
	wav->blockSamplesRemaining = 0;	       /* Samples left in buffer */
	initImaTable();
	break;

#ifdef HAVE_LIBGSM
    case WAVE_FORMAT_GSM610:
	wav->numSamples = (((wDataLength / wav->blockAlign) * wav->samplesPerBlock) * ft->info.channels);
	wavgsminit(ft);
	break;
#endif

    default:
	wav->numSamples = wDataLength/ft->info.size;	/* total samples */

    }

    report("Reading Wave file: %s format, %d channel%s, %d samp/sec",
	   wav_format_str(wav->formatTag), ft->info.channels,
	   wChannels == 1 ? "" : "s", wSamplesPerSecond);
    report("        %d byte/sec, %d block align, %d bits/samp, %u data bytes",
	   wAvgBytesPerSec, wav->blockAlign, wBitsPerSample, wDataLength);

    /* Can also report extended fmt information */
    switch (wav->formatTag)
    {
    case WAVE_FORMAT_ADPCM:
	report("        %d Extsize, %d Samps/block, %d bytes/block %d Num Coefs",
		wExtSize,wav->samplesPerBlock,bytesPerBlock,wav->nCoefs);
	break;

    case WAVE_FORMAT_IMA_ADPCM:
	report("        %d Extsize, %d Samps/block, %d bytes/block",
		wExtSize,wav->samplesPerBlock,bytesPerBlock);
	break;

#ifdef HAVE_LIBGSM
    case WAVE_FORMAT_GSM610:
	report("GSM .wav: %d Extsize, %d Samps/block,  %d samples",
		wExtSize,wav->samplesPerBlock,wav->numSamples);
	break;
#endif

    default:
    }

}


/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

LONG wavread(ft, buf, len) 
ft_t ft;
LONG *buf, len;
{
	wav_t	wav = (wav_t) ft->priv;
	LONG	done;
	
	if (len > wav->numSamples) len = wav->numSamples;

	/* If file is in ADPCM style then read in multiple blocks else */
	/* read as much as possible and return quickly. */
	switch (ft->info.style)
	{
	case IMA_ADPCM:
	case ADPCM:
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
		    wav->blockSamplesRemaining *= ft->info.channels;
		    wav->samplePtr = wav->samples;
		}

		/* Copy interleaved data into buf, converting short to LONG */
		{
		    short *p, *top;
		    int ct;
		    ct = len-done;
		    if (ct > wav->blockSamplesRemaining)
			ct = wav->blockSamplesRemaining;

		    done += ct;
		    wav->blockSamplesRemaining -= ct;
		    p = wav->samplePtr;
		    top = p+ct;
		    /* Output is already signed */
		    while (p<top)
		    {
			*buf++ = LEFT((*p++), 16);
		    }
		    wav->samplePtr = p;
		}
	    }
	    break;

#ifdef HAVE_LIBGSM
	case GSM:
	    done = wavgsmread(ft, buf, len);
	    if (done == 0 && wav->numSamples != 0)
		warn("Premature EOF on .wav input file");
	break;
#endif
	default: /* not ADPCM style */
	    done = rawread(ft, buf, len);
	    /* If software thinks there are more samples but I/O */
	    /* says otherwise, let the user know about this.     */
	    if (done == 0 && wav->numSamples != 0)
		warn("Premature EOF on .wav input file");
	}

	wav->numSamples -= done;
	return done;
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
void wavstopread(ft) 
ft_t ft;
{
    wav_t	wav = (wav_t) ft->priv;

    if (wav->packet) free(wav->packet);
    if (wav->samples) free(wav->samples);
    if (wav->iCoefs) free(wav->iCoefs);

    switch (ft->info.style)
    {
#ifdef HAVE_LIBGSM
    case GSM:
	wavgsmdestroy(ft);
	break;
#endif
    case IMA_ADPCM:
    case ADPCM:
	break;
    default:
	/* Needed for rawread() */
	rawstopread(ft);
    }
}

void wavstartwrite(ft) 
ft_t ft;
{
	wav_t	wav = (wav_t) ft->priv;
	int	littlendian = 1;
	char	*endptr;

  if (sizeof(struct wavstuff)> PRIVSIZE)
    fail("struct wav_t too big (%d); increase PRIVSIZE in st.h and recompile sox",sizeof(struct wavstuff));

	endptr = (char *) &littlendian;
	if (!*endptr) ft->swap = ft->swap ? 0 : 1;

	wav->numSamples = 0;
	wav->dataLength = 0;
	if (!ft->seekable)
		warn("Length in output .wav header will be wrong since can't seek to fix it");
	wavwritehdr(ft, 0);  /* also calculates various wav->* info */
	wav->packet = NULL;
	wav->samples = NULL;
	wav->iCoefs = NULL;
	switch (wav->formatTag)
	{
	int ch, sbsize;
	case WAVE_FORMAT_IMA_ADPCM:
	    initImaTable();
	/* intentional case fallthru! */
	case WAVE_FORMAT_ADPCM:
	    /* #channels already range-checked for overflow in wavwritehdr() */
	    for (ch=0; ch<ft->info.channels; ch++)
	    	wav->state[ch] = 0;
	    sbsize = ft->info.channels * wav->samplesPerBlock;
	    wav->packet = (unsigned char *)malloc(wav->blockAlign);
	    wav->samples = (short *)malloc(sbsize*sizeof(short));
	    wav->sampleTop = wav->samples + sbsize;
	    wav->samplePtr = wav->samples;
	    break;

#ifdef HAVE_LIBGSM
	case WAVE_FORMAT_GSM610:
	    wavgsminit(ft);
	    break;
#endif
	default:
	}
}

/* wavwritehdr:  write .wav headers as follows:
 
bytes      variable      description
0  - 3     'RIFF'
4  - 7     wRiffLength   length of file minus the 8 byte riff header
8  - 11    'WAVE'
12 - 15    'fmt '
16 - 19    wFmtSize       length of format chunk minus 8 byte header 
20 - 21    wFormatTag     identifies PCM, ULAW etc
22 - 23    wChannels      
24 - 27    wSamplesPerSecond   samples per second per channel
28 - 31    wAvgBytesPerSec     non-trivial for compressed formats
32 - 33    wBlockAlign         basic block size
34 - 35    wBitsPerSample      non-trivial for compressed formats

PCM formats then go straight to the data chunk:
36 - 39    'data'
40 - 43     wDataLength   length of data chunk minus 8 byte header
44 - (wDataLength + 43)    the data

non-PCM formats must write an extended format chunk and a fact chunk:

ULAW, ALAW formats:
36 - 37    wExtSize = 0  the length of the format extension
38 - 41    'fact'
42 - 45    wFactSize = 4  length of the fact chunk minus 8 byte header
46 - 49    wSamplesWritten   actual number of samples written out
50 - 53    'data'
54 - 57     wDataLength   length of data chunk minus 8 byte header
58 - (wDataLength + 57)    the data


GSM6.10  format:
36 - 37    wExtSize = 2 the length in bytes of the format-dependent extension
38 - 39    320           number of samples per  block 
40 - 43    'fact'
44 - 47    wFactSize = 4  length of the fact chunk minus 8 byte header
48 - 51    wSamplesWritten   actual number of samples written out
52 - 55    'data'
56 - 59     wDataLength   length of data chunk minus 8 byte header
60 - (wDataLength + 59)     the data
(+ a padding byte if wDataLength is odd) 


note that header contains (up to) 3 separate ways of describing the
length of the file, all derived here from the number of (input)
samples wav->numSamples in a way that is non-trivial for the blocked 
and padded compressed formats:

wRiffLength -      (riff header) the length of the file, minus 8 
wSamplesWritten  -  (fact header) the number of samples written (after padding
                   to a complete block eg for GSM)
wDataLength     -   (data chunk header) the number of (valid) data bytes written

*/

void wavwritehdr(ft, second_header) 
ft_t ft;
int second_header;
{
	wav_t	wav = (wav_t) ft->priv;

	/* variables written to wav file header */
	/* RIFF header */    
	ULONG wRiffLength ;                 /* length of file after 8 byte riff header */
	/* fmt chunk */
	ULONG wFmtSize = 16;                /* size field of the fmt chunk */
	unsigned short wFormatTag = 0;      /* data format */
	unsigned short wChannels;           /* number of channels */
	ULONG  wSamplesPerSecond;           /* samples per second per channel*/
	ULONG  wAvgBytesPerSec=0;           /* estimate of bytes per second needed */
	unsigned short wBlockAlign=0;       /* byte alignment of a basic sample block */
	unsigned short wBitsPerSample=0;    /* bits per sample */
	/* fmt chunk extension (not PCM) */
	unsigned short wExtSize=0;          /* extra bytes in the format extension */
	unsigned short wSamplesPerBlock;    /* samples per channel per block */
	/* wSamplesPerBlock and other things may go into format extension */

	/* fact chunk (not PCM) */
	ULONG wFactSize=4;		/* length of the fact chunk */
	ULONG wSamplesWritten=0;	/* windows doesnt seem to use this*/

	/* data chunk */
	ULONG  wDataLength=0x7ffff000L;	/* length of sound data in bytes */
	/* end of variables written to header */

	/* internal variables, intermediate values etc */
	ULONG bytespersample; 		/* (uncompressed) bytes per sample (per channel) */
	ULONG blocksWritten = 0;

	if (ft->info.style != ADPCM &&
	    ft->info.style != IMA_ADPCM &&
	    ft->info.style != GSM
	   )
		rawstartwrite(ft);

	wSamplesPerSecond = ft->info.rate;
	wChannels = ft->info.channels;

	if (wChannels == 0 || wChannels>64) /* FIXME: arbitrary upper limit */
	    fail("Channels(%d) out-of-range\n",wChannels);

	switch (ft->info.size)
	{
		case BYTE:
		        wBitsPerSample = 8;
			if (ft->info.style != UNSIGNED &&
			    ft->info.style != ULAW &&
			    ft->info.style != ALAW)
			{
				warn("Only support unsigned, ulaw, or alaw with 8-bit data.  Forcing to unsigned");
				ft->info.style = UNSIGNED;
			}
			break;
		case WORD:
			wBitsPerSample = 16;
			if ((ft->info.style == UNSIGNED ||
			     ft->info.style == ULAW ||
			     ft->info.style == ALAW) &&
			    !second_header)
			{
				warn("Do not support Unsigned, ulaw, or alaw with 16 bit data.  Forcing to Signed");
				ft->info.style = SIGN2;
			}
			break;
		case DWORD:
			wBitsPerSample = 32;
			break;
		default:
			wBitsPerSample = 32;
			break;
	}

	bytespersample = WORD;	/* common default */
	wSamplesPerBlock = 1;	/* common default */

	switch (ft->info.style)
	{
		case UNSIGNED:
		case SIGN2:
			wFormatTag = WAVE_FORMAT_PCM;
	    		bytespersample = (wBitsPerSample + 7)/8;
	    		wBlockAlign = wChannels * bytespersample;
			break;
		case ALAW:
			wFormatTag = WAVE_FORMAT_ALAW;
	    		bytespersample = BYTE;
	    		wBlockAlign = wChannels;
			break;
		case ULAW:
			wFormatTag = WAVE_FORMAT_MULAW;
	    		bytespersample = BYTE;
	    		wBlockAlign = wChannels;
			break;
		case IMA_ADPCM:
			/* warn("Experimental support writing IMA_ADPCM style.\n"); */
			if (wChannels>16)
			    fail("Channels(%d) must be <= 16\n",wChannels);
			wFormatTag = WAVE_FORMAT_IMA_ADPCM;
			wBlockAlign = wChannels * 64; /* reasonable default */
			wBitsPerSample = 4;
	    		wExtSize = 2;
			wSamplesPerBlock = ImaSamplesIn(0, wChannels, wBlockAlign, 0);
			break;
		case ADPCM:
			/* warn("Experimental support writing ADPCM style.\n"); */
			if (wChannels>16)
			    fail("Channels(%d) must be <= 16\n",wChannels);
			wFormatTag = WAVE_FORMAT_ADPCM;
			wBlockAlign = wChannels * 128; /* reasonable default */
			wBitsPerSample = 4;
	    		wExtSize = 4+4*7;      /* Ext fmt data length */
			wSamplesPerBlock = AdpcmSamplesIn(0, wChannels, wBlockAlign, 0);
			break;
		case GSM:
#ifdef HAVE_LIBGSM
		    if (wChannels!=1)
			fail("Channels(%d) must be == 1\n",wChannels);
		    wFormatTag = WAVE_FORMAT_GSM610;
		    /* wAvgBytesPerSec = 1625*(wSamplesPerSecond/8000.)+0.5; */
		    wBlockAlign=65;
		    wBitsPerSample=0;  /* not representable as int   */
		    wExtSize=2;        /* length of format extension */
		    wSamplesPerBlock = 320;
#else
		    fail("sorry, no GSM6.10 support, recompile sox with gsm library");
#endif
		    break;
	}
	wav->formatTag = wFormatTag;
	wav->blockAlign = wBlockAlign;
	wav->samplesPerBlock = wSamplesPerBlock;

	if (!second_header) { 	/* adjust for blockAlign */
	    blocksWritten = wDataLength/wBlockAlign;
	    wDataLength = blocksWritten * wBlockAlign;
	    wSamplesWritten = blocksWritten * wSamplesPerBlock;
	} else { 	/* fixup with real length */
	    wSamplesWritten = wav->numSamples;
	    switch(wFormatTag)
		{
	    	case WAVE_FORMAT_ADPCM:
	    	case WAVE_FORMAT_IMA_ADPCM:
		    wDataLength = wav->dataLength;
		    break;
#ifdef HAVE_LIBGSM
		case WAVE_FORMAT_GSM610:
		    /* intentional case fallthrough! */
#endif
		default:
		    wSamplesWritten /= wChannels; /* because how rawwrite()'s work */
		    blocksWritten = (wSamplesWritten+wSamplesPerBlock-1)/wSamplesPerBlock;
		    wDataLength = blocksWritten * wBlockAlign;
		}
	}

#ifdef HAVE_LIBGSM
	if (wFormatTag == WAVE_FORMAT_GSM610)
	    wDataLength = (wDataLength+1) & ~1; /*round up to even */
#endif

	if (wFormatTag != WAVE_FORMAT_PCM)
	    wFmtSize += 2+wExtSize; /* plus ExtData */

	wRiffLength = 4 + (8+wFmtSize) + (8+wDataLength); 
	if (wFormatTag != WAVE_FORMAT_PCM) /* PCM omits the "fact" chunk */
	    wRiffLength += (8+wFactSize);
	
	/* wAvgBytesPerSec <-- this is BEFORE compression, isn't it? guess not. */
	wAvgBytesPerSec = (double)wBlockAlign*ft->info.rate / (double)wSamplesPerBlock + 0.5;

	/* figured out header info, so write it */
	fputs("RIFF", ft->fp);
	wlong(ft, wRiffLength);
	fputs("WAVE", ft->fp);
	fputs("fmt ", ft->fp);
	wlong(ft, wFmtSize);
	wshort(ft, wFormatTag);
	wshort(ft, wChannels);
	wlong(ft, wSamplesPerSecond);
	wlong(ft, wAvgBytesPerSec);
	wshort(ft, wBlockAlign);
	wshort(ft, wBitsPerSample); /* end info common to all fmts */

	/* if not PCM, we need to write out wExtSize even if wExtSize=0 */
	if (wFormatTag != WAVE_FORMAT_PCM)
	    wshort(ft,wExtSize);

	switch (wFormatTag)
	{
	int i;
	case WAVE_FORMAT_IMA_ADPCM:
	    wshort(ft, wSamplesPerBlock);
	    break;
	case WAVE_FORMAT_ADPCM:
	    wshort(ft, wSamplesPerBlock);
	    wshort(ft, 7); /* nCoefs */
	    for (i=0; i<7; i++) {
	      wshort(ft, iCoef[i][0]);
	      wshort(ft, iCoef[i][1]);
	    }
	    break;
#ifdef HAVE_LIBGSM
	case WAVE_FORMAT_GSM610:
	    wshort(ft, wSamplesPerBlock);
	    break;
#endif
	default:
	}

	/* if not PCM, write the 'fact' chunk */
	if (wFormatTag != WAVE_FORMAT_PCM){
	    fputs("fact", ft->fp);
	    wlong(ft,wFactSize); 
	    wlong(ft,wSamplesWritten);
	}

	fputs("data", ft->fp);
	wlong(ft, wDataLength);		/* data chunk size */

	if (!second_header) {
		report("Writing Wave file: %s format, %d channel%s, %d samp/sec",
	        	wav_format_str(wFormatTag), wChannels,
	        	wChannels == 1 ? "" : "s", wSamplesPerSecond);
		report("        %d byte/sec, %d block align, %d bits/samp",
	                wAvgBytesPerSec, wBlockAlign, wBitsPerSample);
	} else {
		report("Finished writing Wave file, %u data bytes %u samples\n",
			wDataLength,wav->numSamples);
#ifdef HAVE_LIBGSM
		if (wFormatTag == WAVE_FORMAT_GSM610){
		    report("GSM6.10 format: %u blocks %u padded samples %u padded data bytes\n",
			blocksWritten, wSamplesWritten, wDataLength);
		    if (wav->gsmbytecount != wDataLength)
			warn("help ! internal inconsistency - data_written %u gsmbytecount %u",
				wDataLength, wav->gsmbytecount);

		}
#endif
	}
}


void wavwrite(ft, buf, len) 
ft_t ft;
LONG *buf, len;
{
	wav_t	wav = (wav_t) ft->priv;

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
		   *p++ = ((*buf++) + 0x8000) >> 16;

		wav->samplePtr = p;
		if (p == wav->sampleTop)
		    xxxAdpcmWriteBlock(ft);

	    }
	    break;

#ifdef HAVE_LIBGSM
	case WAVE_FORMAT_GSM610:
	    wav->numSamples += len;
	    wavgsmwrite(ft, buf, len);
	    break;
#endif
	default:
	    wav->numSamples += len; /* must later be divided by wChannels */
	    rawwrite(ft, buf, len);
	}
}

void wavstopwrite(ft) 
ft_t ft;
{
	wav_t	wav = (wav_t) ft->priv;
	/* Call this to flush out any remaining data. */
	switch (wav->formatTag)
	{
	case WAVE_FORMAT_IMA_ADPCM:
	case WAVE_FORMAT_ADPCM:
	    xxxAdpcmWriteBlock(ft);
	    break;
#ifdef HAVE_LIBGSM
	case WAVE_FORMAT_GSM610:
	    wavgsmstopwrite(ft);
	    break;
#endif
	default:
	    rawstopwrite(ft);
	}
	if (wav->packet) free(wav->packet);
 	if (wav->samples) free(wav->samples);
 	if (wav->iCoefs) free(wav->iCoefs);

	/* All samples are already written out. */
	/* If file header needs fixing up, for example it needs the */
 	/* the number of samples in a field, seek back and write them here. */
	if (!ft->seekable)
		return;
	if (fseek(ft->fp, 0L, SEEK_SET) != 0)
		fail("Sorry, can't rewind output file to rewrite .wav header.");
	wavwritehdr(ft, 1);
}

/*
 * Return a string corresponding to the wave format type.
 */
static char *
wav_format_str(wFormatTag) 
unsigned wFormatTag;
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
