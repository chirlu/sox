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

#include "amrnb/typedef.h"
#include "amrnb/interf_dec.h"
#include "amrnb/sp_dec.h"
#define Mode  _Mode
#define MR102 _MR102
#define MR122 _MR122
#define MR475 _MR475
#define MR515 _MR515
#define MR59  _MR59
#define MR67  _MR67
#define MR74  _MR74
#define MR795 _MR795
#define MRDTX _MRDTX
#include "amrnb/interf_enc.h"

static char const magic[] = "#!AMR\n";
#define AMR_CODED_MAX       32                  /* max coded size */
#define AMR_ENCODING        SOX_ENCODING_AMR_NB
#define AMR_FORMAT_FN       sox_amr_nb_format_fn
#define AMR_FRAME           160                 /* 20ms @ 8kHz */
#define AMR_MODE_MAX        7
#define AMR_NAMES           "amr-nb", "anb"
#define AMR_RATE            8000
#define D_IF_decode         Decoder_Interface_Decode
#define D_IF_exit           Decoder_Interface_exit
#define D_IF_init           Decoder_Interface_init
#define E_IF_encode         Encoder_Interface_Encode
#define E_IF_exit           Encoder_Interface_exit
#define E_IF_init()         Encoder_Interface_init(1)
static unsigned block_size[] = {13,14,16,18,20,21,27,32,6,1,1,1,1,1,1,1};
#include "amr.h"
