/*
 * MP3 support for SoX
 *
 * Uses libmad for MP3 decoding
 * and libmp3lame for MP3 encoding
 *
 * Written by Fabrizio Gennari <fabrizio.ge@tiscali.it>
 *
 * The decoding part is based on the decoder-tutorial program madlld
 * written by Bertrand Petit <madlld@phoe.fmug.org>,
 */

#include "sox_i.h"

#include <string.h>

#ifdef HAVE_LIBMAD
#include <mad.h>
#endif

#ifdef HAVE_LIBMP3LAME
#include <lame/lame.h>
#include <math.h>
#endif

#define INPUT_BUFFER_SIZE       (sox_bufsiz)

/* Private data */
struct mp3priv {
#ifdef HAVE_LIBMAD
        struct mad_stream       *Stream;
        struct mad_frame        *Frame;
        struct mad_synth        *Synth;
        mad_timer_t             *Timer;
        unsigned char           *InputBuffer;
        sox_ssize_t              cursamp;
        sox_size_t               FrameCount;
#endif /*HAVE_LIBMAD*/
#ifdef HAVE_LIBMP3LAME
        lame_global_flags       *gfp;
#endif /*HAVE_LIBMP3LAME*/
};

#ifdef HAVE_LIBMAD

/* This function merges the functions tagtype() and id3_tag_query()
   from MAD's libid3tag, so we don't have to link to it
   Returns 0 if the frame is not an ID3 tag, tag length if it is */

#define ID3_TAG_FLAG_FOOTERPRESENT 0x10

static int tagtype(const unsigned char *data, size_t length)
{
    /* TODO: It would be nice to look for Xing VBR headers
     * or TLE fields in ID3 to detect length of file
     * and set ft->length.
     * For CBR, we should fstat the file and divided
     * by bitrate to find length.
     */

    if (length >= 3 && data[0] == 'T' && data[1] == 'A' && data[2] == 'G')
    {
        return 128; /* ID3V1 */
    }

    if (length >= 10 &&
        (data[0] == 'I' && data[1] == 'D' && data[2] == '3') &&
        data[3] < 0xff && data[4] < 0xff &&
        data[6] < 0x80 && data[7] < 0x80 && data[8] < 0x80 && data[9] < 0x80)
    {     /* ID3V2 */
        unsigned char flags;
        unsigned int size;
        flags = data[5];
        size = (data[6]<<21) + (data[7]<<14) + (data[8]<<7) + data[9];
        if (flags & ID3_TAG_FLAG_FOOTERPRESENT)
            size += 10;
        return 10 + size;
    }

    return 0;
}

/*
 * (Re)fill the stream buffer whish is to be decoded.  If any data
 * still exists in the buffer then they are first shifted to be
 * front of the stream buffer.
 */
static int sox_mp3_input(ft_t ft)
{
    struct mp3priv *p = (struct mp3priv *) ft->priv;
    size_t bytes_read;
    size_t remaining;

    remaining = p->Stream->bufend - p->Stream->next_frame;

    /* libmad does not consume all the buffer it's given. Some
     * data, part of a truncated frame, is left unused at the
     * end of the buffer. That data must be put back at the
     * beginning of the buffer and taken in account for
     * refilling the buffer. This means that the input buffer
     * must be large enough to hold a complete frame at the
     * highest observable bit-rate (currently 448 kb/s).
     * TODO: Is 2016 bytes the size of the largest frame?
     * (448000*(1152/32000))/8
     */
    memmove(p->InputBuffer, p->Stream->next_frame, remaining);

    bytes_read = sox_readbuf(ft, p->InputBuffer+remaining,
                            INPUT_BUFFER_SIZE-remaining);
    if (bytes_read == 0)
    {
        return SOX_EOF;
    }

    mad_stream_buffer(p->Stream, p->InputBuffer, bytes_read+remaining);
    p->Stream->error = 0;

    return SOX_SUCCESS;
}

/* Attempts to read an ID3 tag at the current location in stream and
 * consume it all.  Returns SOX_EOF if no tag is found.  Its up to
 * caller to recover.
 * */
static int sox_mp3_inputtag(ft_t ft)
{
    struct mp3priv *p = (struct mp3priv *) ft->priv;
    int rc = SOX_EOF;
    size_t remaining;
    size_t tagsize;


    /* FIXME: This needs some more work if we are to ever
     * look at the ID3 frame.  This is because the Stream
     * may not be able to hold the complete ID3 frame.
     * We should consume the whole frame inside tagtype()
     * instead of outside of tagframe().  That would support
     * recovering when Stream contains less then 8-bytes (header)
     * and also when ID3v2 is bigger then Stream buffer size.
     * Need to pass in stream so that buffer can be
     * consumed as well as letting additional data to be
     * read in.
     */
    remaining = p->Stream->bufend - p->Stream->next_frame;
    if ((tagsize = tagtype(p->Stream->this_frame, remaining)))
    {
        mad_stream_skip(p->Stream, tagsize);
        rc = SOX_SUCCESS;
    }

    /* We know that a valid frame hasn't been found yet
     * so help libmad out and go back into frame seek mode.
     * This is true whether an ID3 tag was found or not.
     */
    mad_stream_sync(p->Stream);

    return rc;
}

static int sox_mp3startread(ft_t ft) 
{
    struct mp3priv *p = (struct mp3priv *) ft->priv;
    size_t ReadSize;

    p->Stream = NULL;
    p->Frame = NULL;
    p->Synth = NULL;
    p->Timer = NULL;
    p->InputBuffer = NULL;

    p->Stream=(struct mad_stream *)xmalloc(sizeof(struct mad_stream));
    p->Frame=(struct mad_frame *)xmalloc(sizeof(struct mad_frame));
    p->Synth=(struct mad_synth *)xmalloc(sizeof(struct mad_synth));
    p->Timer=(mad_timer_t *)xmalloc(sizeof(mad_timer_t));
    p->InputBuffer=(unsigned char *)xmalloc(INPUT_BUFFER_SIZE);

    mad_stream_init(p->Stream);
    mad_frame_init(p->Frame);
    mad_synth_init(p->Synth);
    mad_timer_reset(p->Timer);

    ft->signal.encoding = SOX_ENCODING_MP3;
    ft->signal.size = SOX_SIZE_16BIT;

    /* Decode at least one valid frame to find out the input
     * format.  The decoded frame will be saved off so that it
     * can be processed later.
     */
    ReadSize=sox_readbuf(ft, p->InputBuffer, INPUT_BUFFER_SIZE);
    if(ReadSize<=0)
    {
        if(sox_error(ft))
            sox_fail_errno(ft,SOX_EOF,"read error on bitstream");
        if(sox_eof(ft))
            sox_fail_errno(ft,SOX_EOF,"end of input stream");
        return(SOX_EOF);
    }

    mad_stream_buffer(p->Stream, p->InputBuffer, ReadSize);

    /* Find a valid frame before starting up.  This makes sure
     * that we have a valid MP3 and also skips past ID3v2 tags
     * at the beginning of the audio file.
     */
    p->Stream->error = 0;
    while (mad_frame_decode(p->Frame,p->Stream)) 
    {
        /* check whether input buffer needs a refill */
        if (p->Stream->error == MAD_ERROR_BUFLEN)
        {
            if (sox_mp3_input(ft) == SOX_EOF)
                return SOX_EOF;

            continue;
        }

        /* Consume any ID3 tags */
        sox_mp3_inputtag(ft);

        /* FIXME: We should probably detect when we've read
         * a bunch of non-ID3 data and still haven't found a
         * frame.  In that case we can abort early without
         * scanning the whole file.
         */
        p->Stream->error = 0;
    }

    if (p->Stream->error)
    {
        sox_fail_errno(ft,SOX_EOF,"No valid MP3 frame found");
        return SOX_EOF;
    }

    switch(p->Frame->header.mode)
    {
        case MAD_MODE_SINGLE_CHANNEL:
        case MAD_MODE_DUAL_CHANNEL:
        case MAD_MODE_JOINT_STEREO:
        case MAD_MODE_STEREO:
            ft->signal.channels = MAD_NCHANNELS(&p->Frame->header);
            break;
        default:
            sox_fail_errno(ft, SOX_EFMT, "Cannot determine number of channels");
            return SOX_EOF;
    }

    p->FrameCount=1;

    mad_timer_add(p->Timer,p->Frame->header.duration);
    mad_synth_frame(p->Synth,p->Frame);
    ft->signal.rate=p->Synth->pcm.samplerate;

    p->cursamp = 0;

    return SOX_SUCCESS;
}

/*
 * Read up to len samples from p->Synth
 * If needed, read some more MP3 data, decode them and synth them
 * Place in buf[].
 * Return number of samples read.
 */
static sox_size_t sox_mp3read(ft_t ft, sox_ssample_t *buf, sox_size_t len)
{
    struct mp3priv *p = (struct mp3priv *) ft->priv;
    sox_size_t donow,i,done=0;
    mad_fixed_t sample;
    size_t chan;

    do {
        donow=min(len,(p->Synth->pcm.length - p->cursamp)*ft->signal.channels);
        i=0;
        while(i<donow){
            for(chan=0;chan<ft->signal.channels;chan++){
                sample=p->Synth->pcm.samples[chan][p->cursamp];
                if (sample < -MAD_F_ONE)
                    sample=-MAD_F_ONE;
                else if (sample >= MAD_F_ONE)
                    sample=MAD_F_ONE-1;
                *buf++=(sox_ssample_t)(sample<<(32-1-MAD_F_FRACBITS));
                i++;
            }
            p->cursamp++;
        };

        len-=donow;
        done+=donow;

        if (len==0) break;

        /* check whether input buffer needs a refill */
        if (p->Stream->error == MAD_ERROR_BUFLEN)
        {
            if (sox_mp3_input(ft) == SOX_EOF)
                return 0;
        }

        if (mad_frame_decode(p->Frame,p->Stream))
        {
            if(MAD_RECOVERABLE(p->Stream->error))
            {
                sox_mp3_inputtag(ft);
                continue;
            }
            else
            {
                if (p->Stream->error == MAD_ERROR_BUFLEN)
                    continue;
                else
                {
                    sox_report("unrecoverable frame level error (%s).",
                              mad_stream_errorstr(p->Stream));
                    return done;
                }
            }
        }
        p->FrameCount++;
        mad_timer_add(p->Timer,p->Frame->header.duration);
        mad_synth_frame(p->Synth,p->Frame);
        p->cursamp=0;
    } while(1);

    return done;
}

static int sox_mp3stopread(ft_t ft)
{
  struct mp3priv *p=(struct mp3priv*) ft->priv;

  mad_synth_finish(p->Synth);
  mad_frame_finish(p->Frame);
  mad_stream_finish(p->Stream);

  free(p->Stream);
  free(p->Frame);
  free(p->Synth);
  free(p->Timer);
  free(p->InputBuffer);

  return SOX_SUCCESS;
}
#else /*HAVE_LIBMAD*/
static int sox_mp3startread(ft_t ft)
{
  sox_fail_errno(ft,SOX_EOF,"SoX was compiled without MP3 decoding support");
  return SOX_EOF;
}

sox_ssize_t sox_mp3read(ft_t ft, sox_sample_t *buf, sox_size_t samp)
{
  sox_fail_errno(ft,SOX_EOF,"SoX was compiled without MP3 decoding support");
  return SOX_EOF;
}

int sox_mp3stopread(ft_t ft)
{
  sox_fail_errno(ft,SOX_EOF,"SoX was compiled without MP3 decoding support");
  return SOX_EOF;
}
#endif /*HAVE_LIBMAD*/

#ifdef HAVE_LIBMP3LAME
static void null_error_func(const char* string UNUSED, va_list va UNUSED)
{
  return;
}

static int sox_mp3startwrite(ft_t ft)
{
  struct mp3priv *p = (struct mp3priv *) ft->priv;
  
  if (ft->signal.encoding != SOX_ENCODING_MP3) {
    if(ft->signal.encoding != SOX_ENCODING_UNKNOWN)
      sox_report("Encoding forced to MP3");
    ft->signal.encoding = SOX_ENCODING_MP3;
  }

  p->gfp = lame_init();
  if (p->gfp == NULL){
    sox_fail_errno(ft,SOX_EOF,"Initialization of LAME library failed");
    return(SOX_EOF);
  }

  if (ft->signal.channels != SOX_ENCODING_UNKNOWN) {
    if ( (lame_set_num_channels(p->gfp,(int)ft->signal.channels)) < 0) {
        sox_fail_errno(ft,SOX_EOF,"Unsupported number of channels");
        return(SOX_EOF);
    }
  }
  else
    ft->signal.channels = lame_get_num_channels(p->gfp); /* LAME default */

  lame_set_in_samplerate(p->gfp,(int)ft->signal.rate);

  lame_set_bWriteVbrTag(p->gfp, 0); /* disable writing VBR tag */

  /* The bitrate, mode, quality and other settings are the default ones,
     since SoX's command line options do not allow to set them */

  /* FIXME: Someone who knows about lame could implement adjustable compression
     here.  E.g. by using the -C value as an index into a table of params or
     as a compressed bit-rate. */
  if (ft->signal.compression != HUGE_VAL)
      sox_warn("-C option not supported for mp3; using default compression rate");
  if (lame_init_params(p->gfp) < 0){
        sox_fail_errno(ft,SOX_EOF,"LAME initialization failed");
        return(SOX_EOF);
  }
  lame_set_errorf(p->gfp,null_error_func);
  lame_set_debugf(p->gfp,null_error_func);
  lame_set_msgf  (p->gfp,null_error_func);

  return(SOX_SUCCESS);
}

static sox_size_t sox_mp3write(ft_t ft, const sox_ssample_t *buf, sox_size_t samp)
{
    struct mp3priv *p = (struct mp3priv *)ft->priv;
    char *mp3buffer;
    sox_size_t mp3buffer_size;
    short signed int *buffer_l, *buffer_r = NULL;
    int nsamples = samp/ft->signal.channels;
    int i,j;
    sox_ssize_t done = 0;
    sox_size_t written;

    /* NOTE: This logic assumes that "short int" is 16-bits
     * on all platforms.  It happens to be for all that I know
     * about.
     *
     * Lame ultimately wants data scaled to 16-bit samples
     * and assumes for the majority of cases that your passing
     * in something scaled based on passed in datatype
     * (16, 32, 64, and float).
     * 
     * If we used long buffers then this means it expects
     * different scalling between 32-bit and 64-bit CPU's.
     *
     * We might as well scale it ourselfs to 16-bit to allow
     * xmalloc()'ing a smaller buffer and call a consistent
     * interface.
     */
    buffer_l = (short signed int *)xmalloc(nsamples * sizeof(short signed int));

    if (ft->signal.channels == 2)
    {
        /* lame doesn't support iterleaved samples so we must break
         * them out into seperate buffers.
         */
        if ((buffer_r = 
             (short signed int *)xmalloc(nsamples*
                                          sizeof(short signed int))) == NULL)
        {
            sox_fail_errno(ft,SOX_ENOMEM,"Memory allocation failed");
            goto end3;
        }

        j=0;
        for (i=0; i<nsamples; i++)
        {
            buffer_l[i]=SOX_SAMPLE_TO_SIGNED_16BIT(buf[j++], ft->clips);
            buffer_r[i]=SOX_SAMPLE_TO_SIGNED_16BIT(buf[j++], ft->clips);
        }
    }
    else
    {
        j=0;
        for (i=0; i<nsamples; i++)
        {
            buffer_l[i]=SOX_SAMPLE_TO_SIGNED_16BIT(buf[j++], ft->clips); 
        }
    }

    mp3buffer_size = 1.25 * nsamples + 7200;
    if ((mp3buffer=(char *)xmalloc(mp3buffer_size)) == NULL)
    {
        sox_fail_errno(ft,SOX_ENOMEM,"Memory allocation failed");
        goto end2;
    }

    if ((written = lame_encode_buffer(p->gfp,buffer_l, buffer_r,
                                      nsamples, (unsigned char *)mp3buffer,
                                      (int)mp3buffer_size)) > mp3buffer_size){
        sox_fail_errno(ft,SOX_EOF,"Encoding failed");
        goto end;
    }

    if (sox_writebuf(ft, mp3buffer, written) < written)
    {
        sox_fail_errno(ft,SOX_EOF,"File write failed");
        goto end;
    }

    done = nsamples*ft->signal.channels;

end:
    free(mp3buffer);
end2:
    if (ft->signal.channels == 2)
        free(buffer_r);
end3:
    free(buffer_l);

    return done;
}

static int sox_mp3stopwrite(ft_t ft)
{
  struct mp3priv *p = (struct mp3priv *) ft->priv;
  char mp3buffer[7200];
  int written;
  size_t written2;
  
  if ( (written=lame_encode_flush(p->gfp, (unsigned char *)mp3buffer, 7200)) <0){
    sox_fail_errno(ft,SOX_EOF,"Encoding failed");
  }
  else if (sox_writebuf(ft, mp3buffer, written2 = written) < written2){
    sox_fail_errno(ft,SOX_EOF,"File write failed");
  }

  lame_close(p->gfp);
  return SOX_SUCCESS;
}

#else /* HAVE_LIBMP3LAME */
static int sox_mp3startwrite(ft_t ft UNUSED)
{
  sox_fail_errno(ft,SOX_EOF,"SoX was compiled without MP3 encoding support");
  return SOX_EOF;
}

static sox_size_t sox_mp3write(ft_t ft UNUSED, const sox_sample_t *buf UNUSED, sox_size_t samp UNUSED)
{
  sox_fail_errno(ft,SOX_EOF,"SoX was compiled without MP3 encoding support");
  return 0;
}

static int sox_mp3stopwrite(ft_t ft)
{
  sox_fail_errno(ft,SOX_EOF,"SoX was compiled without MP3 encoding support");
  return SOX_EOF;
}
#endif /* HAVE_LIBMP3LAME */

/* MP3 */
static const char *mp3names[] = {
  "mp3",
  "mp2",
  NULL,
};

static sox_format_t sox_mp3_format = {
  mp3names,
  0,
  sox_mp3startread,
  sox_mp3read,
  sox_mp3stopread,
  sox_mp3startwrite,
  sox_mp3write,
  sox_mp3stopwrite,
  sox_format_nothing_seek
};

const sox_format_t *sox_mp3_format_fn(void);

const sox_format_t *sox_mp3_format_fn(void)
{
    return &sox_mp3_format;
}
