/*
 * File format: AMR-WB   (c) 2007 robs@users.sourceforge.net
 * 
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

/* In order to use this format with SoX, first build & install:
 *   http://ftp.penguin.cz/pub/users/utx/amr/amrwb-w.x.y.z.tar.bz2
 * or install equivalent package(s) e.g. marillat.
 */

#include "amrwb/typedef.h"
#include "amrwb/enc_if.h"
#include "amrwb/dec_if.h"
#include "amrwb/if_rom.h"

static char const magic[] = "#!AMR-WB\n";
#define AMR_CODED_MAX       NB_SERIAL_MAX
#define AMR_ENCODING        SOX_ENCODING_AMR_WB
#define AMR_FORMAT_FN       sox_amr_wb_format_fn
#define AMR_FRAME           L_FRAME16k
#define AMR_MODE_MAX        8
#define AMR_NAMES           "amr-wb", "awb"
#define AMR_PRIV_TOO_BIG    amr_wb_PRIVSIZE_too_big
#define AMR_RATE            16000
#include "amr.h"
