/* File format: AMR-NB   (c) 2007 robs@users.sourceforge.net
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

#ifdef HAVE_AMRNB

/* Common definitions: */

enum amrnb_mode { amrnb_mode_dummy };

static const unsigned amrnb_block_size[] = {13, 14, 16, 18, 20, 21, 27, 32, 6, 0, 0, 0, 0, 0, 0, 1};
static char const amrnb_magic[] = "#!AMR\n";
#define amr_block_size amrnb_block_size
#define amr_magic amrnb_magic
#define amr_priv_t amrnb_priv_t
#define amr_opencore_funcs amrnb_opencore_funcs
#define amr_gp3_funcs amrnb_gp3_funcs

#define AMR_CODED_MAX       32                  /* max coded size */
#define AMR_ENCODING        SOX_ENCODING_AMR_NB
#define AMR_FORMAT_FN       lsx_amr_nb_format_fn
#define AMR_FRAME           160                 /* 20ms @ 8kHz */
#define AMR_MODE_MAX        7
#define AMR_NAMES           "amr-nb", "anb"
#define AMR_RATE            8000
#define AMR_DESC            "3GPP Adaptive Multi Rate Narrow-Band (AMR-NB) lossy speech compressor"

#if !defined(HAVE_LIBLTDL)
  #undef DL_AMRNB
#endif

#ifdef DL_AMRNB
  #define AMR_FUNC  LSX_DLENTRY_DYNAMIC
#else
  #define AMR_FUNC  LSX_DLENTRY_STATIC
#endif /* DL_AMRNB */

/* OpenCore definitions: */

#if defined(HAVE_OPENCORE_AMRNB_INTERF_DEC_H) || defined(DL_AMRNB)
  #define AMR_OPENCORE 1
  #define AMR_OPENCORE_ENABLE_ENCODE 1
#endif

#define AMR_OPENCORE_FUNC_ENTRIES(f,x) \
  AMR_FUNC(f,x, void*, Encoder_Interface_init,   (int dtx)) \
  AMR_FUNC(f,x, int,   Encoder_Interface_Encode, (void* state, enum amrnb_mode mode, const short* in, unsigned char* out, int forceSpeech)) \
  AMR_FUNC(f,x, void,  Encoder_Interface_exit,   (void* state)) \
  AMR_FUNC(f,x, void*, Decoder_Interface_init,   (void)) \
  AMR_FUNC(f,x, void,  Decoder_Interface_Decode, (void* state, const unsigned char* in, short* out, int bfi)) \
  AMR_FUNC(f,x, void,  Decoder_Interface_exit,   (void* state)) \

#define AmrOpencoreEncoderInit() \
  Encoder_Interface_init(1)
#define AmrOpencoreEncoderEncode(state, mode, in, out, forceSpeech) \
  Encoder_Interface_Encode(state, mode, in, out, forceSpeech)
#define AmrOpencoreEncoderExit(state) \
  Encoder_Interface_exit(state)
#define AmrOpencoreDecoderInit() \
  Decoder_Interface_init()
#define AmrOpencoreDecoderDecode(state, in, out, bfi) \
  Decoder_Interface_Decode(state, in, out, bfi)
#define AmrOpencoreDecoderExit(state) \
  Decoder_Interface_exit(state)

#define AMR_OPENCORE_DESC "amr-nb OpenCore library"
static const char* const amr_opencore_library_names[] =
{
#ifdef DL_AMRWB
  "libopencore-amrnb",
  "libopencore-amrnb-0",
#endif
  NULL
};

/* 3GPP (reference implementation) definitions: */

#if !defined(HAVE_OPENCORE_AMRNB_INTERF_DEC_H) || defined(DL_AMRNB)
  #define AMR_GP3 1
#endif

#define AMR_GP3_FUNC_ENTRIES(f,x) \
  AMR_FUNC(f,x, void*, VADxEncoder_Interface_init,      (int dtx, char vad2_code)) \
  AMR_FUNC(f,x, int,   GP3VADxEncoder_Interface_Encode, (void* state, enum amrnb_mode mode, short* in, unsigned char* out, int forceSpeech, char vad2_code)) \
  AMR_FUNC(f,x, void,  Encoder_Interface_exit,          (void* state)) \
  AMR_FUNC(f,x, void*, Decoder_Interface_init,          (void)) \
  AMR_FUNC(f,x, void,  GP3Decoder_Interface_Decode,     (void* state, unsigned char* in, short* out, int bfi)) \
  AMR_FUNC(f,x, void,  Decoder_Interface_exit,          (void* state)) \

#define AmrGp3EncoderInit() \
  VADxEncoder_Interface_init(1, 0)
#define AmrGp3EncoderEncode(state, mode, in, out, forceSpeech) \
  GP3VADxEncoder_Interface_Encode(state, mode, in, out, forceSpeech, 0)
#define AmrGp3EncoderExit(state) \
  Encoder_Interface_exit(state)
#define AmrGp3DecoderInit() \
  Decoder_Interface_init()
#define AmrGp3DecoderDecode(state, in, out, bfi) \
  GP3Decoder_Interface_Decode(state, in, out, bfi)
#define AmrGp3DecoderExit(state) \
  Decoder_Interface_exit(state)

#define AMR_GP3_DESC "amr-nb 3GPP reference library"
static const char* const amr_gp3_library_names[] =
{
#ifdef DL_AMRWB
  "libamrnb-3",
  "libamrnb",
  "amrnb",
  "cygamrnb-3",
#endif
  NULL
};

#include "amr.h"

#endif /* HAVE_AMRNB */
    
