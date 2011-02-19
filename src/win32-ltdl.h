/* libSoX minimal libtool-ltdl for MS-Windows: (c) 2009 SoX contributors
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

#ifndef LTDL_H
#define LTDL_H 1

#ifdef __cplusplus
extern "C" {
#endif

#define LT_PATHSEP_CHAR ';'
#define LT_DIRSEP_CHAR '\\'

struct lt__handle;
typedef struct lt__handle *lt_dlhandle;
typedef void *lt_ptr;

int
lt_dlinit(void);

int
lt_dlexit(void);

int
lt_dlsetsearchpath(const char *search_path);

int
lt_dlforeachfile(
  const char *szSearchPath,
  int (*pfCallback)(const char *szFileName, lt_ptr pData),
  lt_ptr pData);

lt_dlhandle
lt_dlopen(
  const char *szFileName);

lt_dlhandle
lt_dlopenext(
  const char *szFileName);

lt_ptr
lt_dlsym(
  lt_dlhandle hModule,
  const char *szSymbolName);

const char *
lt_dlerror(void);

int
lt_dlclose(
  lt_dlhandle handle);

#ifdef __cplusplus
}
#endif

#endif /* ifndef LTDL_H */
