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
#endif

#ifndef MIN
#define MIN(s1,s2) ((s1)<(s2)?(s1):(s2))
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
        unsigned long           FrameCount;
        int                     eof;
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

int tagtype(const unsigned char *data, int length)
{
  if (length >= 3 && data[0] == 'T' && data[1] == 'A' && data[2] == 'G')
        return 128; /* ID3V1 */

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

int st_mp3startread(ft_t ft) 
{
        struct mp3priv *p = (struct mp3priv *) ft->priv;
        size_t ReadSize;

        p->Stream=(struct mad_stream *)malloc(sizeof(struct mad_stream));
        if (p->Stream == NULL){
          st_fail_errno(ft, ST_ENOMEM, "Could not allocate memory");
          return ST_EOF;
        }
        
        p->Frame=(struct mad_frame *)malloc(sizeof(struct mad_frame));
        if (p->Frame == NULL){
          st_fail_errno(ft, ST_ENOMEM, "Could not allocate memory");
          free(p->Stream);
          return ST_EOF;
        }
        
        p->Synth=(struct mad_synth *)malloc(sizeof(struct mad_synth));
        if (p->Synth == NULL){
          st_fail_errno(ft, ST_ENOMEM, "Could not allocate memory");
          free(p->Stream);
          free(p->Frame);
          return ST_EOF;
        }
        
        p->Timer=(mad_timer_t *)malloc(sizeof(mad_timer_t));
        if (p->Timer == NULL){
          st_fail_errno(ft, ST_ENOMEM, "Could not allocate memory");
          free(p->Stream);
          free(p->Frame);
          free(p->Synth);
          return ST_EOF;
        }
        
        p->InputBuffer=(unsigned char *)malloc(INPUT_BUFFER_SIZE);
        if (p->InputBuffer == NULL){
          st_fail_errno(ft, ST_ENOMEM, "Could not allocate memory");
          free(p->Stream);
          free(p->Frame);
          free(p->Synth);
          free(p->Timer);
          return ST_EOF;
        }
        
        mad_stream_init(p->Stream);
        mad_frame_init(p->Frame);
        mad_synth_init(p->Synth);
        mad_timer_reset(p->Timer);

        ft->info.encoding = ST_ENCODING_MP3;
        ft->info.size = ST_SIZE_WORD;

        /* We need to decode the first frame,
         * so we know the output format */
more_data:
        ReadSize=st_readbuf(ft, p->InputBuffer, 1, INPUT_BUFFER_SIZE);
        if(ReadSize<=0)
        {
                if(st_error(ft))
                        st_fail_errno(ft,ST_EOF,"read error on bitstream");
                if(st_eof(ft))
                        st_fail_errno(ft,ST_EOF,"end of input stream");
                return(ST_EOF);
        }
        
        mad_stream_buffer(p->Stream,p->InputBuffer,ReadSize);
        p->Stream->error = 0;

        /* Find a valid frame before starting up.  This makes sure
         * that we have a valid MP3 and also skips past ID3v2 tags
         * at the beginning of the audio file.
         */
        while(mad_frame_decode(p->Frame,p->Stream)) {
            int tagsize;
            size_t remaining;

            remaining = p->Stream->bufend - p->Stream->this_frame;
            if (remaining <= 8) {
                /* Read another buffer full of data. */
                memmove(p->InputBuffer, p->Stream->this_frame, remaining);

                ReadSize=st_readbuf(ft, p->InputBuffer+remaining, 1, INPUT_BUFFER_SIZE-remaining);
                if (ReadSize <= 0) {
                  st_fail_errno(ft,ST_EOF,"The file is not an MP3 file or it is corrupted");
                  return ST_EOF;
                }

                remaining+=ReadSize;
                mad_stream_buffer(p->Stream, p->InputBuffer, remaining);
                p->Stream->error = 0;
            }

            /* Skip past this frame, based on tag size.  If invalid
             * tag then Walk threw the stream one byte at a time (tagsize=1)
             * until we find a valid frame.  Previous if() will
             * abort once we got a certain distance.
             */
            if ((tagsize=tagtype(p->Stream->this_frame, remaining)) == 0)
                tagsize = 1; /* Walk through the stream. */

            /* ID3v2 tags can be any size.  That means they can
             * span a buffer larger then INPUT_BUFFER_SIZE.  That
             * means that we really need a loop to continue reading
             * more data.
             */
            if (tagsize > remaining)
            {
                /* Discard the remaining data and read the rest of the tag
                 * data from the file and start over.
                 */
                tagsize -= remaining;
                while (tagsize > 0)
                {
                    if (tagsize < INPUT_BUFFER_SIZE)
                        ReadSize=st_readbuf(ft, p->InputBuffer, 1, tagsize);
                    else
                        ReadSize=st_readbuf(ft, p->InputBuffer, 1, INPUT_BUFFER_SIZE);
                    tagsize -= ReadSize;
                }
                goto more_data;
            }

            mad_stream_skip(p->Stream, tagsize);
        }

        /* TODO: It would be nice to look for Xing VBR headers
         * or TLE fields in ID3 to detect length of file
         * and set ft->length.
         * For CBR, we should fstat the file and divided
         * by bitrate to find length.
         */

        switch(p->Frame->header.mode)
        {
                case MAD_MODE_SINGLE_CHANNEL:
                case MAD_MODE_DUAL_CHANNEL:
                case MAD_MODE_JOINT_STEREO:
                case MAD_MODE_STEREO:
                  ft->info.channels = MAD_NCHANNELS(&p->Frame->header);
                  break;
                default:
                  st_fail_errno(ft,ST_EFMT,"Cannot determine number of channels");
                  return ST_EOF;
        }

        p->FrameCount=1;
        ft->info.rate=p->Frame->header.samplerate;

        mad_timer_add(p->Timer,p->Frame->header.duration);
        mad_synth_frame(p->Synth,p->Frame);
        p->cursamp = 0;
        p->eof     = 0;

        return ST_SUCCESS;
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

  do{
    donow=MIN(len,(p->Synth->pcm.length - p->cursamp)*ft->info.channels);
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

    if (len==0 || p->eof) break;

    /* check whether input buffer needs a refill */

    if(p->Stream->error==MAD_ERROR_BUFLEN)
      {
        size_t          ReadSize, Remaining;
        
        /* libmad does not consume all the buffer it's given. Some
         * datas, part of a truncated frame, is left unused at the
         * end of the buffer. Those datas must be put back at the
         * beginning of the buffer and taken in account for
         * refilling the buffer. This means that the input buffer
         * must be large enough to hold a complete frame at the
         * highest observable bit-rate (currently 448 kb/s). XXX=XXX
         * Is 2016 bytes the size of the largest frame?
         * (448000*(1152/32000))/8
         */
        
        Remaining=p->Stream->bufend - p->Stream->next_frame;
        memmove(p->InputBuffer,p->Stream->next_frame,Remaining);

        ReadSize=st_readbuf(ft, p->InputBuffer+Remaining, 1, 
                         INPUT_BUFFER_SIZE-Remaining);
        if(ReadSize == 0){
          p->eof=1;
          memset(p->InputBuffer+Remaining,0,MAD_BUFFER_GUARD);
          ReadSize=MAD_BUFFER_GUARD;
        }

        mad_stream_buffer(p->Stream,p->InputBuffer,ReadSize+Remaining);
        p->Stream->error = 0;
      }

    if(mad_frame_decode(p->Frame,p->Stream)){
      if(MAD_RECOVERABLE(p->Stream->error))
        {
          int tagsize;
          if ( (tagsize=tagtype(p->Stream->this_frame, p->Stream->bufend - p->Stream->this_frame)) == 0){
            if (!p->eof)
              st_report("recoverable frame level error (%s).\n",
                        mad_stream_errorstr(p->Stream));
          }
          else mad_stream_skip(p->Stream,tagsize);
          continue;
        }
      else
        {
          if(p->Stream->error==MAD_ERROR_BUFLEN)
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
  }while(1);

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
  struct mp3priv *p = (struct mp3priv *) ft->priv;
  char *mp3buffer;
  int mp3buffer_size;
  long *buffer_l, *buffer_r;
  int nsamples = samp/ft->info.channels;
  int i,j;
  st_ssize_t done = 0;
  int written;

  if ( (buffer_r=(long*)malloc(nsamples*sizeof(long))) == NULL){
    st_fail_errno(ft,ST_ENOMEM,"Memory allocation failed");
    goto end4;
  }

  if (ft->info.channels==2){ /* Why isn't there a lame_encode_buffer_long_interleaved? */
    if ( (buffer_l=(long*)malloc(nsamples*sizeof(long))) == NULL){
      st_fail_errno(ft,ST_ENOMEM,"Memory allocation failed");
      goto end3;
    }
    j=0;
    for (i=0;i<nsamples;i++){
      buffer_l[i]=(long)buf[j++];   /* Should we paranoically check whether long is actually 32 bits? */
      buffer_r[i]=(long)buf[j++];
    }
  }
  else{
    buffer_l=(long*)buf;
    memset(buffer_r,0,nsamples*sizeof(long));
  }

  mp3buffer_size=1.25*nsamples + 7200;
  if ( (mp3buffer=(char *)malloc(mp3buffer_size)) == NULL){
    st_fail_errno(ft,ST_ENOMEM,"Memory allocation failed");
    goto end2;
  }
 
  if ( (written = lame_encode_buffer_long2(p->gfp,
                                           buffer_l,
                                           buffer_r,
                                           nsamples,
                                           (unsigned char *)mp3buffer,
                                           mp3buffer_size)) < 0){
    st_fail_errno(ft,ST_EOF,"Encoding failed");
    goto end;
  }

  if (st_writebuf(ft, mp3buffer, 1, written) < written){
     st_fail_errno(ft,ST_EOF,"File write failed");
     goto end;
  }

  done = nsamples;

 end:
  free(mp3buffer);
 end2:
  if (ft->info.channels == 2)
    free(buffer_l);
 end3:
  free(buffer_r);
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
#endif
