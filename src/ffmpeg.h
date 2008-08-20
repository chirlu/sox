/* This library is free software; you can redistribute it and/or modify it
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

#if defined __GNUC__
  #pragma GCC system_header
#elif defined __SUNPRO_C
  #pragma disable_warn
#elif defined _MSC_VER
  #pragma warning(push, 1)
#endif

#if HAVE_LIBAVFORMAT_AVFORMAT_H
#include <libavformat/avformat.h>
#else
#include <ffmpeg/avformat.h>
#endif

#if defined __SUNPRO_C
  #pragma enable_warn
#elif defined _MSC_VER
  #pragma warning(pop)
#endif
