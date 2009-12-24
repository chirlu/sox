/* libSoX device driver: MS-Windows audio   (c) 2009 SoX contributors
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

#include <windows.h>
#include <mmsystem.h>

/* Larger means more latency (difference between the status line and the audio you hear),
 * but it means lower chance of stuttering/glitching. 2 buffers is usually enough. Use
 * 4 if you want to be extra-safe.
 */
#define num_buffers 4

typedef struct waveaudio_priv_t
{
  /* Handle to the input device (microphone, line in, etc.). NULL if playing. */
  HWAVEIN hin;

  /* Handle to the output device (speakers, line out, etc.). NULL if recording. */
  HWAVEOUT hout;

  /* Event that becomes signaled when a the system has finished processing a buffer. */
  HANDLE block_finished_event;

  /* Data transfer buffers. The lpData member of the first buffer points at
   * data[buf_len*sample_size*0], the second buffer's lpData points
   * data[buf_len*sample_size*1], etc. The dwUser field contains the number
   * of samples of this buffer that have already been processed.
   */
  WAVEHDR headers[num_buffers];

  /* The combined data area shared by all transfer buffers. */
  char * data;

  /* The number of samples that can fit into one transfer buffer. */
  size_t buf_len;

  /* Index of the buffer that we're currently processing. For playback, this is the buffer
   * that will receive the next samples. For recording, this is the buffer from which we'll
   * be getting the next samples. If no buffers are ready for processing, this is the buffer
   * that will be the next to become ready.
   */
  unsigned current;

  /* If there has been an error, this has the Win32 error code. Otherwise, this is 0. */
  DWORD error;
} priv_t;

static void fail(sox_format_t* ft, DWORD code, const char* context)
{
  char message[256];
  DWORD formatMessageOk = FormatMessageA(
    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    code,
    0,
    message,
    sizeof(message) / sizeof(message[0]),
    NULL);
  if (formatMessageOk)
    lsx_fail_errno(ft, SOX_EOF, "WaveAudio %s failed with code %d: %s", context, (int)code, message);
  else
    lsx_fail_errno(ft, SOX_EOF, "WaveAudio %s failed with unrecognized MMSYSERR code %d.", context, (int)code);
}

static int stop(sox_format_t* ft)
{
  priv_t *priv = (priv_t*)ft->priv;
  if (priv == NULL) return SOX_EOF;

  if (priv->hin)
  {
    priv->error = waveInReset(priv->hin);
    priv->error = waveInClose(priv->hin);
  }
  
  if (priv->hout && !priv->error)
  {
    while ((priv->error = waveOutClose(priv->hout)) == WAVERR_STILLPLAYING)
    {
      WaitForSingleObject(priv->block_finished_event, INFINITE);
    }
  }
  else if (priv->hout)
  {
    priv->error = waveOutReset(priv->hout);
    priv->error = waveOutClose(priv->hout);
  }

  if (priv->block_finished_event)
    CloseHandle(priv->block_finished_event);

  if (priv->data)
    free(priv->data);

  return SOX_SUCCESS;
}

static int start(sox_format_t* ft)
{
  size_t i;
  UINT dev;
  WAVEFORMATEX fmt;
  int recording = ft->mode == 'r';
  priv_t *priv = (priv_t*)ft->priv;
  if (priv == NULL) return SOX_EOF;

  /* Handle AUDIODEV device selection:
   * NULL, blank, or "default" gets you the default device (WAVE_MAPPER = -1).
   * An integer value gets you the device with that device number, if it exists (counting starts at 0).
   * A string gets you the device with that name, if it exists.
   */
  if (ft->filename == 0 || ft->filename == 0 || !strcasecmp("default", ft->filename))
  {
    dev = WAVE_MAPPER;
  }
  else
  {
    WAVEINCAPSA incaps;
    WAVEOUTCAPSA outcaps;
    const char *dev_name;
    char* dev_num_end;
    dev = strtoul(ft->filename, &dev_num_end, 0);
    if (dev_num_end[0] == 0)
    {
      if (recording)
        priv->error = waveInGetDevCapsA(dev, &incaps, sizeof(incaps));
      else
        priv->error = waveOutGetDevCapsA(dev, &outcaps, sizeof(outcaps));

      if (priv->error)
      {
        lsx_fail_errno(ft, ENODEV, "WaveAudio was unable to find the AUDIODEV %s device \"%s\".", recording ? "input" : "output", ft->filename);
        return SOX_EOF;
      }
    }
    else
    {
      UINT dev_count = recording ? waveInGetNumDevs() : waveOutGetNumDevs();
      for (dev = -1; dev == WAVE_MAPPER || dev < dev_count; dev++)
      {
        if (recording)
        {
          priv->error = waveInGetDevCapsA(dev, &incaps, sizeof(incaps));
          dev_name = incaps.szPname;
        }
        else
        {
          priv->error = waveOutGetDevCapsA(dev, &outcaps, sizeof(outcaps));
          dev_name = outcaps.szPname;
        }

        if (priv->error)
        {
          fail(ft, priv->error, recording ? "waveInGetDevCapsA" : "waveOutGetDevCapsA");
          return SOX_EOF;
        }

        if (!strncasecmp(ft->filename, dev_name, 31))
          break;
      }

      if (dev == dev_count)
      {
        lsx_fail_errno(ft, ENODEV, "WaveAudio was unable to find the AUDIODEV %s device \"%s\".", recording ? "input" : "output", ft->filename);
        return SOX_EOF;
      }
    }
  }

  priv->buf_len = sox_globals.bufsiz;
  priv->data = lsx_malloc(priv->buf_len * sizeof(int16_t) * num_buffers);
  if (!priv->data)
  {
    lsx_fail_errno(ft, SOX_ENOMEM, "Out of memory.");
    return SOX_EOF;
  }

  priv->block_finished_event = CreateEventA(NULL, FALSE, FALSE, NULL);
  if (!priv->block_finished_event)
  {
    priv->error = GetLastError();
    fail(ft, priv->error, "CreateEventA");
    stop(ft);
    return SOX_EOF;
  }

  /* Allow simulation of 8-bit audio playback. */
  ft->signal.precision = ft->signal.precision >= 16 ? 16 : 8;

  fmt.wFormatTag = WAVE_FORMAT_PCM;
  fmt.nChannels = ft->signal.channels;
  fmt.nSamplesPerSec = (DWORD)ft->signal.rate;
  fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * sizeof(int16_t);
  fmt.nBlockAlign = fmt.nChannels*sizeof(int16_t);
  fmt.wBitsPerSample = sizeof(int16_t)*8;
  fmt.cbSize = 0;

  if (recording)
    priv->error = waveInOpen(&priv->hin, dev, &fmt, (DWORD_PTR)priv->block_finished_event, 0, CALLBACK_EVENT);
  else
    priv->error = waveOutOpen(&priv->hout, dev, &fmt, (DWORD_PTR)priv->block_finished_event, 0, CALLBACK_EVENT);

  if (priv->error != MMSYSERR_NOERROR)
  {
    fail(ft, priv->error, recording ? "waveInOpen" : "waveOutOpen");
    stop(ft);
    return SOX_EOF;
  }

  for (i = 0; i < num_buffers; i++)
  {
    priv->headers[i].lpData = priv->data + priv->buf_len * sizeof(int16_t) * i;
    priv->headers[i].dwBufferLength = priv->buf_len * sizeof(int16_t);

    if (recording)
      priv->error = waveInPrepareHeader(priv->hin, &priv->headers[i], sizeof(priv->headers[i]));
    else
      priv->error = waveOutPrepareHeader(priv->hout, &priv->headers[i], sizeof(priv->headers[i]));

    if (priv->error != MMSYSERR_NOERROR)
    {
      fail(ft, priv->error, recording ? "waveInPrepareHeader" : "waveOutPrepareHeader");
      stop(ft);
      return SOX_EOF;
    }

    if (recording)
    {
      priv->error = waveInAddBuffer(priv->hin, &priv->headers[i], sizeof(priv->headers[i]));
      if (priv->error != MMSYSERR_NOERROR)
      {
        fail(ft, priv->error, "waveInAddBuffer");
        stop(ft);
        return SOX_EOF;
      }
    }
  }

  if (recording)
  {
    priv->error = waveInStart(priv->hin);
    if (priv->error != MMSYSERR_NOERROR)
    {
      fail(ft, priv->error, "waveInStart");
      stop(ft);
      return SOX_EOF;
    }
  }

  return SOX_SUCCESS;
}

static size_t read(sox_format_t * ft, sox_sample_t* buf, size_t len)
{
  size_t copied = 0;
  priv_t *priv = (priv_t*)ft->priv;
  if (priv == NULL) return SOX_EOF;

  while (!priv->error && copied < len)
  {
    LPWAVEHDR header = &priv->headers[priv->current];
    if (0 == (header->dwFlags & WHDR_INQUEUE) ||
      0 != (header->dwFlags & WHDR_DONE))
    {
      size_t length = header->dwBytesRecorded / sizeof(int16_t);
      size_t ready = min(len - copied, length - header->dwUser);
      size_t i;

      for (i = 0; i < ready; ++i)
      {
        buf[copied++] = SOX_SIGNED_16BIT_TO_SAMPLE(((int16_t *)header->lpData)[header->dwUser++], dummy);
      }

      if (header->dwUser == length)
      {
        priv->error = waveInAddBuffer(priv->hin, header, sizeof(*header));
        priv->current = (priv->current + 1) % num_buffers;
        priv->headers[priv->current].dwUser = 0;
        if (priv->error)
        {
          fail(ft, priv->error, "waveInAddBuffer");
          copied = 0;
        }
      }
    }
    else
    {
      WaitForSingleObject(priv->block_finished_event, INFINITE);
    }
  }

  return copied;
}

static size_t write(sox_format_t * ft, const sox_sample_t* buf, size_t len)
{
  unsigned clips = 0;
  size_t copied = 0;
  priv_t *priv = (priv_t*)ft->priv;
  if (priv == NULL) return SOX_EOF;

  while (!priv->error && copied < len)
  {
    LPWAVEHDR header = &priv->headers[priv->current];
    if (0 == (header->dwFlags & WHDR_INQUEUE) ||
      0 != (header->dwFlags & WHDR_DONE))
    {
      size_t ready = min(len - copied, priv->buf_len - header->dwUser);
      size_t i;

      if (ft->signal.precision != 8)
      {
        /* Normal case: Play with 16-bit resolution. */
        for (i = 0; i < ready; ++i)
        {
          SOX_SAMPLE_LOCALS;
          ((int16_t *)header->lpData)[header->dwUser++] = SOX_SAMPLE_TO_SIGNED_16BIT(buf[copied++], clips);
        }
      }
      else
      {
        /* Special case: Simulate 8-bit audio playback. */
        for (i = 0; i < ready; ++i)
        {
          SOX_SAMPLE_LOCALS;
          ((int16_t *)header->lpData)[header->dwUser++] = SOX_SAMPLE_TO_SIGNED_8BIT(buf[copied++], clips) << 8;
        }
      }

      header->dwBufferLength = header->dwUser * sizeof(int16_t);
      priv->error = waveOutWrite(priv->hout, header, sizeof(*header));
      priv->current = (priv->current + 1) % num_buffers;
      priv->headers[priv->current].dwUser = 0;

      if (priv->error)
      {
        fail(ft, priv->error, "waveOutWrite");
        copied = 0;
      }
    }
    else
    {
      WaitForSingleObject(priv->block_finished_event, INFINITE);
    }
  }

  return copied;
}

LSX_FORMAT_HANDLER(waveaudio)
{
  static const char * const names[] = {"waveaudio", NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_SIGN2, 16, 0,
    SOX_ENCODING_SIGN2, 8, 0,
    0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
  "Windows Multimedia Audio", names, 
  SOX_FILE_DEVICE | SOX_FILE_NOSTDIO,
  start, read, stop,
  start, write, stop,
  NULL, write_encodings, NULL, sizeof(priv_t)
  };
  return &handler;
}
