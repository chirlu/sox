/*
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, write to the Free Software Foundation,
 * Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301, USA.
 */

/* ADPCM CODECs: IMA, OKI.   (c) 2007 robs@users.sourceforge.net */

#include "sox_i.h"
#include "adpcms.h"

static int const ima_steps[89] = { /* ~16-bit precision */
  7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
  50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
  253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
  1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
  3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
  11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
  32767 };

static int const oki_steps[49] = { /* ~12-bit precision */
  256, 272, 304, 336, 368, 400, 448, 496, 544, 592, 656, 720, 800, 880, 960,
  1056, 1168, 1280, 1408, 1552, 1712, 1888, 2080, 2288, 2512, 2768, 3040, 3344,
  3680, 4048, 4464, 4912, 5392, 5936, 6528, 7184, 7904, 8704, 9568, 10528,
  11584, 12736, 14016, 15408, 16960, 18656, 20512, 22576, 24832 };

static int const step_changes[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

static void adpcm_init(adpcm_t state, int type)
{
  state->last_output = 0;
  state->step_index = 0;
  state->max_step_index = type? 48 : 88;
  state->steps = type? oki_steps : ima_steps;
  state->mask = type? ~15 : ~0;
}

static int adpcm_decode(int code, adpcm_t state)
{
  int s = ((code & 7) << 1) | 1;
  s = ((state->steps[state->step_index] * s) >> 3) & state->mask;
  if (code & 8)
    s = -s;
  s += state->last_output;
  s = range_limit(s, -0x8000, 0x7fff);
  state->step_index += step_changes[code & 0x07];
  state->step_index = range_limit(state->step_index, 0, state->max_step_index);
  return state->last_output = s;
}

static int adpcm_encode(int sample, adpcm_t state)
{
  int delta = sample - state->last_output;
  int sign = 0;
  int code;
  if (delta < 0) {
    sign = 0x08;
    delta = -delta;
  }
  code = 4 * delta / state->steps[state->step_index];
  code = sign | min(code, 7);
  adpcm_decode(code, state); /* Update encoder state */
  return code;
}


/*
 * Format methods
 *
 * Almost like the raw format functions, but cannot be used directly
 * since they require an additional state parameter.
 */

/******************************************************************************
 * Function   : sox_adpcm_reset
 * Description: Resets the ADPCM codec state.
 * Parameters : state - ADPCM state structure
 *              type - SOX_ENCODING_OKI_ADPCM or SOX_ENCODING_IMA_ADPCM
 * Returns    :
 * Exceptions :
 * Notes      : 1. This function is used for framed ADPCM formats to reset
 *                 the decoder between frames.
 ******************************************************************************/

void sox_adpcm_reset(adpcm_io_t state, sox_encoding_t type)
{
  state->file.count = 0;
  state->file.pos = 0;
  state->store.byte = 0;
  state->store.flag = 0;

  adpcm_init(&state->encoder, (type == SOX_ENCODING_OKI_ADPCM) ? 1 : 0);
}

/******************************************************************************
 * Function   : adpcm_start
 * Description: Initialises the file parameters and ADPCM codec state.
 * Parameters : ft  - file info structure
 *              state - ADPCM state structure
 *              type - SOX_ENCODING_OKI_ADPCM or SOX_ENCODING_IMA_ADPCM
 * Returns    : int - SOX_SUCCESS
 *                    SOX_EOF
 * Exceptions :
 * Notes      : 1. This function can be used as a startread or
 *                 startwrite method.
 *              2. VOX file format is 4-bit OKI ADPCM that decodes to 
 *                 to 12 bit signed linear PCM.
 *              3. Dialogic only supports 6kHz, 8kHz and 11 kHz sampling
 *                 rates but the codecs allows any user specified rate.
 ******************************************************************************/

static int adpcm_start(sox_format_t * ft, adpcm_io_t state, sox_encoding_t type)
{
  /* setup file info */
  state->file.buf = (char *) xmalloc(sox_bufsiz);
  state->file.size = sox_bufsiz;
  ft->signal.channels = 1;

  sox_adpcm_reset(state, type);
  
  return sox_rawstart(ft, sox_true, sox_false, type, SOX_SIZE_16BIT);
}

int sox_adpcm_oki_start(sox_format_t * ft, adpcm_io_t state)
{
  return adpcm_start(ft, state, SOX_ENCODING_OKI_ADPCM);
}

int sox_adpcm_ima_start(sox_format_t * ft, adpcm_io_t state)
{
  return adpcm_start(ft, state, SOX_ENCODING_IMA_ADPCM);
}

/******************************************************************************
 * Function   : sox_adpcm_read 
 * Description: Fills an internal buffer from the VOX file, converts the 
 *              OKI ADPCM 4-bit samples to 12-bit signed PCM and then scales 
 *              the samples to full range 16 bit PCM.
 * Parameters : ft     - file info structure
 *              state  - ADPCM state structure
 *              buffer - output buffer
 *              length - size of output buffer
 * Returns    : int    - number of samples returned in buffer
 * Exceptions :
 * Notes      : 
 ******************************************************************************/

sox_size_t sox_adpcm_read(sox_format_t * ft, adpcm_io_t state, sox_ssample_t * buffer, sox_size_t len)
{
  sox_size_t n;
  uint8_t byte;

  for (n = 0; n < (len&~1u) && sox_readb(ft, &byte) == SOX_SUCCESS; n += 2) {
    short word = adpcm_decode(byte >> 4, &state->encoder);
    *buffer++ = SOX_SIGNED_16BIT_TO_SAMPLE(word, ft->clips);

    word = adpcm_decode(byte, &state->encoder);
    *buffer++ = SOX_SIGNED_16BIT_TO_SAMPLE(word, ft->clips);
  }
  return n;
}

/******************************************************************************
 * Function   : stopread 
 * Description: Frees the internal buffer allocated in voxstart/imastart.
 * Parameters : ft   - file info structure
 *              state  - ADPCM state structure
 * Returns    : int  - SOX_SUCCESS
 * Exceptions :
 * Notes      : 
 ******************************************************************************/

int sox_adpcm_stopread(sox_format_t * ft UNUSED, adpcm_io_t state)
{
  free(state->file.buf);

  return (SOX_SUCCESS);
}


/******************************************************************************
 * Function   : write
 * Description: Converts the supplied buffer to 12 bit linear PCM and encodes
 *              to OKI ADPCM 4-bit samples (packed a two nibbles per byte).
 * Parameters : ft     - file info structure
 *              state  - ADPCM state structure
 *              buffer - output buffer
 *              length - size of output buffer
 * Returns    : int    - SOX_SUCCESS
 *                       SOX_EOF
 * Exceptions :
 * Notes      : 
 ******************************************************************************/

sox_size_t sox_adpcm_write(sox_format_t * ft, adpcm_io_t state, const sox_ssample_t * buffer, sox_size_t length)
{
  sox_size_t count = 0;
  uint8_t byte = state->store.byte;
  uint8_t flag = state->store.flag;
  short word;

  while (count < length) {
    word = SOX_SAMPLE_TO_SIGNED_16BIT(*buffer++, ft->clips);

    byte <<= 4;
    byte |= adpcm_encode(word, &state->encoder) & 0x0F;

    flag = !flag;

    if (flag == 0) {
      state->file.buf[state->file.count++] = byte;

      if (state->file.count >= state->file.size) {
        sox_writebuf(ft, state->file.buf, state->file.count);

        state->file.count = 0;
      }
    }

    count++;
  }

  /* keep last byte across calls */

  state->store.byte = byte;
  state->store.flag = flag;

  return (count);
}

/******************************************************************************
 * Function   : sox_adpcm_flush
 * Description: Flushes any leftover samples.
 * Parameters : ft   - file info structure
 *              state  - ADPCM state structure
 * Returns    :
 * Exceptions :
 * Notes      : 1. Called directly for writing framed formats
 ******************************************************************************/

void sox_adpcm_flush(sox_format_t * ft, adpcm_io_t state)
{
  uint8_t byte = state->store.byte;
  uint8_t flag = state->store.flag;

  /* flush remaining samples */

  if (flag != 0) {
    byte <<= 4;
    byte |= adpcm_encode(0, &state->encoder) & 0x0F;

    state->file.buf[state->file.count++] = byte;
  }

  if (state->file.count > 0)
    sox_writebuf(ft, state->file.buf, state->file.count);
}

/******************************************************************************
 * Function   : sox_adpcm_stopwrite
 * Description: Flushes any leftover samples and frees the internal buffer 
 *              allocated in voxstart/imastart.
 * Parameters : ft   - file info structure
 *              state  - ADPCM state structure
 * Returns    : int  - SOX_SUCCESS
 * Exceptions :
 * Notes      : 
 ******************************************************************************/

int sox_adpcm_stopwrite(sox_format_t * ft, adpcm_io_t state)
{
  sox_adpcm_flush(ft, state);

  free(state->file.buf);

  return (SOX_SUCCESS);
}
