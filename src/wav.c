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
 * November  22, 1999 - Stan Brooks (stabro@megsinet.com)
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

/* Private data for .wav file */
typedef struct wavstuff {
    LONG	   numSamples;
    LONG	   dataLength;     /* needed for ADPCM writing */
    unsigned short formatTag;	   /* What type of encoding file is using */
    
    /* The following are only needed for ADPCM wav files */
    unsigned short samplesPerBlock;
    unsigned short bytesPerBlock;
    unsigned short blockAlign;
    unsigned short nCoefs;	    /* ADPCM: number of coef sets */
    short	  *iCoefs;	    /* ADPCM: coef sets           */
    unsigned char *packet;	    /* Temporary buffer for packets */
    short	  *samples;	    /* interleaved samples buffer */
    short	  *samplePtr;       /* Pointer to current samples */
    short	  *sampleTop;       /* End of samples-buffer      */
    unsigned short blockSamplesRemaining;/* Samples remaining in each channel */    
    /* state holds step-size info for ADPCM or IMA_ADPCM writes */
    int 	   state[16];       /* last, because maybe longer */
} *wav_t;

static char *wav_format_str();

LONG rawread(P3(ft_t, LONG *, LONG));
void rawwrite(P3(ft_t, LONG *, LONG));
void wavwritehdr(P2(ft_t, int));

/****************************************************************************/
/* IMA ADPCM Support Functions Section                                      */
/****************************************************************************/
unsigned short  ImaAdpcmReadBlock(ft)
ft_t ft;    
{
    wav_t	wav = (wav_t) ft->priv;
    int bytesRead;
    int samplesThisBlock;

    /* Pull in the packet and check the header */
    bytesRead = fread(wav->packet,1,wav->blockAlign,ft->fp);
    if (bytesRead < wav->blockAlign) 
    { 
	/* If it looks like a valid header is around then try and */
	/* work with partial blocks.  Specs say it should be null */
	/* padded but I guess this is better then trailing quiet. */
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
    
    wav->samplePtr = wav->samples;
    

    /* For a full block, the following should be true: */
    /* wav->samplesPerBlock = blockAlign - 8byte header + 1 sample in header */
    ImaBlockExpandI(ft->info.channels, wav->packet, wav->samples, samplesThisBlock);
    return samplesThisBlock;

}

static void ImaAdpcmWriteBlock(ft)
ft_t ft;
{
    wav_t wav = (wav_t) ft->priv;
    int chans, ch, ct;
    short *p;

    chans = ft->info.channels;
    p = wav->samplePtr;
    ct = p - wav->samples;
    if (ct>=chans) { 
	/* zero-fill samples if needed to complete block */
	for (p = wav->samplePtr; p < wav->sampleTop; p++) *p=0;
	/* compress the samples to wav->packet */
	for (ch=0; ch<chans; ch++)
	    ImaMashChannel(ch, chans, wav->samples, wav->samplesPerBlock, &wav->state[ch], wav->packet, 9);

	/* write the compressed packet */
	fwrite(wav->packet, wav->blockAlign, 1, ft->fp); /* FIXME: check return value */
	/* update lengths and samplePtr */
	wav->dataLength += wav->blockAlign;
	wav->numSamples += ct/chans;
	wav->samplePtr = wav->samples;
    }
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
    
    errmsg = AdpcmBlockExpandI(ft->info.channels, wav->packet, wav->samples, samplesThisBlock);

    if (errmsg)
	warn((char*)errmsg);

    return samplesThisBlock;
}

static void AdpcmWriteBlock(ft)
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

	AdpcmMashI(chans, wav->samples, wav->samplesPerBlock, wav->state, wav->packet, wav->blockAlign,9);

	/* write the compressed packet */
	fwrite(wav->packet, wav->blockAlign, 1, ft->fp); /* FIXME: check return value */
	/* update lengths and samplePtr */
	wav->dataLength += wav->blockAlign;
	wav->numSamples += ct/chans;
	wav->samplePtr = wav->samples;
    }
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
    /* we only get here if we just read the magic "fmt " */
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
	fail("Sorry, this WAV file is in GSM 6.10 format.");
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

    wav->iCoefs = NULL;
    wav->packet = NULL;
    wav->samples = NULL;

    /* ADPCM formats have extended fmt chunk.  Check for those cases. */
    switch (wav->formatTag)
    {
    case WAVE_FORMAT_ADPCM:
	if (wBitsPerSample != 4)
	    fail("Can only handle 4-bit MS ADPCM in wav files");

	wExtSize = rshort(ft);
	wav->samplesPerBlock = rshort(ft);
	wav->bytesPerBlock = ((wav->samplesPerBlock-2)*ft->info.channels + 1)/2
		             + 7*ft->info.channels;
	wav->nCoefs = rshort(ft);
	if (wav->nCoefs < 7 || wav->nCoefs > 0x100) {
	    fail("ADPCM file nCoefs (%.4hx) makes no sense\n", wav->nCoefs);
	}
	wav->packet = (unsigned char *)malloc(wav->blockAlign);
	len -= 6;

	wav->samples = (short *)malloc(wChannels*wav->samplesPerBlock*sizeof(short));

	/* SJB: will need iCoefs later for adpcm.c */
	wav->iCoefs = (short *)malloc(wav->nCoefs * 2 * sizeof(short));
	{
	    int i;
	    for (i=0; len>=2 && i < 2*wav->nCoefs; i++) {
		wav->iCoefs[i] = rshort(ft);
		/* fprintf(stderr,"iCoefs[%2d] %4d\n",i,wav->iCoefs[i]); */
		len -= 2;
	    }
	}

	bytespersample = WORD;  /* AFTER de-compression */
        break;

    case WAVE_FORMAT_IMA_ADPCM:
	if (wBitsPerSample != 4)
	    fail("Can only handle 4-bit IMA ADPCM in wav files");

	wExtSize = rshort(ft);
	wav->samplesPerBlock = rshort(ft);
	wav->bytesPerBlock = (wav->samplesPerBlock + 7)/2 * ft->info.channels;/* FIXME */
	wav->packet = (unsigned char *)malloc(wav->blockAlign);
	len -= 4;

	wav->samples = (short *)malloc(wChannels*wav->samplesPerBlock*sizeof(short));

	bytespersample = WORD;  /* AFTER de-compression */
	break;
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

    /* Skip past the rest of any left over fmt chunk */
    while (len > 0 && !feof(ft->fp))
    {
	getc(ft->fp);
	len--;
    }

    /* for ADPCM formats, there's a 'fact' chunk before
     * the upcoming 'data' chunk */

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

    switch (wav->formatTag)
    {

    case WAVE_FORMAT_ADPCM:
	/* Compute easiest part of number of samples.  For every block, there
	   are samplesPerBlock samples to read. */
	wav->numSamples = (((data_length / wav->blockAlign) * wav->samplesPerBlock) * ft->info.channels);
	/* Next, for any partial blocks, subtract overhead from it and it
	   will leave # of samples to read. */
	wav->numSamples += 
		((data_length % wav->blockAlign) - (6 * ft->info.channels)) * ft->info.channels;
	/*report("datalen %d, numSamples %d",data_length, wav->numSamples);*/
	wav->blockSamplesRemaining = 0;	       /* Samples left in buffer */
	break;

    case WAVE_FORMAT_IMA_ADPCM:
	/* Compute easiest part of number of samples.  For every block, there
	   are samplesPerBlock samples to read. */
	wav->numSamples = (((data_length / wav->blockAlign) * wav->samplesPerBlock) * ft->info.channels);
	/* Next, for any partial blocks, substract overhead from it and it
	   will leave # of samples to read. */
	wav->numSamples += ((data_length - ((data_length/wav->blockAlign)
					    *wav->blockAlign))
			    - (3 * ft->info.channels)) * ft->info.channels;
	wav->blockSamplesRemaining = 0;	       /* Samples left in buffer */
	initImaTable();
	break;

    default:
	wav->numSamples = data_length/ft->info.size;	/* total samples */

    }

    report("Reading Wave file: %s format, %d channel%s, %d samp/sec",
	   wav_format_str(wav->formatTag), ft->info.channels,
	   wChannels == 1 ? "" : "s", wSamplesPerSecond);
    report("        %d byte/sec, %d block align, %d bits/samp, %u data bytes",
	   wAvgBytesPerSec, wav->blockAlign, wBitsPerSample, data_length);

    /* Can also report extended fmt information */
    if (wav->formatTag == WAVE_FORMAT_ADPCM)
	report("        %d Extsize, %d Samps/block, %d bytes/block %d Num Coefs\n",
		wExtSize,wav->samplesPerBlock,wav->bytesPerBlock,wav->nCoefs);
    else if (wav->formatTag == WAVE_FORMAT_IMA_ADPCM)
	report("        %d Extsize, %d Samps/block, %d bytes/block\n",
		wExtSize,wav->samplesPerBlock,wav->bytesPerBlock);
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
	wav->dataLength = 0;
	if (!ft->seekable)
		warn("Length in output .wav header will be wrong since can't seek to fix it");
	wavwritehdr(ft, 0);  /* also calculates various wav->* info */
	wav->packet = NULL;
	wav->samples = NULL;
	wav->iCoefs = NULL;
	if (wav->formatTag == WAVE_FORMAT_IMA_ADPCM ||
	   wav->formatTag == WAVE_FORMAT_ADPCM)
	{
	    int ch, sbsize;
	    /* #channels already range-checked for overflow in wavwritehdr() */
	    for (ch=0; ch<ft->info.channels; ch++)
	    	wav->state[ch] = 0;
	    sbsize = ft->info.channels * wav->samplesPerBlock;
	    wav->packet = (unsigned char *)malloc(wav->blockAlign);
	    wav->samples = (short *)malloc(sbsize*sizeof(short));
	    wav->sampleTop = wav->samples + sbsize;
	    wav->samplePtr = wav->samples;
	    initImaTable();
	}
}

#define WHDRSIZ1 8 /* "RIFF",(long)len,"WAVE" */
#define FMTSIZ1 24 /* "fmt ",(long)len,... fmt chunk (without any Ext data) */
#define DATASIZ1 8 /* "data",(long)len        */
void wavwritehdr(ft, second_header) 
ft_t ft;
int second_header;
{
	wav_t	wav = (wav_t) ft->priv;
	LONG fmtsize = FMTSIZ1;
	LONG factsize = 0; /* "fact",(long)len,??? */

        /* wave file characteristics */
	unsigned short wFormatTag = 0;          /* data format */
	unsigned short wChannels;               /* number of channels */
	ULONG  wSamplesPerSecond;       	/* samples per second per channel */
	ULONG  wAvgBytesPerSec;        		/* estimate of bytes per second needed */
	unsigned short wBlockAlign;             /* byte alignment of a basic sample block */
	unsigned short wBitsPerSample;          /* bits per sample */
	ULONG  data_length;             	/* length of sound data in bytes */
	ULONG  bytespersample; 			/* bytes per sample (per channel) */

	/* Needed for rawwrite() */
	if (ft->info.style != ADPCM)
		rawstartwrite(ft);

	switch (ft->info.size)
	{
		case BYTE:
		        wBitsPerSample = 8;
			if (ft->info.style != UNSIGNED &&
			    ft->info.style != ULAW &&
			    ft->info.style != ALAW &&
			    !second_header)
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

	switch (ft->info.style)
	{
		case UNSIGNED:
			wFormatTag = WAVE_FORMAT_PCM;
			break;
		case SIGN2:
			wFormatTag = WAVE_FORMAT_PCM;
			break;
		case ALAW:
			wFormatTag = WAVE_FORMAT_ALAW;
			break;
		case ULAW:
			wFormatTag = WAVE_FORMAT_MULAW;
			break;
		case IMA_ADPCM:
			/* warn("Experimental support writing IMA_ADPCM style.\n"); */
			wFormatTag = WAVE_FORMAT_IMA_ADPCM;
			wBitsPerSample = 4;
			fmtsize += 4;  /* plus ExtData */
			factsize = 12; /* fact chunk   */
			break;
		case ADPCM:
			/* warn("Experimental support writing ADPCM style.\n"); */
			wFormatTag = WAVE_FORMAT_ADPCM;
			wBitsPerSample = 4;
			fmtsize += 2+4+4*7;  /* plus ExtData */
			factsize = 12;       /* fact chunk   */
			break;
	}
	wav->formatTag = wFormatTag;
	
	wSamplesPerSecond = ft->info.rate;
	wChannels = ft->info.channels;
	switch (wFormatTag)
	{
	case WAVE_FORMAT_IMA_ADPCM:
	    if (wChannels>16)
	    	fail("Channels(%d) must be <= 16\n",wChannels);
	    bytespersample = 2;
	    wBlockAlign = wChannels * 64; /* reasonable default */
	    break;
	case WAVE_FORMAT_ADPCM:
	    if (wChannels>16)
	    	fail("Channels(%d) must be <= 16\n",wChannels);
	    bytespersample = 2;
	    wBlockAlign = wChannels * 128; /* reasonable default */
	    break;
	default:
	    bytespersample = (wBitsPerSample + 7)/8;
	    wBlockAlign = wChannels * bytespersample;
	}
	wav->blockAlign = wBlockAlign;
	wAvgBytesPerSec = ft->info.rate * wChannels * bytespersample;
	if (!second_header)	/* use max length value first time */
		data_length = 0x7fffffffL - (WHDRSIZ1+fmtsize+factsize+DATASIZ1);
	else	/* fixup with real length */
		switch(wFormatTag)
		{
	    	case WAVE_FORMAT_ADPCM:
	    	case WAVE_FORMAT_IMA_ADPCM:
		    data_length = wav->dataLength;
		    break;
		default:
		    data_length = bytespersample * wav->numSamples;
		}

	/* figured out header info, so write it */
	fputs("RIFF", ft->fp);
	wlong(ft, data_length + WHDRSIZ1+fmtsize+factsize+DATASIZ1);/* Waveform chunk size: FIXUP(4) */
	fputs("WAVE", ft->fp);
	fputs("fmt ", ft->fp);
	wlong(ft, fmtsize-8);	/* fmt chunk size */
	wshort(ft, wFormatTag);
	wshort(ft, wChannels);
	wlong(ft, wSamplesPerSecond);
	wlong(ft, wAvgBytesPerSec);
	wshort(ft, wBlockAlign);
	wshort(ft, wBitsPerSample); /* end of info common to all fmts */
	switch (wFormatTag)
	{
	int i,nsamp;
	case WAVE_FORMAT_IMA_ADPCM:
	    wshort(ft, 2);		/* Ext fmt data length */
	    wav->samplesPerBlock = ((wBlockAlign - 4*wChannels)/(4*wChannels))*8 + 1;
	    wshort(ft, wav->samplesPerBlock);
	    fputs("fact", ft->fp);
	    wlong(ft, factsize-8);	/* fact chunk size */
	    /* use max nsamps value first time */
	    nsamp = (second_header)? wav->numSamples : 0x7fffffffL;
	    wlong(ft, nsamp);
	    break;
	case WAVE_FORMAT_ADPCM:
	    wshort(ft, 4+4*7);		/* Ext fmt data length */
	    wav->samplesPerBlock = 2*(wBlockAlign - 7*wChannels)/wChannels + 2;
	    wshort(ft, wav->samplesPerBlock);
	    wshort(ft, 7); /* nCoefs */
	    for (i=0; i<7; i++) {
	      wshort(ft, iCoef[i][0]);
	      wshort(ft, iCoef[i][1]);
	    }

	    fputs("fact", ft->fp);
	    wlong(ft, factsize-8);	/* fact chunk size */
	    /* use max nsamps value first time */
	    nsamp = (second_header)? wav->numSamples : 0x7fffffffL;
	    wlong(ft, nsamp);
	    break;
	default:
	}

	fputs("data", ft->fp);
	wlong(ft, data_length);		/* data chunk size: FIXUP(40) */

	if (!second_header) {
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
		if (p == wav->sampleTop) {
		    if (wav->formatTag==WAVE_FORMAT_IMA_ADPCM)
			ImaAdpcmWriteBlock(ft);
		    else
			AdpcmWriteBlock(ft);
		}

	    }
	    break;

	default:
	    wav->numSamples += len;
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
	    ImaAdpcmWriteBlock(ft);
	    break;
	case WAVE_FORMAT_ADPCM:
	    AdpcmWriteBlock(ft);
	    break;
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
