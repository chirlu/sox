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
 
/*
 * In order to use the AMR format with SoX, you need to have an AMR
 * library installed at SoX build time. Currently, the SoX build system
 * recognizes two AMR implementations, in the following order:
 *   http://opencore-amr.sourceforge.net/
 *   http://ftp.penguin.cz/pub/users/utx/amr/
 */

#include "sox_i.h"

#ifdef HAVE_AMRWB

/* Common definitions: */

static const uint8_t amrwb_block_size[] = {18, 24, 33, 37, 41, 47, 51, 59, 61, 6, 6, 0, 0, 0, 1, 1};
static char const amrwb_magic[] = "#!AMR-WB\n";
#define amr_block_size amrwb_block_size
#define amr_magic amrwb_magic
#define amr_priv_t amrwb_priv_t
#define amr_opencore_funcs amrwb_opencore_funcs
#define amr_gp3_funcs amrwb_gp3_funcs

#define AMR_CODED_MAX       61 /* NB_SERIAL_MAX */
#define AMR_ENCODING        SOX_ENCODING_AMR_WB
#define AMR_FORMAT_FN       lsx_amr_wb_format_fn
#define AMR_FRAME           320 /* L_FRAME16k */
#define AMR_MODE_MAX        8
#define AMR_NAMES           "amr-wb", "awb"
#define AMR_RATE            16000
#define AMR_DESC            "3GPP Adaptive Multi Rate Wide-Band (AMR-WB) lossy speech compressor"

#if !defined(HAVE_LIBLTDL)
  #undef DL_AMRWB
#endif

#ifdef DL_AMRWB
  #define AMR_FUNC  LSX_DLENTRY_DYNAMIC
#else
  #define AMR_FUNC  LSX_DLENTRY_STATIC
#endif /* DL_AMRWB */

/* OpenCore definitions: */

#if defined(HAVE_OPENCORE_AMRWB_DEC_IF_H) || defined(DL_AMRWB)
  #define AMR_OPENCORE 1
  #define AMR_OPENCORE_ENABLE_ENCODE 0
#endif

#define AMR_OPENCORE_FUNC_ENTRIES(f,x) \
  AMR_FUNC(f,x, void*, D_IF_init,   (void)) \
  AMR_FUNC(f,x, void,  D_IF_decode, (void* state, const unsigned char* in, short* out, int bfi)) \
  AMR_FUNC(f,x, void,  D_IF_exit,   (void* state)) \

#define AmrOpencoreDecoderInit() \
  D_IF_init()
#define AmrOpencoreDecoderDecode(state, in, out, bfi) \
  D_IF_decode(state, in, out, bfi)
#define AmrOpencoreDecoderExit(state) \
  D_IF_exit(state)

#define AMR_OPENCORE_DESC "amr-wb OpenCore library"
static const char* const amr_opencore_library_names[] =
{
#ifdef DL_AMRWB
  "libopencore-amrwb",
  "libopencore-amrwb-0",
#endif
  NULL
};

/* 3GPP (reference implementation) definitions: */

#if !defined(HAVE_OPENCORE_AMRWB_DEC_IF_H) || defined(DL_AMRWB)
  #define AMR_GP3 1
#endif

#define AMR_GP3_FUNC_ENTRIES(f,x) \
  AMR_FUNC(f,x, void*, E_IF_init,     (void)) \
  AMR_FUNC(f,x, int,   GP3E_IF_encode,(void* state, int16_t mode, int16_t* in, uint8_t* out, int16_t dtx)) \
  AMR_FUNC(f,x, void,  E_IF_exit,     (void* state)) \
  AMR_FUNC(f,x, void*, D_IF_init,     (void)) \
  AMR_FUNC(f,x, void,  GP3D_IF_decode,(void* state, uint8_t* in, int16_t* out, int32_t bfi)) \
  AMR_FUNC(f,x, void,  D_IF_exit,     (void* state)) \

#define AmrGp3EncoderInit() \
  E_IF_init()
#define AmrGp3EncoderEncode(state, mode, in, out, forceSpeech) \
  GP3E_IF_encode(state, mode, in, out, forceSpeech)
#define AmrGp3EncoderExit(state) \
  E_IF_exit(state)
#define AmrGp3DecoderInit() \
  D_IF_init()
#define AmrGp3DecoderDecode(state, in, out, bfi) \
  GP3D_IF_decode(state, in, out, bfi)
#define AmrGp3DecoderExit(state) \
  D_IF_exit(state)

#define AMR_GP3_DESC "amr-wb 3GPP reference library"
static const char* const amr_gp3_library_names[] =
{
#ifdef DL_AMRWB
  "libamrwb-3",
  "libamrwb",
  "amrwb",
  "cygamrwb-3",
#endif
  NULL
};

#include "amr.h"

#endif /* HAVE_AMRWB */
