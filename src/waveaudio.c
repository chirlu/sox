/* libSoX (c) 2009 SoX contributors
 * Copyright (c) 2009 Pavel Karneliuk pavel_karneliuk@users.sourceforge.net
 * Implementation of audio output driver for Windows
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

#include "sox_i.h"
#ifdef HAVE_WAVEAUDIO

#include <assert.h>
#include <windows.h>
#include <mmsystem.h>

#define num_buffers 16
typedef struct {
    HWAVEOUT    waveout;
    WAVEFORMATEX  format;

    HGLOBAL handle_data;
    HGLOBAL handle_wavheader;

    HPSTR   ptr_data[num_buffers];
    WAVEHDR*   ptr_wavheader;

    HANDLE need_more_data_blocks;
} priv_t;

void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR instance, DWORD_PTR Param1, DWORD_PTR Param2);

int setup_write(sox_format_t* ft)
{
    DWORD buf_len;
    int i;
    priv_t *priv = (priv_t *)ft->priv;
    if(priv == NULL) return SOX_EOF;

    ft->signal.precision = 16;

    priv->format.wFormatTag = WAVE_FORMAT_PCM;
    priv->format.nChannels = ft->signal.channels;
    priv->format.nSamplesPerSec = ft->signal.rate;
    priv->format.wBitsPerSample = sizeof(int16_t)*8;
    priv->format.nAvgBytesPerSec = priv->format.nSamplesPerSec * priv->format.wBitsPerSample/8;
    priv->format.nBlockAlign = (priv->format.nChannels*priv->format.wBitsPerSample)/8;
    priv->format.cbSize = 0;

    buf_len = sox_globals.bufsiz * sizeof(int16_t);

    priv->handle_data = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, buf_len * num_buffers);
    priv->ptr_data[0]      = (HPSTR) GlobalLock(priv->handle_data);
    priv->handle_wavheader         = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, (DWORD)sizeof(WAVEHDR) * num_buffers);
    priv->ptr_wavheader = (WAVEHDR*) GlobalLock(priv->handle_wavheader);
    memset(priv->ptr_wavheader, 0, sizeof(WAVEHDR) * num_buffers);
    for(i=0; i<num_buffers; i++)
    {
        priv->ptr_data[i] = priv->ptr_data[0] + buf_len*i;

        priv->ptr_wavheader[i].lpData = priv->ptr_data[i];
        priv->ptr_wavheader[i].dwBufferLength = buf_len;
        priv->ptr_wavheader[i].dwFlags = 0L;
        priv->ptr_wavheader[i].dwLoops = 0L;
    }

    priv->waveout = NULL;

    waveOutOpen((LPHWAVEOUT)&priv->waveout, WAVE_MAPPER, &priv->format,
                               (DWORD_PTR)waveOutProc, (DWORD_PTR)priv, CALLBACK_FUNCTION);

    priv->need_more_data_blocks = CreateEvent(NULL, TRUE, TRUE, NULL);

    return priv->waveout ? SOX_SUCCESS : SOX_EOF;
}

int stop_write(sox_format_t* ft)
{
    priv_t *priv = (priv_t *)ft->priv;
    if(priv == NULL) return SOX_EOF;

    while(WAVERR_STILLPLAYING == waveOutClose(priv->waveout))
    {
/*        // terminate
        if( priv->is_cancelled() )
        {
            waveOutReset(priv->waveout);
        }
        else priv->update_pos();*/
        Sleep(50);
    }

    CloseHandle(priv->need_more_data_blocks);

    GlobalUnlock(priv->handle_wavheader);
    GlobalFree(priv->handle_wavheader);

    GlobalUnlock(priv->handle_data);
    GlobalFree(priv->handle_data);

    return SOX_SUCCESS;
}

static size_t write(sox_format_t * ft, const sox_sample_t* buf, size_t len)
{
    int i;
    int clips = 0;
    size_t j;
    WAVEHDR* header = NULL;
    MMRESULT res = 0;
    priv_t *priv = (priv_t *)ft->priv;
    if(priv == NULL) return SOX_EOF;

    while(header == NULL)
    {
        // find first free header
        for(i=0; i<num_buffers; i++)
        {
            if(priv->ptr_wavheader[i].dwFlags == 0 || priv->ptr_wavheader[i].dwFlags & WHDR_DONE )
            {
                header = &priv->ptr_wavheader[i];
                break;
            }
        }

        if(header == NULL) // not found free data blocks
        {
            while(WAIT_TIMEOUT == WaitForSingleObject(priv->need_more_data_blocks, 50))
            {
/*                if(task->is_cancelled())
                {
                    waveOutReset(task->waveout);
                    *osamp = 0;
                    return SOX_SUCCESS;
                }
                else priv->update_pos();*/
            }
            ResetEvent(priv->need_more_data_blocks);
        }
    }

    /* put ibuf into data block for playback */
    if(header)
    {
        res = waveOutUnprepareHeader(priv->waveout, header, sizeof(WAVEHDR));
        assert(MMSYSERR_NOERROR == res);

        for(j=0; j< len; ++j)
        {
            SOX_SAMPLE_LOCALS;
            ((int16_t *)header->lpData)[j] = SOX_SAMPLE_TO_SIGNED_16BIT(buf[j],clips);
        }

        header->dwBufferLength = len * sizeof(int16_t);
        header->dwFlags = 0;

        res = waveOutPrepareHeader(priv->waveout, header, sizeof(WAVEHDR));
        assert(MMSYSERR_NOERROR == res);

        waveOutWrite(priv->waveout, header, sizeof(WAVEHDR));
        assert(MMSYSERR_NOERROR == res);
    }

    return len;
}

void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR instance, DWORD_PTR Param1, DWORD_PTR Param2)
{
    priv_t* priv = (priv_t*) instance;
    /* unlock Sox pipeline if some data block has been processed*/
    if( uMsg == WOM_DONE )
    {
        SetEvent(priv->need_more_data_blocks);
    }
}

LSX_FORMAT_HANDLER(waveaudio)
{
  static const char * const names[] = {"waveaudio", NULL};
  static unsigned const write_encodings[] = { SOX_ENCODING_UNKNOWN, 16, 0, 0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "Windows Multimedia Audio Output", names, 
    SOX_FILE_DEVICE | SOX_FILE_NOSTDIO,
    NULL, NULL, NULL,
    setup_write, write, stop_write,
    NULL, write_encodings, NULL, sizeof(priv_t)
  };
  return &handler;
}
#endif
