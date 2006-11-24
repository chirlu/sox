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

#include "st_i.h"

#include <string.h>

#if defined(HAVE_LIBMAD) || defined(HAVE_LAME)

#if defined(HAVE_LIBMAD)
#include <mad.h>
#endif

#if defined(HAVE_LAME)
#include <lame/lame.h>
#include <math.h>
#endif

#define INPUT_BUFFER_SIZE       (ST_BUFSIZ)

/* Private data */
struct mp3priv {
#if defined(HAVE_LIBMAD)
        struct mad_stream       *Stream;
        struct mad_frame        *Frame;
        struct mad_synth        *Synth;
        mad_timer_t             *Timer;
        unsigned char           *InputBuffer;
        st_ssize_t              cursamp;
        st_size_t               FrameCount;
#endif /*HAVE_LIBMAD*/
#if defined(HAVE_LAME)
        lame_global_flags       *gfp;
#endif /*HAVE_LAME*/
};

#if defined(HAVE_LIBMAD)

/* This function merges the functions tagtype() and id3_tag_query()
   from MAD's libid3tag, so we don't have to link to it
   Returns 0 if the frame is not an ID3 tag, tag length if it is */

#define ID3_TAG_FLAG_FOOTERPRESENT 0x10

static int tagtype(const unsigned char *data, int length)
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
static int st_mp3_input(ft_t ft)
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

    bytes_read = st_readbuf(ft, p->InputBuffer+remaining, 1, 
                            INPUT_BUFFER_SIZE-remaining);
    if (bytes_read == 0)
    {
        return ST_EOF;
    }

    mad_stream_buffer(p->Stream, p->InputBuffer, bytes_read+remaining);
    p->Stream->error = 0;

    return ST_SUCCESS;
}

/* Attempts to read an ID3 tag at the current location in stream and
 * consume it all.  Returns ST_EOF if no tag is found.  Its up to
 * caller to recover.
 * */
static int st_mp3_inputtag(ft_t ft)
{
    struct mp3priv *p = (struct mp3priv *) ft->priv;
    int rc = ST_EOF;
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
        rc = ST_SUCCESS;
    }

    /* We know that a valid frame hasn't been found yet
     * so help libmad out and go back into frame seek mode.
     * This is true whether an ID3 tag was found or not.
     */
    mad_stream_sync(p->Stream);

    return rc;
}

int st_mp3startread(ft_t ft) 
{
    struct mp3priv *p = (struct mp3priv *) ft->priv;
    size_t ReadSize;

    p->Stream = NULL;
    p->Frame = NULL;
    p->Synth = NULL;
    p->Timer = NULL;
    p->InputBuffer = NULL;

    p->Stream=(struct mad_stream *)malloc(sizeof(struct mad_stream));
    p->Frame=(struct mad_frame *)malloc(sizeof(struct mad_frame));
    p->Synth=(struct mad_synth *)malloc(sizeof(struct mad_synth));
    p->Timer=(mad_timer_t *)malloc(sizeof(mad_timer_t));
    p->InputBuffer=(unsigned char *)malloc(INPUT_BUFFER_SIZE);

    if (!p->Stream || !p->Frame || !p->Synth || 
        !p->Timer || !p->InputBuffer)
    {
        st_fail_errno(ft, ST_ENOMEM, "Could not allocate memory");
        goto error;
    }

    mad_stream_init(p->Stream);
    mad_frame_init(p->Frame);
    mad_synth_init(p->Synth);
    mad_timer_reset(p->Timer);

    ft->info.encoding = ST_ENCODING_MP3;
    ft->info.size = ST_SIZE_WORD;

    /* Decode at least one valid frame to find out the input
     * format.  The decoded frame will be saved off so that it
     * can be processed later.
     */
    ReadSize=st_readbuf(ft, p->InputBuffer, 1, INPUT_BUFFER_SIZE);
    if(ReadSize<=0)
    {
        if(st_error(ft))
            st_fail_errno(ft,ST_EOF,"read error on bitstream");
        if(st_eof(ft))
            st_fail_errno(ft,ST_EOF,"end of input stream");
        return(ST_EOF);
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
            if (st_mp3_input(ft) == ST_EOF)
                return ST_EOF;

            continue;
        }

        /* Consume any ID3 tags */
        st_mp3_inputtag(ft);

        /* FIXME: We should probably detect when we've read
         * a bunch of non-ID3 data and still haven't found a
         * frame.  In that case we can abort early without
         * scanning the whole file.
         */
        p->Stream->error = 0;
    }

    if (p->Stream->error)
    {
        st_fail_errno(ft,ST_EOF,"No valid MP3 frame found");
        return ST_EOF;
    }

    switch(p->Frame->header.mode)
    {
        case MAD_MODE_SINGLE_CHANNEL:
        case MAD_MODE_DUAL_CHANNEL:
        case MAD_MODE_JOINT_STEREO:
        case MAD_MODE_STEREO:
            ft->info.channels = MAD_NCHANNELS(&p->Frame->header);
            break;
        default:
            st_fail_errno(ft, ST_EFMT, "Cannot determine number of channels");
            return ST_EOF;
    }

    p->FrameCount=1;

    mad_timer_add(p->Timer,p->Frame->header.duration);
    mad_synth_frame(p->Synth,p->Frame);
    ft->info.rate=p->Synth->pcm.samplerate;

    p->cursamp = 0;

    return ST_SUCCESS;

error:
    if (p->Stream) free(p->Stream);
    if (p->Frame) free(p->Frame);
    if (p->Synth) free(p->Synth);
    if (p->Timer) free(p->Timer);
    if (p->InputBuffer) free(p->InputBuffer);

    return ST_EOF;
}

/*
 * Read up to len samples from p->Synth
 * If needed, read some more MP3 data, decode them and synth them
 * Place in buf[].
 * Return number of samples read.
 */
st_ssize_t st_mp3read(ft_t ft, st_sample_t *buf, st_ssize_t len)
{
    struct mp3priv *p = (struct mp3priv *) ft->priv;
    st_ssize_t donow,i,done=0;
    mad_fixed_t sample;
    int chan;

    do {
        donow=min(len,(p->Synth->pcm.length - p->cursamp)*ft->info.channels);
        i=0;
        while(i<donow){
            for(chan=0;chan<ft->info.channels;chan++){
                sample=p->Synth->pcm.samples[chan][p->cursamp];
                if (sample < -MAD_F_ONE)
                    sample=-MAD_F_ONE;
                else if (sample >= MAD_F_ONE)
                    sample=MAD_F_ONE-1;
                *buf++=(st_sample_t)(sample<<(32-1-MAD_F_FRACBITS));
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
            if (st_mp3_input(ft) == ST_EOF)
                return ST_EOF;
        }

        if (mad_frame_decode(p->Frame,p->Stream))
        {
            if(MAD_RECOVERABLE(p->Stream->error))
            {
                st_mp3_inputtag(ft);
                continue;
            }
            else
            {
                if (p->Stream->error == MAD_ERROR_BUFLEN)
                    continue;
                else
                {
                    st_report("unrecoverable frame level error (%s).\n",
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

int st_mp3stopread(ft_t ft)
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

  return ST_SUCCESS;
}
#else /*HAVE_LIBMAD*/
int st_mp3startread(ft_t ft)
{
  st_fail_errno(ft,ST_EOF,"SoX was compiled without MP3 decoding support");
  return ST_EOF;
}

st_ssize_t st_mp3read(ft_t ft, st_sample_t *buf, st_ssize_t samp)
{
  st_fail_errno(ft,ST_EOF,"SoX was compiled without MP3 decoding support");
  return ST_EOF;
}

int st_mp3stopread(ft_t ft)
{
  st_fail_errno(ft,ST_EOF,"SoX was compiled without MP3 decoding support");
  return ST_EOF;
}
#endif /*HAVE_LIBMAD*/

#if defined (HAVE_LAME)
void null_error_func(const char* string, va_list va){
  return;
}

int st_mp3startwrite(ft_t ft)
{
  struct mp3priv *p = (struct mp3priv *) ft->priv;
  
  if (ft->info.encoding != ST_ENCODING_MP3){
    if(ft->info.encoding != -1)
      st_report("Encoding forced to MP3");
    ft->info.encoding = ST_ENCODING_MP3;
  }

  p->gfp = lame_init();
  if (p->gfp == NULL){
    st_fail_errno(ft,ST_EOF,"Initialization of LAME library failed");
    return(ST_EOF);
  }

  if (ft->info.channels != -1){
    if ( (lame_set_num_channels(p->gfp,ft->info.channels)) < 0) {
        st_fail_errno(ft,ST_EOF,"Unsupported number of channels");
        return(ST_EOF);
    }
  }
  else
    ft->info.channels = lame_get_num_channels(p->gfp); /* LAME default */

  lame_set_in_samplerate(p->gfp,ft->info.rate);

  lame_set_bWriteVbrTag(p->gfp, 0); /* disable writing VBR tag */

  /* The bitrate, mode, quality and other settings are the default ones,
     since SoX's command line options do not allow to set them */

  /* FIXME: Someone who knows about lame could implement adjustable compression
     here.  E.g. by using the -C value as an index into a table of params or
     as a compressed bit-rate. */
  if (ft->info.compression != HUGE_VAL)
      st_warn("-C option not supported for mp3; using default compression rate");
  if (lame_init_params(p->gfp) < 0){
        st_fail_errno(ft,ST_EOF,"LAME initialization failed");
        return(ST_EOF);
  }
  lame_set_errorf(p->gfp,null_error_func);
  lame_set_debugf(p->gfp,null_error_func);
  lame_set_msgf  (p->gfp,null_error_func);

  return(ST_SUCCESS);
}

st_ssize_t st_mp3write(ft_t ft, st_sample_t *buf, st_ssize_t samp)
{
    struct mp3priv *p = (struct mp3priv *)ft->priv;
    char *mp3buffer;
    int mp3buffer_size;
    short signed int *buffer_l, *buffer_r = NULL;
    int nsamples = samp/ft->info.channels;
    int i,j;
    st_ssize_t done = 0;
    int written;

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
     * malloc()'ing a smaller buffer and call a consistent
     * interface.
     */
    if ((buffer_l = 
         (short signed int *)malloc(nsamples*
                                    sizeof(short signed int))) == NULL)
    {
        st_fail_errno(ft, ST_ENOMEM, "Memory allocation failed");
        goto end4;
    }

    if (ft->info.channels == 2)
    {
        /* lame doesn't support iterleaved samples so we must break
         * them out into seperate buffers.
         */
        if ((buffer_r = 
             (short signed int *)malloc(nsamples*
                                          sizeof(short signed int))) == NULL)
        {
            st_fail_errno(ft,ST_ENOMEM,"Memory allocation failed");
            goto end3;
        }

        j=0;
        for (i=0; i<nsamples; i++)
        {
            buffer_l[i]=ST_SAMPLE_TO_SIGNED_WORD(buf[j++]);
            buffer_r[i]=ST_SAMPLE_TO_SIGNED_WORD(buf[j++]);
        }
    }
    else
    {
        j=0;
        for (i=0; i<nsamples; i++)
        {
            buffer_l[i]=ST_SAMPLE_TO_SIGNED_WORD(buf[j++]); 
        }
    }

    mp3buffer_size = 1.25 * nsamples + 7200;
    if ((mp3buffer=(char *)malloc(mp3buffer_size)) == NULL)
    {
        st_fail_errno(ft,ST_ENOMEM,"Memory allocation failed");
        goto end2;
    }

    if ((written = lame_encode_buffer(p->gfp,buffer_l, buffer_r,
                                      nsamples, (unsigned char *)mp3buffer,
                                      mp3buffer_size)) < 0){
        st_fail_errno(ft,ST_EOF,"Encoding failed");
        goto end;
    }

    if (st_writebuf(ft, mp3buffer, 1, written) < written)
    {
        st_fail_errno(ft,ST_EOF,"File write failed");
        goto end;
    }

    done = nsamples*ft->info.channels;

end:
    free(mp3buffer);
end2:
    if (ft->info.channels == 2)
        free(buffer_r);
end3:
    free(buffer_l);
end4:
    return done;
}

int st_mp3stopwrite(ft_t ft)
{
  struct mp3priv *p = (struct mp3priv *) ft->priv;
  char mp3buffer[7200];
  int written;
  
  if ( (written=lame_encode_flush(p->gfp, (unsigned char *)mp3buffer, 7200)) <0){
    st_fail_errno(ft,ST_EOF,"Encoding failed");
  }
  else if (st_writebuf(ft, mp3buffer, 1, written) < written){
    st_fail_errno(ft,ST_EOF,"File write failed");
  }

  lame_close(p->gfp);
  return ST_SUCCESS;
}

#else /* HAVE_LAME */
int st_mp3startwrite(ft_t ft)
{
  st_fail_errno(ft,ST_EOF,"Sorry, no MP3 encoding support");
  return ST_EOF;
}

st_ssize_t st_mp3write(ft_t ft, st_sample_t *buf, st_ssize_t samp)
{
  st_fail_errno(ft,ST_EOF,"Sorry, no MP3 encoding support");
  return ST_EOF;
}

int st_mp3stopwrite(ft_t ft)
{
  st_fail_errno(ft,ST_EOF,"Sorry, no MP3 encoding support");
  return ST_EOF;
}
#endif /* HAVE_LAME */

/* MP3 */
static char *mp3names[] = {
  "mp3",
  "mp2",
  NULL,
};

static st_format_t st_mp3_format = {
  mp3names,
  NULL,
  ST_FILE_STEREO,
  st_mp3startread,
  st_mp3read,
  st_mp3stopread,
  st_mp3startwrite,
  st_mp3write,
  st_mp3stopwrite,
  st_format_nothing_seek
};

const st_format_t *st_mp3_format_fn(void)
{
    return &st_mp3_format;
}
#endif
