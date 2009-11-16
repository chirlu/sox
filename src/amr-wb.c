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

#ifdef HAVE_OPENCORE_AMRWB_DEC_IF_H
#include "opencore-amrwb/dec_if.h"
#include "opencore-amrwb/if_rom.h"
#define DISABLE_AMR_WB_ENCODE
#else
#include "amrwb/typedef.h"
#include "amrwb/enc_if.h"
#include "amrwb/dec_if.h"
#include "amrwb/if_rom.h"
#endif

static char const magic[] = "#!AMR-WB\n";
#define AMR_CODED_MAX       61 /* max serial size */
#define AMR_ENCODING        SOX_ENCODING_AMR_WB
#define AMR_FORMAT_FN       lsx_amr_wb_format_fn
#define AMR_FRAME           320 /* Frame size at 16kHz */
#define AMR_MODE_MAX        8
#define AMR_NAMES           "amr-wb", "awb"
#define AMR_RATE            16000
#ifdef HAVE_OPENCORE_AMRWB_DEC_IF_H
static const unsigned block_size[16] = {18, 24, 33, 37, 41, 47, 51, 59, 61, 6, 6, 0, 0, 0, 1, 1};
#endif
#include "amr.h"
