/*
 * Ogg Vorbis sound format driver
 * Copyright 2001, Stan Seibert <indigo@aztec.asu.edu>
 *
 * Portions from oggenc, (c) Michael Smith <msmith@labyrinth.net.au>,
 * ogg123, (c) Kenneth Arnold <kcarnold@yahoo.com>, and
 * libvorbisfile (c) Xiphophorus Company
 * 
 * May 9, 2001 - Stan Seibert (indigo@aztec.asu.edu)
 * Ogg Vorbis driver initially written.
 *
 * July 5, 1991 - Skeleton file
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */
#include "st_i.h"

#if defined(HAVE_LIBVORBIS)
#include <stdio.h>
#include <math.h>
#include <string.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include <vorbis/vorbisenc.h>

#define DEF_BUF_LEN 4096

#define BUF_ERROR -1
#define BUF_EOF  0
#define BUF_DATA 1

#define HEADER_ERROR 0
#define HEADER_OK   1

/* Private data for Ogg Vorbis file */
typedef struct vorbis_enc {
	ogg_stream_state os;
	ogg_page         og;
	ogg_packet       op;
	
	vorbis_dsp_state vd;
	vorbis_block     vb;
        vorbis_info      vi;
} vorbis_enc_t;

typedef struct vorbisstuff {
/* Decoding data */
	OggVorbis_File *vf;
	char *buf;
	int buf_len;
	int start;
	int end;  /* Unsent data samples in buf[start] through buf[end-1] */
	int current_section;
	int eof;

	vorbis_enc_t *vorbis_enc_data;
} *vorbis_t;

/******** Callback functions used in ov_open_callbacks ************/
int myclose (void *datasource)
{
	/* Do nothing so sox can close the file for us */
	return 0;
}

/* Taken from vorbisfile.c in libvorbis source code */
static int _fseek64_wrap(FILE *f,ogg_int64_t off,int whence){
  if(f==NULL)return(-1);
  return fseek(f,(int)off,whence);
}

/********************* End callbacks *****************************/


/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *	Find out sampling rate, 
 *	size and encoding of samples, 
 *	mono/stereo/quad.
 */
int st_vorbisstartread(ft_t ft) 
{
	vorbis_t vb = (vorbis_t) ft->priv;
	vorbis_info *vi;
	vorbis_comment *vc;
	int comment_size;
	int i, offset;

	ov_callbacks callbacks = {
		(size_t (*)(void *, size_t, size_t, void *))  fread,
		(int (*)(void *, ogg_int64_t, int))           _fseek64_wrap,
		(int (*)(void *))                             myclose,
		(long (*)(void *))                            ftell
	};

	
	
	/* Allocate space for decoding structure */
	vb->vf = malloc(sizeof(OggVorbis_File));
	if (vb->vf == NULL) 
	{
	    st_fail_errno(ft, ST_ENOMEM, "Could not allocate memory");
	    return (ST_EOF);
	}

	/* Init the decoder */
	if (ov_open_callbacks((void *)ft->fp,vb->vf,NULL,0,callbacks) < 0)
	{
		st_fail_errno(ft,ST_EHDR,
			      "Input not an Ogg Vorbis audio stream");
		return (ST_EOF);
	}

	/* Get info about the Ogg Vorbis stream */
	vi = ov_info(vb->vf, -1);
	vc = ov_comment(vb->vf, -1);

	/* Record audio info */
	ft->info.rate = vi->rate;
	ft->info.size = ST_SIZE_16BIT;
	ft->info.encoding = ST_ENCODING_SIGN2;
	ft->info.channels = vi->channels;
	
	/* Record comments */
	if (vc->comments == 0)
		ft->comment = NULL;
	else 
	{
		comment_size = 0;
		for (i = 0; i < vc->comments; i++)
			comment_size += vc->comment_lengths[i] + 1;

		if ( (ft->comment = calloc(comment_size, sizeof(char))) 
		     == NULL)
		{
			ov_clear(vb->vf);
			free(vb->vf);

			st_fail_errno(ft, ST_ENOMEM, 
				      "Could not allocate memory");
			return (ST_EOF);
		}
		
		offset = 0;
		for (i = 0; i < vc->comments; i++)
		{
			strncpy(ft->comment + i, vc->user_comments[i],
				vc->comment_lengths[i]);
			offset += vc->comment_lengths[i];
			ft->comment[offset] = '\n';
			offset++;
		}
		ft->comment[offset] = 0; // End comment
	}

	/* Setup buffer */
	vb->buf_len = DEF_BUF_LEN;
	if ( (vb->buf = calloc(vb->buf_len, sizeof(char))) == NULL )
	{
		ov_clear(vb->vf);
		free(vb->vf);
		st_fail_errno(ft, ST_ENOMEM, "Could not allocate memory");
		return (ST_EOF);
	}
	vb->start = vb->end = 0;

	/* Fill in other info */
	vb->eof = 0;
	vb->current_section = -1;

	return (ST_SUCCESS);
}


/* Refill the buffer with samples.  Returns BUF_EOF if the end of the
   vorbis data was reached while the buffer was being filled,
   BUF_ERROR is something bad happens, and BUF_DATA otherwise */
int refill_buffer (vorbis_t vb)
{
	int num_read;

	if (vb->start == vb->end) /* Samples all played */
		vb->start = vb->end = 0;

	while (vb->end < vb->buf_len)
	{
		num_read = ov_read(vb->vf, vb->buf + vb->end,
				   vb->buf_len - vb->end, 0, 2, 1, 
				   &vb->current_section);
		
		if (num_read == 0)
			return (BUF_EOF);
		else if (num_read == OV_HOLE)
			fprintf(stderr, "Warning: hole in stream; probably harmless\n");
		else if (num_read < 0)
			return (BUF_ERROR);
		else
			vb->end += num_read;
			
	}

	return (BUF_DATA);
}


/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

st_ssize_t st_vorbisread(ft_t ft, st_sample_t *buf, st_ssize_t len) 
{
	vorbis_t vb = (vorbis_t) ft->priv;
	int i;
	int ret;
	st_sample_t l;


	for(i = 0; i < len; i++) {
		if (vb->start == vb->end)
		{
			if (vb->eof)
				break;
			ret = refill_buffer(vb);
			if (ret == BUF_EOF || ret == BUF_ERROR)
			{
			    vb->eof = 1;
			    if (vb->end == 0)
				break;
			}
		}

		l = (vb->buf[vb->start+1]<<24) 
			| (0xffffff &  (vb->buf[vb->start]<<16));
		*(buf + i) = l;
		vb->start += 2;
	}

	return i;
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_vorbisstopread(ft_t ft) 
{
	vorbis_t vb = (vorbis_t) ft->priv;

	free(vb->buf);
	ov_clear(vb->vf);

	return (ST_SUCCESS);
}

/* Write a page of ogg data to a file.  Taken directly from encode.c in 
   oggenc.   Returns the number of bytes written. */
int oe_write_page(ogg_page *page, FILE *fp)
{
        int written;
        written = fwrite(page->header,1,page->header_len, fp);
        written += fwrite(page->body,1,page->body_len, fp);

        return written;
}

/* Write out the header packets.  Derived mostly from encode.c in
   oggenc.  Returns HEADER_ERROR if the header cannot be written and
   HEADER_OK otherwise. */
int write_vorbis_header(ft_t ft, vorbis_enc_t *ve)
{
	ogg_packet header_main;
	ogg_packet header_comments;
	ogg_packet header_codebooks;
	vorbis_comment vc;
	int result;
	int ret;

	/* Make the comment structure */
	vc.user_comments = calloc(1, sizeof(char *));
	vc.comment_lengths = calloc(1, sizeof(int));
	vc.comments = 1;

	vc.user_comments[0] = ft->comment;
	vc.comment_lengths[0] = strlen(ft->comment);

	/* Build the packets */
	vorbis_analysis_headerout(&ve->vd,&vc,
				  &header_main,
				  &header_comments,
				  &header_codebooks);
	
	/* And stream them out */
	ogg_stream_packetin(&ve->os,&header_main);
	ogg_stream_packetin(&ve->os,&header_comments);
	ogg_stream_packetin(&ve->os,&header_codebooks);
	
	while((result = ogg_stream_flush(&ve->os, &ve->og)))
	{
		if(!result) break;
		ret = oe_write_page(&ve->og, ft->fp);
		if(!ret)
		{
			return HEADER_ERROR;
		}
	}

	return HEADER_OK;
}

int st_vorbisstartwrite(ft_t ft) 
{
	vorbis_t vb = (vorbis_t) ft->priv;
	vorbis_enc_t *ve;
	long rate;

	/* Allocate memory for all of the structures */
	ve = vb->vorbis_enc_data = malloc(sizeof(vorbis_enc_t));
	if (ve == NULL)
	{
	    st_fail_errno(ft, ST_ENOMEM, "Could not allocate memory");
	    return (ST_EOF);
	}

	vorbis_info_init(&ve->vi);

	/* DEBUG */
	rate = ft->info.rate;
	fprintf(stdout, "Channels: %d  Rate: %ld\n", ft->info.channels,
		rate);

	/* Set encoding to average bit rate of 128kbps with no min or max */
	vorbis_encode_init(&ve->vi, ft->info.channels, ft->info.rate,
			   -1, 128000, -1);

	vorbis_analysis_init(&ve->vd, &ve->vi);
	vorbis_block_init(&ve->vd, &ve->vb);
	
	ogg_stream_init(&ve->os, rand()); /* Random serial number */
	
	if (write_vorbis_header(ft, ve) == HEADER_ERROR)
	{
    	    st_fail_errno(ft,ST_EHDR,
			  "Error writing headre for Ogg Vorbis audio stream");
    	    return (ST_EOF);
	}
	
	return(ST_SUCCESS);	
}

st_ssize_t st_vorbiswrite(ft_t ft, st_sample_t *buf, st_ssize_t len) 
{
	vorbis_t vb = (vorbis_t) ft->priv;
	vorbis_enc_t *ve = vb->vorbis_enc_data;
	st_ssize_t samples = len / ft->info.channels;	
	float **buffer = vorbis_analysis_buffer(&ve->vd, samples);
	st_ssize_t i, j;
	int ret;
	int eos = 0;

	/* Copy samples into vorbis buffer */
	for (i = 0; i < samples; i++)
		for (j = 0; j < ft->info.channels; j++)
			buffer[j][i] = buf[i*ft->info.channels + j] 
				/ 2147483648.0f;

	vorbis_analysis_wrote(&ve->vd, samples);
	
	while(vorbis_analysis_blockout(&ve->vd,&ve->vb)==1)
	{
		/* Do the main analysis, creating a packet */
		vorbis_analysis(&ve->vb, &ve->op);
		
		/* Add packet to bitstream */
		ogg_stream_packetin(&ve->os,&ve->op);
		
		/* If we've gone over a page boundary, we can do actual output,
		   so do so (for however many pages are available) */
		
		while(!eos)
		{
			int result = ogg_stream_pageout(&ve->os,&ve->og);
			if(!result) break;
			
			ret = oe_write_page(&ve->og, ft->fp);
			if(!ret)
				return (ST_EOF);

			if(ogg_page_eos(&ve->og))
				eos = 1;
		}
	}
 

	return (ST_SUCCESS);	
}

int st_vorbisstopwrite(ft_t ft) 
{
	vorbis_t vb = (vorbis_t) ft->priv;
	vorbis_enc_t *ve = vb->vorbis_enc_data;

	/* Close out the remaining data */
	st_vorbiswrite(ft, NULL, 0);

	ogg_stream_clear(&ve->os);
	vorbis_block_clear(&ve->vb);
	vorbis_dsp_clear(&ve->vd);
	vorbis_info_clear(&ve->vi);

	return (ST_SUCCESS);
}

#endif /* HAVE_LIBVORBIS */
