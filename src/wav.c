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
 *   DVI) ADPCM format for wav files.  Info on these formats
 *   was taken from the xanim project, written by
 *   Mark Podlipec (podlipec@ici.net).  For those pieces of code,
 *   the following copyrights notice applies:
 *
 *    XAnim Copyright (C) 1990-1997 by Mark Podlipec.
 *    All rights reserved.
 * 
 *    This software may be freely copied, modified and redistributed without
 *    fee for non-commerical purposes provided that this copyright notice is
 *    preserved intact on all copies and modified copies.
 * 
 *    There is no warranty or other guarantee of fitness of this software.
 *    It is provided solely "as is". The author(s) disclaim(s) all
 *    responsibility and liability with respect to this software's usage
 *    or its effect upon hardware or computer systems.
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
 */

#include <string.h>		/* Included for strncmp */
#include <stdlib.h>		/* Included for malloc and free */
#include "st.h"
#include "wav.h"

/* Private data for .wav file */
typedef struct wavstuff {
    LONG	   numSamples;
    int		   second_header;  /* non-zero on second header write */
    unsigned short formatTag;	   /* What type of encoding file is using */
    
    /* The following are only needed for ADPCM wav files */
    unsigned short samplesPerBlock;
    unsigned short bytesPerBlock;
    unsigned short blockAlign;
    short	  *samples[2];	    /* Left and Right sample buffers */
    short	  *samplePtr[2];    /* Pointers to current samples */
    unsigned short blockSamplesRemaining;/* Samples remaining in each channel */    
    unsigned char *packet;	    /* Temporary buffer for packets */
} *wav_t;

static char *wav_format_str();

LONG rawread(P3(ft_t, LONG *, LONG));
void rawwrite(P3(ft_t, LONG *, LONG));
void wavwritehdr(P1(ft_t));


/*
 *
 * Lookup tables for MS ADPCM format
 *
 */

static LONG gaiP4[]    = { 230, 230, 230, 230, 307, 409, 512, 614,
			   768, 614, 512, 409, 307, 230, 230, 230 };

/* TODO : The first 7 coef's are are always hardcode and must
   appear in the actual WAVE file.  They should be read in
   in case a sound program added extras to the list. */

static LONG gaiCoef1[] = { 256, 512, 0, 192, 240, 460,  392 };
static LONG gaiCoef2[] = { 0, -256,  0,  64,   0,-208, -232};

/*
 *
 * Lookup tables for IMA ADPCM format
 *
 */
static int imaIndexAdjustTable[16] = {
   -1, -1, -1, -1,  /* +0 - +3, decrease the step size */
    2, 4, 6, 8,     /* +4 - +7, increase the step size */
   -1, -1, -1, -1,  /* -0 - -3, decrease the step size */
    2, 4, 6, 8,     /* -4 - -7, increase the step size */
};

static int imaStepSizeTable[89] = {
   7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34,
   37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
   157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494,
   544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552,
   1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026,
   4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
   11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
   27086, 29794, 32767
};

/****************************************************************************/
/* IMA ADPCM Support Functions Section                                      */
/****************************************************************************/

/*
 *
 * MsAdpcmDecode - Decode a given sample and update state tables
 *
 */

short ImaAdpcmDecode(deltaCode, state) 
unsigned char deltaCode;
ImaState_t *state;
{
    /* Get the current step size */
   int step;
   int difference;

   step = imaStepSizeTable[state->index];
   
   /* Construct the difference by scaling the current step size */
   /* This is approximately: difference = (deltaCode+.5)*step/4 */
   difference = step>>3;
   if ( deltaCode & 1 ) difference += step>>2;
   if ( deltaCode & 2 ) difference += step>>1;
   if ( deltaCode & 4 ) difference += step;

   if ( deltaCode & 8 ) difference = -difference;

   /* Build the new sample */
   state->previousValue += difference;

   if (state->previousValue > 32767) state->previousValue = 32767;
   else if (state->previousValue < -32768) state->previousValue = -32768;

   /* Update the step for the next sample */
   state->index += imaIndexAdjustTable[deltaCode];
   if (state->index < 0) state->index = 0;
   else if (state->index > 88) state->index = 88;

   return state->previousValue;

}

/*
 *
 * ImaAdpcmNextBlock - Grab and decode complete block of samples
 *
 */
unsigned short  ImaAdpcmNextBlock(ft)
ft_t ft;    
{
    wav_t	wav = (wav_t) ft->priv;
    
    /* Pull in the packet and check the header */
    unsigned short bytesRead;
    unsigned char *bytePtr;

    ImaState_t state[2];  /* One decompressor state for each channel */
    int ch;
    unsigned short remaining;
    unsigned short samplesThisBlock;

    int i;
    unsigned char b;

    bytesRead = fread(wav->packet,1,wav->blockAlign,ft->fp);
    if (bytesRead < wav->blockAlign) 
    { 
	/* If it looks like a valid header is around then try and */
	/* work with partial blocks.  Specs say it should be null */
	/* padded but I guess this is better then trailing quite. */
	if (bytesRead >= (4 * ft->info.channels))
	{
	    samplesThisBlock = (wav->blockAlign - (3 * ft->info.channels));
	}
	else
	{
	    warn ("Premature EOF on .wav input file");
	    return 0;
	}
    }
    else
	samplesThisBlock = wav->samplesPerBlock;
    
    bytePtr = wav->packet;

    /* Read the four-byte header for each channel */

    /* Reset the decompressor */
    for(ch=0;ch < ft->info.channels; ch++) {
       
	/* Got this from xanim */

	state[ch].previousValue = ((int)bytePtr[1]<<8) +
	    (int)bytePtr[0];
	if (state[ch].previousValue & 0x8000)
	    state[ch].previousValue -= 0x10000;

	if (bytePtr[2] > 88)
	{
	    warn("IMA ADPCM Format Error (bad index value) in wav file");
	    state[ch].index = 88;
	}
	else
	    state[ch].index = bytePtr[2];
	
	if (bytePtr[3])
	    warn("IMA ADPCM Format Error (synchronization error) in wav file");
	
	bytePtr+=4; /* Skip this header */

	wav->samplePtr[ch] = wav->samples[ch];
	/* Decode one sample for the header */
	*(wav->samplePtr[ch]++) = state[ch].previousValue;
    }

    /* Decompress nybbles. Remainging is bytes in block minus header  */
    /* Subtract the one sample taken from header */
    remaining = samplesThisBlock-1;
    
    while (remaining) {
	/* Always decode 8 samples */
	remaining -= 8;
	/* Decode 8 left samples */
	for (i=0;i<4;i++) {
	    b = *bytePtr++;
	    *(wav->samplePtr[0]++) = ImaAdpcmDecode(b & 0x0f,&state[0]);
	    *(wav->samplePtr[0]++) = ImaAdpcmDecode((b>>4) & 0x0f,&state[0]);
	}
	if (ft->info.channels < 2)
	    continue; /* If mono, skip rest of loop */
	/* Decode 8 right samples */
	for (i=0;i<4;i++) {
	    b = *bytePtr++;
	    *(wav->samplePtr[1]++) = ImaAdpcmDecode(b & 0x0f,&state[1]);
	    *(wav->samplePtr[1]++) = ImaAdpcmDecode((b>>4) & 0x0f,&state[1]);
	}
    }
    /* For a full block, the following should be true: */
    /* wav->samplesPerBlock = blockAlign - 8byte header + 1 sample in header */
    return wav->samplesPerBlock;
}     

/****************************************************************************/
/* MS ADPCM Support Functions Section                                       */
/****************************************************************************/

/*
 *
 * MsAdpcmDecode - Decode a given sample and update state tables
 *
 */

LONG MsAdpcmDecode(deltaCode, state) 
LONG deltaCode;
MsState_t *state;
{
    LONG predict;
    LONG sample;
    LONG idelta;

    /** Compute next Adaptive Scale Factor (ASF) **/
    idelta = state->index;
    state->index = (gaiP4[deltaCode] * idelta) >> 8;
    if (state->index < 16) state->index = 16;
    if (deltaCode & 0x08) deltaCode = deltaCode - 0x10;
    
    /** Predict next sample **/
    predict = ((state->sample1 * gaiCoef1[state->bpred]) + (state->sample2 * gaiCoef2[state->bpred])) >> 8;
    /** reconstruct original PCM **/
    sample = (deltaCode * idelta) + predict;
    
    if (sample > 32767) sample = 32767;
    else if (sample < -32768) sample = -32768;
    
    state->sample2 = state->sample1;
    state->sample1 = sample;
    
    return (sample);
}
    

/*
 *
 * MsAdpcmNextBlock - Grab and decode complete block of samples
 *
 */
unsigned short  MsAdpcmNextBlock(ft)
ft_t ft;    
{
    wav_t	wav = (wav_t) ft->priv;
    
    unsigned short bytesRead;
    unsigned char *bytePtr;

    MsState_t state[2];  /* One decompressor state for each channel */
    unsigned short samplesThisBlock;
    unsigned short remaining;

    unsigned char b;

    /* Pull in the packet and check the header */
    bytesRead = fread(wav->packet,1,wav->blockAlign,ft->fp);
    if (bytesRead < wav->blockAlign) 
    {
	/* If it looks like a valid header is around then try and */
	/* work with partial blocks.  Specs say it should be null */
	/* padded but I guess this is better then trailing quite. */
	if (bytesRead >= (7 * ft->info.channels))
	{
	    samplesThisBlock = (wav->blockAlign - (6 * ft->info.channels));
	}
	else
	{
	    warn ("Premature EOF on .wav input file");
	    return 0;
	}
    }
    else
	samplesThisBlock = wav->samplesPerBlock;
    
    bytePtr = wav->packet;

    /* Read the four-byte header for each channel */

    /* Reset the decompressor */
    state[0].bpred = *bytePtr++;	/* Left */
    if (ft->info.channels > 1)
	state[1].bpred = *bytePtr++;	/* Right */
    else
	state[1].bpred = 0;

    /* 7 should be variable from AVI/WAV header */
    if (state[0].bpred >= 7)
    {
	warn("MSADPCM bpred %x and should be less than 7\n",state[0].bpred);
	return(0);
    }
    if (state[1].bpred >= 7)
    {
	warn("MSADPCM bpred %x and should be less than 7\n",state[1].bpred);
	return(0);
    }
	
    state[0].index = *bytePtr++;  state[0].index |= (*bytePtr++)<<8;
    if (state[0].index & 0x8000) state[0].index -= 0x10000;
    if (ft->info.channels > 1)
    {
	state[1].index = *bytePtr++;  state[1].index |= (*bytePtr++)<<8;
	if (state[1].index & 0x8000) state[1].index -= 0x10000;
    }

    state[0].sample1 = *bytePtr++;  state[0].sample1 |= (*bytePtr++)<<8;
    if (state[0].sample1 & 0x8000) state[0].sample1 -= 0x10000;
    if (ft->info.channels > 1)
    {
	state[1].sample1 = *bytePtr++;  state[1].sample1 |= (*bytePtr++)<<8;
	if (state[1].sample1 & 0x8000) state[1].sample1 -= 0x10000;
    }

    state[0].sample2 = *bytePtr++;  state[0].sample2 |= (*bytePtr++)<<8;
    if (state[0].sample2 & 0x8000) state[0].sample2 -= 0x10000;
    if (ft->info.channels > 1)
    {
	state[1].sample2 = *bytePtr++;  state[1].sample2 |= (*bytePtr++)<<8;
	if (state[1].sample2 & 0x8000) state[1].sample2 -= 0x10000;
    }

    wav->samplePtr[0] = wav->samples[0];
    wav->samplePtr[1] = wav->samples[1];
    
    /* Decode two samples for the header */
    *(wav->samplePtr[0]++) = state[0].sample2;
    *(wav->samplePtr[0]++) = state[0].sample1;
    if (ft->info.channels > 1)
    {
	*(wav->samplePtr[1]++) = state[1].sample2;
	*(wav->samplePtr[1]++) = state[1].sample1;
    }

    /* Decompress nybbles.  Minus 2 included in header */
    remaining = samplesThisBlock-2;

    while (remaining) {
	b = *bytePtr++;
	*(wav->samplePtr[0]++) = MsAdpcmDecode((b>>4) & 0x0f, &state[0]);
	remaining--;
	if (ft->info.channels == 1)
	{	    
	    *(wav->samplePtr[0]++) = MsAdpcmDecode(b & 0x0f, &state[0]);
	    remaining--;
	}
	else
	{
	    *(wav->samplePtr[1]++) = MsAdpcmDecode(b & 0x0f, &state[1]);
	}
    }
    return samplesThisBlock;
}

/****************************************************************************/
/* General Sox WAV file code                                                */
/****************************************************************************/

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
    unsigned short wChannels;	    /* number of channels */
    ULONG    wSamplesPerSecond;     /* samples per second per channel */
    ULONG    wAvgBytesPerSec;	    /* estimate of bytes per second needed */
    unsigned short wBitsPerSample;  /* bits per sample */
    unsigned short wExtSize = 0;    /* extended field for ADPCM */
    unsigned short wNumCoefs = 0;   /* Related to IMA ADPCM */
	
    ULONG    data_length;	    /* length of sound data in bytes */
    ULONG    bytespersample;	    /* bytes per sample (per channel */

    /* This is needed for rawread() */
    rawstartread(ft);

    endptr = (char *) &littlendian;
    if (!*endptr) ft->swap = ft->swap ? 0 : 1;

    /* If you need to seek around the input file. */
    if (0 && ! ft->seekable)
	fail("WAVE input file must be a file, not a pipe");

    if ( fread(magic, 1, 4, ft->fp) != 4 || strncmp("RIFF", magic, 4))
	fail("WAVE: RIFF header not found");

    len = rlong(ft);

    if ( fread(magic, 1, 4, ft->fp) != 4 || strncmp("WAVE", magic, 4))
	fail("WAVE header not found");

    /* Now look for the format chunk */
    for (;;)
    {
	if ( fread(magic, 1, 4, ft->fp) != 4 )
	    fail("WAVE file missing fmt spec");
	len = rlong(ft);
	if (strncmp("fmt ", magic, 4) == 0)
	    break;				/* Found the format chunk */

	/* skip to next chunk */	
	while (len > 0 && !feof(ft->fp))
	{
	    getc(ft->fp);
	    len--;
	}
    }

    if ( len < 16 )
	fail("WAVE file fmt chunk is too short");

    wav->formatTag = rshort(ft);
    len -= 2;
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
	
    case WAVE_FORMAT_ADPCM:
    case WAVE_FORMAT_IMA_ADPCM:
	if (ft->info.style == -1 || ft->info.style == ADPCM)
	    ft->info.style = ADPCM;
	else
	    warn("User options overriding style read in .wav header");
	break;
	
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
    case IBM_FORMAT_MULAW:
	fail("Sorry, this WAV file is in IBM U-law format.");
    case IBM_FORMAT_ALAW:
	fail("Sorry, this WAV file is in IBM A-law format.");
    case IBM_FORMAT_ADPCM:
	fail("Sorry, this WAV file is in IBM ADPCM format.");
    default:	fail("WAV file has unknown format type of %x",wav->formatTag);
    }

    wChannels = rshort(ft);
    len -= 2;
    /* User options take precedence */
    if (ft->info.channels == -1 || ft->info.channels == wChannels)
	ft->info.channels = wChannels;
    else
	warn("User options overriding channels read in .wav header");
	
    wSamplesPerSecond = rlong(ft);
    len -= 4;
    if (ft->info.rate == 0 || ft->info.rate == wSamplesPerSecond)
	ft->info.rate = wSamplesPerSecond;
    else
	warn("User options overriding rate read in .wav header");
    
    wAvgBytesPerSec = rlong(ft);	/* Average bytes/second */
    wav->blockAlign = rshort(ft);	/* Block align */
    len -= 6;

    /* bits per sample per channel */	
    wBitsPerSample =  rshort(ft);
    len -= 2;

    /* ADPCM formats have extended fmt chunk.  Check for those cases. */
    if (wav->formatTag == WAVE_FORMAT_ADPCM)
    {
	if (wBitsPerSample != 4)
	    fail("Can only handle 4-bit MS ADPCM in wav files");

	wExtSize = rshort(ft);
	wav->samplesPerBlock = rshort(ft);
	wav->bytesPerBlock = (wav->samplesPerBlock + 7)/2 * ft->info.channels;
	wNumCoefs = rshort(ft);
	wav->packet = (unsigned char *)malloc(wav->blockAlign);
	len -= 6;
	    
	wav->samples[1] = wav->samples[0] = 0;
	/* Use ft->info.channels after this becuase wChannels is now bad */
	while (wChannels-- > 0)
	    wav->samples[wChannels] = (short *)malloc(wav->samplesPerBlock*sizeof(short));
	/* Here we are setting the bytespersample AFTER de-compression */
	bytespersample = WORD;
    }
    else if (wav->formatTag == WAVE_FORMAT_IMA_ADPCM)
    {
	if (wBitsPerSample != 4)
	    fail("Can only handle 4-bit IMA ADPCM in wav files");

	wExtSize = rshort(ft);
	wav->samplesPerBlock = rshort(ft);
	wav->bytesPerBlock = (wav->samplesPerBlock + 7)/2 * ft->info.channels;
	wav->packet = (unsigned char *)malloc(wav->blockAlign);
	len -= 4;
	    
	wav->samples[1] = wav->samples[0] = 0;
	/* Use ft->info.channels after this becuase wChannels is now bad */
	while (wChannels-- > 0)
	    wav->samples[wChannels] = (short *)malloc(wav->samplesPerBlock*sizeof(short));
	/* Here we are setting the bytespersample AFTER de-compression */
	bytespersample = WORD;
    }
    else
    {
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

    /* Skip past the rest of any left over fmt chunk */
    while (len > 0 && !feof(ft->fp))
    {
	getc(ft->fp);
	len--;
    }

    /* Now look for the wave data chunk */
    for (;;)
    {
	if ( fread(magic, 1, 4, ft->fp) != 4 )
	    fail("WAVE file has missing data chunk");
	len = rlong(ft);
	if (strncmp("data", magic, 4) == 0)
	    break;				/* Found the data chunk */
	
	while (len > 0 && !feof(ft->fp)) 	/* skip to next chunk */
	{
	    getc(ft->fp);
	    len--;
	}
    }
    
    data_length = len;
    if (wav->formatTag == WAVE_FORMAT_ADPCM)
    {
	/* Compute easiest part of number of samples.  For every block, there
	   are samplesPerBlock samples to read. */
	wav->numSamples = (((data_length / wav->blockAlign) * wav->samplesPerBlock) * ft->info.channels);
	/* Next, for any partial blocks, substract overhead from it and it
	   will leave # of samples to read. */
	wav->numSamples += ((data_length - ((data_length/wav->blockAlign)
					    *wav->blockAlign))
			    - (6 * ft->info.channels)) * ft->info.channels;
	wav->blockSamplesRemaining = 0;	       /* Samples left in buffer */
    }
    else if (wav->formatTag == WAVE_FORMAT_IMA_ADPCM)
    {
	/* Compute easiest part of number of samples.  For every block, there
	   are samplesPerBlock samples to read. */
	wav->numSamples = (((data_length / wav->blockAlign) * wav->samplesPerBlock) * ft->info.channels);
	/* Next, for any partial blocks, substract overhead from it and it
	   will leave # of samples to read. */
	wav->numSamples += ((data_length - ((data_length/wav->blockAlign)
					    *wav->blockAlign))
			    - (3 * ft->info.channels)) * ft->info.channels;
	wav->blockSamplesRemaining = 0;	       /* Samples left in buffer */
    }
    else
	wav->numSamples = data_length/ft->info.size;	/* total samples */

    report("Reading Wave file: %s format, %d channel%s, %d samp/sec",
	   wav_format_str(wav->formatTag), ft->info.channels,
	   wChannels == 1 ? "" : "s", wSamplesPerSecond);
    report("        %d byte/sec, %d block align, %d bits/samp, %u data bytes",
	   wAvgBytesPerSec, wav->blockAlign, wBitsPerSample, data_length);

    /* Can also report exteded fmt information */
    if (wav->formatTag == WAVE_FORMAT_ADPCM)
	report("        %d Extsize, %d Samps/block, %d bytes/block %d Num Coefs\n",wExtSize,wav->samplesPerBlock,wav->bytesPerBlock,wNumCoefs);
    else if (wav->formatTag == WAVE_FORMAT_IMA_ADPCM)
	report("        %d Extsize, %d Samps/block, %d bytes/block\n",wExtSize,wav->samplesPerBlock,wav->bytesPerBlock);
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
	if (ft->info.style == ADPCM)
	{
	    done = 0;
	    while (done < len) { /* Still want data? */
		/* See if need to read more from disk */
		if (wav->blockSamplesRemaining == 0) { 
		    if (wav->formatTag == WAVE_FORMAT_IMA_ADPCM)
			wav->blockSamplesRemaining = ImaAdpcmNextBlock(ft);
		    else
			wav->blockSamplesRemaining = MsAdpcmNextBlock(ft);
		    if (wav->blockSamplesRemaining == 0)
		    {
			/* Don't try to read any more samples */
			wav->numSamples = 0;
			return done;
		    }
		    wav->samplePtr[0] = wav->samples[0];
		    wav->samplePtr[1] = wav->samples[1];
		}

		switch(ft->info.channels) { /* Copy data into buf */
		case 1: /* Mono: Just copy left channel data */
		    while ((wav->blockSamplesRemaining > 0) && (done < len))
		    {
			/* Output is already signed */
			*buf++ = LEFT(*(wav->samplePtr[0]++), 16);
			done++;
			wav->blockSamplesRemaining--;
		    }
		    break;
		case 2: /* Stereo: Interleave samples */
		    while ((wav->blockSamplesRemaining > 0) && (done < len))
		    {
			/* Output is already signed */
			*buf++ = LEFT(*(wav->samplePtr[0]++),16); /* Left */
			*buf++ = LEFT(*(wav->samplePtr[1]++),16); /* Right */
			done += 2;
			wav->blockSamplesRemaining--;
		    }
		    break;
		default:
		    fail ("Can only handle stereo or mono files");
		}
	    }
	}
	else /* else not ADPCM style */
	{
	    done = rawread(ft, buf, len);
	    /* If software thinks there are more samples but I/O */
	    /* says otherwise, let the user no about this.       */
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
    if (wav->samples[0]) free(wav->samples[0]);
    if (wav->samples[1]) free(wav->samples[1]);

    /* Needed for rawread() */
    rawstopread(ft);
}

void wavstartwrite(ft) 
ft_t ft;
{
	wav_t	wav = (wav_t) ft->priv;
	int	littlendian = 1;
	char	*endptr;

	endptr = (char *) &littlendian;
	if (!*endptr) ft->swap = ft->swap ? 0 : 1;

	wav->numSamples = 0;
	wav->second_header = 0;
	if (! ft->seekable)
		warn("Length in output .wav header will wrong since can't seek to fix it");
	wavwritehdr(ft);
}

void wavwritehdr(ft) 
ft_t ft;
{
	wav_t	wav = (wav_t) ft->priv;

        /* wave file characteristics */
        unsigned short wFormatTag = 0;          /* data format */
        unsigned short wChannels;               /* number of channels */
        ULONG  wSamplesPerSecond;       	/* samples per second per channel */
        ULONG  wAvgBytesPerSec;        		 /* estimate of bytes per second needed */
        unsigned short wBlockAlign;             /* byte alignment of a basic sample block */
        unsigned short wBitsPerSample;          /* bits per sample */
        ULONG  data_length;             	/* length of sound data in bytes */
	ULONG  bytespersample; 			/* bytes per sample (per channel) */

	/* Needed for rawwrite() */
	rawstartwrite(ft);

	switch (ft->info.size)
	{
		case BYTE:
		        wBitsPerSample = 8;
			break;
		case WORD:
			wBitsPerSample = 16;
			break;
		case DWORD:
			wBitsPerSample = 32;
			break;
		default:
			wBitsPerSample = 32;
			break;
	}

	switch (ft->info.style)
	{
		case UNSIGNED:
			wFormatTag = WAVE_FORMAT_PCM;
			if (wBitsPerSample != 8 && !wav->second_header)
				warn("Warning - writing bad .wav file using unsigned data and %d bits/sample",wBitsPerSample);
			break;
		case SIGN2:
			wFormatTag = WAVE_FORMAT_PCM;
			if (wBitsPerSample == 8 && !wav->second_header)
				warn("Warning - writing bad .wav file using signed data and %d bits/sample",wBitsPerSample);
			break;
		case ALAW:
			wFormatTag = WAVE_FORMAT_ALAW;
			if (wBitsPerSample != 8 && !wav->second_header)
				warn("Warning - writing bad .wav file using A-law data and %d bits/sample",wBitsPerSample);
			break;
		case ULAW:
			wFormatTag = WAVE_FORMAT_MULAW;
			if (wBitsPerSample != 8 && !wav->second_header)
				warn("Warning - writing bad .wav file using U-law data and %d bits/sample",wBitsPerSample);
			break;
		case ADPCM:
			wFormatTag = WAVE_FORMAT_PCM;
		        warn("Can not support writing ADPCM style. Overriding to Signed Words\n");
			ft->info.style = SIGN2;
			wBitsPerSample = 16;
			/* wFormatTag = WAVE_FORMAT_IMA_ADPCM;
			   wBitsPerSample = 4;
			if (wBitsPerSample != 4 && !wav->second_header)
			warn("Warning - writing bad .wav file using IMA ADPCM and %d bits/sample",wBitsPerSample);
			break; */
	}
	
	
	wSamplesPerSecond = ft->info.rate;
	bytespersample = (wBitsPerSample + 7)/8;
	wAvgBytesPerSec = ft->info.rate * ft->info.channels * bytespersample;
	wChannels = ft->info.channels;
	wBlockAlign = ft->info.channels * bytespersample;
	if (!wav->second_header)	/* use max length value first time */
		data_length = 0x7fffffffL - (8+16+12);
	else	/* fixup with real length */
	{
	    if (ft->info.style == ADPCM)
		data_length = wav->numSamples / 2;
	    else
		data_length = bytespersample * wav->numSamples;
	}

	/* figured out header info, so write it */
	fputs("RIFF", ft->fp);
	wlong(ft, data_length + 8+16+12);	/* Waveform chunk size: FIXUP(4) */
	fputs("WAVE", ft->fp);
	fputs("fmt ", ft->fp);
	wlong(ft, (LONG)16);		/* fmt chunk size */
	wshort(ft, wFormatTag);
	wshort(ft, wChannels);
	wlong(ft, wSamplesPerSecond);
	wlong(ft, wAvgBytesPerSec);
	wshort(ft, wBlockAlign);
	wshort(ft, wBitsPerSample);
	
	fputs("data", ft->fp);
	wlong(ft, data_length);		/* data chunk size: FIXUP(40) */

	if (!wav->second_header) {
		report("Writing Wave file: %s format, %d channel%s, %d samp/sec",
	        	wav_format_str(wFormatTag), wChannels,
	        	wChannels == 1 ? "" : "s", wSamplesPerSecond);
		report("        %d byte/sec, %d block align, %d bits/samp",
	                wAvgBytesPerSec, wBlockAlign, wBitsPerSample);
	} else
		report("Finished writing Wave file, %u data bytes\n",data_length);
}

void wavwrite(ft, buf, len) 
ft_t ft;
LONG *buf, len;
{
	wav_t	wav = (wav_t) ft->priv;

	wav->numSamples += len;
	rawwrite(ft, buf, len);
}

void
wavstopwrite(ft) 
ft_t ft;
{
	/* All samples are already written out. */
	/* If file header needs fixing up, for example it needs the */
 	/* the number of samples in a field, seek back and write them here. */
	if (!ft->seekable)
		return;
	if (fseek(ft->fp, 0L, 0) != 0)
		fail("Sorry, can't rewind output file to rewrite .wav header.");
	((wav_t) ft->priv)->second_header = 1;
	wavwritehdr(ft);

	/* Needed for rawwrite() */
	rawstopwrite(ft);
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
		case IBM_FORMAT_MULAW:
			return "IBM U-law format.";
		case IBM_FORMAT_ALAW:
			return "IBM A-law";
                case IBM_FORMAT_ADPCM:
                	return "IBM ADPCM";
		default:
			return "Unknown";
	}
}
