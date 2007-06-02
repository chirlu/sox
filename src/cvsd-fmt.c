/*
 *      CVSD format module (implementation in cvsd.c)
 *
 *      Copyright (C) 1996-2007 Thomas Sailer and SoX Contributors
 *      Thomas Sailer (sailer@ife.ee.ethz.ch) (HB9JNX/AE4WA)
 *      Swiss Federal Institute of Technology, Electronics Lab
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "sox_i.h"
#include "cvsd.h"
 
 /* Cont. Variable Slope Delta */
static const char *cvsdnames[] = {
  "cvs",
  "cvsd",
  NULL
};

static sox_format_handler_t sox_cvsd_format = {
  cvsdnames,
  0,
  sox_cvsdstartread,
  sox_cvsdread,
  sox_cvsdstopread,
  sox_cvsdstartwrite,
  sox_cvsdwrite,
  sox_cvsdstopwrite,
  sox_format_nothing_seek
};

const sox_format_handler_t *sox_cvsd_format_fn(void);

const sox_format_handler_t *sox_cvsd_format_fn(void)
{
    return &sox_cvsd_format;
}
