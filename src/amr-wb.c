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

/* File format: AMR-WB   (c) 2007 robs@users.sourceforge.net */

#include "st_i.h"
#include "../amr-wb/amr-wb.h"
#include <string.h>
#include <math.h>

static char const magic[] = "#!AMR-WB\n";

typedef struct amr_wb
{
  RX_State * rx_state;
  TX_State * tx_state;
  void * state;
  Word16 coding_mode;
  Word16 mode_previous;
  st_bool reset;
  st_bool reset_previous;
  Word16 pcm[L_FRAME16k];
  st_size_t pcm_index;
} * amr_wb_t;

assert_static(sizeof(struct amr_wb) <= ST_MAX_FILE_PRIVSIZE,
              /* else */ amr_wb_PRIVSIZE_too_big);

#define ENCODING 2 /* 0..2 */

static st_size_t decode_1_frame(ft_t ft)
{
  amr_wb_t this = (amr_wb_t) ft->priv;
  Word16 nb_bits, i;
  Word16 mode, frame_type, frame_length;
  Word16 prms[NB_BITS_MAX];

  nb_bits = Read_serial(ft->fp, prms, &frame_type, &mode, this->rx_state, ENCODING);
  if (nb_bits == 0)
    return L_FRAME16k;

  if (frame_type == RX_NO_DATA || frame_type == RX_SPEECH_LOST) {
    mode = this->mode_previous;
    this->reset = st_false;
  } else {
    this->mode_previous = mode;

    if (this->reset_previous)
      this->reset = decoder_homing_frame_test_first(prms, mode);
  }
  if (this->reset && this->reset_previous)
    for (i = 0; i < L_FRAME16k; i++)
      this->pcm[i] = EHF_MASK;
  else
    decoder(mode, prms, this->pcm, &frame_length, this->state, frame_type);
  if (!this->reset_previous)
    this->reset = decoder_homing_frame_test(prms, mode);
  if (this->reset)
    Reset_decoder(this->state, 1);
  this->reset_previous = this->reset;
  return 0;
}

static void encode_1_frame(ft_t ft)
{
  amr_wb_t this = (amr_wb_t) ft->priv;
  st_size_t i;
  Word16 nb_bits;
  Word16 prms[NB_BITS_MAX];
  Word16 reset = encoder_homing_frame_test(this->pcm);
  Word16 mode = this->coding_mode;

  for (i = 0; i < L_FRAME16k; ++i)
    this->pcm[i] = this->pcm[i] & ~3;
  coder(&mode, this->pcm, prms, &nb_bits, this->state, 1);
  Write_serial(ft->fp, prms, mode, this->coding_mode, this->tx_state, ENCODING);
  if (reset)
    Reset_encoder(this->state, 1);
}

static void set_format(ft_t ft)
{
  ft->signal.rate = 16000;
  ft->signal.size = ST_SIZE_16BIT;
  ft->signal.encoding = ST_ENCODING_AMR_WB;
  ft->signal.channels = 1;
}

static int startread(ft_t ft)
{
  amr_wb_t this = (amr_wb_t) ft->priv;

  this->reset_previous = st_true;
  this->pcm_index = L_FRAME16k;

  Init_decoder(&this->state);
  Init_read_serial(&this->rx_state);

  if (ENCODING == 2) {
    char buffer[sizeof(magic)];

    fread(buffer, sizeof(char), sizeof(buffer) - 1, ft->fp);
    buffer[sizeof(buffer) - 1] = 0;
    if (strcmp(buffer, magic)) {
      st_fail("Invalid magic number");
      return ST_EOF;
    }
  }
  set_format(ft);
  return ST_SUCCESS;
}

static st_size_t read(ft_t ft, st_sample_t * buf, st_size_t len)
{
  amr_wb_t this = (amr_wb_t) ft->priv;
  st_size_t done;

  for (done = 0; done < len; done++) {
    if (this->pcm_index >= L_FRAME16k)
      this->pcm_index = decode_1_frame(ft);
    if (this->pcm_index >= L_FRAME16k)
      break;
    *buf++ = ST_SIGNED_WORD_TO_SAMPLE(0xfffc & this->pcm[this->pcm_index++], ft->clips);
  }
  return done;
}

static int stopread(ft_t ft)
{
  amr_wb_t this = (amr_wb_t) ft->priv;
  Close_decoder(this->state);
  Close_read_serial(this->rx_state);
  return ST_SUCCESS;
}

static int startwrite(ft_t ft)
{
  amr_wb_t this = (amr_wb_t) ft->priv;

  if (ft->signal.compression != HUGE_VAL) {
    this->coding_mode = ft->signal.compression;
    if (this->coding_mode != ft->signal.compression || this->coding_mode > 8) {
      st_fail_errno(ft, ST_EINVAL, "compression level must be a whole number from 0 to 8");
      return ST_EOF;
    }
  }
  else this->coding_mode = 0;

  set_format(ft);
  Init_coder(&this->state);
  Init_write_serial(&this->tx_state);
  if (ENCODING == 2)
    st_writes(ft, magic);
  this->pcm_index = 0;
  return ST_SUCCESS;
}

static st_size_t write(ft_t ft, const st_sample_t * buf, st_size_t len)
{
  amr_wb_t this = (amr_wb_t) ft->priv;
  st_size_t done;

  for (done = 0; done < len; ++done) {
    this->pcm[this->pcm_index++] = (Word16) (ST_SAMPLE_TO_SIGNED_WORD(*buf++, ft->clips));
    if (this->pcm_index == L_FRAME16k) {
      this->pcm_index = 0;
      encode_1_frame(ft);
    }
  }
  return done;
}

static int stopwrite(ft_t ft)
{
  amr_wb_t this = (amr_wb_t) ft->priv;

  if (this->pcm_index) {
    do {
      this->pcm[this->pcm_index++] = 0;
    } while (this->pcm_index < L_FRAME16k);
    encode_1_frame(ft);
  }
  Close_coder(this->state);
  Close_write_serial(this->tx_state);
  return ST_SUCCESS;
}

st_format_t const * st_amr_wb_format_fn(void)
{
  static char const * names[] = {"amr-wb", NULL};
  static st_format_t driver = {
    names, NULL, 0,
    startread, read, stopread,
    startwrite, write, stopwrite,
    st_format_nothing_seek
  };
  return &driver;
}
