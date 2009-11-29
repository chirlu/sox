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

/* In order to use this format with SoX, first build & install:
 *   http://ftp.penguin.cz/pub/users/utx/amr/amrnb-w.x.y.z.tar.bz2
 * or install equivalent package(s) e.g. marillat.
 */

#include "sox_i.h"

#ifdef HAVE_AMRNB

#ifdef HAVE_OPENCORE_AMRNB_INTERF_DEC_H

  enum Mode { amrnb_mode_dummy };

  int Encoder_Interface_Encode(void* state, enum Mode mode, const short* speech, unsigned char* out, int forceSpeech);
  void* Encoder_Interface_init(int dtx);
  void Encoder_Interface_exit(void* state);
  void Decoder_Interface_Decode(void* state, const unsigned char* in, short* out, int bfi);
  void* Decoder_Interface_init(void);
  void Decoder_Interface_exit(void* state);

#define AMR_FUNC_ENTRIES(f,x) \
  AMR_FUNC(f,x, int, Encoder_Interface_Encode, (void* state, enum Mode mode, const short* speech, unsigned char* out, int forceSpeech)) \
  AMR_FUNC(f,x, void*, Encoder_Interface_init, (int dtx)) \
  AMR_FUNC(f,x, void, Encoder_Interface_exit, (void* state)) \
  AMR_FUNC(f,x, void, Decoder_Interface_Decode, (void* state, const unsigned char* in, short* out, int bfi)) \
  AMR_FUNC(f,x, void*, Decoder_Interface_init, (void)) \
  AMR_FUNC(f,x, void, Decoder_Interface_exit, (void* state))

#define D_IF_decode         Decoder_Interface_Decode
#define D_IF_exit           Decoder_Interface_exit
#define D_IF_init           Decoder_Interface_init
#define E_IF_encode         Encoder_Interface_Encode
#define E_IF_exit           Encoder_Interface_exit
#define E_IF_init()         Encoder_Interface_init(1)

#else /* HAVE_OPENCORE_AMRNB_INTERF_DEC_H */

enum amrnb_mode { amrnb_mode_dummy };

int GP3VADxEncoder_Interface_Encode(void *st, enum amrnb_mode mode, short *speech, unsigned char *serial, int forceSpeech, char vad2_code);
void *VADxEncoder_Interface_init(int dtx, char vad2_code);
void Encoder_Interface_exit(void *state);
void GP3Decoder_Interface_Decode(void *st, unsigned char *bits, short *synth, int bfi);
void *Decoder_Interface_init(void);
void Decoder_Interface_exit(void *state);

#define AMR_FUNC_ENTRIES(f,x) \
  AMR_FUNC(f,x, int, GP3VADxEncoder_Interface_Encode, (void *st, enum amrnb_mode mode, short *speech, unsigned char *serial, int forceSpeech, char vad2_code)) \
  AMR_FUNC(f,x, void*, VADxEncoder_Interface_init, (int dtx, char vad2_code)) \
  AMR_FUNC(f,x, void, Encoder_Interface_exit, (void *state)) \
  AMR_FUNC(f,x, void, GP3Decoder_Interface_Decode, (void *st, unsigned char *bits, short *synth, int bfi)) \
  AMR_FUNC(f,x, void*, Decoder_Interface_init, (void)) \
  AMR_FUNC(f,x, void, Decoder_Interface_exit, (void *state))

#define E_IF_encode(st,m,sp,ser,fs) \
                            GP3VADxEncoder_Interface_Encode(st,m,sp,ser,fs,0)
#define E_IF_init()         VADxEncoder_Interface_init(1,0)
#define E_IF_exit           Encoder_Interface_exit
#define D_IF_decode         GP3Decoder_Interface_Decode
#define D_IF_init           Decoder_Interface_init
#define D_IF_exit           Decoder_Interface_exit

#endif /* HAVE_OPENCORE_AMRNB_INTERF_DEC_H */

static const unsigned amrnb_block_size[] = {13,14,16,18,20,21,27,32,6,0,0,0,0,0,0,1};
#define block_size amrnb_block_size

static char const magic[] = "#!AMR\n";
#define AMR_CODED_MAX       32                  /* max coded size */
#define AMR_ENCODING        SOX_ENCODING_AMR_NB
#define AMR_FORMAT_FN       lsx_amr_nb_format_fn
#define AMR_FRAME           160                 /* 20ms @ 8kHz */
#define AMR_MODE_MAX        7
#define AMR_NAMES           "amr-nb", "anb"
#define AMR_RATE            8000
#define AMR_DESC            "amr-nb library"

#if !defined(HAVE_LIBLTDL)
#undef DL_AMRNB
#endif

static const char* const amr_library_names[] =
{
#ifdef DL_AMRNB
#ifdef HAVE_OPENCORE_AMRNB_INTERF_DEC_H
  "libopencore-amrnb",
#else
  "libamrnb-3",
  "libamrnb",
  "amrnb",
#endif
#endif
  NULL
};

#ifdef DL_AMRNB
  #define AMR_FUNC  LSX_DLENTRY_DYNAMIC
#else
  #define AMR_FUNC  LSX_DLENTRY_STATIC
#endif /* DL_AMRNB */

#include "amr.h"

#endif /* HAVE_AMRNB */
