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

#include "win32-ltdl.h"
#include <stdio.h>
#include <stdlib.h>
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

#ifndef _countof
#define _countof(A) (sizeof(A)/sizeof((A)[0]))
#endif

static DWORD
s_dwLastError;

static char
s_szLastError[MAX_PATH];

static char
s_szSearchPath[MAX_PATH];

static int
CopyPath(
    const char* szSource,
    char* szDest,
    unsigned cchDest,
    int chStop)
{
    unsigned i = 0;
    char ch;
    if (szSource != 0 && cchDest != 0)
    {
        for (; i < cchDest - 1 && (ch = szSource[i]) != 0 && ch != chStop; i++)
        {
            if (ch == '/')
            {
                ch = '\\';
            }

            szDest[i] = ch;
        }
    }

    if (cchDest != 0)
    {
        szDest[i] = 0;
    }

    return i;
}

static lt_dlhandle
LoadLib(
    const char* szFileName,
    const char* const szExtensions[])
{
    lt_dlhandle hMod = 0;
    char szFull[MAX_PATH];
    const char* szExt;
    unsigned iPath;
    unsigned iExtension;
    unsigned iCur;
    unsigned iEnd = 0;
    unsigned cPaths = 0;
    const char* szPaths[2];

    if (!szFileName || !szFileName[0])
    {
        s_dwLastError = ERROR_INVALID_PARAMETER;
        goto Done;
    }

    /* Search starting with current directory */
    szPaths[cPaths++] = "";

    /* If the file name doesn't already have a path, also search the search path. */
    if (s_szSearchPath[0] &&
        szFileName[0] != '/' &&
        szFileName[0] != '\\' &&
        szFileName[1] != ':')
    {
        szPaths[cPaths++] = s_szSearchPath;
    }

    for (iPath = 0; !hMod && iPath < cPaths; iPath++)
    {
        iEnd = 0;

        /* Add search path only if non-empty and filename does not
         * contain absolute path.
         */
        if (szPaths[iPath][0])
        {
            iEnd += CopyPath(szPaths[iPath], szFull, _countof(szFull), 0);

            if (szFull[iEnd - 1] != '\\' && iEnd < _countof(szFull))
            {
                szFull[iEnd++] = '\\';
            }
        }

        iEnd += CopyPath(szFileName, &szFull[iEnd], _countof(szFull)-iEnd, 0);
        if (iEnd == _countof(szFull))
        {
            s_dwLastError = ERROR_BUFFER_OVERFLOW;
            goto Done;
        }

        for (iExtension = 0; !hMod && szExtensions[iExtension]; iExtension++)
        {
            szExt = szExtensions[iExtension];
            for (iCur = 0; szExt[iCur] && iEnd + iCur < _countof(szFull); iCur++)
            {
                szFull[iEnd + iCur] = szExt[iCur];
            }

            if (iEnd + iCur >= _countof(szFull))
            {
                s_dwLastError = ERROR_BUFFER_OVERFLOW;
                goto Done;
            }
            else
            {
                szFull[iEnd + iCur] = 0;
            }

            hMod = (lt_dlhandle)LoadLibraryA(szFull);
        }
    }

    s_dwLastError = hMod ? 0 : GetLastError();

Done:
    return hMod;
}

int
lt_dlinit(void)
{
    int cErrors = 0;
    s_dwLastError = 0;
    return cErrors;
}

int
lt_dlexit(void)
{
    int cErrors = 0;
    s_dwLastError = 0;
    s_szSearchPath[0] = 0;
    return cErrors;
}

int
lt_dlsetsearchpath(const char *szSearchPath)
{
    int cErrors=0;
    s_dwLastError = 0;
    s_szSearchPath[0] = 0;
    if (szSearchPath)
    {
        int iEnd = CopyPath(szSearchPath, s_szSearchPath, _countof(s_szSearchPath), 0);
        if (szSearchPath[iEnd])
        {
            /* path was truncated. */
            cErrors++;
            s_dwLastError = ERROR_BUFFER_OVERFLOW;
            s_szSearchPath[0] = 0;
        }
    }

    return cErrors;
}

int
lt_dlforeachfile(
    const char *szSearchPath,
    int (*pfCallback)(const char *szFileName, lt_ptr pData),
    lt_ptr pData)
{
    char szExePath[MAX_PATH];
    char szOnePath[MAX_PATH];
    int cErrors = 0;
    unsigned iSearchPath = 0;
    unsigned iOnePath;
    unsigned iExePath = 0;
    unsigned cchCopied;
    HANDLE hFind;
    WIN32_FIND_DATAA data;

    szExePath[0] = 0;

    if (pfCallback == 0)
    {
        s_dwLastError = ERROR_INVALID_PARAMETER;
        cErrors++;
        goto Done;
    }

    if (szSearchPath != 0)
    {
        while (1)
        {
            while (szSearchPath[iSearchPath] == LT_PATHSEP_CHAR)
            {
                iSearchPath++;
            }

            if (szSearchPath[iSearchPath] == 0)
            {
                s_dwLastError = 0;
                break;
            }

            if (szSearchPath[iSearchPath] == '.' &&
                (szSearchPath[iSearchPath + 1] == '\\' || szSearchPath[iSearchPath + 1] == '/'))
            {
                if (szExePath[0] == 0)
                {
                    iExePath = GetModuleFileNameA(0, szExePath, _countof(szExePath));
                    if (iExePath == 0)
                    {
                        s_dwLastError = GetLastError();
                        cErrors++;
                        goto Done;
                    }
                    else if (iExePath == _countof(szExePath))
                    {
                        s_dwLastError = ERROR_BUFFER_OVERFLOW;
                        cErrors++;
                        goto Done;
                    }

                    while (iExePath > 0 && szExePath[iExePath - 1] != '\\')
                    {
                        iExePath--;
                    }

                    if (iExePath > 0)
                    {
                        iExePath--;
                    }

                    szExePath[iExePath] = 0;
                }

                strcpy(szOnePath, szExePath);
                iOnePath = iExePath;
                iSearchPath++;
            }
            else
            {
                iOnePath = 0;
            }

            cchCopied = CopyPath(
                szSearchPath + iSearchPath,
                szOnePath + iOnePath,
                _countof(szOnePath) - iOnePath,
                LT_PATHSEP_CHAR);
            iSearchPath += cchCopied;
            iOnePath += cchCopied;

            if (0 < iOnePath && iOnePath + 1 < _countof(szOnePath) && szOnePath[iOnePath - 1] != '\\')
            {
                szOnePath[iOnePath++] = '\\';
            }

            if (iOnePath + 1 >= _countof(szOnePath))
            {
                s_dwLastError = ERROR_BUFFER_OVERFLOW;
                cErrors++;
                goto Done;
            }

            szOnePath[iOnePath++] = '*';
            szOnePath[iOnePath] = 0;

            hFind = FindFirstFileA(szOnePath, &data);
            while (hFind != INVALID_HANDLE_VALUE)
            {
                if (0 == (data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_OFFLINE)))
                {
                    cErrors = pfCallback(data.cFileName, pData);
                    if (cErrors)
                    {
                        s_dwLastError = ERROR_CANCELLED;
                        FindClose(hFind);
                        goto Done;
                    }
                }

                if (!FindNextFileA(hFind, &data))
                {
                    s_dwLastError = ERROR_SUCCESS;
                    FindClose(hFind);
                    hFind = INVALID_HANDLE_VALUE;
                }
            }
        }
    }

Done:
    return cErrors;
}

lt_dlhandle
lt_dlopen(
    const char *szFileName)
{
    const char* const szExtensions[] = { ".", 0 };
    lt_dlhandle hModule = LoadLib(szFileName, szExtensions);
    return hModule;
}

lt_dlhandle
lt_dlopenext(
    const char *szFileName)
{
    const char* const szExtensions[] = { ".", ".la.", ".dll.", 0 };
    lt_dlhandle hModule = LoadLib(szFileName, szExtensions);
    return hModule;
}

int
lt_dlclose(
    lt_dlhandle handle)
{
    int cErrors = 0;
    if (FreeLibrary((HMODULE)handle))
    {
        s_dwLastError = 0;
    }
    else
    {
        s_dwLastError = GetLastError();
        cErrors++;
    }

    return cErrors;
}

lt_ptr
lt_dlsym(
    lt_dlhandle hModule,
    const char *szSymbolName)
{
    union {FARPROC fn; lt_ptr ptr;} func;
    func.fn = GetProcAddress((HMODULE)hModule, szSymbolName);
    s_dwLastError = func.fn ? 0 : GetLastError();
    return func.ptr;
}

const char *
lt_dlerror(void)
{
    if (!FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        0,
        s_dwLastError,
        0,
        s_szLastError,
        _countof(s_szLastError),
        0))
    {
        _snprintf(s_szLastError, _countof(s_szLastError), "Unknown error %u occurred.", s_dwLastError);
    }

    return s_szLastError;
}
