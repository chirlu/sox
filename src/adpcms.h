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

typedef struct adpcm_struct
{
  int last_output;
  int step_index;
  int max_step_index;
  int const * steps;
  int mask;
} * adpcm_t;

typedef struct adpcm_io {
  struct adpcm_struct encoder;
  struct {
    uint8_t byte;               /* write store */
    uint8_t flag;
  } store;
  st_fileinfo_t file;
} *adpcm_io_t;

/* Format methods */
void st_adpcm_reset(adpcm_io_t state, st_encoding_t type);
int st_adpcm_oki_start(ft_t ft, adpcm_io_t state);
int st_adpcm_ima_start(ft_t ft, adpcm_io_t state);
st_size_t st_adpcm_read(ft_t ft, adpcm_io_t state, st_sample_t *buffer, st_size_t len);
int st_adpcm_stopread(ft_t ft, adpcm_io_t state);
st_size_t st_adpcm_write(ft_t ft, adpcm_io_t state, const st_sample_t *buffer, st_size_t length);
void st_adpcm_flush(ft_t ft, adpcm_io_t state);
int st_adpcm_stopwrite(ft_t ft, adpcm_io_t state);
