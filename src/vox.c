
/************************************************************************
 *                                   SOX                                *
 *                                                                      *
 *                       AUDIO FILE PROCESSING UTILITY                  *
 *                                                                      *
 * Project : SOX                                                        *
 * File    : vox.c                                                      *
 *                                                                      *
 * Version History : V12.17.4 - Tony Seebregts                          *
 *                              5 May 2004                              *
 *                                                                      *
 * Description : SOX file format handler for Dialogic/Oki ADPCM VOX     *
 *               files.                                                 *
 *                                                                      *
 * Notes : Coded from SOX skeleton code supplied with SOX source.       *
 *                                                                      *
 ************************************************************************/

/************************************************************************
 * July 5, 1991                                                         *
 *                                                                      *
 * Copyright 1991 Lance Norskog And Sundry Contributors                 *
 *                                                                      *
 * This source code is freely redistributable and may be used for any   *
 * purpose.  This copyright notice must be maintained.                  *
 *                                                                      *
 * Lance Norskog And Sundry Contributors are not responsible for the    *
 * consequences of using this software.                                 *
 *                                                                      *
 ************************************************************************/

#include "adpcms.h"
#include "st_i.h"

typedef struct voxstuff
{
  struct adpcm_struct encoder;

  struct {
    uint8_t byte;               /* write store */
    uint8_t flag;
  } store;
  st_fileinfo_t file;
} *vox_t;


/******************************************************************************
 * Function   : voxstart
 * Description: Initialises the file parameters and ADPCM codec state.
 * Parameters : ft  - file info structure
 * Returns    : int - ST_SUCCESS
 *                    ST_EOF
 * Exceptions :
 * Notes      : 1. VOX file format is 4-bit OKI ADPCM that decodes to 
 *                 to 12 bit signed linear PCM.
 *              2. Dialogic only supports 6kHz, 8kHz and 11 kHz sampling
 *                 rates but the codecs allows any user specified rate. 
 ******************************************************************************/

static int voxstart(ft_t ft)
{
  vox_t state = (vox_t) ft->priv;

  /* ... setup file info */
  state->file.buf = (char *) xmalloc(ST_BUFSIZ);
  state->file.size = ST_BUFSIZ;
  state->file.count = 0;
  state->file.pos = 0;
  state->file.eof = 0;
  state->store.byte = 0;
  state->store.flag = 0;

  adpcm_init(&state->encoder, 1);
  ft->signal.channels = 1;
  return st_rawstart(ft, st_true, st_false, ST_ENCODING_OKI_ADPCM, ST_SIZE_16BIT, ST_OPTION_DEFAULT);
}


static int imastart(ft_t ft)
{
  vox_t state = (vox_t) ft->priv;

  /* ... setup file info */
  state->file.buf = (char *) xmalloc(ST_BUFSIZ);
  state->file.size = ST_BUFSIZ;
  state->file.count = 0;
  state->file.pos = 0;
  state->file.eof = 0;
  state->store.byte = 0;
  state->store.flag = 0;

  adpcm_init(&state->encoder, 0);
  ft->signal.channels = 1;
  return st_rawstart(ft, st_true, st_false, ST_ENCODING_IMA_ADPCM, ST_SIZE_16BIT, ST_OPTION_DEFAULT);
}


/******************************************************************************
 * Function   : read 
 * Description: Fills an internal buffer from the VOX file, converts the 
 *              OKI ADPCM 4-bit samples to 12-bit signed PCM and then scales 
 *              the samples to full range 16 bit PCM.
 * Parameters : ft     - file info structure
 *              buffer - output buffer
 *              length - size of output buffer
 * Returns    : int    - number of samples returned in buffer
 * Exceptions :
 * Notes      : 
 ******************************************************************************/

static st_size_t read(ft_t ft, st_sample_t * buffer, st_size_t len)
{
  vox_t state = (vox_t) ft->priv;
  st_size_t n;
  uint8_t byte;

  for (n = 0; n < (len&~1) && st_readb(ft, &byte) == ST_SUCCESS; n += 2) {
    short word = adpcm_decode(byte >> 4, &state->encoder);
    *buffer++ = ST_SIGNED_WORD_TO_SAMPLE(word, ft->clips);

    word = adpcm_decode(byte, &state->encoder);
    *buffer++ = ST_SIGNED_WORD_TO_SAMPLE(word, ft->clips);
  }
  return n;
}

/******************************************************************************
 * Function   : stopread 
 * Description: Frees the internal buffer allocated in voxstart/imastart.
 * Parameters : ft   - file info structure
 * Returns    : int  - ST_SUCCESS
 * Exceptions :
 * Notes      : 
 ******************************************************************************/

static int stopread(ft_t ft)
{
  vox_t state = (vox_t) ft->priv;

  free(state->file.buf);

  return (ST_SUCCESS);
}


/******************************************************************************
 * Function   : write
 * Description: Converts the supplied buffer to 12 bit linear PCM and encodes
 *              to OKI ADPCM 4-bit samples (packed a two nibbles per byte).
 * Parameters : ft     - file info structure
 *              buffer - output buffer
 *              length - size of output buffer
 * Returns    : int    - ST_SUCCESS
 *                       ST_EOF
 * Exceptions :
 * Notes      : 
 ******************************************************************************/

static st_size_t write(ft_t ft, const st_sample_t * buffer, st_size_t length)
{
  vox_t state = (vox_t) ft->priv;
  st_size_t count = 0;
  uint8_t byte = state->store.byte;
  uint8_t flag = state->store.flag;
  short word;

  while (count < length) {
    word = ST_SAMPLE_TO_SIGNED_WORD(*buffer++, ft->clips);

    byte <<= 4;
    byte |= adpcm_encode(word, &state->encoder) & 0x0F;

    flag++;
    flag %= 2;

    if (flag == 0) {
      state->file.buf[state->file.count++] = byte;

      if (state->file.count >= state->file.size) {
        st_writebuf(ft, state->file.buf, 1, state->file.count);

        state->file.count = 0;
      }
    }

    count++;
  }

  /* ... keep last byte across calls */

  state->store.byte = byte;
  state->store.flag = flag;

  return (count);
}

/******************************************************************************
 * Function   : stopwrite
 * Description: Flushes any leftover samples and frees the internal buffer 
 *              allocated in voxstart/imastart.
 * Parameters : ft   - file info structure
 * Returns    : int  - ST_SUCCESS
 * Exceptions :
 * Notes      : 
 ******************************************************************************/

static int stopwrite(ft_t ft)
{
  vox_t state = (vox_t) ft->priv;
  uint8_t byte = state->store.byte;
  uint8_t flag = state->store.flag;

  /* ... flush remaining samples */

  if (flag != 0) {
    byte <<= 4;
    byte |= adpcm_encode(0, &state->encoder) & 0x0F;

    state->file.buf[state->file.count++] = byte;
  }

  if (state->file.count > 0)
    st_writebuf(ft, state->file.buf, 1, state->file.count);

  free(state->file.buf);

  return (ST_SUCCESS);
}

const st_format_t *st_vox_format_fn(void)
{
  static char const * names[] = {"vox", NULL};
  static st_format_t driver = {
    names, NULL, 0,
    voxstart, read, stopread,
    voxstart, write, stopwrite,
    st_format_nothing_seek
  };
  return &driver;
}

const st_format_t *st_ima_format_fn(void)
{
  static char const * names[] = {"ima", NULL};
  static st_format_t driver = {
    names, NULL, 0,
    imastart, read, stopread,
    imastart, write, stopwrite,
    st_format_nothing_seek
  };
  return &driver;
}
