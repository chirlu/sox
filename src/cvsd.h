/*
 *      DVMS format module (implementation in cvsd.c)
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

int sox_cvsdstartread(sox_format_t * ft);
int sox_cvsdstartwrite(sox_format_t * ft);
sox_size_t sox_cvsdread(sox_format_t * ft, sox_sample_t *buf, sox_size_t nsamp);
sox_size_t sox_cvsdwrite(sox_format_t * ft, const sox_sample_t *buf, sox_size_t nsamp);
int sox_cvsdstopread(sox_format_t * ft);
int sox_cvsdstopwrite(sox_format_t * ft);

int sox_dvmsstartread(sox_format_t * ft);
int sox_dvmsstartwrite(sox_format_t * ft);
int sox_dvmsstopwrite(sox_format_t * ft);
