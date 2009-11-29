/* File format: AMR-WB   (c) 2007 robs@users.sourceforge.net
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* In order to use this format with SoX, first build & install:
 *   http://ftp.penguin.cz/pub/users/utx/amr/amrwb-w.x.y.z.tar.bz2
 * or install equivalent package(s) e.g. marillat.
 */

#include "sox_i.h"

#ifdef HAVE_AMRWB

#ifdef HAVE_OPENCORE_AMRWB_DEC_IF_H

#define DISABLE_AMR_WB_ENCODE

  void D_IF_decode(void* state, const unsigned char* bits, short* synth, int bfi);
  void* D_IF_init(void);
  void D_IF_exit(void* state);

#define AMR_FUNC_ENTRIES(f,x) \
  AMR_FUNC(f,x, void,  D_IF_decode,(void* state, const unsigned char* bits, short* synth, int bfi)) \
  AMR_FUNC(f,x, void*, D_IF_init,  (void)) \
  AMR_FUNC(f,x, void,  D_IF_exit,  (void* state))

#else /* HAVE_OPENCORE_AMRWB_DEC_IF_H */

  int GP3E_IF_encode(void *st, int16_t mode, int16_t *speech, uint8_t *serial, int16_t dtx);
  void *E_IF_init(void);
  void E_IF_exit(void *state);
  void GP3D_IF_decode(void *st, uint8_t *bits, int16_t *synth, int32_t bfi);
  void * D_IF_init(void);
  void D_IF_exit(void *state);

#define AMR_FUNC_ENTRIES(f,x) \
  AMR_FUNC(f,x, int,   GP3E_IF_encode,(void *st, int16_t mode, int16_t *speech, uint8_t *serial, int16_t dtx)) \
  AMR_FUNC(f,x, void*, E_IF_init,     (void)) \
  AMR_FUNC(f,x, void,  E_IF_exit,     (void *state)) \
  AMR_FUNC(f,x, void,  GP3D_IF_decode,(void *st, uint8_t *bits, int16_t *synth, int32_t bfi)) \
  AMR_FUNC(f,x, void*, D_IF_init,     (void)) \
  AMR_FUNC(f,x, void,  D_IF_exit,     (void *state))

#define D_IF_decode GP3D_IF_decode
#define E_IF_encode GP3E_IF_encode

#endif /* HAVE_OPENCORE_AMRWB_DEC_IF_H */

static const uint8_t amrwb_block_size[16]= {18, 24, 33, 37, 41, 47, 51, 59, 61, 6, 6, 0, 0, 0, 1, 1};
#define block_size amrwb_block_size

static char const magic[] = "#!AMR-WB\n";
#define AMR_CODED_MAX       61 /* NB_SERIAL_MAX */
#define AMR_ENCODING        SOX_ENCODING_AMR_WB
#define AMR_FORMAT_FN       lsx_amr_wb_format_fn
#define AMR_FRAME           320 /* L_FRAME16k */
#define AMR_MODE_MAX        8
#define AMR_NAMES           "amr-wb", "awb"
#define AMR_RATE            16000
#define AMR_DESC            "amr-wb library"

#if !defined(HAVE_LIBLTDL)
#undef DL_AMRWB
#endif

static const char* const amr_library_names[] =
{
#ifdef DL_AMRWB
#ifdef HAVE_OPENCORE_AMRWB_DEC_IF_H
  "libopencore-amrwb",
#else
  "libamrwb-3",
  "libamrwb",
  "amrwb",
#endif
#endif
  NULL
};

#ifdef DL_AMRWB
  #define AMR_FUNC  LSX_DLENTRY_DYNAMIC
#else
  #define AMR_FUNC  LSX_DLENTRY_STATIC
#endif /* DL_AMRWB */

#include "amr.h"

#endif /* HAVE_AMRWB */
