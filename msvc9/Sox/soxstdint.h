/* libSoX stub file for MSVC9: (c) 2009 SoX contributors
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

#include "soxconfig.h"

#ifdef HAVE_STDINT_H
  #include <stdint.h>
#else
  #ifdef HAVE_INTTYPES_H
    #include <inttypes.h>
  #else
    #ifdef _MSC_VER
      typedef __int64 int64_t;
      typedef unsigned __int64 uint64_t;
    #else
      typedef long long int64_t;
      typedef unsigned long long uint64_t;
    #endif
    typedef char int8_t;
    typedef int int32_t;
    typedef short int16_t;
    typedef unsigned char uint8_t;
    typedef unsigned int uint32_t;
    typedef unsigned short uint16_t;

    typedef int8_t  INT8;
    typedef int16_t INT16;
    typedef int32_t INT32;
    typedef int64_t INT64;
  #endif
#endif
