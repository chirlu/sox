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

/* In order to use this format with SoX, first obtain, build & install:
 *   http://ftp.penguin.cz/pub/users/utx/amr/amrwb-7.0.0.0.tar.bz2
 */

#ifdef HAVE_LIBAMRWB

#include "sox_i.h"
#include "amrwb/typedef.h"
#include "amrwb/enc_if.h"
#include "amrwb/dec_if.h"
#include "amrwb/if_rom.h"
#include <string.h>
#include <math.h>

static char const magic[] = "#!AMR-WB\n";

typedef struct amr_wb
{
  void * state;
  Word16 coding_mode;
  Word16 pcm[L_FRAME16k];
  sox_size_t pcm_index;
} * amr_wb_t;

assert_static(sizeof(struct amr_wb) <= SOX_MAX_FILE_PRIVSIZE,
              /* else */ amr_wb_PRIVSIZE_too_big);

static sox_size_t decode_1_frame(ft_t ft)
{
  amr_wb_t this = (amr_wb_t) ft->priv;
  Word16 mode;
  UWord8 serial[NB_SERIAL_MAX];

  if (fread(serial, sizeof(UWord8), 1, ft->fp) != 1)
    return L_FRAME16k;
  mode = (Word16)((serial[0] >> 3) & 0x0F);
  if (fread(&serial[1], sizeof(UWord8), block_size[mode] - 1, ft->fp) != block_size[mode] - 1)
    return L_FRAME16k;
  D_IF_decode(this->state, serial, this->pcm, _good_frame);
  return 0;
}

static void encode_1_frame(ft_t ft)
{
  amr_wb_t this = (amr_wb_t) ft->priv;
  UWord8 serial[NB_SERIAL_MAX];
  Word16 mode = this->coding_mode;
  Word32 serial_size = E_IF_encode(this->state, mode, this->pcm, serial, 1);
  fwrite(serial, 1, serial_size, ft->fp);
}

static void set_format(ft_t ft)
{
  ft->signal.rate = 16000;
  ft->signal.size = SOX_SIZE_16BIT;
  ft->signal.encoding = SOX_ENCODING_AMR_WB;
  ft->signal.channels = 1;
}

static int startread(ft_t ft)
{
  amr_wb_t this = (amr_wb_t) ft->priv;
  char buffer[sizeof(magic)];

  this->pcm_index = L_FRAME16k;

  this->state = D_IF_init();

  fread(buffer, sizeof(char), sizeof(buffer) - 1, ft->fp);
  buffer[sizeof(buffer) - 1] = 0;
  if (strcmp(buffer, magic)) {
    sox_fail("Invalid magic number");
    return SOX_EOF;
  }
  set_format(ft);
  return SOX_SUCCESS;
}

static sox_size_t read(ft_t ft, sox_ssample_t * buf, sox_size_t len)
{
  amr_wb_t this = (amr_wb_t) ft->priv;
  sox_size_t done;

  for (done = 0; done < len; done++) {
    if (this->pcm_index >= L_FRAME16k)
      this->pcm_index = decode_1_frame(ft);
    if (this->pcm_index >= L_FRAME16k)
      break;
    *buf++ = SOX_SIGNED_16BIT_TO_SAMPLE(this->pcm[this->pcm_index++], ft->clips);
  }
  return done;
}

static int stopread(ft_t ft)
{
  amr_wb_t this = (amr_wb_t) ft->priv;
  D_IF_exit(this->state);
  return SOX_SUCCESS;
}

static int startwrite(ft_t ft)
{
  amr_wb_t this = (amr_wb_t) ft->priv;

  if (ft->signal.compression != HUGE_VAL) {
    this->coding_mode = ft->signal.compression;
    if (this->coding_mode != ft->signal.compression || this->coding_mode > 8) {
      sox_fail_errno(ft, SOX_EINVAL, "compression level must be a whole number from 0 to 8");
      return SOX_EOF;
    }
  }
  else this->coding_mode = 0;

  set_format(ft);
  this->state = E_IF_init();
  sox_writes(ft, magic);
  this->pcm_index = 0;
  return SOX_SUCCESS;
}

static sox_size_t write(ft_t ft, const sox_ssample_t * buf, sox_size_t len)
{
  amr_wb_t this = (amr_wb_t) ft->priv;
  sox_size_t done;

  for (done = 0; done < len; ++done) {
    this->pcm[this->pcm_index++] = (Word16) (SOX_SAMPLE_TO_SIGNED_16BIT(*buf++, ft->clips));
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
  E_IF_exit(this->state);
  return SOX_SUCCESS;
}

const sox_format_t *sox_amr_wb_format_fn(void);

const sox_format_t *sox_amr_wb_format_fn(void)
{
  static char const * names[] = {"amr-wb", "awb", NULL};
  static sox_format_t driver = {
    names, 0,
    startread, read, stopread,
    startwrite, write, stopwrite,
    sox_format_nothing_seek
  };
  return &driver;
}

#endif
